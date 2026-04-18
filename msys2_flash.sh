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
    shift
    local search_dirs=("$@")
    local found=()

    for dir in "${search_dirs[@]}"; do
        if [[ -d "$dir" ]]; then
            while IFS= read -r -d '' f; do
                found+=("$f")
            done < <(find "$dir" -maxdepth 2 -name "$pattern" -print0 2>/dev/null)
        fi
    done

    if [[ ${#found[@]} -gt 0 ]]; then
        printf '%s\n' "${found[@]}" | sort -t/ -k1 | head -20
    fi
}

select_firmware_file() {
    local pattern="$1"
    local label="$2"
    shift 2
    local search_dirs=("$@")

    info "Suche nach $label Firmware-Dateien..."

    local files
    files=$(list_firmware_files "$pattern" "${search_dirs[@]}")

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
            pacman -S --noconfirm mingw-w64-x86_64-python-pyserial 2>/dev/null || \
            pacman -S --noconfirm "${MINGW_PACKAGE_PREFIX}-python-pyserial" 2>/dev/null || \
            "$PY" -m pip install --break-system-packages pyserial 2>/dev/null || \
            "$PY" -m pip install pyserial 2>/dev/null || {
                fail "pyserial konnte nicht installiert werden."
                echo "  Manuell: pacman -S \${MINGW_PACKAGE_PREFIX}-python-pyserial"
                exit 1
            }
            ok "pyserial installiert."
        else
            fail "pyserial wird fuer V1-Flash benoetigt."
            exit 1
        fi
    fi

    if ! "$PY" -c "import crcmod" 2>/dev/null; then
        warn "crcmod nicht gefunden."
        if [[ -n "${MSYSTEM:-}" ]] && ask_yes_no "crcmod installieren?"; then
            pacman -S --noconfirm mingw-w64-x86_64-python-crcmod 2>/dev/null || \
            pacman -S --noconfirm "${MINGW_PACKAGE_PREFIX}-python-crcmod" 2>/dev/null || \
            "$PY" -m pip install --break-system-packages crcmod 2>/dev/null || \
            "$PY" -m pip install crcmod 2>/dev/null || {
                fail "crcmod konnte nicht installiert werden."
                echo "  Manuell: pacman -S \${MINGW_PACKAGE_PREFIX}-python-crcmod"
                exit 1
            }
            ok "crcmod installiert."
        else
            fail "crcmod wird fuer V1-Flash benoetigt."
            exit 1
        fi
    fi

    local k5flash="$REPO_ROOT/firmware-v1/k5flash.py"
    if [[ ! -f "$k5flash" ]]; then
        fail "k5flash.py nicht gefunden in firmware-v1/"
        exit 1
    fi

    select_firmware_file "*.packed.bin" "V1" "$REPO_ROOT/build-output" "$REPO_ROOT/firmware-v1"

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

    local port_list=()

    if [[ -n "${MSYSTEM:-}" ]]; then
        # Windows/MSYS2: check COMx ports via Python pyserial
        while IFS= read -r line; do
            [[ -n "$line" ]] && port_list+=("$line")
        done < <("$PY" -c "
import serial.tools.list_ports
for p in serial.tools.list_ports.comports():
    print(p.device + ' — ' + p.description)
" 2>/dev/null || true)
    fi

    # Fallback: Linux-style serial devices
    if [[ ${#port_list[@]} -eq 0 ]]; then
        while IFS= read -r dev; do
            [[ -n "$dev" ]] && port_list+=("$dev")
        done < <(ls /dev/ttyUSB* /dev/ttyACM* 2>/dev/null || true)
    fi

    local selected_port=""

    if [[ ${#port_list[@]} -gt 0 ]]; then
        echo ""
        info "Gefundene serielle Ports:"
        echo ""
        local idx=0
        for entry in "${port_list[@]}"; do
            ((idx++)) || true
            printf "  ${BOLD}%d${RESET}) %s\n" "$idx" "$entry"
        done
        echo ""

        local port_choice
        while true; do
            read -rp "${BOLD}Port waehlen [1-$idx] oder manuell eingeben (z.B. COM3): ${RESET}" port_choice
            if [[ "$port_choice" =~ ^[0-9]+$ ]] && (( port_choice >= 1 && port_choice <= idx )); then
                # Extract device name (part before " — " if present)
                selected_port="${port_list[$((port_choice-1))]}"
                selected_port="${selected_port%% —*}"
                selected_port="${selected_port%% *}"
                break
            elif [[ -n "$port_choice" ]]; then
                selected_port="$port_choice"
                break
            fi
            echo "Bitte eine gueltige Auswahl treffen."
        done
    else
        warn "Keine seriellen Ports gefunden."
        read -rp "${BOLD}COM-Port manuell eingeben (z.B. COM3 oder /dev/ttyUSB0): ${RESET}" selected_port
        if [[ -z "$selected_port" ]]; then
            fail "Kein Port angegeben. Abbruch."
            exit 1
        fi
    fi

    ok "Gewaehlter Port: $selected_port"

    echo ""
    info "Starte Flash-Vorgang..."
    echo ""

    "$PY" "$k5flash" "$selected_port" "$SELECTED_FW" || {
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
# K5TOOL automatic installation
# ---------------------------------------------------------------------------
# K5TOOL is a C# / Mono project (NOT C).
#   - On Windows/MSYS2: download pre-built K5TOOL.exe from GitHub Releases
#   - On Linux/macOS:   build from source using cmake + mono-mcs
# ---------------------------------------------------------------------------

# Version of K5TOOL release to download
K5TOOL_RELEASE_TAG="v1.8"
K5TOOL_RELEASE_ZIP="K5TOOL-v1.8.zip"
K5TOOL_DOWNLOAD_URL="https://github.com/qrp73/K5TOOL/releases/download/${K5TOOL_RELEASE_TAG}/${K5TOOL_RELEASE_ZIP}"

install_k5tool() {
    local k5tool_dir="$REPO_ROOT/tools/K5TOOL"
    mkdir -p "$REPO_ROOT/tools"

    # -----------------------------------------------------------------------
    # Strategy 1 (Windows/MSYS2): Download pre-built K5TOOL.exe from release
    # -----------------------------------------------------------------------
    if [[ -n "${MSYSTEM:-}" ]] || [[ "$(uname -s)" == *MINGW* ]] || [[ "$(uname -s)" == *MSYS* ]] || [[ "$(uname -s)" == *CYGWIN* ]]; then
        install_k5tool_download "$k5tool_dir"
        return
    fi

    # -----------------------------------------------------------------------
    # Strategy 2 (Linux/macOS): Build from source with cmake + mono-mcs
    # -----------------------------------------------------------------------
    install_k5tool_build "$k5tool_dir"
}

# Download pre-built K5TOOL.exe from GitHub Releases (Windows)
install_k5tool_download() {
    local k5tool_dir="$1"
    local zip_path="$REPO_ROOT/tools/${K5TOOL_RELEASE_ZIP}"

    info "Lade K5TOOL ${K5TOOL_RELEASE_TAG} herunter (vorkompilierte Windows-Version)..."
    echo "  ${CYAN}${K5TOOL_DOWNLOAD_URL}${RESET}"
    echo ""

    # Need curl or wget
    local dl_cmd=""
    if command -v curl &>/dev/null; then
        dl_cmd="curl"
    elif command -v wget &>/dev/null; then
        dl_cmd="wget"
    else
        warn "Weder curl noch wget gefunden."
        if ask_yes_no "curl jetzt installieren?"; then
            pacman -S --noconfirm curl 2>/dev/null || {
                fail "curl konnte nicht installiert werden."
                exit 1
            }
            dl_cmd="curl"
        else
            fail "Ohne curl/wget kann K5TOOL nicht heruntergeladen werden."
            exit 1
        fi
    fi

    # Need unzip
    if ! command -v unzip &>/dev/null; then
        warn "unzip nicht gefunden."
        if ask_yes_no "unzip jetzt installieren?"; then
            pacman -S --noconfirm unzip 2>/dev/null || {
                fail "unzip konnte nicht installiert werden."
                exit 1
            }
        else
            fail "Ohne unzip kann K5TOOL nicht entpackt werden."
            exit 1
        fi
    fi

    # Download
    rm -f "$zip_path"
    if [[ "$dl_cmd" == "curl" ]]; then
        curl -fSL --progress-bar -o "$zip_path" "$K5TOOL_DOWNLOAD_URL" || {
            fail "Download fehlgeschlagen."
            fail "Pruefe die Internetverbindung und versuche es erneut."
            rm -f "$zip_path"
            exit 1
        }
    else
        wget -q --show-progress -O "$zip_path" "$K5TOOL_DOWNLOAD_URL" || {
            fail "Download fehlgeschlagen."
            fail "Pruefe die Internetverbindung und versuche es erneut."
            rm -f "$zip_path"
            exit 1
        }
    fi

    ok "Download abgeschlossen: $(wc -c < "$zip_path") Bytes"

    # Extract
    mkdir -p "$k5tool_dir"
    info "Entpacke ${K5TOOL_RELEASE_ZIP}..."
    unzip -o "$zip_path" -d "$k5tool_dir" || {
        fail "Entpacken fehlgeschlagen."
        exit 1
    }
    rm -f "$zip_path"

    # Find K5TOOL.exe — it may be at top level or in a subfolder
    local exe_path
    exe_path=$(find "$k5tool_dir" -maxdepth 2 -iname 'K5TOOL.exe' -print -quit 2>/dev/null)

    if [[ -z "$exe_path" ]]; then
        fail "K5TOOL.exe wurde im Archiv nicht gefunden."
        fail "Bitte pruefe: $k5tool_dir"
        ls -laR "$k5tool_dir"/ 2>/dev/null || true
        exit 1
    fi

    # If exe is in a subfolder, move contents up
    local exe_dir
    exe_dir=$(dirname "$exe_path")
    if [[ "$exe_dir" != "$k5tool_dir" ]]; then
        mv "$exe_dir"/* "$k5tool_dir"/ 2>/dev/null || true
    fi

    K5TOOL_PATH="$k5tool_dir/K5TOOL.exe"
    ok "K5TOOL ${K5TOOL_RELEASE_TAG} bereit: $K5TOOL_PATH"
    echo ""
}

# Build K5TOOL from source (Linux/macOS) — requires mono-mcs + cmake
install_k5tool_build() {
    local k5tool_dir="$1"

    # Check prerequisites
    if ! command -v git &>/dev/null; then
        fail "git nicht gefunden — wird zum Klonen von K5TOOL benoetigt."
        exit 1
    fi

    # mono-mcs (C# compiler) is required
    if ! command -v mcs &>/dev/null && ! command -v csc &>/dev/null; then
        warn "Mono C#-Compiler (mcs) nicht gefunden."
        echo ""
        echo "  K5TOOL ist ein C#-Projekt und benoetigt den Mono-Compiler."
        echo ""
        echo "  Installation:"
        echo "    Ubuntu/Debian:  sudo apt install mono-runtime mono-mcs"
        echo "    Fedora:         sudo dnf install mono-devel"
        echo "    macOS:          brew install mono"
        echo ""
        fail "Bitte mono-mcs installieren und erneut versuchen."
        exit 1
    fi

    if ! command -v cmake &>/dev/null; then
        warn "cmake nicht gefunden."
        echo "  Installation: sudo apt install cmake"
        fail "Bitte cmake installieren und erneut versuchen."
        exit 1
    fi

    if ! command -v make &>/dev/null; then
        warn "make nicht gefunden."
        echo "  Installation: sudo apt install make"
        fail "Bitte make installieren und erneut versuchen."
        exit 1
    fi

    # Clone or update
    if [[ -d "$k5tool_dir/.git" ]]; then
        info "K5TOOL-Verzeichnis existiert bereits, aktualisiere..."
        (cd "$k5tool_dir" && git pull --ff-only) || {
            warn "git pull fehlgeschlagen, versuche trotzdem zu bauen..."
        }
    else
        info "Klone K5TOOL von GitHub..."
        rm -rf "$k5tool_dir"
        git clone https://github.com/qrp73/K5TOOL.git "$k5tool_dir" || {
            fail "git clone fehlgeschlagen."
            fail "Pruefe die Internetverbindung und versuche es erneut."
            exit 1
        }
        ok "K5TOOL geklont nach: tools/K5TOOL/"
    fi

    # Build with cmake (the K5TOOL CMakeLists.txt compiles C# via mcs)
    info "Baue K5TOOL (cmake + mcs)..."
    local build_dir="$k5tool_dir/build"
    mkdir -p "$build_dir"
    (cd "$build_dir" && cmake .. && make) || {
        fail "K5TOOL konnte nicht gebaut werden."
        fail "Pruefe die Build-Ausgabe oben fuer Details."
        exit 1
    }

    # The cmake build puts k5tool.exe + k5tool launcher in build/
    if [[ -x "$build_dir/k5tool" ]]; then
        K5TOOL_PATH="$build_dir/k5tool"
    elif [[ -f "$build_dir/k5tool.exe" ]]; then
        K5TOOL_PATH="$build_dir/k5tool.exe"
    else
        fail "K5TOOL wurde gebaut, aber die ausfuehrbare Datei wurde nicht gefunden."
        fail "Bitte pruefe: $build_dir"
        ls -la "$build_dir"/ 2>/dev/null || true
        exit 1
    fi

    ok "K5TOOL erfolgreich gebaut: $K5TOOL_PATH"
    echo ""
}

# ---------------------------------------------------------------------------
# pyocd automatic installation (PEP 668 compatible)
# ---------------------------------------------------------------------------
# On MSYS2: tries pacman first, then falls back to a venv.
# On Linux/macOS: uses a venv to avoid PEP 668 "externally-managed" errors.
# ---------------------------------------------------------------------------
install_pyocd() {
    local PY
    PY=$(command -v python3 2>/dev/null || command -v python 2>/dev/null || true)
    if [[ -z "$PY" ]]; then
        fail "Python3 nicht gefunden — wird fuer pyocd benoetigt."
        if [[ -n "${MSYSTEM:-}" ]] && ask_yes_no "Python jetzt installieren?"; then
            pacman -S --noconfirm mingw-w64-x86_64-python 2>/dev/null || \
            pacman -S --noconfirm "${MINGW_PACKAGE_PREFIX}-python" || {
                fail "Python konnte nicht installiert werden."
                exit 1
            }
            PY=$(command -v python3 2>/dev/null || command -v python)
        else
            exit 1
        fi
    fi

    # --- Strategy 1 (MSYS2): try pacman package first ---
    if [[ -n "${MSYSTEM:-}" ]]; then
        info "Versuche pyocd ueber pacman zu installieren..."
        if pacman -S --noconfirm mingw-w64-x86_64-python-pyocd 2>/dev/null || \
           pacman -S --noconfirm "${MINGW_PACKAGE_PREFIX}-python-pyocd" 2>/dev/null; then
            ok "pyocd ueber pacman installiert."
            return 0
        fi
        warn "pyocd ist nicht als pacman-Paket verfuegbar."
        info "Installiere pyocd in einem Python-venv (virtuelle Umgebung)..."
    fi

    # --- Strategy 2: install in a venv (PEP 668 safe) ---
    local venv_dir="$REPO_ROOT/tools/.venv_pyocd"

    info "Erstelle virtuelle Umgebung: tools/.venv_pyocd/"
    "$PY" -m venv "$venv_dir" || {
        fail "venv konnte nicht erstellt werden."
        echo ""
        echo "  Unter MSYS2 ggf. noetig:"
        echo "    pacman -S ${MINGW_PACKAGE_PREFIX:-mingw-w64-x86_64}-python-virtualenv"
        echo ""
        exit 1
    }

    # Activate venv's pip
    local venv_pip="$venv_dir/bin/pip"
    if [[ -n "${MSYSTEM:-}" ]] || [[ "$(uname -s)" == *MINGW* ]]; then
        venv_pip="$venv_dir/Scripts/pip"
    fi

    info "Installiere pyocd in venv..."
    "$venv_pip" install --upgrade pip 2>/dev/null || true
    "$venv_pip" install pyocd || {
        fail "pyocd konnte nicht installiert werden."
        exit 1
    }

    ok "pyocd in venv installiert: tools/.venv_pyocd/"
    echo ""
}

# ---------------------------------------------------------------------------
# V3 Flash (multiple methods)
# ---------------------------------------------------------------------------
flash_v3() {
    info "=== V3 Firmware Flash ==="
    echo ""

    echo "  V3/K1 Geraete unterstuetzen verschiedene Flash-Methoden:"
    echo ""
    echo "  ${BOLD}1${RESET}) k5flash_v3.py (USB-Kabel, seriell, Python) — empfohlen"
    echo "  ${BOLD}2${RESET}) K5TOOL (USB-Kabel, seriell, externes Tool)"
    echo "  ${BOLD}3${RESET}) SWD Programmer (pyocd/openocd) — fuer Entwickler"
    echo "  ${BOLD}4${RESET}) Browser Flasher (UV-Tools 2) — kein lokales Tool noetig"
    echo ""

    local method
    while true; do
        read -rp "${BOLD}Flash-Methode waehlen [1-4]: ${RESET}" method
        case "$method" in
            1) flash_v3_python; break ;;
            2) flash_v3_k5tool; break ;;
            3) flash_v3_swd; break ;;
            4) flash_v3_browser; break ;;
            *) echo "Bitte 1, 2, 3 oder 4 eingeben." ;;
        esac
    done
}

flash_v3_python() {
    info "=== V3 Flash via k5flash_v3.py ==="

    local k5flash="$REPO_ROOT/firmware-v3/k5flash_v3.py"
    if [[ ! -f "$k5flash" ]]; then
        fail "k5flash_v3.py nicht gefunden in firmware-v3/"
        exit 1
    fi

    # Python finden
    local PY=""
    for candidate in python3 python; do
        if command -v "$candidate" &>/dev/null; then
            PY="$candidate"
            break
        fi
    done
    if [[ -z "$PY" ]]; then
        fail "Python nicht gefunden!"
        echo "  Installiere Python 3 und versuche es erneut."
        exit 1
    fi
    ok "Python gefunden: $PY ($($PY --version 2>&1))"

    # pyserial pruefen
    if ! "$PY" -c "import serial" 2>/dev/null; then
        warn "pyserial nicht installiert."
        if command -v pacman &>/dev/null; then
            info "Installiere pyserial via pacman..."
            pacman -S --noconfirm mingw-w64-x86_64-python-pyserial 2>/dev/null \
                || "$PY" -m pip install pyserial
        else
            info "Installiere pyserial via pip..."
            "$PY" -m pip install pyserial
        fi
    fi

    select_firmware_file "*.bin" "V3" "$REPO_ROOT/build-output" "$REPO_ROOT/firmware-v3"

    echo ""
    info "Starte k5flash_v3.py..."
    "$PY" "$k5flash" "$SELECTED_FW" || {
        fail "Flash fehlgeschlagen!"
        exit 1
    }

    ok "=== V3 Flash via k5flash_v3.py abgeschlossen ==="
}

flash_v3_k5tool() {
    info "=== V3 Flash via K5TOOL ==="

    local K5TOOL_PATH=""

    # 1) Check PATH
    if command -v K5TOOL &>/dev/null || command -v k5tool &>/dev/null; then
        K5TOOL_PATH=$(command -v K5TOOL 2>/dev/null || command -v k5tool)
        ok "K5TOOL gefunden: $K5TOOL_PATH"

    # 2) Check local tools/ cache (downloaded .exe or cmake-built binary)
    elif [[ -f "$REPO_ROOT/tools/K5TOOL/K5TOOL.exe" ]]; then
        K5TOOL_PATH="$REPO_ROOT/tools/K5TOOL/K5TOOL.exe"
        ok "K5TOOL gefunden (lokal): $K5TOOL_PATH"
    elif [[ -x "$REPO_ROOT/tools/K5TOOL/K5TOOL" ]]; then
        K5TOOL_PATH="$REPO_ROOT/tools/K5TOOL/K5TOOL"
        ok "K5TOOL gefunden (lokal): $K5TOOL_PATH"
    elif [[ -x "$REPO_ROOT/tools/K5TOOL/build/k5tool" ]]; then
        K5TOOL_PATH="$REPO_ROOT/tools/K5TOOL/build/k5tool"
        ok "K5TOOL gefunden (lokal): $K5TOOL_PATH"
    elif [[ -f "$REPO_ROOT/tools/K5TOOL/build/k5tool.exe" ]]; then
        K5TOOL_PATH="$REPO_ROOT/tools/K5TOOL/build/k5tool.exe"
        ok "K5TOOL gefunden (lokal): $K5TOOL_PATH"
    elif [[ -x "$REPO_ROOT/tools/K5TOOL/k5tool" ]]; then
        K5TOOL_PATH="$REPO_ROOT/tools/K5TOOL/k5tool"
        ok "K5TOOL gefunden (lokal): $K5TOOL_PATH"
    elif [[ -f "$REPO_ROOT/tools/K5TOOL/k5tool.exe" ]]; then
        K5TOOL_PATH="$REPO_ROOT/tools/K5TOOL/k5tool.exe"
        ok "K5TOOL gefunden (lokal): $K5TOOL_PATH"

    # 3) Not found — offer automatic installation
    else
        echo ""
        warn "K5TOOL nicht gefunden."
        echo ""

        if ask_yes_no "K5TOOL jetzt automatisch herunterladen und bauen?"; then
            install_k5tool
        else
            echo ""
            echo "  Manuelle Installation:"
            echo "  ${CYAN}https://github.com/qrp73/K5TOOL${RESET}"
            echo ""
            echo "  Windows: K5TOOL.exe von GitHub Releases herunterladen:"
            echo "    ${CYAN}https://github.com/qrp73/K5TOOL/releases${RESET}"
            echo ""
            echo "  Linux/macOS (Mono + CMake):"
            echo "    sudo apt install mono-runtime mono-mcs cmake"
            echo "    git clone https://github.com/qrp73/K5TOOL.git"
            echo "    cd K5TOOL && mkdir build && cd build && cmake .. && make"
            echo ""

            if ! ask_yes_no "Trotzdem fortfahren (K5TOOL-Pfad manuell angeben)?"; then
                exit 0
            fi

            read -rp "${BOLD}Pfad zu K5TOOL: ${RESET}" K5TOOL_PATH
            if [[ ! -x "$K5TOOL_PATH" ]]; then
                fail "K5TOOL nicht gefunden unter: $K5TOOL_PATH"
                exit 1
            fi
        fi
    fi

    select_firmware_file "*.bin" "V3" "$REPO_ROOT/build-output" "$REPO_ROOT/firmware-v3"

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
    # Also check venv-installed pyocd
    local venv_pyocd="$REPO_ROOT/tools/.venv_pyocd/bin/pyocd"
    if [[ -n "${MSYSTEM:-}" ]]; then
        venv_pyocd="$REPO_ROOT/tools/.venv_pyocd/Scripts/pyocd"
    fi

    if command -v pyocd &>/dev/null; then
        swd_tool="pyocd"
        ok "pyocd gefunden: $(pyocd --version 2>&1 | head -1)"
    elif [[ -x "$venv_pyocd" ]]; then
        swd_tool="$venv_pyocd"
        ok "pyocd gefunden (venv): $venv_pyocd"
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

        if ask_yes_no "pyocd jetzt installieren?"; then
            install_pyocd
            # After installation, find pyocd
            if command -v pyocd &>/dev/null; then
                swd_tool="pyocd"
            elif [[ -x "$venv_pyocd" ]]; then
                swd_tool="$venv_pyocd"
            else
                fail "pyocd Installation fehlgeschlagen."
                exit 1
            fi
        else
            fail "Kein SWD-Tool verfuegbar."
            exit 1
        fi
    fi

    select_firmware_file "*.bin" "V3" "$REPO_ROOT/build-output" "$REPO_ROOT/firmware-v3"

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

    if [[ "$swd_tool" == "openocd" ]]; then
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
    else
        # swd_tool is "pyocd" or a full path to venv pyocd
        info "Starte pyocd Flash..."
        "$swd_tool" flash -t py32f071xb "$SELECTED_FW" || {
            fail "SWD-Flash fehlgeschlagen!"
            exit 1
        }
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
