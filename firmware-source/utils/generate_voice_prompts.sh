#!/bin/bash
# ==============================================================================
# generate_voice_prompts.sh — eSpeak + FFmpeg Voice-Prompt-Generator
# ==============================================================================
#
# Erzeugt Sprachdateien fuer die Quansheng K5 ARDF Firmware.
# Laeuft unter MSYS2 (MINGW64), Linux oder macOS.
#
# Voraussetzungen:
#   MSYS2:  pacman -S mingw-w64-x86_64-espeak-ng mingw-w64-x86_64-ffmpeg
#   Linux:  sudo apt install espeak-ng ffmpeg
#   macOS:  brew install espeak ffmpeg
#
# Verwendung:
#   ./generate_voice_prompts.sh [--mode full|letters|test]
#
# Modi:
#   full    — Erzeugt alle Wort-Prompts (fuer Option B)
#   letters — Erzeugt nur Einzelbuchstaben A-Z und Ziffern 0-9 (fuer Option C)
#   test    — Erzeugt nur 3 Test-Prompts zum Pruefen der Groesse
#
# Ausgabe:
#   output/wav/          — WAV-Dateien (16-bit, 22050 Hz)
#   output/raw/          — Rohdaten (8-bit unsigned, 8000 Hz mono)
#   output/adpcm/        — IMA-ADPCM komprimiert (4-bit, 8000 Hz mono)
#   output/headers/      — C-Header-Dateien mit const uint8_t Arrays
#   output/summary.txt   — Groessenuebersicht
#
# ==============================================================================

set -euo pipefail

# --- Konfiguration ---

ESPEAK_VOICE="en"          # Sprache: en = Englisch
ESPEAK_SPEED=130           # Sprechgeschwindigkeit (eSpeak-Standard: 175, hier 130 fuer deutlichere Aussprache)
ESPEAK_PITCH=50            # Tonhoehe (Standard: 50)
ESPEAK_AMPLITUDE=100       # Lautstaerke (0-200)

SAMPLE_RATE_RAW=8000       # Samplerate fuer Rohdaten
SAMPLE_RATE_ADPCM=8000     # Samplerate fuer ADPCM
BITS_RAW=8                 # 8-bit unsigned PCM

OUTPUT_DIR="output"
MODE="${1:---mode}"
MODE_VALUE="${2:-test}"

# Wenn nur ein Argument: --mode value
if [[ "$MODE" == "--mode" ]]; then
    MODE="$MODE_VALUE"
elif [[ "$MODE" == "full" || "$MODE" == "letters" || "$MODE" == "test" ]]; then
    MODE="$MODE"
else
    MODE="test"
fi

# --- Hilfsfunktionen ---

