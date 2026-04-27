<p align="center">
  <img src="../../assets/branding/thread-banner.png" alt="thread-router" width="100%">
</p>

# thread-router

The Thread border router of the AntyBrainFogur system — bridges the Thread network with Wi-Fi infrastructure and exposes satellite nodes to Matter ecosystems.

<br>

## Platform

<table align="center">
  <tr>
    <th>Hardware</th>
    <th>Protocols</th>
  </tr>
  <tr>
    <td align="center">
      <img src="https://img.shields.io/badge/ESP32--C6-E7352C?style=flat-square&logo=espressif&logoColor=white" height="20" alt="ESP32-C6">
    </td>
    <td align="center">
      <img src="https://img.shields.io/badge/Thread-00B2FF?style=flat-square&logoColor=white" height="20" alt="Thread">
      <img src="https://img.shields.io/badge/Matter-4E4FEB?style=flat-square&logoColor=white" height="20" alt="Matter">
      <img src="https://img.shields.io/badge/Wi--Fi_6-00B2FF?style=flat-square&logo=wi-fi&logoColor=white" height="20" alt="Wi-Fi 6">
      <img src="https://img.shields.io/badge/BLE_5-0082FC?style=flat-square&logo=bluetooth&logoColor=white" height="20" alt="BLE 5">
    </td>
  </tr>
</table>

<br>

**Board:** Waveshare ESP32-C6-Zero (`27035`)  
**MCU:** ESP32-C6, RISC-V single-core @ 160 MHz, Wi-Fi 6 (802.11ax), BLE 5, IEEE 802.15.4  
**Role in the system:** Thread border router — bridge between the H2 Thread node network and Wi-Fi/Matter infrastructure. Aggregates data from satellites and forwards it to the S3 node and smart home ecosystems.

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
cd firmware/thread-router

# 2. Load the IDF environment (once per terminal session)
. $IDF_PATH/export.sh

# 3. Set target, build and flash
idf.py set-target esp32c6
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

`/dev/ttyUSB0` is the standard ESP32 port on Linux. Check yours: `ls /dev/ttyUSB*`

<br>

## Project structure

```
thread-router/
├── main/                   # Application entry point
│   ├── main.c
│   └── CMakeLists.txt
├── components/             # Local components (OpenThread, border router)
├── CMakeLists.txt
└── sdkconfig.defaults
```
