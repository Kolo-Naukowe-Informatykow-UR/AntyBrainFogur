# Contributing

<br>

## Od zera — środowisko deweloperskie

### 1. Na czym pracować — Linux, WSL czy Windows?

Zależy od tego który subprojekt chcesz rozwijać.

| Subprojekt | Windows natywny | WSL2 / Linux |
|------------|:-:|:-:|
| `matter-display` (ESP32-S3) | ✗ | ✓ |
| `thread-router` (ESP32-C6) | ✓ | ✓ |
| `thread-satellite` (ESP32-H2) | ✓ | ✓ |

**matter-display** używa **esp-matter** który nie wspiera Windows — wymagany Linux lub WSL2.  
**thread-router** i **thread-satellite** używają czystego OpenThread — budują się na wszystkim.

<br>

### 2a. Instalacja ESP-IDF na Linux / WSL2

Oficjalna dokumentacja: **https://docs.espressif.com/projects/esp-idf/en/stable/esp32/get-started/**

Na WSL2 najpierw zainstaluj zależności systemowe:

```bash
sudo apt update && sudo apt install -y git wget flex bison gperf python3 python3-pip \
python3-venv cmake ninja-build ccache libffi-dev libssl-dev dfu-util libusb-1.0-0
```

Następnie sklonuj i zainstaluj ESP-IDF:

```bash
git clone --recursive https://github.com/espressif/esp-idf.git
cd esp-idf
./install.sh all
. ./export.sh
```

> **Port USB w WSL2:** WSL2 domyślnie nie widzi urządzeń USB. Żeby flashować musisz przekazać port przez **usbipd**: https://learn.microsoft.com/en-us/windows/wsl/connect-usb

> **`export.sh`** działa tylko w bieżącej sesji terminala — wywołuj przy każdym nowym oknie, albo dodaj do `.bashrc`.

<br>

### 2b. Instalacja ESP-IDF na Windows (tylko thread-router i thread-satellite)

Espressif udostępnia gotowy instalator dla Windows — nie musisz nic robić ręcznie.

Pobierz instalator: **https://dl.espressif.com/dl/esp-idf/**

Instalator sam pobiera toolchain, Python i wszystkie zależności. Po instalacji otwórz **ESP-IDF CMD** lub **ESP-IDF PowerShell** ze Start Menu — środowisko jest już załadowane, `idf.py` działa od razu.

> **Port na Windows:** zamiast `/dev/ttyUSB0` używasz `COM*` — sprawdź numer w Menedżerze urządzeń pod "Porty (COM i LPT)". Podmień w komendach flash: `idf.py -p COM3 flash monitor`

<br>

### 3. Sklonuj repozytorium

```bash
git clone https://github.com/kni-informatycy/AntyBrainFogur.git
cd AntyBrainFogur
```

<br>

### 4. Zbuduj wybrany subprojekt

Każdy subprojekt to osobny projekt ESP-IDF z własnym targetem.

**matter-display** (ESP32-S3, ekran dotykowy):
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

**thread-satellite** (ESP32-H2, węzły sensorowe):
```bash
cd firmware/thread-satellite
. $IDF_PATH/export.sh
idf.py set-target esp32h2
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

Port `/dev/ttyUSB0` to standardowy port ESP32 na Linux. Sprawdź swój: `ls /dev/ttyUSB*`

<br>

## Workflow

Pracujemy na branchach. Żadnych commitów bezpośrednio na `main`.

```
main              ← stabilny, zawsze buildowalny
dev               ← branch integracyjny
feature/<nazwa>   ← nowa funkcjonalność
fix/<nazwa>       ← poprawka błędu
refactor/<nazwa>  ← refactoring bez zmiany zachowania
```

Standardowy flow:

```bash
# 1. Utwórz branch od dev
git checkout dev
git pull
git checkout -b feature/nazwa-funkcji

# 2. Wprowadź zmiany, commituj regularnie
git add .
git commit -m "feat(scope): opis"

# 3. Wypchnij branch i otwórz PR do dev
git push -u origin feature/nazwa-funkcji
```

<br>

## Konwencja commitów

Format: `typ(scope): krótki opis`

| Typ | Kiedy |
|-----|-------|
| `feat` | nowa funkcjonalność |
| `fix` | poprawka błędu |
| `refactor` | zmiana kodu bez wpływu na zachowanie |
| `test` | testy |
| `docs` | dokumentacja |
| `chore` | narzędzia, konfiguracja, porządki |

**Scope** to nazwa subprojektu: `matter-display`, `thread-router`, `thread-satellite` lub `repo` dla zmian ogólnych.

Przykłady:
```
feat(thread-satellite): dodanie odczytu temperatury z SHT40
fix(thread-router): naprawa restartu przy utracie sieci Thread
docs(matter-display): aktualizacja opisu pinout LCD
```

<br>

## Pull Requesty

- PR zawsze do `dev`, nigdy bezpośrednio do `main`
- Przed otwarciem PR upewnij się że projekt się buduje (`idf.py build`)
- Opis PR powinien zawierać co zostało zmienione i dlaczego
- Jeden PR — jedna odpowiedzialność

<br>

## CI — automatyczny build check

CI odpala się automatycznie na każdy push — na `feature/*`, `fix/*`, `refactor/*`, `dev` i `main`. Wypchnij branch i wejdź w zakładkę **Actions** na GitHubie — zobaczysz czy build przeszedł zanim otworzysz PR.

| Subprojekt | CI |
|------------|:--:|
| `thread-router` (ESP32-C6) | ✓ |
| `thread-satellite` (ESP32-H2) | ✓ |
| `matter-display` (ESP32-S3) | ✗ |

`matter-display` jest wyłączony z CI — esp-matter wymaga osobnego setupu i znacznie dłuższego czasu buildu. Zostanie dodany gdy subprojekt będzie miał pierwszą realną logikę.

Przed otwarciem PR sprawdź lokalnie czy projekt się buduje. Zacznij od usunięcia poprzedniego buildu żeby mieć pewność że budujesz od zera:

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

Jeśli `idf.py build` kończy się bez błędów — możesz pushować.

Po pushu CI automatycznie zbuduje projekt na GitHubie. Sprawdź wynik z terminala (wymaga [GitHub CLI](https://cli.github.com/)):

```bash
gh run list --branch $(git branch --show-current)
```

Status `success` — wszystko gra, możesz otwierać PR.  
Status `failure` — build się nie zbudował, wróć do kodu, popraw błędy i pushuj ponownie. Nie otwieraj PR z czerwonym CI.

<br>

## Linki

- [ESP-IDF Getting Started](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/get-started/)
- [esp-matter SDK](https://github.com/espressif/esp-matter)
- [OpenThread na ESP32](https://docs.espressif.com/projects/esp-idf/en/stable/esp32h2/api-guides/openthread.html)
- [usbipd — USB w WSL2](https://learn.microsoft.com/en-us/windows/wsl/connect-usb)
