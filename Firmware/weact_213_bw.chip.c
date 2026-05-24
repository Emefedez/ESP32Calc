#include "wokwi-api.h"
#include <stdlib.h>
#include <string.h>

#define WIDTH    128
#define HEIGHT   250
#define ROW_BYTES (WIDTH / 8)
#define RAM_SIZE  (ROW_BYTES * HEIGHT)

/* SSD1680 emulation for WeAct 2.13" B/W e-paper (128×250).
 *
 * Pins: 1=BUSY 2=RST 3=DC 4=CS 5=CLK 6=DIN 7=VCC 8=GND
 * SPI mode 0, MSB-first, DC=LOW → command, DC=HIGH → data.
 */

typedef struct {
  /* pin handles */
  pin_t      busy_pin, rst_pin, dc_pin, cs_pin;
  pin_t      clk_pin, din_pin, vcc_pin, gnd_pin;

  /* SPI */
  spi_dev_t  spi;

  /* frame buffers */
  uint8_t    bw_ram[RAM_SIZE];
  uint8_t    rd_ram[RAM_SIZE];

  /* address registers */
  uint16_t   x_start, x_end, y_start, y_end;
  uint16_t   x_ptr,   y_ptr;
  uint8_t    entry_mode;

  /* SPI buffer (reused across transactions – safe because our
     firmware never has overlapping CS-active transactions) */
  uint8_t    spi_buf[4096];

  /* command pipeline */
  uint8_t    cur_cmd;         /* last command byte received */
  uint8_t    cmd_arg[256];    /* argument bytes collected so far */
  uint32_t   cmd_arg_len;     /* argument bytes collected so far */

  /* display controller state */
  uint8_t    update_ctrl2;    /* reg 0x22 */

  /* framebuffer + busy timer */
  buffer_t   fb;
  timer_t    busy_timer;
  bool       busy;
} chip_t;


/* ------------------------------------------------------------------ *
 * helpers
 * ------------------------------------------------------------------ */

static void busy_on(chip_t *c, uint32_t ms) {
  c->busy = true;
  pin_write(c->busy_pin, HIGH);
  if (ms) timer_start(c->busy_timer, ms * 1000, false);
}

static void busy_off(chip_t *c) {
  c->busy = false;
  pin_write(c->busy_pin, LOW);
}

static void reset_chip(chip_t *c) {
  memset(c->bw_ram, 0xFF, RAM_SIZE);
  memset(c->rd_ram, 0xFF, RAM_SIZE);
  c->x_start  = 0;    c->x_end  = WIDTH - 1;
  c->y_start  = 0;    c->y_end  = HEIGHT - 1;
  c->x_ptr    = 0;    c->y_ptr  = 0;
  c->entry_mode = 0x03;
  c->cur_cmd    = 0;
  c->update_ctrl2 = 0;
  c->cmd_arg_len  = 0;
}


/* ------------------------------------------------------------------ *
 * RAM write with automatic pointer advance (data entry mode)
 * ------------------------------------------------------------------ */

static void ram_write(chip_t *c, const uint8_t *src, uint32_t n, bool bw) {
  uint8_t *ram = bw ? c->bw_ram : c->rd_ram;
  for (uint32_t i = 0; i < n; i++) {
    uint32_t off = c->y_ptr * ROW_BYTES + c->x_ptr / 8;
    if (off < RAM_SIZE) ram[off] = src[i];

    c->x_ptr += 8;
    if (c->x_ptr > c->x_end) {
      c->x_ptr  = c->x_start;
      c->y_ptr++;
      if (c->y_ptr > c->y_end) c->y_ptr = c->y_start;
    }
  }
}


/* ------------------------------------------------------------------ *
 * render BW RAM → framebuffer
 * ------------------------------------------------------------------ */

static void render(chip_t *c) {
  uint32_t row[WIDTH];                 /* 128 px × 4 bytes = 512 B */
  for (int y = 0; y < HEIGHT; y++) {
    for (int x = 0; x < WIDTH; x++) {
      int bi   = y * ROW_BYTES + x / 8;
      int bit  = 7 - (x % 8);
      uint8_t px = (c->bw_ram[bi] >> bit) & 1;
      /* RGBA: 0=black, 1=white */
      row[x] = px ? 0xFFFFFFFF : 0xFF000000;
    }
    buffer_write(c->fb, y * WIDTH * 4, row, sizeof(row));
  }
}


/* ------------------------------------------------------------------ *
 * process a complete command + its collected arguments
 * ------------------------------------------------------------------ */

