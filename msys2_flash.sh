#!/usr/bin/env bash
# =============================================================================
# msys2_flash.sh — Unified Flash Script for V1 and V3 Firmware (MSYS2)
# =============================================================================
# Flashes the Quansheng UV-K5 ARDF Talking firmware onto V1 or V3 hardware.
# Supports multiple flash methods depending on hardware version.
#
# Usage:
#   1.  Open MSYS2 MinGW64 shell
#   2.  cd to the repository root:
#         cd /c/Users/User/Documents/.../quansheng-K5-talking-ardf-rx
#   3.  chmod +x msys2_flash.sh   (once)
#   4.  ./msys2_flash.sh
#
# =============================================================================

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")" && pwd)"

# ---------------------------------------------------------------------------
# Colors
# ---------------------------------------------------------------------------
if [[ -t 1 ]] && command -v tput &>/dev/null && [[ $(tput colors 2>/dev/null || echo 0) -ge 8 ]]; then
    RED=$(tput setaf 1)
    GREEN=$(tput setaf 2)
    YELLOW=$(tput setaf 3)
    CYAN=$(tput setaf 6)
    BOLD=$(tput bold)
    RESET=$(tput sgr0)
else
    RED="" GREEN="" YELLOW="" CYAN="" BOLD="" RESET=""
fi

# ---------------------------------------------------------------------------
# Helper functions
# ---------------------------------------------------------------------------
info()  { echo "${CYAN}[INFO]${RESET}  $*"; }
ok()    { echo "${GREEN}[OK]${RESET}    $*"; }
warn()  { echo "${YELLOW}[WARN]${RESET}  $*"; }
fail()  { echo "${RED}[FAIL]${RESET}  $*"; }

ask_yes_no() {
    local answer
    while true; do
        read -rp "${BOLD}$1 [j/n]: ${RESET}" answer
        case "$answer" in
            [jJyY]*) return 0 ;;
            [nN]*)   return 1 ;;
            *)       echo "Bitte j oder n eingeben." ;;
        esac
    done
}

# ---------------------------------------------------------------------------
# Find firmware files
# ---------------------------------------------------------------------------
list_firmware_files() {
    local pattern="$1"
    local search_dirs=("$REPO_ROOT/build-output" "$REPO_ROOT/firmware-v1" "$REPO_ROOT/firmware-v3")
    local found=()

    for dir in "${search_dirs[@]}"; do
        if [[ -d "$dir" ]]; then
            while IFS= read -r -d '' f; do
                found+=("$f")
            done < <(find "$dir" -maxdepth 2 -name "$pattern" -print0 2>/dev/null)
        fi
    done

    printf '%s\n' "${found[@]}" | sort -t/ -k1 | head -20
}

select_firmware_file() {
    local pattern="$1"
    local label="$2"

    info "Suche nach $label Firmware-Dateien..."

    local files
    files=$(list_firmware_files "$pattern")

    if [[ -z "$files" ]]; then
        fail "Keine $label Firmware-Dateien gefunden."
        fail "Bitte zuerst ./msys2_build.sh ausfuehren."
        exit 1
    fi

    echo ""
    info "Gefundene Firmware-Dateien:"
    echo ""

    local i=0
    local file_array=()
    while IFS= read -r f; do
        ((i++)) || true
        local relpath="${f#$REPO_ROOT/}"
        local size
        size=$(wc -c < "$f")
        printf "  ${BOLD}%d${RESET}) %s (%d Bytes)\n" "$i" "$relpath" "$size"
        file_array+=("$f")
    done <<< "$files"

    echo ""

    local choice
    while true; do
        read -rp "${BOLD}Firmware-Datei waehlen [1-$i]: ${RESET}" choice
        if [[ "$choice" =~ ^[0-9]+$ ]] && (( choice >= 1 && choice <= i )); then
            SELECTED_FW="${file_array[$((choice-1))]}"
            ok "Gewaehlt: ${SELECTED_FW#$REPO_ROOT/}"
            return 0
        fi
        echo "Bitte eine Zahl zwischen 1 und $i eingeben."
    done
}

