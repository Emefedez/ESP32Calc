


# Open Source Calculator intended for Symbolic Calculus and custom programs, based on the ESP32-S3
* Main Features:
0.   Open Source Firmware
0.   Eink Screen
0.   SD Card slot
0.   Wireless Connectivity

![image](/calculator_pcb/ViewFront.jpg)
![image](/calculator_pcb/ViewBack.jpg)
![image](/CAD/exploded_view.png)
![image](/poster.png)


* Current 3D case: https://a360.co/4ukxaMP

* Using AI, `screen_color_probe.py` was created, a tool to help diagnose the simulated screen in wokwi.
 - AI was also used to port the Rust driver for the chosen display into a C++ based RTOS, interfacing so it is compatible with MonoCanvas with our resolution.

## FIRMWARE WIP:




## PARTS LIST:

- There is a proper spreadsheet, but since that is not supported in markdown, this is an ASCII version:
```
| **No.** | **Quantity** | **Comment**           | **Designator**                                                                                                                                                                                                                                   | **Footprint**                                         | **Value** | **Manufacturer Part**  | **Manufacturer**   | **Supplier Part**    | **Supplier** | **JLCPCB Unit Price** | **JLCPCB Price** | **JLCPCB Stock**  |
| ------- | ------------ | --------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ | ----------------------------------------------------- | --------- | ---------------------- | ------------------ | -------------------- | ------------ | --------------------- | ---------------- | ---------------- -|
| **1**   | 3            | 100uF                 | C1,C2,C5                                                                                                                                                                                                                                         | C1206                                                 | 100uF     | GRM31CR60J107ME39L     | muRata(村田)         | C77085               | LCSC         | 0,0171 €              | 0,0513 €         | 42714            |
| **2**   | 1            | 22uF                  | C3                                                                                                                                                                                                                                               | C0603                                                 | 22uF      | CL10A226MP8NUNE        | SAMSUNG(三星)        | C86295               | LCSC         | 0,0024 €              | 0,0024 €         | 8697             |
| **3**   | 1            | 1uF                   | C4                                                                                                                                                                                                                                               | C0603                                                 | 1uF       | CL10B105KA8NNNC        | SAMSUNG(三星)        | C29936               | LCSC         | 0,0011 €              | 0,0011 €         | 2742739          |
| **4**   | 50           | BAS321,115            | D1,D2,D3,D4,D5,D6,D7,D8,D9,D10,D11,D12,D13,D14,D15,D16,D17,D18,D19,D20,D21,D22,D23,D24,D25,D26,D27,D28,D29,D30,D31,D32,D33,D34,D35,D36,D37,D38,D39,D40,D41,D42,D43,D44,D45,D46,D47,D48,D49,D50                                                   | SOD-323_L1.8-W1.3-LS2.5-RD                            |           | BAS321,115             | Nexperia(安世)       | C24372               | LCSC         | 0,0031 €              | 0,1550 €         | 90588            |
| **5**   | 1            | PZ2.54-2X4P-H25       | H3                                                                                                                                                                                                                                               | HDR-TH_8P-P2.54-V-M-R2-C4-S2.54_PZ254V-22-XX-A31-1120 |           | PZ2.54-2X4P-H25        | JXTCONN(聚兴泰)      | C42431809            | LCSC         | 0,0058 €              | 0,0058 €         | 4744             |
| **6**   | 1            | HX FH254-01-06-W-H8.5 | H4                                                                                                                                                                                                                                               | HDR-TH_6P-P2.54-H-F-W10.2                             |           | HX FH254-01-06-W-H8.5  | hanxia(韩下)         | C50878477            | LCSC         | 0,0213 €              | 0,0213 €         | 5                |
| **7**   | 1            | Custom Power PCB      | P1                                                                                                                                                                                                                                               | ESP32-3.3V and sense                                  |           | XXXXXXXXXXXX           | Ordered by user     |                      | Aliexpress   | 8,5000 €              | 8,5000 €         | 0                |
| **8**   | 1            | 2.13in Epaper Display | PAPER1                                                                                                                                                                                                                                           | WEACT_2_9_E_PAPER                                     |           |                        | WeAct               | YRD0290BBS800F6HP-M7 | Waveshare    | 5,0000 €              | 5,0000 €         | 0                |
| **9**   | 2            | BC847                 | Q1,Q2                                                                                                                                                                                                                                            | SOT-23-3_L2.9-W1.3-P1.90-LS2.4-BR                     |           | BC847                  | SHIKUES(时科)        | C475630              | LCSC         | 0,0021 €              | 0,0042 €         | 50985            |
| **10**  | 9            | 10kΩ                  | R11,R12,R13,R14,R15,R16,R17,R18,R19                                                                                                                                                                                                              | R0603                                                 | 10kΩ      | RC0603FR-0710KL        | YAGEO(国巨)          | C98220               | LCSC         | 0,0003 €              | 0,0027 €         | 49               |
| **11**  | 50           | GT-TZ084B-H015-L1     | SW1,SW2,SW3,SW4,SW5,SW6,SW7,SW8,SW9,SW10,SW11,SW12,SW13,SW14,SW15,SW16,SW17,SW18,SW19,SW20,SW21,SW22,SW23,SW24,SW25,SW26,SW27,SW28,SW29,SW30,SW31,SW32,SW33,SW34,SW35,SW36,SW37,SW38,SW39,SW40,SW41,SW42,SW43,SW44,SW45,SW46,SW47,SW48,SW49,SW50 | SW-SMD_4P-L5.2-W5.2-P3.70-LS6.4                       |           | GT-TZ084B-H015-L1      | G-Switch(品赞)       | C29780122            | LCSC         | 0,0020 €              | 0,1000 €         | 177811           |
| **12**  | 1            | A-MicroTF-1.85A       | U2                                                                                                                                                                                                                                               | TF-SMD_A-MICROTF-1.85A                                |           | A-MicroTF-1.85A        | MyAntenna(摩天射频)   | C22467599            | LCSC         | 0,0414 €              | 0,0414 €         | 1534             |
| **13**  | 1            | 2.4GHz                | U5                                                                                                                                                                                                                                               | WIRELM-SMD_ESP32-S3-WROOM-1                           | 2.4GHz    | ESP32-S3-WROOM-1-N16R8 | ESPRESSIF(乐鑫)      | C2913202             | LCSC         | 0,7270 €              | 0,7270 €         | 27666            |
|         | 1            | FTDI FT232RL CH340N   | _Not on Board_                                                                                                                                                                                                                                   | _Not on Board_                                        |           | FT232RL CH340N         | Aitver Electronics  |                      |              | 14,3236 €             | 14,6122 €         |                  |
```

