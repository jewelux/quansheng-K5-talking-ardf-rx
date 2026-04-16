#!/usr/bin/env bash
# =============================================================================
# msys2_build.sh — Unified Build Script for V1 and V3 Firmware (MSYS2)
# =============================================================================
# Builds the Quansheng UV-K5 ARDF Talking firmware for V1 or V3 hardware.
# Checks all dependencies, offers interactive installation, and supports
# both the V1 (Make) and V3 (CMake + Ninja) build systems.
#
# Usage:
#   1.  Open MSYS2 MinGW64 shell
#   2.  cd to the repository root:
#         cd /c/Users/User/Documents/.../quansheng-K5-talking-ardf-rx
#   3.  chmod +x msys2_build.sh   (once)
#   4.  ./msys2_build.sh
#
# =============================================================================

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")" && pwd)"

# ---------------------------------------------------------------------------
# Colors (only if terminal supports them)
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
# Shell check (MSYS2 MinGW64 preferred)
# ---------------------------------------------------------------------------
check_shell() {
    if [[ -n "${MSYSTEM:-}" ]]; then
        case "$MSYSTEM" in
            MINGW64) ok "MSYS2 MinGW64 Shell erkannt." ;;
            UCRT64)  warn "UCRT64 Shell erkannt — MinGW64 wird empfohlen, aber sollte funktionieren." ;;
            MINGW32) warn "MINGW32 Shell — 32-Bit, MinGW64 wird empfohlen." ;;
            *)       warn "MSYS2 Shell '$MSYSTEM' erkannt — MinGW64 wird empfohlen." ;;
        esac
    else
        warn "Keine MSYS2-Umgebung erkannt. Das Skript laeuft auch unter Linux/macOS."
    fi
}

