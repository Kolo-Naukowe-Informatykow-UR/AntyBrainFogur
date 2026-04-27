<p align="center">
  <img src="../../assets/branding/matter-banner.png" alt="matter-display" width="100%">
</p>

# matter-display

The main node of the AntyBrainFogur system — standalone device with a touchscreen LCD display, user interface, and Matter gateway to smart home ecosystems.

<br>

## Platform

<table align="center">
  <tr>
    <th>Hardware</th>
    <th>Protocols</th>
  </tr>
  <tr>
    <td align="center">
      <img src="https://img.shields.io/badge/ESP32--S3-E7352C?style=flat-square&logo=espressif&logoColor=white" height="20" alt="ESP32-S3">
    </td>
    <td align="center">
      <img src="https://img.shields.io/badge/Matter-4E4FEB?style=flat-square&logoColor=white" height="20" alt="Matter">
      <img src="https://img.shields.io/badge/Wi--Fi-00B2FF?style=flat-square&logo=wi-fi&logoColor=white" height="20" alt="Wi-Fi">
      <img src="https://img.shields.io/badge/BLE_5-0082FC?style=flat-square&logo=bluetooth&logoColor=white" height="20" alt="BLE 5">
    </td>
  </tr>
</table>

<br>

**Board:** Waveshare ESP32-S3-Touch-LCD-2.8 (`27690`)  
**Display:** IPS 2.8" 240×320, capacitive touchscreen  
**MCU:** ESP32-S3, dual-core Xtensa LX7 @ 240 MHz, 512 KB SRAM + PSRAM  
**Role in the system:** user interaction point, displays data from all nodes, integrates with Google Home / Home Assistant via Matter

<br>

## Requirements

<table>
  <tr>
    <th>Tool</th>
    <th>Version</th>
    <th>Purpose</th>
  </tr>
  <tr>
    <td><img src="https://img.shields.io/badge/ESP--IDF-E7352C?style=flat-square&logo=espressif&logoColor=white" height="20" alt="ESP-IDF"></td>
    <td><code>v5.x</code></td>
    <td>Full firmware stack, build tools, flash</td>
  </tr>
  <tr>
    <td><img src="https://img.shields.io/badge/CMake-064F8C?style=flat-square&logo=cmake&logoColor=white" height="20" alt="CMake"></td>
    <td><code>≥ 3.16</code></td>
    <td>Build system</td>
  </tr>
  <tr>
    <td><img src="https://img.shields.io/badge/Python-3776AB?style=flat-square&logo=python&logoColor=white" height="20" alt="Python"></td>
    <td><code>≥ 3.8</code></td>
    <td>IDF scripts</td>
  </tr>
</table>

> **Build environment:** Linux or **WSL2** (Ubuntu 22.04+) required. Matter SDK does not support native Windows builds.

<br>

## Quick Start

```bash
# 1. Enter the project directory
cd firmware/matter-display

# 2. Load the IDF environment (once per terminal session)
. $IDF_PATH/export.sh

# 3. Set target, build and flash
idf.py set-target esp32s3
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

`/dev/ttyUSB0` is the standard ESP32 port on Linux. Check yours: `ls /dev/ttyUSB*`

<br>

## Project structure

```
matter-display/
├── main/                   # Application entry point
│   ├── main.c
│   └── CMakeLists.txt
├── components/             # Local components (UI, LCD driver, Matter)
├── CMakeLists.txt
└── sdkconfig.defaults
```
