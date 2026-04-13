#!/usr/bin/env bash
# =============================================================================
# msys2_flash.sh — Firmware auf Quansheng UV-K5 flashen (MSYS2)
# =============================================================================
#
# Interaktives Skript zum Flashen der Firmware ueber die serielle
# Schnittstelle (USB-Programmierkabel) direkt aus MSYS2 heraus.
#
# Benutzung:
#   1.  MSYS2 MinGW64-Shell oeffnen
#   2.  In den firmware-source Ordner wechseln:
#         cd /c/Users/User/Documents/.../quansheng-K5-talking-ardf-rx/firmware-source
#   3.  Skript ausfuehrbar machen (einmalig):
#         chmod +x msys2_flash.sh
#   4.  Skript starten:
#         ./msys2_flash.sh
#
# Voraussetzungen:
#   - MSYS2 MinGW64-Shell
#   - Python 3 mit pyserial
#   - USB-Programmierkabel (Baofeng/Kenwood-Typ, CH340 oder CP2102)
#
# Was macht dieses Skript?
#   - Prueft Abhaengigkeiten (Python3, pyserial)
#   - Installiert fehlende Pakete (mit Bestaetigung)
#   - Startet k5flash.py im interaktiven Modus
#
# =============================================================================

set -euo pipefail

# ---------------------------------------------------------------------------
# Farben (nur wenn Terminal Farben unterstuetzt)
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
# Hilfsfunktionen
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

separator() {
    echo "${CYAN}$(printf '=%.0s' {1..70})${RESET}"
}

# ---------------------------------------------------------------------------
# Skript-Verzeichnis bestimmen
# ---------------------------------------------------------------------------
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
K5FLASH="$SCRIPT_DIR/k5flash.py"

# ---------------------------------------------------------------------------
# Pruefen, ob wir in der richtigen MSYS2-Umgebung laufen
# ---------------------------------------------------------------------------
check_msys2_env() {
    if [[ -z "${MSYSTEM:-}" ]]; then
        fail "MSYSTEM nicht gesetzt — vermutlich keine MSYS2-Shell."
        fail "Dieses Skript muss in einer MSYS2 ${BOLD}MINGW64${RESET}${RED}-Shell gestartet werden."
        echo ""
        echo "  So geht es:"
        echo "    Start > MSYS2 MinGW x64"
        echo ""
        exit 1
    fi

    if [[ "${MSYSTEM}" != "MINGW64" ]]; then
        fail "Falsche MSYS2-Umgebung: ${BOLD}${MSYSTEM}${RESET}"
        fail "Dieses Skript benoetigt ${BOLD}MINGW64${RESET}."
        echo ""
        echo "  Bitte die richtige Shell oeffnen:"
        echo "    Start > MSYS2 MinGW x64"
        echo ""
        exit 1
    fi

    ok "MSYS2-Umgebung: ${BOLD}$MSYSTEM${RESET}"
}

# ---------------------------------------------------------------------------
# Python 3 pruefen/installieren
# ---------------------------------------------------------------------------
PYTHON_CMD=""

check_python() {
    info "Pruefe Python 3 ..."
    for cmd in python3 python py; do
        if command -v "$cmd" &>/dev/null; then
            local ver
            ver=$("$cmd" --version 2>&1 || true)
            if echo "$ver" | grep -qi "python 3"; then
                PYTHON_CMD="$cmd"
                ok "Python 3 gefunden: $ver ($cmd)"
                return 0
            fi
        fi
    done

    fail "Python 3 nicht gefunden."
    if ask_yes_no "Python 3 jetzt installieren? (pacman -S mingw-w64-x86_64-python)"; then
        pacman -S --noconfirm mingw-w64-x86_64-python
        for cmd in python3 python py; do
            if command -v "$cmd" &>/dev/null; then
                local ver
                ver=$("$cmd" --version 2>&1 || true)
                if echo "$ver" | grep -qi "python 3"; then
                    PYTHON_CMD="$cmd"
                    ok "Python 3 installiert: $ver"
                    return 0
                fi
            fi
        done
    fi

    fail "Python 3 konnte nicht gefunden werden. Abbruch."
    exit 1
}