static void exec_cmd(chip_t *c) {
  switch (c->cur_cmd) {

  case 0x01:                           /* DRIVER_OUTPUT_CONTROL – 3 B */
    break;

  case 0x11:                           /* DATA_ENTRY_MODE – 1 B */
    if (c->cmd_arg_len >= 1) c->entry_mode = c->cmd_arg[0];
    break;

  case 0x12:                           /* SW_RESET */
    reset_chip(c);
    busy_on(c, 50);
    return;                            /* keep BUSY high */

  case 0x18:                           /* TEMP_SENSOR – 1 B, ignore */
    break;

  case 0x21:                           /* DISPLAY_UPDATE_CTRL – 2 B */
    break;

  case 0x22:                           /* UPDATE_DISPLAY_CTRL2 – 1 B */
    if (c->cmd_arg_len >= 1) c->update_ctrl2 = c->cmd_arg[0];
    break;

  case 0x20:                           /* MASTER_ACTIVATE */
    render(c);
    busy_on(c, c->update_ctrl2 == 0xF7 ? 100 : 50);
    return;

  case 0x24:                           /* WRITE_BW_DATA */
    if (c->cmd_arg_len) ram_write(c, c->cmd_arg, c->cmd_arg_len, true);
    break;

  case 0x26:                           /* WRITE_RED_DATA */
    if (c->cmd_arg_len) ram_write(c, c->cmd_arg, c->cmd_arg_len, false);
    break;

  case 0x32:                           /* WRITE_LUT – 153 B, ignore */
    break;

  case 0x3C:                           /* BORDER_WAVEFORM – 1 B */
    break;

  case 0x44:                           /* SET_RAM_X_POS – 2 B */
    if (c->cmd_arg_len >= 2) {
      c->x_start = c->cmd_arg[0] * 8;
      c->x_end   = c->cmd_arg[1] * 8 + 7;
    }
    break;

  case 0x45:                           /* SET_RAM_Y_POS – 4 B */
    if (c->cmd_arg_len >= 4) {
      c->y_start = c->cmd_arg[0] | (c->cmd_arg[1] << 8);
      c->y_end   = c->cmd_arg[2] | (c->cmd_arg[3] << 8);
    }
    break;

  case 0x4E:                           /* SET_RAM_X_COUNTER – 1 B */
    if (c->cmd_arg_len >= 1) c->x_ptr = c->cmd_arg[0] * 8;
    break;

  case 0x4F:                           /* SET_RAM_Y_COUNTER – 2 B */
    if (c->cmd_arg_len >= 2) {
      c->y_ptr = c->cmd_arg[0] | (c->cmd_arg[1] << 8);
    }
    break;

  case 0x10:                           /* DEEP_SLEEP */
    break;
  }

  c->cmd_arg_len = 0;
}


/* ------------------------------------------------------------------ *
 * SPI done callback
 * ------------------------------------------------------------------ */

static void spi_done(void *user, uint8_t *buf, uint32_t count) {
  chip_t *c = (chip_t *)user;
  if (!count) goto next;

  if (pin_read(c->dc_pin) == LOW) {
    /* command byte — execute any pending command first */
    exec_cmd(c);
    c->cur_cmd      = buf[0];
    c->cmd_arg_len  = 0;
  } else if (c->cur_cmd == 0x24 || c->cur_cmd == 0x26) {
    /* 0x24/0x26 data written directly to RAM (avoids 256B cmd_arg limit) */
    ram_write(c, buf, count, c->cur_cmd == 0x24);
  } else {
    /* other command data – buffer it (small: ≤ 4 bytes typically) */
    uint32_t room = sizeof(c->cmd_arg) - c->cmd_arg_len;
    uint32_t copy = count < room ? count : room;
    memcpy(c->cmd_arg + c->cmd_arg_len, buf, copy);
    c->cmd_arg_len += copy;
  }

next:
  if (pin_read(c->cs_pin) == LOW) {
    spi_start(c->spi, buf, 4096);
  }
}


/* ------------------------------------------------------------------ *
 * CS pin change  (active low)
 * ------------------------------------------------------------------ */

static void cs_edge(void *user, pin_t pin, uint32_t val) {
  (void)pin;
  chip_t *c = (chip_t *)user;
  if (val == LOW) {
    spi_start(c->spi, c->spi_buf, sizeof(c->spi_buf));
  } else {
    spi_stop(c->spi);
    exec_cmd(c);                  /* flush any pending command */
  }
}


/* ------------------------------------------------------------------ *
 * BUSY timer expiry → release BUSY
 * ------------------------------------------------------------------ */

static void timer_done(void *user) {
  busy_off((chip_t *)user);
}


/* ================================================================== *
 * Entry point
 * ================================================================== */

/* shared pin names for init */
static const char *PIN_NAMES[] = {"BUSY","RST","DC","CS","CLK","DIN","VCC","GND"};

void chip_init(void) {
  chip_t *c = calloc(1, sizeof(*c));

  c->busy_pin = pin_init(PIN_NAMES[0], OUTPUT);
  c->rst_pin  = pin_init(PIN_NAMES[1], INPUT);
  c->dc_pin   = pin_init(PIN_NAMES[2], INPUT);
  c->cs_pin   = pin_init(PIN_NAMES[3], INPUT);
  c->clk_pin  = pin_init(PIN_NAMES[4], INPUT);
  c->din_pin  = pin_init(PIN_NAMES[5], INPUT);
  c->vcc_pin  = pin_init(PIN_NAMES[6], INPUT);
  c->gnd_pin  = pin_init(PIN_NAMES[7], INPUT);

  pin_write(c->busy_pin, LOW);

  spi_config_t sc = {
    .sck       = c->clk_pin,
    .mosi      = c->din_pin,
    .miso      = NO_PIN,
    .mode      = 0,
    .done      = spi_done,
    .user_data = c,
  };
  c->spi = spi_init(&sc);

  pin_watch_config_t pw = {
    .user_data  = c,
    .edge       = BOTH,
    .pin_change = cs_edge,
  };
  pin_watch(c->cs_pin, &pw);

  { uint32_t w = WIDTH, h = HEIGHT; c->fb = framebuffer_init(&w, &h); }

  timer_config_t tc = {
    .callback  = timer_done,
    .user_data = c,
  };
  c->busy_timer = timer_init(&tc);

  reset_chip(c);
}
