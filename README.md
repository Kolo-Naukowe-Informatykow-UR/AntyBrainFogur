<p align="center">
  <img src="assets/branding/hero-banner.png" alt="AntyBrainFogur" width="100%">
</p>

# AntyBrainFogUR

Embedded system for environmental monitoring and cognitive performance support based on ESP32 — ESP32-S3 as a standalone device with a display, ESP32-C6 as a Thread border router, ESP32-H2 as satellite network nodes.

[![Ask DeepWiki](https://deepwiki.com/badge.svg)](#)

<br>

## Supported Platforms & Protocols

<table align="center">
  <tr>
    <th>Microcontrollers</th>
    <th>Protocols &amp; Ecosystems</th>
  </tr>
  <tr>
    <td align="center">
      <img src="https://img.shields.io/badge/ESP32--S3-E7352C?style=flat-square&logo=espressif&logoColor=white" height="20" alt="ESP32-S3">
      <img src="https://img.shields.io/badge/ESP32--C6-E7352C?style=flat-square&logo=espressif&logoColor=white" height="20" alt="ESP32-C6">
      <img src="https://img.shields.io/badge/ESP32--H2-E7352C?style=flat-square&logo=espressif&logoColor=white" height="20" alt="ESP32-H2">
    </td>
    <td align="center">
      <img src="https://img.shields.io/badge/Matter-4E4FEB?style=flat-square&logoColor=white" height="20" alt="Matter">
      <img src="https://img.shields.io/badge/Thread-00B2FF?style=flat-square&logoColor=white" height="20" alt="Thread">
      <img src="https://img.shields.io/badge/Google_Home-4285F4?style=flat-square&logo=google&logoColor=white" height="20" alt="Google Home">
      <img src="https://img.shields.io/badge/Home_Assistant-41BDF5?style=flat-square&logo=home-assistant&logoColor=white" height="20" alt="Home Assistant">
    </td>
  </tr>
</table>

<br>

**ESP32-S3** — standalone device with touchscreen display, user interface, main interaction point.  
**ESP32-C6** — Thread border router, bridges the Thread network with Wi-Fi/Matter.  
**ESP32-H2** — satellite Thread network nodes, sensor data acquisition, no Wi-Fi.

<br>

## Requirements

All tools must be installed **before** the first build.

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

> **Build environment:** Matter SDK requires Linux. On Windows, **WSL2** is required (Ubuntu 22.04 recommended). Native Windows builds are not supported.

<br>

## Firmware

The system consists of three independent ESP-IDF projects, each targeting a different chip and role in the architecture.

<table>
  <tr>
    <th>Project</th>
    <th>Chip</th>
    <th>Role</th>
  </tr>
  <tr>
    <td><a href="firmware/matter-display/README.md"><strong>matter-display</strong></a></td>
    <td><img src="https://img.shields.io/badge/ESP32--S3-E7352C?style=flat-square&logo=espressif&logoColor=white" height="18" alt="ESP32-S3"></td>
    <td>Standalone device with touchscreen LCD, user interface, Matter gateway</td>
  </tr>
  <tr>
    <td><a href="firmware/thread-router/README.md"><strong>thread-router</strong></a></td>
    <td><img src="https://img.shields.io/badge/ESP32--C6-E7352C?style=flat-square&logo=espressif&logoColor=white" height="18" alt="ESP32-C6"></td>
    <td>Thread border router — bridge between the Thread network and Wi-Fi/Matter</td>
  </tr>
  <tr>
    <td><a href="firmware/thread-satellite/README.md"><strong>thread-satellite</strong></a></td>
    <td><img src="https://img.shields.io/badge/ESP32--H2-E7352C?style=flat-square&logo=espressif&logoColor=white" height="18" alt="ESP32-H2"></td>
    <td>Satellite Thread network nodes, sensor data acquisition</td>
  </tr>
</table>

Each project has its own `README.md` with board description, system role, requirements, and build instructions.

<br>

## Quick Start

Required environment: **Linux** or **WSL2** (Ubuntu 22.04+).

```bash
git clone https://github.com/kni-informatycy/AntyBrainFogur.git
cd AntyBrainFogur
```

Next steps depend on which node you want to build:

- → **[matter-display](firmware/matter-display/README.md)** — ESP32-S3, touchscreen display, Matter
- → **[thread-router](firmware/thread-router/README.md)** — ESP32-C6, Thread border router
- → **[thread-satellite](firmware/thread-satellite/README.md)** — ESP32-H2, sensor nodes