# ---------------------------------------------------------------------------
# pyserial pruefen/installieren
# ---------------------------------------------------------------------------
check_pyserial() {
    info "Pruefe Python-Modul pyserial ..."

    if "$PYTHON_CMD" -c "import serial" &>/dev/null; then
        ok "pyserial ist installiert."
        return 0
    fi

    warn "pyserial fehlt."

    # 1. Versuch: pacman
    local pacman_pkg="mingw-w64-x86_64-python-pyserial"
    if pacman -Si "$pacman_pkg" &>/dev/null; then
        if ask_yes_no "pyserial via pacman installieren? ($pacman_pkg)"; then
            pacman -S --noconfirm "$pacman_pkg"
            if "$PYTHON_CMD" -c "import serial" &>/dev/null; then
                ok "pyserial via pacman installiert."
                return 0
            fi
        fi
    fi

    # 2. Versuch: pip in venv
    info "Versuche pip-Installation in venv ..."
    local venv_dir="$SCRIPT_DIR/.venv_flash"

    if ask_yes_no "pyserial in lokalem venv installieren? ($venv_dir)"; then
        "$PYTHON_CMD" -m venv "$venv_dir" 2>/dev/null || {
            fail "venv konnte nicht erstellt werden."
            exit 1
        }

        # pip im venv
        if [[ -x "$venv_dir/bin/pip" ]]; then
            "$venv_dir/bin/pip" install --upgrade pyserial 2>/dev/null
            PYTHON_CMD="$venv_dir/bin/python"
        elif [[ -x "$venv_dir/Scripts/pip" ]]; then
            "$venv_dir/Scripts/pip" install --upgrade pyserial 2>/dev/null
            PYTHON_CMD="$venv_dir/Scripts/python.exe"
        else
            fail "pip im venv nicht gefunden."
            exit 1
        fi

        if "$PYTHON_CMD" -c "import serial" &>/dev/null; then
            ok "pyserial im venv installiert."
            return 0
        fi
    fi

    fail "pyserial konnte nicht installiert werden. Abbruch."
    echo ""
    echo "  Manuelle Installation:"
    echo "    pacman -S mingw-w64-x86_64-python-pyserial"
    echo "  oder:"
    echo "    pip install pyserial"
    echo ""
    exit 1
}

# ---------------------------------------------------------------------------
# k5flash.py pruefen
# ---------------------------------------------------------------------------
check_k5flash() {
    info "Pruefe Flash-Skript ..."
    if [[ -f "$K5FLASH" ]]; then
        ok "k5flash.py gefunden: $K5FLASH"
    else
        fail "k5flash.py nicht gefunden!"
        fail "Erwartet in: $K5FLASH"
        exit 1
    fi
}

# ---------------------------------------------------------------------------
# Treiber-Hinweis
# ---------------------------------------------------------------------------
show_driver_hint() {
    echo ""
    echo "  ${BOLD}Treiber-Hinweis:${RESET}"
    echo ""
    echo "  Das USB-Programmierkabel benoetigt einen Treiber:"
    echo "    - ${BOLD}CH340${RESET}: https://www.wch.cn/download/CH341SER_EXE.html"
    echo "    - ${BOLD}CP2102${RESET}: https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers"
    echo ""
    echo "  Nach dem Anschliessen des Kabels sollte ein neuer COM-Port"
    echo "  im Geraete-Manager erscheinen (z.B. COM3, COM4, ...)."
    echo ""
}

# ===========================================================================
# Hauptprogramm
# ===========================================================================
main() {
    echo ""
    separator
    echo "  ${BOLD}Quansheng UV-K5 ARDF Firmware — MSYS2 Flash-Tool${RESET}"
    separator
    echo ""

    check_msys2_env
    echo ""

    info "${BOLD}Pruefe Abhaengigkeiten ...${RESET}"
    echo ""

    check_python
    check_pyserial
    check_k5flash

    echo ""
    separator
    ok "Alle Abhaengigkeiten vorhanden."
    separator

    show_driver_hint

    echo "  ${BOLD}Anleitung:${RESET}"
    echo ""
    echo "  1. Radio mit USB-Programmierkabel an den PC anschliessen"
    echo "  2. Radio in den Flash-Modus versetzen:"
    echo "     ${BOLD}PTT-Taste gedrueckt halten + Radio einschalten${RESET}"
    echo "     → Display bleibt dunkel, LED leuchtet konstant weiss"
    echo "  3. Im folgenden Dialog COM-Port und Firmware waehlen"
    echo ""

    if ask_yes_no "Flash-Vorgang jetzt starten?"; then
        echo ""
        separator
        "$PYTHON_CMD" "$K5FLASH"
        separator
    else
        info "Abgebrochen."
    fi

    echo ""
}

main "$@"
