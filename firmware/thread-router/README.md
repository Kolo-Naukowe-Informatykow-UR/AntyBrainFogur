<p align="center">
  <img src="../../assets/branding/thread-banner.png" alt="thread-router" width="100%">
</p>

# thread-router

Thread border router systemu AntyBrainFogur — łączy sieć Thread z infrastrukturą Wi-Fi i udostępnia węzły satelitarne ekosystemom Matter.

<br>

## Platforma

<table align="center">
  <tr>
    <th>Hardware</th>
    <th>Protokoły</th>
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

**Płytka:** Waveshare ESP32-C6-Zero (`27035`)  
**MCU:** ESP32-C6, RISC-V single-core @ 160 MHz, Wi-Fi 6 (802.11ax), BLE 5, IEEE 802.15.4  
**Rola w systemie:** Thread border router — most między siecią Thread węzłów H2 a infrastrukturą Wi-Fi/Matter. Agreguje dane z satelitów i przekazuje je do węzła S3 oraz ekosystemów smart home.

<br>

## Wymagania

<table>
  <tr>
    <th>Narzędzie</th>
    <th>Wersja</th>
    <th>Do czego</th>
  </tr>
  <tr>
    <td><img src="https://img.shields.io/badge/ESP--IDF-E7352C?style=flat-square&logo=espressif&logoColor=white" height="20" alt="ESP-IDF"></td>
    <td><code>v5.x</code></td>
    <td>Cały firmware, narzędzia budowania, flash</td>
  </tr>
  <tr>
    <td><img src="https://img.shields.io/badge/CMake-064F8C?style=flat-square&logo=cmake&logoColor=white" height="20" alt="CMake"></td>
    <td><code>≥ 3.16</code></td>
    <td>System budowania</td>
  </tr>
  <tr>
    <td><img src="https://img.shields.io/badge/Python-3776AB?style=flat-square&logo=python&logoColor=white" height="20" alt="Python"></td>
    <td><code>≥ 3.8</code></td>
    <td>Skrypty IDF</td>
  </tr>
</table>

> **Środowisko budowania:** wymagany **Linux** lub **WSL2** (Ubuntu 22.04+). Matter SDK nie wspiera natywnego buildu na Windows.

<br>

## Szybki start

```bash
# 1. Wejdź do katalogu projektu
cd firmware/thread-router

# 2. Załaduj środowisko IDF (raz na sesję terminala)
. $IDF_PATH/export.sh

# 3. Skonfiguruj target, zbuduj i wgraj
idf.py set-target esp32c6
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

Port `/dev/ttyUSB0` to standardowy port ESP32 na Linux. Sprawdź swój: `ls /dev/ttyUSB*`

<br>

## Struktura projektu

```
thread-router/
├── main/                   # Punkt wejścia aplikacji
│   ├── main.c
│   └── CMakeLists.txt
├── components/             # Komponenty lokalne (OpenThread, border router)
├── CMakeLists.txt
└── sdkconfig.defaults
```