# ---------------------------------------------------------------------------
# V1 Flash (k5flash.py serial bootloader)
# ---------------------------------------------------------------------------
flash_v1() {
    info "=== V1 Firmware Flash (k5flash.py) ==="
    echo ""

    # Check python + pyserial
    local PY
    PY=$(command -v python3 2>/dev/null || command -v python 2>/dev/null || true)
    if [[ -z "$PY" ]]; then
        fail "Python3 nicht gefunden."
        if [[ -n "${MSYSTEM:-}" ]] && ask_yes_no "Python installieren?"; then
            pacman -S --noconfirm mingw-w64-x86_64-python 2>/dev/null || \
            pacman -S --noconfirm $MINGW_PACKAGE_PREFIX-python
            PY=$(command -v python3 2>/dev/null || command -v python)
        else
            exit 1
        fi
    fi

    if ! "$PY" -c "import serial" 2>/dev/null; then
        warn "pyserial nicht gefunden."
        if [[ -n "${MSYSTEM:-}" ]] && ask_yes_no "pyserial installieren?"; then
            "$PY" -m pip install pyserial 2>/dev/null || \
            pacman -S --noconfirm mingw-w64-x86_64-python-pyserial 2>/dev/null || true
        fi
    fi

    if ! "$PY" -c "import crcmod" 2>/dev/null; then
        warn "crcmod nicht gefunden."
        if [[ -n "${MSYSTEM:-}" ]] && ask_yes_no "crcmod installieren?"; then
            pacman -S --noconfirm mingw-w64-x86_64-python-crcmod 2>/dev/null || \
            "$PY" -m pip install crcmod 2>/dev/null || true
        fi
    fi

    local k5flash="$REPO_ROOT/firmware-v1/k5flash.py"
    if [[ ! -f "$k5flash" ]]; then
        fail "k5flash.py nicht gefunden in firmware-v1/"
        exit 1
    fi

    select_firmware_file "*.packed.bin" "V1"

    echo ""
    echo "${YELLOW}============================================${RESET}"
    echo "${YELLOW}  WICHTIG: V1 Flash-Vorbereitung${RESET}"
    echo "${YELLOW}============================================${RESET}"
    echo ""
    echo "  1. Radio AUSSCHALTEN"
    echo "  2. USB-Programmierkabel anschliessen"
    echo "  3. PTT gedrueckt halten"
    echo "  4. Radio EINSCHALTEN (PTT weiter halten)"
    echo "  5. PTT loslassen wenn LED weiss leuchtet"
    echo "  6. Display bleibt dunkel = Flash-Modus aktiv"
    echo ""

    if ! ask_yes_no "Radio im Flash-Modus? Jetzt flashen?"; then
        info "Flash abgebrochen."
        exit 0
    fi

    # Detect COM port
    echo ""
    info "Suche serielle Ports..."
    local ports
    ports=$(ls /dev/ttyS* /dev/ttyUSB* /dev/ttyACM* 2>/dev/null | head -10 || true)

    if [[ -n "${MSYSTEM:-}" ]]; then
        # Windows: check COMx ports
        for i in $(seq 1 20); do
            if [[ -e "/dev/ttyS$((i-1))" ]] 2>/dev/null; then
                ports+=$'\n'"/dev/ttyS$((i-1)) (COM$i)"
            fi
        done
    fi

    if [[ -z "$ports" ]]; then
        warn "Keine seriellen Ports gefunden."
        read -rp "${BOLD}COM-Port manuell eingeben (z.B. COM3): ${RESET}" manual_port
        ports="$manual_port"
    fi

    echo ""
    info "Starte Flash-Vorgang..."
    echo ""

    "$PY" "$k5flash" "$SELECTED_FW" || {
        fail "Flash fehlgeschlagen!"
        echo ""
        echo "Tipps:"
        echo "  - Ist das Radio im Flash-Modus (PTT + Power)?"
        echo "  - Ist das richtige USB-Kabel angeschlossen?"
        echo "  - Versuche einen anderen COM-Port."
        exit 1
    }

    ok "=== V1 Flash abgeschlossen ==="
}