# ---------------------------------------------------------------------------
# Dependency checking
# ---------------------------------------------------------------------------
check_common_deps() {
    local missing=()

    if ! command -v arm-none-eabi-gcc &>/dev/null; then
        missing+=("arm-none-eabi-gcc")
    else
        ok "arm-none-eabi-gcc: $(arm-none-eabi-gcc --version | head -1)"
    fi

    if [[ ${#missing[@]} -gt 0 ]]; then
        fail "Fehlende gemeinsame Abhaengigkeiten: ${missing[*]}"
        if [[ -n "${MSYSTEM:-}" ]]; then
            if ask_yes_no "Fehlende Pakete jetzt installieren?"; then
                pacman -S --noconfirm \
                    mingw-w64-x86_64-arm-none-eabi-gcc \
                    mingw-w64-x86_64-arm-none-eabi-newlib 2>/dev/null || \
                pacman -S --noconfirm \
                    $MINGW_PACKAGE_PREFIX-arm-none-eabi-gcc \
                    $MINGW_PACKAGE_PREFIX-arm-none-eabi-newlib
                ok "Cross-Compiler installiert."
            else
                fail "Ohne Cross-Compiler kann nicht gebaut werden."
                exit 1
            fi
        else
            info "Bitte arm-none-eabi-gcc installieren (apt, brew, etc.)"
            exit 1
        fi
    fi
}

check_v1_deps() {
    local missing=()

    if ! command -v make &>/dev/null; then
        missing+=("make")
    else
        ok "GNU Make: $(make --version | head -1)"
    fi

    if ! command -v python3 &>/dev/null && ! command -v python &>/dev/null; then
        missing+=("python3")
    else
        local PY
        PY=$(command -v python3 2>/dev/null || command -v python)
        ok "Python: $($PY --version 2>&1)"
    fi

    # Check crcmod
    local PY
    PY=$(command -v python3 2>/dev/null || command -v python)
    if [[ -n "$PY" ]] && ! "$PY" -c "import crcmod" 2>/dev/null; then
        missing+=("python-crcmod")
    fi

    if [[ ${#missing[@]} -gt 0 ]]; then
        fail "Fehlende V1-Abhaengigkeiten: ${missing[*]}"
        if [[ -n "${MSYSTEM:-}" ]] && ask_yes_no "Fehlende Pakete jetzt installieren?"; then
            for pkg in "${missing[@]}"; do
                case "$pkg" in
                    make)
                        pacman -S --noconfirm make
                        ;;
                    python3)
                        pacman -S --noconfirm mingw-w64-x86_64-python 2>/dev/null || \
                        pacman -S --noconfirm $MINGW_PACKAGE_PREFIX-python
                        ;;
                    python-crcmod)
                        pacman -S --noconfirm mingw-w64-x86_64-python-crcmod 2>/dev/null || \
                        "$PY" -m pip install crcmod 2>/dev/null || true
                        ;;
                esac
            done
            ok "V1-Abhaengigkeiten installiert."
        else
            fail "Fehlende Pakete muessen installiert werden."
            exit 1
        fi
    fi
}

check_v3_deps() {
    local missing=()

    if ! command -v cmake &>/dev/null; then
        missing+=("cmake")
    else
        ok "CMake: $(cmake --version | head -1)"
    fi

    if ! command -v ninja &>/dev/null; then
        missing+=("ninja")
    else
        ok "Ninja: $(ninja --version 2>&1)"
    fi

    if [[ ${#missing[@]} -gt 0 ]]; then
        fail "Fehlende V3-Abhaengigkeiten: ${missing[*]}"
        if [[ -n "${MSYSTEM:-}" ]] && ask_yes_no "Fehlende Pakete jetzt installieren?"; then
            for pkg in "${missing[@]}"; do
                case "$pkg" in
                    cmake)
                        pacman -S --noconfirm mingw-w64-x86_64-cmake 2>/dev/null || \
                        pacman -S --noconfirm $MINGW_PACKAGE_PREFIX-cmake
                        ;;
                    ninja)
                        pacman -S --noconfirm mingw-w64-x86_64-ninja 2>/dev/null || \
                        pacman -S --noconfirm $MINGW_PACKAGE_PREFIX-ninja
                        ;;
                esac
            done
            ok "V3-Abhaengigkeiten installiert."
        else
            fail "Fehlende Pakete muessen installiert werden."
            exit 1
        fi
    fi
}

# ---------------------------------------------------------------------------
# Build functions
# ---------------------------------------------------------------------------
build_v1() {
    info "=== V1 Firmware Build (Makefile) ==="
    local fw_dir="$REPO_ROOT/firmware-v1"

    if [[ ! -f "$fw_dir/Makefile" ]]; then
        fail "Makefile nicht gefunden in $fw_dir"
        exit 1
    fi

    check_common_deps
    check_v1_deps

    cd "$fw_dir"

    if ask_yes_no "Vorherige Build-Artefakte loeschen (make clean)?"; then
        make clean 2>/dev/null || true
        ok "Clean durchgefuehrt."
    fi

    info "Starte Build..."
    make -j"$(nproc 2>/dev/null || echo 2)"

    # Copy output to build-output/
    mkdir -p "$REPO_ROOT/build-output"
    local packed
    packed=$(find "$fw_dir" -maxdepth 1 -name '*.packed.bin' -printf '%T@ %p\n' 2>/dev/null | sort -rn | head -1 | cut -d' ' -f2-)
    if [[ -n "$packed" && -f "$packed" ]]; then
        local ts
        ts=$(date +%Y%m%d_%H%M%S)
        local outname="firmware_uvk5_v1_${ts}.packed.bin"
        cp "$packed" "$REPO_ROOT/build-output/$outname"
        ok "Firmware kopiert nach: build-output/$outname"
        ok "Groesse: $(wc -c < "$REPO_ROOT/build-output/$outname") Bytes"
    else
        warn "Keine .packed.bin gefunden. Pruefe Build-Ausgabe."
    fi

    ok "=== V1 Build abgeschlossen ==="
}

build_v3() {
    info "=== V3 Firmware Build (CMake + Ninja) ==="
    local fw_dir="$REPO_ROOT/firmware-v3"

    if [[ ! -f "$fw_dir/CMakeLists.txt" ]]; then
        fail "CMakeLists.txt nicht gefunden in $fw_dir"
        exit 1
    fi

    check_common_deps
    check_v3_deps

    cd "$fw_dir"

    # List available presets
    echo ""
    info "Verfuegbare CMake-Presets:"
    echo ""
    echo "  ${BOLD} 1${RESET}) ARDF-Morse    — ARDF-Empfaenger mit Morse-Accessibility (empfohlen)"
    echo "  ${BOLD} 2${RESET}) ARDF-Voice    — ARDF-Empfaenger mit Voice-Prompt-Accessibility"
    echo "  ${BOLD} 3${RESET}) ARDF-SAM      — ARDF-Empfaenger mit SAM-TTS-Accessibility"
    echo "  ${BOLD} 4${RESET}) ARDF-SAM-A11y — ARDF + SAM + Kompass + Sonifikation (Accessibility)"
    echo "  ${BOLD} 5${RESET}) ARDF          — Standard-ARDF (Dennis Real / DL9CAT)"
    echo "  ${BOLD} 6${RESET}) Custom        — Benutzerdefiniert"
    echo "  ${BOLD} 7${RESET}) Bandscope     — Mit Spektrumanzeige"
    echo "  ${BOLD} 8${RESET}) Broadcast     — UKW/KW-Empfaenger"
    echo "  ${BOLD} 9${RESET}) Basic         — Basis-Firmware"
    echo "  ${BOLD}10${RESET}) RescueOps     — Such- und Rettungsfunk"
    echo "  ${BOLD}11${RESET}) Game          — Mit Spielen"
    echo "  ${BOLD}12${RESET}) Fusion        — Alle Features"
    echo ""

    local preset
    while true; do
        read -rp "${BOLD}Preset waehlen [1-12, Standard=1]: ${RESET}" choice
        case "${choice:-1}" in
            1)  preset="ARDF-Morse"; break ;;
            2)  preset="ARDF-Voice"; break ;;
            3)  preset="ARDF-SAM"; break ;;
            4)  preset="ARDF-SAM-Access"; break ;;
            5)  preset="ARDF"; break ;;
            6)  preset="Custom"; break ;;
            7)  preset="Bandscope"; break ;;
            8)  preset="Broadcast"; break ;;
            9)  preset="Basic"; break ;;
            10) preset="RescueOps"; break ;;
            11) preset="Game"; break ;;
            12) preset="Fusion"; break ;;
            *)  echo "Bitte 1-12 eingeben." ;;
        esac
    done

    info "Verwende Preset: $preset"

    # Clean previous build
    local build_dir="$fw_dir/build/$preset"
    if [[ -d "$build_dir" ]] && ask_yes_no "Vorherigen Build loeschen?"; then
        rm -rf "$build_dir"
        ok "Build-Verzeichnis geloescht."
    fi

    info "CMake Configure..."
    cmake --preset "$preset"

    info "CMake Build..."
    cmake --build --preset "$preset"

    # Copy output to build-output/
    mkdir -p "$REPO_ROOT/build-output"
    local binfile
    binfile=$(find "$build_dir" -maxdepth 1 -name '*.bin' -printf '%T@ %p\n' 2>/dev/null | sort -rn | head -1 | cut -d' ' -f2-)
    if [[ -n "$binfile" && -f "$binfile" ]]; then
        local ts
        ts=$(date +%Y%m%d_%H%M%S)
        local outname
        outname="firmware_k5v3_${preset}_${ts}.bin"
        cp "$binfile" "$REPO_ROOT/build-output/$outname"
        ok "Firmware kopiert nach: build-output/$outname"
        ok "Groesse: $(wc -c < "$REPO_ROOT/build-output/$outname") Bytes"
    else
        warn "Keine .bin-Datei gefunden. Pruefe Build-Verzeichnis: $build_dir"
        ls -la "$build_dir"/ 2>/dev/null || true
    fi

    ok "=== V3 Build abgeschlossen ==="
}

# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------
main() {
    echo ""
    echo "${BOLD}============================================${RESET}"
    echo "${BOLD}  Quansheng UV-K5 ARDF Talking Firmware${RESET}"
    echo "${BOLD}  Unified Build Script${RESET}"
    echo "${BOLD}============================================${RESET}"
    echo ""

    check_shell

    echo ""
    info "Fuer welche Hardware-Version soll gebaut werden?"
    echo ""
    echo "  ${BOLD}1${RESET}) V1 — UV-K5 (DP32G030, 64 KB Flash, Makefile)"
    echo "  ${BOLD}2${RESET}) V3 — UV-K5v3 / UV-K1 (PY32F071, 128 KB Flash, CMake)"
    echo ""

    local version
    while true; do
        read -rp "${BOLD}Hardware-Version waehlen [1/2]: ${RESET}" version
        case "$version" in
            1) build_v1; break ;;
            2) build_v3; break ;;
            *) echo "Bitte 1 oder 2 eingeben." ;;
        esac
    done
}

main "$@"
