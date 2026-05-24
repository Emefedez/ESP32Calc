#include "storage/sd_storage.h"

#include <sys/stat.h>

#include "app_config.h"
#include "driver/sdspi_host.h"
#include "driver/spi_common.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "pins.h"
#include "sdmmc_cmd.h"

namespace esp32calc {
namespace {
constexpr const char* TAG = "sd";
}

esp_err_t SdStorage::mount() {
  spi_bus_config_t bus_cfg {};
  bus_cfg.mosi_io_num = pins::kSdMosi;
  bus_cfg.miso_io_num = pins::kSdMiso;
  bus_cfg.sclk_io_num = pins::kSdSclk;
  bus_cfg.quadwp_io_num = -1;
  bus_cfg.quadhd_io_num = -1;
  bus_cfg.max_transfer_sz = 4096;

  esp_err_t err = spi_bus_initialize(pins::kSdSpiHost, &bus_cfg, SPI_DMA_CH_AUTO);
  if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
    ESP_LOGE(TAG, "initialize sd spi failed: %s (0x%x)", esp_err_to_name(err), err);
    return err;
  }

  sdmmc_host_t host = SDSPI_HOST_DEFAULT();
  host.slot = pins::kSdSpiHost;

  sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
  slot_config.gpio_cs = pins::kSdCs;
  slot_config.host_id = pins::kSdSpiHost;

  esp_vfs_fat_sdmmc_mount_config_t mount_config {};
  mount_config.format_if_mount_failed = false;
  mount_config.max_files = 5;
  mount_config.allocation_unit_size = 16 * 1024;

  sdmmc_card_t* card = nullptr;
  err = esp_vfs_fat_sdspi_mount(config::kSdMountPoint,
                                &host,
                                &slot_config,
                                &mount_config,
                                &card);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "card not mounted: %s", esp_err_to_name(err));
    mounted_ = false;
    return err;
  }

  mounted_ = true;
  mkdir(config::kProgramsPath, 0775);
  ESP_LOGI(TAG, "mounted at %s", config::kSdMountPoint);
  return ESP_OK;
}

}  // namespace esp32calc