check_dependencies() {
    local missing=()

    if ! command -v espeak-ng &>/dev/null && ! command -v espeak &>/dev/null; then
        missing+=("espeak-ng")
    fi

    if ! command -v ffmpeg &>/dev/null; then
        missing+=("ffmpeg")
    fi

    if ! command -v bc &>/dev/null; then
        missing+=("bc")
    fi

    if [[ ${#missing[@]} -gt 0 ]]; then
        echo "FEHLER: Fehlende Abhaengigkeiten: ${missing[*]}"
        echo ""
        echo "Installation:"
        if [[ "${MSYSTEM:-}" == "MINGW64" ]]; then
            echo "  pacman -S mingw-w64-x86_64-espeak-ng mingw-w64-x86_64-ffmpeg bc"
        elif [[ "$(uname)" == "Linux" ]]; then
            echo "  sudo apt install espeak-ng ffmpeg bc"
        elif [[ "$(uname)" == "Darwin" ]]; then
            echo "  brew install espeak ffmpeg"  # bc is included in macOS
        fi
        exit 1
    fi
}

get_espeak_cmd() {
    if command -v espeak-ng &>/dev/null; then
        echo "espeak-ng"
    else
        echo "espeak"
    fi
}

# Erzeugt eine WAV-Datei mit eSpeak
generate_wav() {
    local text="$1"
    local filename="$2"
    local espeak_cmd
    espeak_cmd=$(get_espeak_cmd)

    mkdir -p "$OUTPUT_DIR/wav"

    "$espeak_cmd" \
        -v "$ESPEAK_VOICE" \
        -s "$ESPEAK_SPEED" \
        -p "$ESPEAK_PITCH" \
        -a "$ESPEAK_AMPLITUDE" \
        -w "$OUTPUT_DIR/wav/${filename}.wav" \
        "$text"

    echo "  WAV: $filename.wav ($(wc -c < "$OUTPUT_DIR/wav/${filename}.wav") bytes)"
}

# Konvertiert WAV in 8-bit unsigned Raw PCM
convert_to_raw() {
    local filename="$1"
    mkdir -p "$OUTPUT_DIR/raw"

    ffmpeg -y -i "$OUTPUT_DIR/wav/${filename}.wav" \
        -ar "$SAMPLE_RATE_RAW" \
        -ac 1 \
        -f u8 \
        -acodec pcm_u8 \
        "$OUTPUT_DIR/raw/${filename}.raw" \
        2>/dev/null

    echo "  RAW: $filename.raw ($(wc -c < "$OUTPUT_DIR/raw/${filename}.raw") bytes)"
}

# Konvertiert WAV in IMA-ADPCM
convert_to_adpcm() {
    local filename="$1"
    mkdir -p "$OUTPUT_DIR/adpcm"

    ffmpeg -y -i "$OUTPUT_DIR/wav/${filename}.wav" \
        -ar "$SAMPLE_RATE_ADPCM" \
        -ac 1 \
        -c:a adpcm_ima_wav \
        "$OUTPUT_DIR/adpcm/${filename}.wav" \
        2>/dev/null

    echo "  ADPCM: $filename.wav ($(wc -c < "$OUTPUT_DIR/adpcm/${filename}.wav") bytes)"
}

# Erzeugt C-Header aus Raw-Datei
generate_c_header() {
    local filename="$1"
    local varname="$2"
    mkdir -p "$OUTPUT_DIR/headers"

    local raw_file="$OUTPUT_DIR/raw/${filename}.raw"
    local header_file="$OUTPUT_DIR/headers/${filename}.h"
    local size
    size=$(wc -c < "$raw_file")

    {
        echo "/* Auto-generated voice prompt: $filename */"
        echo "/* Sample rate: ${SAMPLE_RATE_RAW} Hz, 8-bit unsigned, mono */"
        echo "/* Size: $size bytes, Duration: $(echo "scale=2; $size / $SAMPLE_RATE_RAW" | bc)s */"
        echo ""
        echo "#ifndef VOICE_PROMPT_${varname}_H"
        echo "#define VOICE_PROMPT_${varname}_H"
        echo ""
        echo "#include <stdint.h>"
        echo ""
        echo "#define VOICE_PROMPT_${varname}_SIZE ${size}U"
        echo "#define VOICE_PROMPT_${varname}_SAMPLE_RATE ${SAMPLE_RATE_RAW}U"
        echo ""
        echo "static const uint8_t voice_prompt_${varname}[${size}] = {"

        xxd -i < "$raw_file" | sed 's/^/    /'

        echo "};"
        echo ""
        echo "#endif /* VOICE_PROMPT_${varname}_H */"
    } > "$header_file"

    echo "  Header: $filename.h ($size bytes data)"
}

# --- Prompt-Definitionen ---

# Option B: Vollstaendige Wort-Prompts fuer alle Menuepunkte ohne Voice-Clip
declare -A FULL_PROMPTS=(
    ["prompt_amfm"]="A M F M"
    ["prompt_ardf"]="A R D F"
    ["prompt_number_fox"]="Number fox"
    ["prompt_fox_duration"]="Fox duration"
    ["prompt_active_fox"]="Active fox"
    ["prompt_time_reset"]="Time reset"
    ["prompt_gain_remember"]="Gain remember"
    ["prompt_end_signal"]="End signal"
    ["prompt_snapshot_speed"]="Snapshot speed"
    ["prompt_backlight"]="Backlight"
    ["prompt_backlight_min"]="Backlight minimum"
    ["prompt_backlight_max"]="Backlight maximum"
    ["prompt_morse_speed"]="Morse speed"
    # Zusaetzliche Status-Prompts
    ["prompt_on"]="On"
    ["prompt_off"]="Off"
    ["prompt_battery"]="Battery"
    ["prompt_frequency"]="Frequency"
)

# Option C: Einzelbuchstaben
declare -A LETTER_PROMPTS=()
for letter in A B C D E F G H I J K L M N O P Q R S T U V W X Y Z; do
    LETTER_PROMPTS["letter_${letter,,}"]="$letter"
done
for digit in 0 1 2 3 4 5 6 7 8 9; do
    LETTER_PROMPTS["digit_${digit}"]="$digit"
done

# Test: Nur 3 Prompts
declare -A TEST_PROMPTS=(
    ["test_ardf"]="A R D F"
    ["test_backlight"]="Backlight"
    ["test_on"]="On"
)

# --- Hauptprogramm ---

main() {
    check_dependencies

    echo "============================================"
    echo "Voice Prompt Generator fuer Quansheng K5"
    echo "Modus: $MODE"
    echo "============================================"
    echo ""

    # Prompt-Array waehlen
    local -n prompts
    case "$MODE" in
        full)    prompts=FULL_PROMPTS ;;
        letters) prompts=LETTER_PROMPTS ;;
        test)    prompts=TEST_PROMPTS ;;
        *)
            echo "Unbekannter Modus: $MODE"
            echo "Verwendung: $0 [--mode full|letters|test]"
            exit 1
            ;;
    esac

    local total_raw=0
    local total_adpcm=0
    local count=0

    # Zusammenfassung vorbereiten
    mkdir -p "$OUTPUT_DIR"
    : > "$OUTPUT_DIR/summary.txt"

    for filename in "${!prompts[@]}"; do
        local text="${prompts[$filename]}"
        local varname
        varname=$(echo "$filename" | tr '[:lower:]' '[:upper:]')

        echo "--- $filename: \"$text\" ---"

        generate_wav "$text" "$filename"
        convert_to_raw "$filename"
        convert_to_adpcm "$filename"
        generate_c_header "$filename" "$varname"

        local raw_size adpcm_size
        raw_size=$(wc -c < "$OUTPUT_DIR/raw/${filename}.raw")
        adpcm_size=$(wc -c < "$OUTPUT_DIR/adpcm/${filename}.wav")

        total_raw=$((total_raw + raw_size))
        total_adpcm=$((total_adpcm + adpcm_size))
        count=$((count + 1))

        printf "%-25s  %-20s  RAW: %6d B  ADPCM: %6d B\n" \
            "$filename" "\"$text\"" "$raw_size" "$adpcm_size" \
            >> "$OUTPUT_DIR/summary.txt"

        echo ""
    done

    # Zusammenfassung
    {
        echo ""
        echo "============================================"
        echo "ZUSAMMENFASSUNG"
        echo "============================================"
        echo "Anzahl Prompts:     $count"
        echo "Gesamt RAW:         $total_raw Bytes ($(echo "scale=1; $total_raw / 1024" | bc) KB)"
        echo "Gesamt ADPCM:       $total_adpcm Bytes ($(echo "scale=1; $total_adpcm / 1024" | bc) KB)"
        echo ""
        echo "Verfuegbarer Flash: ~15360 Bytes (15 KB)"
        echo "RAW passt:          $(if [[ $total_raw -le 15360 ]]; then echo "JA"; else echo "NEIN ($total_raw > 15360)"; fi)"
        echo "ADPCM passt:        $(if [[ $total_adpcm -le 15360 ]]; then echo "JA"; else echo "NEIN ($total_adpcm > 15360)"; fi)"
    } | tee -a "$OUTPUT_DIR/summary.txt"

    echo ""
    echo "Dateien in: $OUTPUT_DIR/"
    echo "Zusammenfassung: $OUTPUT_DIR/summary.txt"
}

main "$@"
