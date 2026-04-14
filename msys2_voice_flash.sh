#!/usr/bin/env bash
# =============================================================================
# msys2_voice_flash.sh — Voice Pack Flasher for V3 Firmware (MSYS2)
# =============================================================================
# Flashes a .vpk voice pack file to the PY25Q16 SPI flash on V3/K1 radios.
# Wraps k5flash_voice_v3.py with dependency checking and auto-installation.
#
# Usage:
#   1.  Open MSYS2 MinGW64 shell
#   2.  cd to the repository root
#   3.  chmod +x msys2_voice_flash.sh
#   4.  ./msys2_voice_flash.sh [voice_pack.vpk] [COM-Port]
#
# =============================================================================

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")" && pwd)"
FLASH_TOOL="$REPO_ROOT/firmware-v3/k5flash_voice_v3.py"

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
# Dependency checking and installation
# ---------------------------------------------------------------------------
install_msys2_package() {
    local pkg="$1"
    info "Installiere $pkg ..."
    if [[ -n "${MINGW_PACKAGE_PREFIX:-}" ]]; then
        pacman -S --noconfirm "${MINGW_PACKAGE_PREFIX}-${pkg}" 2>/dev/null || \
        pacman -S --noconfirm "mingw-w64-x86_64-${pkg}" 2>/dev/null || \
        pacman -S --noconfirm "$pkg" 2>/dev/null || {
            fail "Konnte $pkg nicht installieren."
            return 1
        }
    else
        if command -v pip3 &>/dev/null; then
            pip3 install "$pkg" 2>/dev/null || true
        elif command -v pip &>/dev/null; then
            pip install "$pkg" 2>/dev/null || true
        fi
    fi
    ok "$pkg installiert."
}

check_dependencies() {
    local missing=()

    # Check python3
    local py_cmd
    py_cmd=$(command -v python3 2>/dev/null || command -v python 2>/dev/null || echo "")
    if [[ -z "$py_cmd" ]]; then
        missing+=("python3")
    else
        ok "Python: $($py_cmd --version 2>&1)"
    fi

    # Check pyserial
    if [[ -n "$py_cmd" ]]; then
        if ! $py_cmd -c "import serial" 2>/dev/null; then
            missing+=("pyserial")
        else
            ok "pyserial: installiert"
        fi
    fi

    # Check flash tool
    if [[ ! -f "$FLASH_TOOL" ]]; then
        fail "Flash-Tool nicht gefunden: $FLASH_TOOL"
        exit 1
    fi

    if [[ ${#missing[@]} -gt 0 ]]; then
        fail "Fehlende Abhaengigkeiten: ${missing[*]}"

        if ask_yes_no "Fehlende Pakete jetzt installieren?"; then
            for pkg in "${missing[@]}"; do
                case "$pkg" in
                    python3)
                        if [[ -n "${MSYSTEM:-}" ]]; then
                            install_msys2_package "python"
                        else
                            fail "Bitte Python3 manuell installieren."
                            exit 1
                        fi
                        ;;
                    pyserial)
                        if [[ -n "${MSYSTEM:-}" ]]; then
                            install_msys2_package "python-pyserial"
                        else
                            pip3 install pyserial 2>/dev/null || pip install pyserial 2>/dev/null
                        fi
                        ;;
                esac
            done
        else
            fail "Ohne Abhaengigkeiten kann nicht geflasht werden."
            exit 1
        fi
    fi
}

# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------
main() {
    echo ""
    echo "${BOLD}============================================${RESET}"
    echo "${BOLD}  Voice Pack Flasher${RESET}"
    echo "${BOLD}  fuer Quansheng UV-K5 V3/K1${RESET}"
    echo "${BOLD}============================================${RESET}"
    echo ""

    check_dependencies
    echo ""

    local py_cmd
    py_cmd=$(command -v python3 2>/dev/null || command -v python)

    # Forward all arguments to the Python tool
    info "Starte Voice-Pack Flasher ..."
    echo ""

    "$py_cmd" "$FLASH_TOOL" "$@"
}

main "$@"
