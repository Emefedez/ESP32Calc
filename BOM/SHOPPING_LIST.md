# ESP32Calc shopping list

| Section | Estimated cost |
|---|---:|
| Power-module components | $2.14 |
| Calculator-board components | $5.25 |
| Display estimate | $5.00 |
| Battery estimate | $5.00 |
| Required FTDI adapter estimate | $2.50 |
| Bare PCBs after $6 new-user coupon | $4.00 |
| Estimated JLCPCB economical shipping | $8.00 |
| **Estimated cash cost for one build** | **$31.89** |
| Optional SD card, cable, jumpers, screws and self-print material | $19.00 |
| **All-in estimate if nothing is already owned** | **$50.89** |

## Supplier Comparison

| Supplier | Buy |
|---|---|
| JLCPCB | Both fabricated PCBs using the repository Gerber ZIPs |
| JLCPCB/LCSC | All board-mounted electronic components in one basket |
| AliExpress | Battery, FTDI adapter, and 2.13-inch B/W display using the user-provided links |
| Local / 3D printing service | Enclosure parts, buttons, screws, microSD, cable |


## 1. Fabricate the PCBs


| Build order | Needed | Order | Item | File to upload | Link |
|---:|---:|---:|---|---|---|
| 1 | 1 | 5 PCBs (typical minimum) | Power module PCB | [`power_gerber_v1.5.zip`](../power_board/power_board_pcb/power_gerber_v1.5.zip) | [JLCPCB quote/upload](https://jlcpcb.com/) |
| 2 | 1 | 5 PCBs (typical minimum) | Calculator main PCB | [`calculator_gerber_v1.5.zip`](../calculator_mainboard/calculator_pcb/calculator_gerber_v1.5.zip) | [JLCPCB quote/upload](https://jlcpcb.com/) |

If using PCB assembly, upload the matching BOM and pick-and-place file for each
board. 


## 2. Power module parts — build this first
-> This list uses only LCSC stock, the libreoffice document has everything, prices remain similar though.

The `Buy` quantity includes modest hand-assembly spares for very small passives.
Exact ICs, connectors, switches, inductors, fuse, and ESD parts should not be
substituted solely by package size.

| BOM qty | Buy | Ref. | Exact BOM part | Selected LCSC code | Live stock | Purchase link / action |
|---:|---:|---|---|---|---:|---|
| 3 | 5 | C1, C4, C7 | CL10A226MP8NUNE, 22 uF 0603 | C86295 | 146,960 | [Buy exact](https://www.lcsc.com/product-detail/C86295.html) |
| 2 | 5 | C2, C3 | CL10A106KP8NNNC, 10 uF 0603 | **C109457, CC0603MRX5R7BB106** | 32,650 | [Buy in-stock 10 uF, 16 V, X5R, 0603 replacement](https://www.lcsc.com/product-detail/C109457.html); [exact C19702 is out of stock](https://www.lcsc.com/product-detail/C19702.html) |
| 1 | 3 | C5 | CL10A226MQ8NRNC, 22 uF 0603 | C59461 | 172,760 | [Buy exact](https://www.lcsc.com/product-detail/C59461.html) |
| 2 | 5 | C6, C9 | CL10B104KB8NNNC, 100 nF 0603 | C1591 | 215,300 | [Buy exact](https://www.lcsc.com/product-detail/C1591.html) |
| 1 | 3 | C8 | CL10B105KA8NNNC, 1 uF 0603 | C29936 | 139,550 | [Buy exact](https://www.lcsc.com/product-detail/C29936.html) |
| 1 | 2 | F1 | 0603L150SLYR resettable fuse | C207018 | 2,115 | [Buy exact](https://www.lcsc.com/product-detail/C207018.html) |
| 1 | 2 | L1 | CMI201209U1R5KT, 1.5 uH 0805 | C139220 | 18,900 | [Buy exact](https://www.lcsc.com/product-detail/C139220.html) |
| 1 | 2 | LED1 | XL-1608SURC-06 red LED 0603 | C965799 | 7,547,000 | [Buy exact](https://www.lcsc.com/product-detail/C965799.html) |
| 1 | 3 | R1 | RMC04021.5K1%N, 1.5 kΩ 0402 | C269672 | 7,900 | [Buy exact](https://www.lcsc.com/product-detail/C269672.html) |
| 1 | 3 | R2 | FRC0603F1004TS, 1 MΩ 0603 | C2907003 | 858,900 | [Buy exact](https://www.lcsc.com/product-detail/C2907003.html) |
| 1 | 1 | R3 | FRC0603J102 TS, 1 kΩ 0603 | C2907113 | **5** | [Buy exact now](https://www.lcsc.com/product-detail/C2907113.html); stock is critically low |
| 1 | 3 | R4 | RC0603FR-07180KL, 180 kΩ 0603 | C123419 | 189,600 | [Buy exact](https://www.lcsc.com/product-detail/C123419.html) |
| 1 | 3 | R5 | FRC0603J202 TS, 2 kΩ 0603 | C2907137 | 7,673,400 | [Buy exact](https://www.lcsc.com/product-detail/C2907137.html) |
| 1 | 3 | R6 | RC0603FR-0747KL, 47 kΩ 1% 0603 | **C3016374, SCR0603F47K** | 200 | [Buy equivalent](https://www.lcsc.com/product-detail/C3016374.html); [exact C105579 is out of stock](https://www.lcsc.com/product-detail/C105579.html) |
| 2 | 5 | R7, R10 | RC0603FR-07100KL, 100 kΩ 1% 0603 | **C216797, CR-03FL7--100K** | 19,900 | [Buy equivalent](https://www.lcsc.com/product-detail/C216797.html); [exact C14675 is out of stock](https://www.lcsc.com/product-detail/C14675.html) |
| 2 | 5 | R8, R9 | RMC06035.1K1%N, 5.1 kΩ 0603 | C269716 | 100 | [Buy exact](https://www.lcsc.com/product-detail/C269716.html) |
| 1 | 2 | SW1 | MINI MSK12CO2 slide switch | C2681570 | 81,015 | [Buy exact](https://www.lcsc.com/product-detail/C2681570.html) |
| 1 | 2 | U1 | TPS63020DSJT | C544952 | 241 | [Buy exact](https://www.lcsc.com/product-detail/C544952.html) |
| 1 | 2 | U2 | BQ24092DGQR | C133237 | 267 | [Buy exact](https://www.lcsc.com/product-detail/C133237.html) |
| 2 | 3 | U3, U6 | UCLAMP0571P.TNT | C512387 | 15,455 | [Buy exact](https://www.lcsc.com/product-detail/C512387.html) |
| 1 | 2 | U4 | Molex 532610271 | C177225 | 17,625 | [Buy exact](https://www.lcsc.com/product-detail/C177225.html) |
| 1 | 2 | U5 | HX TYPE-C 6PIN | C18357552 | 17,770 | [Buy exact](https://www.lcsc.com/product-detail/C18357552.html) |


## 3. Calculator main-board parts

| BOM qty | Buy | Ref. | Exact BOM part | Selected LCSC code | Live stock | Purchase link / action |
|---:|---:|---|---|---|---:|---|
| 3 | 5 | C1, C2, C5 | GRM31CR60J107ME39L, 100 uF 1206 | C77085 | 41,510 | [Buy exact](https://www.lcsc.com/product-detail/C77085.html) |
| 1 | 2 | C3 | CL10A226MP8NUNE, 22 uF 0603 | C86295 | 146,960 | [Buy exact](https://www.lcsc.com/product-detail/C86295.html) |
| 1 | 2 | C4 | CL10B105KA8NNNC, 1 uF 0603 | C29936 | 139,550 | [Buy exact](https://www.lcsc.com/product-detail/C29936.html) |
| 50 | 55 | D1–D50 | BAS321,115 | C24372 | 479,540 | [Buy exact](https://www.lcsc.com/product-detail/C24372.html) |
| 1 | 2 | H3 | PZ2.54-2X4P-H25 | C42431809 | 2,050 | [Buy exact](https://www.lcsc.com/product-detail/C42431809.html) |
| 1 | 2 | H4 | HX FH254-01-06-W-H8.5 | C50878477 | 405 | [Buy exact](https://www.lcsc.com/product-detail/C50878477.html) |
| 1 | 1 | P1 | Completed custom power module | — | — | Build from sections 1–2; its parts and PCB are included directly in the total |
| 1 | 1 | PAPER1 | **WeAct 2.13-inch black/white e-paper, 250×122** | Non-LCSC | Seller-dependent | [AliExpress listing supplied by user](https://es.aliexpress.com/item/1005005183232092.html?mp=1&gatewayAdapt=glo2esp) — select the 2.13-inch Black-White variant |
| 2 | 3 | Q1, Q2 | BC847 | C475630 | 21,000 | [Buy exact](https://www.lcsc.com/product-detail/C475630.html) |
| 9 | 12 | R11–R19 | RC0603FR-0710KL, 10 kΩ 1% 0603 | **C116677, AC0603FR-0710KL** | 3,200 | [Buy equivalent Yageo 10 kΩ 1% 0603](https://www.lcsc.com/product-detail/C116677.html); [exact C98220 is out of stock](https://www.lcsc.com/product-detail/C98220.html) |
| 50 | 55 | SW1–SW50 | GT-TZ084B-H015-L1 | C29780122 | 346,320 | [Buy exact](https://www.lcsc.com/product-detail/C29780122.html) |
| 1 | 2 | U2 | A-MicroTF-1.85A | C22467599 | 1,860 | [Buy exact](https://www.lcsc.com/product-detail/C22467599.html) |
| 1 | 2 | U5 | ESP32-S3-WROOM-1-N16R8 | C2913202 | 20,792 | [Buy exact](https://www.lcsc.com/product-detail/C2913202.html) |

### Consolidated shared passives

If ordering both boards in one cart, combine the overlapping capacitor lines:

| Part | Power qty | Main qty | Exact total | Suggested buy |
|---|---:|---:|---:|---:|
| CL10A226MP8NUNE / C86295 | 3 | 1 | 4 | 6 |
| CL10B105KA8NNNC / C29936 | 1 | 1 | 2 | 4 |

## 4. Required off-board and mechanical items

These are needed to complete a usable calculator but are absent or incomplete
in the electrical BOMs.

| Needed | Suggested order | Item | Purchase / source link | Important constraint |
|---:|---:|---|---|---|
| 1 | 1 | Single-cell 3.7 V LiPo battery | [AliExpress listing supplied by user](https://es.aliexpress.com/item/1005008721775874.html?mp=1&gatewayAdapt=glo2esp) | Listing offers Heltec 3000 mAh / 800 mAh variants. Choose only after checking enclosure dimensions. Verify connector type and polarity with a multimeter before connection. |
| 1 | 1 | Molex PicoBlade 51021-0200 2-pin female housing/cable, if battery is not supplied with the correct plug | [Molex PicoBlade family](https://www.molex.com/en-us/products/connectors/wire-to-board-connectors/picoblade-connectors) | Mates with board header 53261-0271; 1.25 mm pitch. |
| 1 | 1 | microSD card | [SanDisk product family](https://www.westerndigital.com/brand/sandisk) | A small genuine card is sufficient; FAT32 is the safest initial format. |
| 1 | 1 | 3.3 V USB-to-UART programmer with RX, TX, and GND | [AliExpress FT232RL listing supplied by user](https://es.aliexpress.com/item/1005009171804987.html?mp=1&gatewayAdapt=glo2esp) | Select the desired USB connector variant and set the adapter to 3.3 V logic before connecting the ESP32. |
| 1 | 1 | USB-C cable and 5 V USB supply | [USB-IF certified cable guidance](https://www.usb.org/products) | Used for charging through the power module. |
| 1 set | 1 set | Hook-up wires / header jumpers for initial programming | [DigiKey jumper-wire category](https://www.digikey.com/en/products/filter/jumper-wire/459) | Match H3's 2.54 mm header. |
| 1 | 1 print | Front panel | [`front_panel.step`](../CAD/STEP/front_panel.step) | Print once. |
| 1 | 1 print | Main body | [`main_body.step.f3d`](../CAD/STEP/main_body.step.f3d) | This repository contains an `.f3d` file despite the `.step.f3d` name; export it to STL/3MF before slicing. |
| 1 | 1 print | Midplate/button holder | [`midplate_button_holder.step`](../CAD/STEP/midplate_button_holder.step) | Print once. |
| 50 | 52 prints | Key buttons | [`button.step`](../CAD/STEP/button.step) | Print 50; two spares recommended. |
| As fitted | 1 assortment | Small case screws | [McMaster metric screw overview](https://www.mcmaster.com/products/screws/system-of-measurement~metric/) | The README does not specify thread, length, or quantity. Measure the CAD holes before ordering. |

## 5. Practical order grouping

1. **PCB fab:** upload both Gerber ZIPs and order the minimum board quantity.
2. **JLCPCB/LCSC cart:** order all power and main-board electronic components
   in one cart, using the consolidated quantities for shared capacitors. See
   `SUPPLIER_COMPARISON.csv` for exact part, stock, alternate source, selected
   part, and rationale.
3. **Display:** order the **2.13-inch black/white 250×122** WeAct SPI module.
   The seller page contains multiple sizes and colors, so select the option
   explicitly before checkout.
4. **Battery and cable:** select after measuring the enclosure and verify
   PicoBlade polarity.
5. **Programming and storage:** obtain a 3.3 V USB-UART adapter and microSD.
6. **Mechanical:** print the enclosure set and determine screw dimensions from
   the printed parts/CAD before buying hardware.
