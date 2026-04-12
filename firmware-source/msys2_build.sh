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
#   - Das Skript erfordert die MSYS2 MinGW64-Shell.
#   - Andere Shells (UCRT64, MINGW32, MSYS) werden mit erklaerenden
#     Fehlermeldungen abgelehnt.
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
# Pruefen, ob wir in der richtigen MSYS2-Umgebung laufen (MINGW64)
# ---------------------------------------------------------------------------
check_msys2_env() {
    if [[ -z "${MSYSTEM:-}" ]]; then
        fail "MSYSTEM nicht gesetzt — vermutlich keine MSYS2-Shell."
        fail "Dieses Skript muss in einer MSYS2 ${BOLD}MINGW64${RESET}${RED}-Shell gestartet werden."
        echo ""
        echo "  So geht es:"
        echo "    Start > MSYS2 MinGW x64"
        echo "    oder im MSYS2-Installer: Environments > MINGW64"
        echo ""
        exit 1
    fi

    if [[ "${MSYSTEM}" != "MINGW64" ]]; then
        fail "Falsche MSYS2-Umgebung: ${BOLD}${MSYSTEM}${RESET}"
        fail "Dieses Skript benoetigt ${BOLD}MINGW64${RESET}."
        echo ""
        echo "  Aktuell:  $MSYSTEM"
        echo "  Erwartet: MINGW64"
        echo ""
        echo "  Bitte die richtige Shell oeffnen:"
        echo "    Start > MSYS2 MinGW x64"
        echo ""
        case "$MSYSTEM" in
            UCRT64)
                echo "  Hinweis: UCRT64 verwendet eine andere C-Runtime (ucrt)."
                echo "  Die ARM-Toolchain-Pakete sind fuer MINGW64 gebaut."
                ;;
            MINGW32)
                echo "  Hinweis: MINGW32 ist die 32-Bit-Variante."
                echo "  Fuer dieses Projekt wird 64-Bit (MINGW64) benoetigt."
                ;;
            MSYS)
                echo "  Hinweis: Die MSYS-Shell nutzt eine POSIX-Schicht."
                echo "  Fuer native Windows-Builds bitte MINGW64 verwenden."
                ;;
        esac
        echo ""
        exit 1
    fi

    ok "MSYS2-Umgebung erkannt: ${BOLD}$MSYSTEM${RESET}"
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
# Pip-Wrapper: Python-Paket installieren (PEP 668 sicher)
# Versucht zuerst pacman, dann venv-basiertes pip als Fallback.
# ---------------------------------------------------------------------------
pip_install_crcmod() {
    # 1. Versuch: pacman (MSYS2-natives Paket)
    local pacman_pkg="mingw-w64-x86_64-python-crcmod"
    info "Versuche crcmod ueber pacman ($pacman_pkg) ..."
    if pacman -Si "$pacman_pkg" &>/dev/null; then
        if ask_yes_no "Soll crcmod via pacman installiert werden? ($pacman_pkg)"; then
            pacman -S --noconfirm "$pacman_pkg"
            if "$PYTHON_CMD" -c "import crcmod" &>/dev/null; then
                ok "crcmod via pacman installiert."
                return 0
            else
                warn "pacman-Installation war erfolgreich, aber import schlaegt fehl."
            fi
        fi
    else
        info "Paket $pacman_pkg nicht in pacman verfuegbar — nutze Fallback."
    fi

    # 2. Versuch: pip in einem temporaeren venv (PEP 668 sicher)
    info "Erstelle temporaeres Python-venv fuer crcmod ..."
    local venv_dir
    venv_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/.venv_build"

    if ask_yes_no "Soll crcmod in einem lokalen venv ($venv_dir) installiert werden?"; then
        "$PYTHON_CMD" -m venv "$venv_dir" 2>/dev/null || {
            fail "venv konnte nicht erstellt werden."
            warn "crcmod fehlt — .packed.bin wird nicht erzeugt."
            return 1
        }
        # pip im venv ausfuehren
        "$venv_dir/bin/pip" install --upgrade crcmod 2>/dev/null \
            || "$venv_dir/Scripts/pip" install --upgrade crcmod 2>/dev/null \
            || {
                fail "pip install crcmod im venv fehlgeschlagen."
                warn ".packed.bin wird nicht erzeugt."
                return 1
            }
        # PYTHON_CMD auf das venv-Python umstellen
        if [[ -x "$venv_dir/bin/python" ]]; then
            PYTHON_CMD="$venv_dir/bin/python"
        elif [[ -x "$venv_dir/Scripts/python.exe" ]]; then
            PYTHON_CMD="$venv_dir/Scripts/python.exe"
        fi
        if "$PYTHON_CMD" -c "import crcmod" &>/dev/null; then
            ok "crcmod im venv installiert. Python: $PYTHON_CMD"
            return 0
        else
            fail "crcmod import schlaegt auch im venv fehl."
            return 1
        fi
    else
        warn "crcmod fehlt — .packed.bin wird nicht erzeugt, aber der Build ist moeglich."
        return 1
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
        pacman_install "make" "GNU Make"
        # Erneut pruefen
        if ! command -v make &>/dev/null; then
            fail "make immer noch nicht verfuegbar. Bitte manuell installieren."
            exit 1
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
        echo "  Die ARM Embedded Toolchain wird ueber pacman installiert:"
        echo "    pacman -S mingw-w64-x86_64-arm-none-eabi-gcc"
        echo ""
        echo "  Alternativ: offizielle ARM-Toolchain manuell installieren:"
        echo "    https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads"
        echo ""
        pacman_install "mingw-w64-x86_64-arm-none-eabi-gcc" "ARM GCC Toolchain"

        if ! command -v arm-none-eabi-gcc &>/dev/null; then
            fail "arm-none-eabi-gcc immer noch nicht gefunden."
            fail "Bitte Terminal neu starten oder PATH anpassen."
            exit 1
        fi
    fi

    # Zusaetzlich arm-none-eabi-newlib pruefen (wird fuer --specs=nano.specs gebraucht)
    info "Pruefe arm-none-eabi-newlib ..."
    local newlib_pkg="mingw-w64-x86_64-arm-none-eabi-newlib"
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
    pacman_install "mingw-w64-x86_64-python" "Python 3"

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
# 4. Abhaengigkeit: crcmod (fuer .packed.bin)
# ---------------------------------------------------------------------------
check_pip_crcmod() {
    if [[ -z "$PYTHON_CMD" ]]; then
        warn "Python fehlt — ueberspringe crcmod Pruefung."
        return
    fi

    info "Pruefe Python-Modul crcmod ..."
    if "$PYTHON_CMD" -c "import crcmod" &>/dev/null; then
        ok "crcmod ist installiert."
    else
        warn "crcmod fehlt."
        pip_install_crcmod
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

    # Ausgabeverzeichnis (neben firmware-source/)
    local out_dir="$script_dir/../build-output"
    mkdir -p "$out_dir"
    out_dir="$(cd "$out_dir" && pwd)"
    info "Ausgabeverzeichnis: $out_dir"

    # Alte Build-Artefakte loeschen (im Quellverzeichnis, via Makefile)
    info "Loesche alte Build-Artefakte (make clean) ..."
    make clean 2>/dev/null || true
    # Sicherheitshalber ELF und lose Binaries entfernen
    rm -f firmware_uvk5_v1 firmware_uvk5_v1.bin firmware_uvk5_v1.packed.bin
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

    # Firmware-Binaries ins Ausgabeverzeichnis verschieben
    if [[ -f firmware_uvk5_v1.bin ]]; then
        local stamp
        stamp=$(date +%Y%m%d_%H%M%S)

        # Basis-Dateien verschieben
        mv firmware_uvk5_v1.bin "$out_dir/"
        ok "Verschoben: firmware_uvk5_v1.bin -> build-output/"

        # Zeitstempel-Kopien im Ausgabeverzeichnis
        cp "$out_dir/firmware_uvk5_v1.bin" "$out_dir/firmware_uvk5_v1_${stamp}.bin"
        ok "Kopie: firmware_uvk5_v1_${stamp}.bin"

        if [[ -f firmware_uvk5_v1.packed.bin ]]; then
            mv firmware_uvk5_v1.packed.bin "$out_dir/"
            ok "Verschoben: firmware_uvk5_v1.packed.bin -> build-output/"

            cp "$out_dir/firmware_uvk5_v1.packed.bin" "$out_dir/firmware_uvk5_v1_${stamp}.packed.bin"
            ok "Kopie: firmware_uvk5_v1_${stamp}.packed.bin"
        fi

        # ELF-Datei und Objekt-Dateien aufraeumen (Quellverzeichnis sauber halten)
        info "Raeume Quellverzeichnis auf (make clean) ..."
        make clean 2>/dev/null || true
        rm -f firmware_uvk5_v1
        ok "Quellverzeichnis aufgeraeumt."

        separator
        echo ""
        echo "  ${GREEN}${BOLD}Fertig!${RESET}"
        echo ""
        echo "  Alle Firmware-Dateien liegen in:"
        echo "    ${BOLD}$out_dir/${RESET}"
        echo ""
        echo "  Zum Flashen diese Datei verwenden:"
        echo "    ${BOLD}build-output/firmware_uvk5_v1_${stamp}.packed.bin${RESET}"
        echo ""
        echo "  Browser-Flasher:  https://egzumer.github.io/uvtools/"
        echo ""
        separator
    else
        fail "firmware_uvk5_v1.bin wurde nicht erzeugt."
        exit 1
    fi
}

# ---------------------------------------------------------------------------
# Build-Verzeichnis aufraeumen (alte Dateien entfernen)
# ---------------------------------------------------------------------------
do_cleanup() {
    local script_dir
    script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
    local out_dir="$script_dir/../build-output"

    separator
    info "Aufraeumen ..."
    separator
    echo ""

    # --- Teil 1: Quellverzeichnis aufraeumen (Objekt-/Dependency-Dateien) ---
    info "Raeume Quellverzeichnis auf (make clean) ..."
    cd "$script_dir"
    make clean 2>/dev/null || true
    rm -f firmware_uvk5_v1 firmware_uvk5_v1.bin firmware_uvk5_v1.packed.bin
    ok "Quellverzeichnis sauber (alle .o, .d, ELF, .bin entfernt)."
    echo ""

    # --- Teil 2: Build-Ausgabeverzeichnis aufraeumen ---
    if [[ ! -d "$out_dir" ]]; then
        info "Kein Build-Ausgabeverzeichnis vorhanden — nichts aufzuraeumen."
        return
    fi

    out_dir="$(cd "$out_dir" && pwd)"

    # Zaehle Dateien NUR im Build-Output-Verzeichnis
    local total_files
    total_files=$(find "$out_dir" -maxdepth 1 -type f | wc -l)

    if [[ "$total_files" -eq 0 ]]; then
        info "Build-Ausgabeverzeichnis ist bereits leer."
        return
    fi

    info "Build-Ausgabeverzeichnis: $out_dir"
    info "Enthaltene Dateien: $total_files"
    echo ""

    # Aktuellste .bin und .packed.bin identifizieren (nur im Ausgabeverzeichnis)
    local newest_bin newest_packed
    newest_bin=$(find "$out_dir" -maxdepth 1 -name 'firmware_uvk5_v1*.bin' ! -name '*.packed.bin' -type f -printf '%T@ %p\n' 2>/dev/null | sort -rn | head -1 | cut -d' ' -f2-)
    newest_packed=$(find "$out_dir" -maxdepth 1 -name 'firmware_uvk5_v1*.packed.bin' -type f -printf '%T@ %p\n' 2>/dev/null | sort -rn | head -1 | cut -d' ' -f2-)

    if [[ -n "$newest_bin" || -n "$newest_packed" ]]; then
        info "Aktuellste Dateien (werden behalten):"
        [[ -n "$newest_bin" ]]    && echo "    ${BOLD}$(basename "$newest_bin")${RESET}"
        [[ -n "$newest_packed" ]] && echo "    ${BOLD}$(basename "$newest_packed")${RESET}"
        echo ""
    fi

    # Dateien die geloescht werden sollen auflisten
    local to_delete=()
    while IFS= read -r -d '' file; do
        # Aktuellste Dateien behalten
        if [[ "$file" == "$newest_bin" || "$file" == "$newest_packed" ]]; then
            continue
        fi
        to_delete+=("$file")
    done < <(find "$out_dir" -maxdepth 1 -type f -print0)

    if [[ ${#to_delete[@]} -eq 0 ]]; then
        info "Keine alten Dateien zum Loeschen gefunden."
        return
    fi

    echo "  Folgende ${#to_delete[@]} aeltere Dateien wuerden geloescht:"
    for f in "${to_delete[@]}"; do
        echo "    - $(basename "$f")"
    done
    echo ""

    if ask_yes_no "Alte Dateien jetzt loeschen?"; then
        for f in "${to_delete[@]}"; do
            rm -f "$f"
        done
        ok "${#to_delete[@]} Dateien geloescht."
    else
        info "Aufraeumen abgebrochen."
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

        echo ""
        if ask_yes_no "Build-Verzeichnis aufraeumen (alte Dateien entfernen)?"; then
            do_cleanup
        fi
    else
        info "Build abgebrochen. Alle Abhaengigkeiten sind vorhanden."
    fi
}

main "$@"
