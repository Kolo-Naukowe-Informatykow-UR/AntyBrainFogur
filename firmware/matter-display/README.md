<p align="center">
  <img src="../../assets/branding/matter-banner.png" alt="matter-display" width="100%">
</p>

# matter-display

Główny węzeł systemu AntyBrainFogur — standalone device z dotykowym ekranem LCD, interfejsem użytkownika i bramką Matter do ekosystemów smart home.

<br>

## Platforma

<table align="center">
  <tr>
    <th>Hardware</th>
    <th>Protokoły</th>
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

**Płytka:** Waveshare ESP32-S3-Touch-LCD-2.8 (`27690`)  
**Wyświetlacz:** IPS 2,8" 240×320, pojemnościowy panel dotykowy  
**MCU:** ESP32-S3, dual-core Xtensa LX7 @ 240 MHz, 512 KB SRAM + PSRAM  
**Rola w systemie:** punkt interakcji z użytkownikiem, wyświetlanie danych ze wszystkich węzłów, integracja z Google Home / Home Assistant przez Matter

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
cd firmware/matter-display

# 2. Załaduj środowisko IDF (raz na sesję terminala)
. $IDF_PATH/export.sh

# 3. Skonfiguruj target, zbuduj i wgraj
idf.py set-target esp32s3
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

Port `/dev/ttyUSB0` to standardowy port ESP32 na Linux. Sprawdź swój: `ls /dev/ttyUSB*`

<br>

## Struktura projektu

```
matter-display/
├── main/                   # Punkt wejścia aplikacji
│   ├── main.c
│   └── CMakeLists.txt
├── components/             # Komponenty lokalne (UI, sterownik LCD, Matter)
├── CMakeLists.txt
└── sdkconfig.defaults
```
