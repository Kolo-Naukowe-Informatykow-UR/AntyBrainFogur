# Contributing

<br>

## From scratch — development environment

### 1. What to work on — Linux, WSL or Windows?

Depends on which subproject you want to develop.

| Subproject | Native Windows | WSL2 / Linux |
|------------|:-:|:-:|
| `matter-display` (ESP32-S3) | ✗ | ✓ |
| `thread-router` (ESP32-C6) | ✓ | ✓ |
| `thread-satellite` (ESP32-H2) | ✓ | ✓ |

**matter-display** uses **esp-matter** which does not support Windows — Linux or WSL2 is required.  
**thread-router** and **thread-satellite** use plain OpenThread — they build everywhere, including native Windows.

<br>

### 2a. Installing ESP-IDF on Linux / WSL2

Official documentation: **https://docs.espressif.com/projects/esp-idf/en/stable/esp32/get-started/**

On WSL2, first install the system dependencies:

```bash
sudo apt update && sudo apt install -y git wget flex bison gperf python3 python3-pip \
python3-venv cmake ninja-build ccache libffi-dev libssl-dev dfu-util libusb-1.0-0
```

Then clone and install ESP-IDF:

```bash
git clone --recursive https://github.com/espressif/esp-idf.git
cd esp-idf
./install.sh all
. ./export.sh
```

> **USB port in WSL2:** WSL2 does not see USB devices by default. To flash from WSL2 you need to forward the USB port via **usbipd**: https://learn.microsoft.com/en-us/windows/wsl/connect-usb

> **`export.sh`** only works for the current terminal session — run it again in every new window, or add it to `.bashrc`.

<br>

### 2b. Installing ESP-IDF on Windows (thread-router and thread-satellite only)

Espressif provides a ready-to-use Windows installer — no manual setup needed.

Download the installer: **https://dl.espressif.com/dl/esp-idf/**

The installer downloads the toolchain, Python, and all dependencies automatically. After installation, open **ESP-IDF CMD** or **ESP-IDF PowerShell** from the Start Menu — the environment is already loaded and `idf.py` works immediately.

> **Port on Windows:** instead of `/dev/ttyUSB0` you use `COM*` — check the port number in Device Manager under "Ports (COM & LPT)". Replace it in flash commands: `idf.py -p COM3 flash monitor`

<br>

### 3. Clone the repository

```bash
git clone https://github.com/kni-informatycy/AntyBrainFogur.git
cd AntyBrainFogur
```

<br>

### 4. Build the chosen subproject

Each subproject is a separate ESP-IDF project with its own target.

**matter-display** (ESP32-S3, touchscreen display):
```bash
cd firmware/matter-display
. $IDF_PATH/export.sh
idf.py set-target esp32s3
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

**thread-router** (ESP32-C6, border router):
```bash
cd firmware/thread-router
. $IDF_PATH/export.sh
idf.py set-target esp32c6
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

**thread-satellite** (ESP32-H2, sensor nodes):
```bash
cd firmware/thread-satellite
. $IDF_PATH/export.sh
idf.py set-target esp32h2
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

`/dev/ttyUSB0` is the standard ESP32 port on Linux. Check yours: `ls /dev/ttyUSB*`

<br>

## Workflow

We work on branches. No direct commits to `main`.

```
main              ← stable, always buildable
dev               ← integration branch
feature/<name>    ← new functionality
fix/<name>        ← bug fixes
refactor/<name>   ← refactoring without behavior changes
```

Standard flow:

```bash
# 1. Create a branch from dev
git checkout dev
git pull
git checkout -b feature/feature-name

# 2. Make changes, commit regularly
git add .
git commit -m "feat(scope): description"

# 3. Push the branch and open a PR to dev
git push -u origin feature/feature-name
```

<br>

## Commit convention

Format: `type(scope): short description`

| Type | When |
|------|------|
| `feat` | new functionality |
| `fix` | bug fix |
| `refactor` | code change without behavior impact |
| `test` | tests |
| `docs` | documentation |
| `chore` | tooling, configuration, housekeeping |

**Scope** is the subproject name: `matter-display`, `thread-router`, `thread-satellite`, or `repo` for general changes.

Examples:
```
feat(thread-satellite): add SHT40 temperature reading
fix(thread-router): fix restart on Thread network loss
docs(matter-display): update LCD pinout description
```

<br>

## Pull Requests

- PRs always target `dev`, never directly `main`
- Before opening a PR make sure the project builds (`idf.py build`)
- PR description should explain what changed and why
- One PR — one responsibility

<br>

## CI — automated build check

CI runs automatically on every push — on `feature/*`, `fix/*`, `refactor/*`, `dev`, and `main`. Push your branch and check the **Actions** tab on GitHub to see whether the build passed before opening a PR.

| Subproject | CI |
|------------|:--:|
| `thread-router` (ESP32-C6) | ✓ |
| `thread-satellite` (ESP32-H2) | ✓ |
| `matter-display` (ESP32-S3) | ✗ |

`matter-display` is excluded from CI — esp-matter requires a separate setup and significantly longer build times. It will be added once the subproject has its first real logic.

Before opening a PR, verify locally that the project builds. Start by removing the previous build directory to ensure a clean build from scratch:

```bash
# thread-router
cd firmware/thread-router
. $IDF_PATH/export.sh
idf.py set-target esp32c6
rm -rf build
idf.py build

# thread-satellite
cd firmware/thread-satellite
. $IDF_PATH/export.sh
idf.py set-target esp32h2
rm -rf build
idf.py build
```

If `idf.py build` finishes without errors — you are ready to push.

After pushing, CI will build the project automatically on GitHub. Check the result from the terminal (requires [GitHub CLI](https://cli.github.com/)):

```bash
gh run list --branch $(git branch --show-current)
```

Status `success` — everything is fine, you can open a PR.  
Status `failure` — the build failed. Go back to the code, fix the errors, and push again. Do not open a PR with a failing CI.

<br>

## Links

- [ESP-IDF Getting Started](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/get-started/)
- [esp-matter SDK](https://github.com/espressif/esp-matter)
- [OpenThread on ESP32](https://docs.espressif.com/projects/esp-idf/en/stable/esp32h2/api-guides/openthread.html)
- [usbipd — USB in WSL2](https://learn.microsoft.com/en-us/windows/wsl/connect-usb)
