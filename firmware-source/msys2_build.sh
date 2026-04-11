#!/usr/bin/env bash
# =============================================================================
# msys2_build.sh — Narrensicheres Build-Skript fuer MSYS2 / MSYS64
# =============================================================================
# Prueft alle Abhaengigkeiten, bietet interaktive Nachinstallation an und
# baut die Quansheng UV-K5 ARDF Firmware.
#
# Benutzung:
#   1.  MSYS2 MinGW64-Shell oeffnen
#   2.  In den firmware-source Ordner wechseln:
#         cd /c/Users/User/Documents/.../quansheng-K5-talking-ardf-rx/firmware-source
#   3.  Skript ausfuehrbar machen (einmalig):
#         chmod +x msys2_build.sh
#   4.  Skript starten:
#         ./msys2_build.sh
#
# Hinweise:
#   - Das Skript funktioniert in MSYS2 MinGW64, MinGW32, UCRT64 und MSYS Shells.
#   - Farbige Ausgabe wird nur verwendet, wenn das Terminal es unterstuetzt.
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
    # $1 = Frage, gibt 0 (ja) oder 1 (nein) zurueck
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
# Pruefen, ob wir in einer MSYS2-Umgebung laufen
# ---------------------------------------------------------------------------
check_msys2_env() {
    if [[ -z "${MSYSTEM:-}" ]]; then
        warn "MSYSTEM nicht gesetzt — vermutlich keine MSYS2-Shell."
        warn "Dieses Skript ist fuer MSYS2 optimiert."
        if ! ask_yes_no "Trotzdem fortfahren?"; then
            echo "Abbruch."
            exit 1
        fi
    else
        ok "MSYS2-Umgebung erkannt: ${BOLD}$MSYSTEM${RESET}"
    fi
}

# ---------------------------------------------------------------------------
# Pacman-Wrapper: Paket installieren (mit Bestaetigung)
# ---------------------------------------------------------------------------
pacman_install() {
    local pkg="$1"
    local desc="${2:-$pkg}"
    if ask_yes_no "Soll '$desc' ($pkg) jetzt installiert werden?"; then
        info "Installiere $pkg ..."
        pacman -S --noconfirm "$pkg"
        ok "$pkg installiert."
    else
        fail "Ohne $desc kann nicht gebaut werden. Abbruch."
        exit 1
    fi
}

# ---------------------------------------------------------------------------
# Pip-Wrapper: Python-Paket installieren (mit Bestaetigung)
# ---------------------------------------------------------------------------
pip_install() {
    local pkg="$1"
    local desc="${2:-$pkg}"
    if ask_yes_no "Soll Python-Paket '$desc' ($pkg) jetzt installiert werden?"; then
        info "Installiere $pkg via pip ..."
        "$PYTHON_CMD" -m pip install --upgrade "$pkg"
        ok "$pkg installiert."
    else
        warn "$desc fehlt — .packed.bin wird nicht erzeugt, aber der Build ist moeglich."
    fi
}

# ---------------------------------------------------------------------------
# 1. Abhaengigkeit: make
# ---------------------------------------------------------------------------
check_make() {
    info "Pruefe GNU Make ..."
    if command -v make &>/dev/null; then
        local ver
        ver=$(make --version 2>/dev/null | head -1)
        ok "make gefunden: $ver"
    else
        fail "make nicht gefunden."
        # MSYS2-Paketname variiert je nach Umgebung
        local pkg="make"
        if [[ "${MSYSTEM:-}" == MINGW64 ]]; then
            pkg="mingw-w64-x86_64-make"
            # Fallback: plain make laeuft auch in MinGW-Shells
            if ! pacman -Qi "$pkg" &>/dev/null; then
                pkg="make"
            fi
        elif [[ "${MSYSTEM:-}" == MINGW32 ]]; then
            pkg="mingw-w64-i686-make"
            if ! pacman -Qi "$pkg" &>/dev/null; then
                pkg="make"
            fi
        fi
        pacman_install "$pkg" "GNU Make"
        # Erneut pruefen
        if ! command -v make &>/dev/null; then
            # Unter MinGW kann der Befehl mingw32-make heissen
            if command -v mingw32-make &>/dev/null; then
                warn "make nicht im PATH, aber mingw32-make gefunden."
                warn "Symlink wird erstellt ..."
                ln -sf "$(command -v mingw32-make)" /usr/local/bin/make 2>/dev/null || true
            else
                fail "make immer noch nicht verfuegbar. Bitte manuell installieren."
                exit 1
            fi
        fi
    fi
}