# ---------------------------------------------------------------------------
# V3 Flash (multiple methods)
# ---------------------------------------------------------------------------
flash_v3() {
    info "=== V3 Firmware Flash ==="
    echo ""

    echo "  V3/K1 Geraete unterstuetzen verschiedene Flash-Methoden:"
    echo ""
    echo "  ${BOLD}1${RESET}) K5TOOL (USB-Kabel, seriell) — empfohlen"
    echo "  ${BOLD}2${RESET}) SWD Programmer (pyocd/openocd) — fuer Entwickler"
    echo "  ${BOLD}3${RESET}) Browser Flasher (UV-Tools 2) — kein lokales Tool noetig"
    echo ""

    local method
    while true; do
        read -rp "${BOLD}Flash-Methode waehlen [1-3]: ${RESET}" method
        case "$method" in
            1) flash_v3_k5tool; break ;;
            2) flash_v3_swd; break ;;
            3) flash_v3_browser; break ;;
            *) echo "Bitte 1, 2 oder 3 eingeben." ;;
        esac
    done
}

flash_v3_k5tool() {
    info "=== V3 Flash via K5TOOL ==="

    # Check if K5TOOL is available
    if ! command -v K5TOOL &>/dev/null && ! command -v k5tool &>/dev/null; then
        echo ""
        warn "K5TOOL nicht gefunden."
        echo ""
        echo "  K5TOOL muss separat installiert werden:"
        echo "  ${CYAN}https://github.com/qrp73/K5TOOL${RESET}"
        echo ""
        echo "  Installation:"
        echo "  1. Repo klonen: git clone https://github.com/qrp73/K5TOOL.git"
        echo "  2. Bauen: cd K5TOOL && make"
        echo "  3. In den PATH kopieren oder direkt aufrufen"
        echo ""

        if ! ask_yes_no "Trotzdem fortfahren (K5TOOL-Pfad manuell angeben)?"; then
            exit 0
        fi

        read -rp "${BOLD}Pfad zu K5TOOL: ${RESET}" K5TOOL_PATH
        if [[ ! -x "$K5TOOL_PATH" ]]; then
            fail "K5TOOL nicht gefunden unter: $K5TOOL_PATH"
            exit 1
        fi
    else
        K5TOOL_PATH=$(command -v K5TOOL 2>/dev/null || command -v k5tool)
    fi

    select_firmware_file "*.bin" "V3"

    echo ""
    echo "${YELLOW}============================================${RESET}"
    echo "${YELLOW}  WICHTIG: V3 Flash-Vorbereitung${RESET}"
    echo "${YELLOW}============================================${RESET}"
    echo ""
    echo "  1. Radio AUSSCHALTEN"
    echo "  2. USB-Programmierkabel anschliessen"
    echo "  3. PTT gedrueckt halten"
    echo "  4. Radio EINSCHALTEN (PTT weiter halten)"
    echo "  5. PTT loslassen wenn LED leuchtet"
    echo "  6. Display bleibt dunkel = Flash-Modus aktiv"
    echo ""

    if ! ask_yes_no "Radio im Flash-Modus? Jetzt flashen?"; then
        info "Flash abgebrochen."
        exit 0
    fi

    info "Starte K5TOOL Flash..."
    "$K5TOOL_PATH" -write "$SELECTED_FW" || {
        fail "Flash fehlgeschlagen!"
        exit 1
    }

    ok "=== V3 Flash via K5TOOL abgeschlossen ==="
}