# ---------------------------------------------------------------------------
# 2. Abhaengigkeit: arm-none-eabi-gcc (ARM Toolchain)
# ---------------------------------------------------------------------------
check_arm_toolchain() {
    info "Pruefe ARM GCC Toolchain (arm-none-eabi-gcc) ..."
    if command -v arm-none-eabi-gcc &>/dev/null; then
        local ver
        ver=$(arm-none-eabi-gcc --version 2>/dev/null | head -1)
        ok "arm-none-eabi-gcc gefunden: $ver"
    else
        fail "arm-none-eabi-gcc nicht gefunden."
        echo ""
        echo "  Die ARM Embedded Toolchain kann in MSYS2 so installiert werden:"
        echo ""

        local pkg=""
        case "${MSYSTEM:-MSYS}" in
            MINGW64|UCRT64) pkg="mingw-w64-x86_64-arm-none-eabi-gcc" ;;
            MINGW32)        pkg="mingw-w64-i686-arm-none-eabi-gcc"  ;;
            *)              pkg="mingw-w64-x86_64-arm-none-eabi-gcc" ;;
        esac

        echo "    pacman -S $pkg"
        echo ""
        echo "  Alternativ kann die offizielle ARM-Toolchain manuell installiert"
        echo "  und zum PATH hinzugefuegt werden:"
        echo "    https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads"
        echo ""
        pacman_install "$pkg" "ARM GCC Toolchain"

        if ! command -v arm-none-eabi-gcc &>/dev/null; then
            fail "arm-none-eabi-gcc immer noch nicht gefunden."
            fail "Bitte Terminal neu starten oder PATH anpassen."
            exit 1
        fi
    fi

    # Zusaetzlich arm-none-eabi-newlib pruefen (wird fuer --specs=nano.specs gebraucht)
    info "Pruefe arm-none-eabi-newlib ..."
    local newlib_pkg=""
    case "${MSYSTEM:-MSYS}" in
        MINGW64|UCRT64) newlib_pkg="mingw-w64-x86_64-arm-none-eabi-newlib" ;;
        MINGW32)        newlib_pkg="mingw-w64-i686-arm-none-eabi-newlib"  ;;
        *)              newlib_pkg="mingw-w64-x86_64-arm-none-eabi-newlib" ;;
    esac
    if pacman -Qi "$newlib_pkg" &>/dev/null; then
        ok "newlib vorhanden ($newlib_pkg)"
    else
        warn "newlib ($newlib_pkg) fehlt eventuell."
        pacman_install "$newlib_pkg" "ARM newlib (fuer nano.specs)"
    fi
}

# ---------------------------------------------------------------------------
# 3. Abhaengigkeit: Python 3
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
                return
            fi
        fi
    done

    fail "Python 3 nicht gefunden."
    local pkg=""
    case "${MSYSTEM:-MSYS}" in
        MINGW64|UCRT64) pkg="mingw-w64-x86_64-python" ;;
        MINGW32)        pkg="mingw-w64-i686-python"   ;;
        *)              pkg="python"                   ;;
    esac
    pacman_install "$pkg" "Python 3"

    # Erneut suchen
    for cmd in python3 python py; do
        if command -v "$cmd" &>/dev/null; then
            local ver
            ver=$("$cmd" --version 2>&1 || true)
            if echo "$ver" | grep -qi "python 3"; then
                PYTHON_CMD="$cmd"
                ok "Python 3 gefunden nach Installation: $ver"
                return
            fi
        fi
    done

    warn "Python 3 wurde nicht gefunden — .packed.bin kann nicht erzeugt werden."
}

# ---------------------------------------------------------------------------
# 4. Abhaengigkeit: pip + crcmod
# ---------------------------------------------------------------------------
check_pip_crcmod() {
    if [[ -z "$PYTHON_CMD" ]]; then
        warn "Python fehlt — ueberspringe pip/crcmod Pruefung."
        return
    fi

    info "Pruefe pip ..."
    if ! "$PYTHON_CMD" -m pip --version &>/dev/null; then
        warn "pip ist nicht verfuegbar."
        local pkg=""
        case "${MSYSTEM:-MSYS}" in
            MINGW64|UCRT64) pkg="mingw-w64-x86_64-python-pip" ;;
            MINGW32)        pkg="mingw-w64-i686-python-pip"   ;;
            *)              pkg="python-pip"                   ;;
        esac
        pacman_install "$pkg" "Python pip"
    else
        ok "pip verfuegbar."
    fi

    info "Pruefe Python-Modul crcmod ..."
    if "$PYTHON_CMD" -c "import crcmod" &>/dev/null; then
        ok "crcmod ist installiert."
    else
        warn "crcmod fehlt."
        pip_install "crcmod" "crcmod (fuer .packed.bin Erzeugung)"
    fi
}

# ---------------------------------------------------------------------------
# 5. Abhaengigkeit: git (optional, fuer Versionserkennung)
# ---------------------------------------------------------------------------
check_git() {
    info "Pruefe git (optional, fuer Versionsstring) ..."
    if command -v git &>/dev/null; then
        ok "git gefunden: $(git --version)"
    else
        warn "git nicht gefunden — Versionsstring wird 'NOGIT' sein."
        if ask_yes_no "Soll git installiert werden?"; then
            pacman_install "git" "Git"
        fi
    fi
}

# ---------------------------------------------------------------------------
# Build ausfuehren
# ---------------------------------------------------------------------------
do_build() {
    separator
    info "Starte Build ..."
    separator
    echo ""

    # In das Verzeichnis des Skripts wechseln
    local script_dir
    script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
    cd "$script_dir"
    info "Arbeitsverzeichnis: $(pwd)"

    # Alte Build-Artefakte loeschen
    info "Loesche alte Build-Artefakte ..."
    rm -f firmware_uvk5_v1 firmware_uvk5_v1.bin firmware_uvk5_v1.packed.bin
    rm -f firmware_uvk5_v1_*.bin firmware_uvk5_v1_*.packed.bin
    find . -name '*.o' -delete 2>/dev/null || true
    find . -name '*.d' -delete 2>/dev/null || true
    ok "Alte Artefakte geloescht."

    # Build-Kommando zusammensetzen
    local make_args="-B"
    if [[ -n "$PYTHON_CMD" ]]; then
        make_args="$make_args MY_PYTHON=$PYTHON_CMD"
    fi

    info "Fuehre aus: make $make_args"
    echo ""

    if make $make_args; then
        echo ""
        ok "Build erfolgreich!"
    else
        echo ""
        fail "Build fehlgeschlagen! Siehe Fehlermeldungen oben."
        exit 1
    fi

    # Zeitstempel-Kopien erstellen
    if [[ -f firmware_uvk5_v1.bin ]]; then
        local stamp
        stamp=$(date +%Y%m%d_%H%M%S)

        cp firmware_uvk5_v1.bin "firmware_uvk5_v1_MENU_TEST.bin"
        cp firmware_uvk5_v1.bin "firmware_uvk5_v1_${stamp}.bin"
        ok "Kopie: firmware_uvk5_v1_${stamp}.bin"

        if [[ -f firmware_uvk5_v1.packed.bin ]]; then
            cp firmware_uvk5_v1.packed.bin "firmware_uvk5_v1_MENU_TEST.packed.bin"
            cp firmware_uvk5_v1.packed.bin "firmware_uvk5_v1_${stamp}.packed.bin"
            ok "Kopie: firmware_uvk5_v1_${stamp}.packed.bin"
        fi

        separator
        echo ""
        echo "  ${GREEN}${BOLD}Fertig!${RESET}"
        echo ""
        echo "  Zum Flashen diese Datei verwenden:"
        echo "    ${BOLD}firmware_uvk5_v1_${stamp}.packed.bin${RESET}"
        echo ""
        echo "  Browser-Flasher:  https://egzumer.github.io/uvtools/"
        echo ""
        separator
    else
        fail "firmware_uvk5_v1.bin wurde nicht erzeugt."
        exit 1
    fi
}

# ===========================================================================
# Hauptprogramm
# ===========================================================================
main() {
    echo ""
    separator
    echo "  ${BOLD}Quansheng UV-K5 ARDF Firmware — MSYS2 Build${RESET}"
    separator
    echo ""

    check_msys2_env
    echo ""

    info "${BOLD}Pruefe Abhaengigkeiten ...${RESET}"
    echo ""

    check_make
    check_arm_toolchain
    check_python
    check_pip_crcmod
    check_git

    echo ""
    separator
    ok "Alle Abhaengigkeiten geprueft."
    separator
    echo ""

    if ask_yes_no "Firmware jetzt bauen?"; then
        do_build
    else
        info "Build abgebrochen. Alle Abhaengigkeiten sind vorhanden."
    fi
}

main "$@"