flash_v3_swd() {
    info "=== V3 Flash via SWD ==="
    echo ""

    # Check for pyocd or openocd
    local swd_tool=""
    if command -v pyocd &>/dev/null; then
        swd_tool="pyocd"
        ok "pyocd gefunden: $(pyocd --version 2>&1 | head -1)"
    elif command -v openocd &>/dev/null; then
        swd_tool="openocd"
        ok "openocd gefunden: $(openocd --version 2>&1 | head -1)"
    else
        warn "Weder pyocd noch openocd gefunden."
        echo ""
        echo "  SWD-Programmierung erfordert:"
        echo "  - Einen SWD-Adapter (ST-Link, J-Link, etc.)"
        echo "  - pyocd oder openocd Software"
        echo ""
        echo "  Installation von pyocd:"
        echo "    pip install pyocd"
        echo ""

        if [[ -n "${MSYSTEM:-}" ]] && ask_yes_no "pyocd jetzt installieren?"; then
            local PY
            PY=$(command -v python3 2>/dev/null || command -v python)
            "$PY" -m pip install pyocd
            swd_tool="pyocd"
        else
            fail "Kein SWD-Tool verfuegbar."
            exit 1
        fi
    fi

    select_firmware_file "*.bin" "V3"

    echo ""
    echo "${YELLOW}============================================${RESET}"
    echo "${YELLOW}  WICHTIG: SWD-Verbindung${RESET}"
    echo "${YELLOW}============================================${RESET}"
    echo ""
    echo "  1. SWD-Adapter an die Debug-Pads des Radios anschliessen"
    echo "  2. Radio mit Strom versorgen (Akku)"
    echo "  3. SWDIO, SWCLK, GND muessen verbunden sein"
    echo ""

    if ! ask_yes_no "SWD verbunden? Jetzt flashen?"; then
        info "Flash abgebrochen."
        exit 0
    fi

    if [[ "$swd_tool" == "pyocd" ]]; then
        info "Starte pyocd Flash..."
        pyocd flash -t py32f071xb "$SELECTED_FW" || {
            fail "SWD-Flash fehlgeschlagen!"
            exit 1
        }
    elif [[ "$swd_tool" == "openocd" ]]; then
        info "Starte openocd Flash..."
        # Use the config from the V3 firmware tree if available
        local ocd_cfg="$REPO_ROOT/firmware-v3/tools/unbrick_k5_v1/target/dp32g030.cfg"
        if [[ -f "$ocd_cfg" ]]; then
            openocd -f interface/stlink.cfg -f "$ocd_cfg" \
                -c "program $SELECTED_FW 0x08000000 verify reset exit" || {
                fail "SWD-Flash fehlgeschlagen!"
                exit 1
            }
        else
            openocd -f interface/stlink.cfg -f target/stm32f0x.cfg \
                -c "program $SELECTED_FW 0x08000000 verify reset exit" || {
                fail "SWD-Flash fehlgeschlagen!"
                exit 1
            }
        fi
    fi

    ok "=== V3 SWD-Flash abgeschlossen ==="
}

flash_v3_browser() {
    info "=== V3 Flash via Browser ==="
    echo ""
    echo "  Der einfachste Weg fuer V3/K1 Geraete:"
    echo ""
    echo "  1. Oeffne im Browser:"
    echo "     ${CYAN}https://armel.github.io/uvtools2/${RESET}"
    echo ""
    echo "  2. Radio in den Flash-Modus bringen (PTT + Power)"
    echo "  3. USB-Kabel anschliessen"
    echo "  4. Im Browser 'Connect' klicken"
    echo "  5. Die .bin-Datei aus build-output/ hochladen"
    echo "  6. 'Flash' klicken"
    echo ""

    info "Verfuegbare Firmware-Dateien in build-output/:"
    ls -la "$REPO_ROOT/build-output/"*.bin 2>/dev/null || \
        warn "Keine .bin Dateien in build-output/. Bitte zuerst bauen."

    echo ""
    ok "Bitte den Browser-Flasher verwenden."
}

# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------
main() {
    echo ""
    echo "${BOLD}============================================${RESET}"
    echo "${BOLD}  Quansheng UV-K5 ARDF Talking Firmware${RESET}"
    echo "${BOLD}  Unified Flash Script${RESET}"
    echo "${BOLD}============================================${RESET}"
    echo ""

    echo "  ${BOLD}WARNUNG:${RESET} Falsches Flashen kann das Radio unbrauchbar machen!"
    echo "  Immer die Hardware-Version pruefen (Aufkleber unter dem Akku)."
    echo ""

    echo "  Welche Hardware-Version hat Ihr Radio?"
    echo ""
    echo "  ${BOLD}1${RESET}) V1 — UV-K5 (weisser Aufkleber, DP32G030 MCU)"
    echo "  ${BOLD}2${RESET}) V3 — UV-K5v3 / UV-K1 (gruene Platine, PY32F071 MCU)"
    echo ""

    local version
    while true; do
        read -rp "${BOLD}Hardware-Version waehlen [1/2]: ${RESET}" version
        case "$version" in
            1) flash_v1; break ;;
            2) flash_v3; break ;;
            *) echo "Bitte 1 oder 2 eingeben." ;;
        esac
    done
}

main "$@"
