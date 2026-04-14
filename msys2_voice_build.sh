#!/usr/bin/env bash
# =============================================================================
# msys2_voice_build.sh — Voice Prompt Builder for V3 Firmware (MSYS2)
# =============================================================================
# Generates voice prompt files for the Quansheng UV-K5 V3/K1 radios using
# eSpeak-NG for text-to-speech and FFmpeg for audio conversion.
#
# The output is a .vpk (Voice Pack) binary file that can be flashed to the
# PY25Q16 SPI flash using msys2_voice_flash.sh or k5flash_voice_v3.py.
#
# Usage:
#   1.  Open MSYS2 MinGW64 shell
#   2.  cd to the repository root
#   3.  chmod +x msys2_voice_build.sh
#   4.  ./msys2_voice_build.sh [--lang de|en] [--output file.vpk]
#
# =============================================================================

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")" && pwd)"
OUTPUT_DIR="$REPO_ROOT/build-output"
TEMP_DIR=""
LANG_CODE="de"
OUTPUT_FILE=""

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

cleanup() {
    if [[ -n "$TEMP_DIR" && -d "$TEMP_DIR" ]]; then
        rm -rf "$TEMP_DIR"
    fi
}
trap cleanup EXIT

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
# Parse arguments
# ---------------------------------------------------------------------------
while [[ $# -gt 0 ]]; do
    case "$1" in
        --lang)
            LANG_CODE="$2"
            shift 2
            ;;
        --output)
            OUTPUT_FILE="$2"
            shift 2
            ;;
        *)
            echo "Unbekannter Parameter: $1"
            echo "Verwendung: $0 [--lang de|en] [--output datei.vpk]"
            exit 1
            ;;
    esac
done

if [[ -z "$OUTPUT_FILE" ]]; then
    OUTPUT_FILE="$OUTPUT_DIR/voice_prompts_${LANG_CODE}.vpk"
fi

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
        pacman -S --noconfirm "$pkg" 2>/dev/null || {
            # Try apt/brew for non-MSYS2
            if command -v apt-get &>/dev/null; then
                sudo apt-get install -y "$pkg" 2>/dev/null || true
            elif command -v brew &>/dev/null; then
                brew install "$pkg" 2>/dev/null || true
            fi
        }
    fi
    ok "$pkg installiert."
}

check_dependencies() {
    local missing=()

    # Check espeak-ng
    if ! command -v espeak-ng &>/dev/null && ! command -v espeak &>/dev/null; then
        missing+=("espeak-ng")
    else
        local espeak_cmd
        espeak_cmd=$(command -v espeak-ng 2>/dev/null || command -v espeak)
        ok "eSpeak: $($espeak_cmd --version 2>&1 | head -1)"
    fi

    # Check ffmpeg
    if ! command -v ffmpeg &>/dev/null; then
        missing+=("ffmpeg")
    else
        ok "FFmpeg: $(ffmpeg -version 2>&1 | head -1)"
    fi

    # Check python3
    if ! command -v python3 &>/dev/null && ! command -v python &>/dev/null; then
        missing+=("python3")
    else
        local py_cmd
        py_cmd=$(command -v python3 2>/dev/null || command -v python)
        ok "Python: $($py_cmd --version 2>&1)"
    fi

    if [[ ${#missing[@]} -gt 0 ]]; then
        fail "Fehlende Abhaengigkeiten: ${missing[*]}"

        if [[ -n "${MSYSTEM:-}" ]]; then
            if ask_yes_no "Fehlende Pakete jetzt installieren?"; then
                for pkg in "${missing[@]}"; do
                    case "$pkg" in
                        espeak-ng)
                            install_msys2_package "espeak-ng" || \
                            install_msys2_package "espeak"
                            ;;
                        ffmpeg)
                            install_msys2_package "ffmpeg"
                            ;;
                        python3)
                            install_msys2_package "python"
                            ;;
                    esac
                done
            else
                fail "Ohne Abhaengigkeiten kann nicht gebaut werden."
                exit 1
            fi
        else
            info "Bitte installiere: ${missing[*]}"
            info "  Unter Ubuntu/Debian: sudo apt install espeak-ng ffmpeg python3"
            info "  Unter macOS: brew install espeak ffmpeg python3"
            exit 1
        fi
    fi
}

# ---------------------------------------------------------------------------
# Voice prompt definitions
# ---------------------------------------------------------------------------
# Format: VOICE_ID "Text to speak"
# Voice IDs match the enum in audio.h
#
# Standard voice IDs (0x00 - 0x4B) — these replace the factory English clips
# Extended voice IDs (0x4C+) — new prompts for menu items
#
declare -A VOICE_PROMPTS_DE=(
    # Standard numeric prompts
    [0x00]="null"
    [0x01]="eins"
    [0x02]="zwei"
    [0x03]="drei"
    [0x04]="vier"
    [0x05]="fuenf"
    [0x06]="sechs"
    [0x07]="sieben"
    [0x08]="acht"
    [0x09]="neun"
    [0x0A]="zehn"
    [0x0B]="hundert"
    # Standard status prompts
    [0x0C]="Willkommen"
    [0x0D]="Gesperrt"
    [0x0E]="Entsperrt"
    [0x0F]="Scan Start"
    [0x10]="Scan Stopp"
    [0x11]="Scrambler ein"
    [0x12]="Scrambler aus"
    [0x13]="Funktion"
    [0x14]="C T C S S"
    [0x15]="D C S"
    [0x16]="Leistung"
    [0x17]="Sparmodus"
    [0x18]="Speicher"
    [0x19]="Loeschen"
    [0x1A]="Schritt"
    [0x1B]="Rauschsperre"
    [0x1C]="Sendezeit"
    [0x1D]="Licht"
    [0x1E]="VOX"
    [0x1F]="Offset Richtung"
    [0x20]="Offset Frequenz"
    [0x21]="Sendespeicher"
    [0x22]="Empfangsspeicher"
    [0x23]="Notruf"
    [0x24]="Batterie schwach"
    [0x25]="Kanal"
    [0x26]="Frequenz"
    [0x27]="Sprache"
    [0x28]="Band"
    [0x29]="Dualwatch"
    [0x2A]="Bandbreite"
    [0x2B]="Signal"
    [0x2C]="Stumm"
    [0x2D]="Belegtsperre"
    [0x2E]="Beep"
    [0x2F]="A N I Code"
    [0x30]="Init"
    [0x31]="Bestaetigt"
    [0x32]="Abgebrochen"
    [0x33]="Ein"
    [0x34]="Aus"
    [0x35]="Zwei Ton"
    [0x36]="Fuenf Ton"
    [0x37]="Digital"
    [0x38]="Repeater"
    [0x39]="Menue"
    [0x3A]="elf"
    [0x3B]="zwoelf"
    [0x3C]="dreizehn"
    [0x3D]="vierzehn"
    [0x3E]="fuenfzehn"
    [0x3F]="sechzehn"
    [0x40]="siebzehn"
    [0x41]="achtzehn"
    [0x42]="neunzehn"
    [0x43]="zwanzig"
    [0x44]="dreissig"
    [0x45]="vierzig"
    [0x46]="fuenfzig"
    [0x47]="sechzig"
    [0x48]="siebzig"
    [0x49]="achtzig"
    [0x4A]="neunzig"
    # Extended voice IDs for menu items
    [0x4C]="A R D F"
    [0x4D]="Fuchszahl"
    [0x4E]="Fuchsdauer"
    [0x4F]="Aktiver Fuchs"
    [0x50]="Zeit Reset"
    [0x51]="Gain merken"
    [0x52]="Endsignal"
    [0x53]="Uhr Korrektur"
    [0x54]="Kompander"
    [0x55]="A M F M"
    [0x56]="Sendesperre"
    [0x57]="Kanalliste"
    [0x58]="Kanalname"
    [0x59]="Scanliste"
    [0x5A]="Scan Prio"
    [0x5B]="Prio eins"
    [0x5C]="Prio zwei"
    [0x5D]="Scan Wiederholung"
    [0x5E]="F eins kurz"
    [0x5F]="F eins lang"
    [0x60]="F zwei kurz"
    [0x61]="F zwei lang"
    [0x62]="M lang"
    [0x63]="Tastensperre"
    [0x64]="Sendezeit"
    [0x65]="Batteriesparen"
    [0x66]="Batterietext"
    [0x67]="Mikro"
    [0x68]="Mikro Balken"
    [0x69]="Anzeige"
    [0x6A]="Startmeldung"
    [0x6B]="Lichtzeit"
    [0x6C]="Licht Min"
    [0x6D]="Licht Max"
    [0x6E]="Licht Senden Empfang"
    [0x6F]="Roger Ton"
    [0x70]="S T E"
    [0x71]="Repeater S T E"
    [0x72]="Einzel Anruf"
    [0x73]="Sende Code"
    [0x74]="Empfangs Code"
    [0x75]="P T T Kennung"
    [0x76]="D T M F Ton"
    [0x77]="D T M F Vorlauf"
    [0x78]="D T M F Live"
    [0x79]="VOX"
    [0x7A]="Systeminfo"
    [0x7B]="Empfang"
    [0x7C]="Rauschsperre"
    [0x7D]="Morse Tempo"
    [0x7E]="Snapshot Tempo"
    [0x7F]="Verstimmung"
    [0x80]="Verstimmung Gain"
    [0x81]="Zugang"
    [0x82]="Reset"
)

declare -A VOICE_PROMPTS_EN=(
    # Standard numeric prompts
    [0x00]="zero"
    [0x01]="one"
    [0x02]="two"
    [0x03]="three"
    [0x04]="four"
    [0x05]="five"
    [0x06]="six"
    [0x07]="seven"
    [0x08]="eight"
    [0x09]="nine"
    [0x0A]="ten"
    [0x0B]="hundred"
    # Standard status prompts
    [0x0C]="Welcome"
    [0x0D]="Locked"
    [0x0E]="Unlocked"
    [0x0F]="Scan start"
    [0x10]="Scan stop"
    [0x11]="Scrambler on"
    [0x12]="Scrambler off"
    [0x13]="Function"
    [0x14]="C T C S S"
    [0x15]="D C S"
    [0x16]="TX power"
    [0x17]="Save mode"
    [0x18]="Memory"
    [0x19]="Delete"
    [0x1A]="Frequency step"
    [0x1B]="Squelch"
    [0x1C]="TX timeout"
    [0x1D]="Backlight"
    [0x1E]="VOX"
    [0x1F]="Offset direction"
    [0x20]="Offset frequency"
    [0x21]="TX memory"
    [0x22]="RX memory"
    [0x23]="Emergency"
    [0x24]="Low voltage"
    [0x25]="Channel"
    [0x26]="Frequency"
    [0x27]="Voice"
    [0x28]="Band"
    [0x29]="Dual standby"
    [0x2A]="Bandwidth"
    [0x2B]="Signal"
    [0x2C]="Mute"
    [0x2D]="Busy lock"
    [0x2E]="Beep"
    [0x2F]="A N I code"
    [0x30]="Init"
    [0x31]="Confirm"
    [0x32]="Cancel"
    [0x33]="On"
    [0x34]="Off"
    [0x35]="Two tone"
    [0x36]="Five tone"
    [0x37]="Digital"
    [0x38]="Repeater"
    [0x39]="Menu"
    [0x3A]="eleven"
    [0x3B]="twelve"
    [0x3C]="thirteen"
    [0x3D]="fourteen"
    [0x3E]="fifteen"
    [0x3F]="sixteen"
    [0x40]="seventeen"
    [0x41]="eighteen"
    [0x42]="nineteen"
    [0x43]="twenty"
    [0x44]="thirty"
    [0x45]="forty"
    [0x46]="fifty"
    [0x47]="sixty"
    [0x48]="seventy"
    [0x49]="eighty"
    [0x4A]="ninety"
    # Extended voice IDs for menu items
    [0x4C]="A R D F"
    [0x4D]="Fox count"
    [0x4E]="Fox duration"
    [0x4F]="Active fox"
    [0x50]="Time reset"
    [0x51]="Gain save"
    [0x52]="End signal"
    [0x53]="Clock adjust"
    [0x54]="Compander"
    [0x55]="AM FM"
    [0x56]="TX lock"
    [0x57]="Channel list"
    [0x58]="Channel name"
    [0x59]="Scan list"
    [0x5A]="Scan prio"
    [0x5B]="Prio one"
    [0x5C]="Prio two"
    [0x5D]="Scan resume"
    [0x5E]="F one short"
    [0x5F]="F one long"
    [0x60]="F two short"
    [0x61]="F two long"
    [0x62]="M long"
    [0x63]="Key lock"
    [0x64]="TX timeout"
    [0x65]="Battery save"
    [0x66]="Battery text"
    [0x67]="Mic"
    [0x68]="Mic bar"
    [0x69]="Display"
    [0x6A]="Startup"
    [0x6B]="Light time"
    [0x6C]="Light min"
    [0x6D]="Light max"
    [0x6E]="Light TX RX"
    [0x6F]="Roger"
    [0x70]="S T E"
    [0x71]="Repeater S T E"
    [0x72]="One call"
    [0x73]="Up code"
    [0x74]="Down code"
    [0x75]="P T T ID"
    [0x76]="D T M F tone"
    [0x77]="D T M F start"
    [0x78]="D T M F live"
    [0x79]="VOX"
    [0x7A]="System info"
    [0x7B]="RX mode"
    [0x7C]="Squelch"
    [0x7D]="Morse speed"
    [0x7E]="Snap speed"
    [0x7F]="Mistune"
    [0x80]="Mistune gain"
    [0x81]="Access"
    [0x82]="Reset"
)

# ---------------------------------------------------------------------------
# Voice generation
# ---------------------------------------------------------------------------
generate_voice_prompts() {
    local lang_code="$1"
    local temp_dir="$2"

    local espeak_cmd
    espeak_cmd=$(command -v espeak-ng 2>/dev/null || command -v espeak)

    local espeak_voice
    case "$lang_code" in
        de) espeak_voice="de" ;;
        en) espeak_voice="en" ;;
        *)  espeak_voice="$lang_code" ;;
    esac

    # Select prompt set
    local -n prompts
    case "$lang_code" in
        de) prompts=VOICE_PROMPTS_DE ;;
        en) prompts=VOICE_PROMPTS_EN ;;
        *)
            fail "Unbekannte Sprache: $lang_code"
            exit 1
            ;;
    esac

    local wav_dir="$temp_dir/wav"
    local raw_dir="$temp_dir/raw"
    mkdir -p "$wav_dir" "$raw_dir"

    local total="${#prompts[@]}"
    local count=0

    info "Generiere $total Voice-Prompts ($lang_code) ..."

    # Sort keys for deterministic processing
    local sorted_keys
    sorted_keys=$(printf '%s\n' "${!prompts[@]}" | sort -t'x' -k2 -n)

    for voice_id_hex in $sorted_keys; do
        local text="${prompts[$voice_id_hex]}"
        local id_dec=$((voice_id_hex))
        local id_str
        id_str=$(printf '%02x' "$id_dec")

        count=$((count + 1))

        # Generate WAV with eSpeak (faster speech rate to keep clips short)
        local wav_file="$wav_dir/voice_${id_str}.wav"
        $espeak_cmd -v "$espeak_voice" -s 170 -p 50 -a 200 \
            --stdout "$text" > "$wav_file" 2>/dev/null

        # Convert to 8kHz 8-bit unsigned mono PCM with FFmpeg
        # Filter chain:
        #   1. silenceremove (forward): strip leading silence below -40dB
        #   2. areverse + silenceremove + areverse: strip trailing silence
        #      (FFmpeg's silenceremove only works on leading silence, so we
        #       reverse → strip → reverse to handle the trailing end)
        #   3. highpass/lowpass: bandpass 200-3500 Hz for radio speaker
        #   4. volume: boost level for small speaker output
        local raw_file="$raw_dir/voice_${id_str}.raw"
        ffmpeg -y -i "$wav_file" \
            -af "silenceremove=start_periods=1:start_threshold=-40dB:start_silence=0.02,areverse,silenceremove=start_periods=1:start_threshold=-40dB:start_silence=0.02,areverse,highpass=f=200,lowpass=f=3500,volume=1.5" \
            -ar 8000 -ac 1 -acodec pcm_u8 \
            -f u8 "$raw_file" 2>/dev/null

        local clip_size
        clip_size=$(stat -c%s "$raw_file" 2>/dev/null || stat -f%z "$raw_file" 2>/dev/null || echo 0)
        printf "\r  [%3d/%3d] 0x%s: %-30s (%d Bytes, %.2fs)" \
            "$count" "$total" "$id_str" "$text" "$clip_size" "$(echo "scale=2; $clip_size / 8000" | bc 2>/dev/null || echo '?')"
    done

    echo ""
    ok "Alle Voice-Prompts generiert."
}

# ---------------------------------------------------------------------------
# Pack voice data into .vpk binary
# ---------------------------------------------------------------------------
pack_voice_data() {
    local raw_dir="$1"
    local output_file="$2"
    local lang_code="$3"

    local py_cmd
    py_cmd=$(command -v python3 2>/dev/null || command -v python)

    info "Packe Voice-Daten in ${output_file} ..."

    $py_cmd - "$raw_dir" "$output_file" "$lang_code" << 'PYTHON_SCRIPT'
import sys
import os
import struct
import glob

raw_dir = sys.argv[1]
output_file = sys.argv[2]
lang_code = sys.argv[3]

# SPI Flash layout for voice data:
# Index table: 0x14c000 (Chinese) or 0x14c800 (English)
# Each entry: uint32_t offset_from_data_base, uint32_t size
# Data base: 0x14d000
#
# The .vpk file contains:
#   Header (16 bytes):
#     Magic: "VPK1" (4 bytes)
#     Version: uint16_t (2 bytes)
#     Language: uint16_t (0=Chinese, 1=English, 2=German) (2 bytes)
#     Num entries: uint16_t (2 bytes)
#     Max voice ID: uint16_t (2 bytes)
#     Reserved: 4 bytes
#   Index table (max_id * 8 bytes):
#     For each voice ID: uint32_t offset, uint32_t size
#   Voice data (concatenated raw PCM)

MAX_VOICE_ID = 0x82  # Maximum voice ID (inclusive)

# PY25Q16 flash layout constraints
# Index table: 0x14c000 or 0x14c800 (2 KB each, up to 256 entries × 8 bytes)
# Voice data:  0x14d000 – 0x200000 = 733,184 bytes (716 KB)
FLASH_INDEX_SIZE = 0x1000          # 4 KB for index tables (Chinese + English)
FLASH_DATA_START = 0x14d000
FLASH_DATA_END   = 0x200000
MAX_VOICE_DATA   = FLASH_DATA_END - FLASH_DATA_START  # 733,184 bytes

# Collect all raw files
voice_data = {}
for raw_file in sorted(glob.glob(os.path.join(raw_dir, "voice_*.raw"))):
    basename = os.path.basename(raw_file)
    # Extract hex ID from filename: voice_XX.raw
    id_hex = basename.replace("voice_", "").replace(".raw", "")
    voice_id = int(id_hex, 16)
    with open(raw_file, "rb") as f:
        data = f.read()
    if len(data) > 0:
        voice_data[voice_id] = data

print(f"  {len(voice_data)} Voice-Clips geladen")

# Show per-clip statistics
clip_sizes = [(vid, len(d)) for vid, d in sorted(voice_data.items())]
if clip_sizes:
    sizes_only = [s for _, s in clip_sizes]
    print(f"  Clip-Groessen: min={min(sizes_only)} B, max={max(sizes_only)} B, "
          f"avg={sum(sizes_only)//len(sizes_only)} B")
    print(f"  Clip-Dauer:    min={min(sizes_only)/8000:.2f}s, max={max(sizes_only)/8000:.2f}s, "
          f"avg={sum(sizes_only)/len(sizes_only)/8000:.2f}s")

# Build index table and concatenated data
index_entries = []
data_blob = bytearray()
current_offset = 0

for vid in range(MAX_VOICE_ID + 1):
    if vid in voice_data:
        clip = voice_data[vid]
        index_entries.append((current_offset, len(clip)))
        data_blob.extend(clip)
        current_offset += len(clip)
    else:
        # Empty entry (offset=0xFFFFFFFF, size=0)
        index_entries.append((0xFFFFFFFF, 0))

# Language code mapping
lang_map = {"zh": 0, "cn": 0, "de": 2, "en": 1}
lang_num = lang_map.get(lang_code, 1)

# Build .vpk file
header = struct.pack("<4sHHHH4s",
    b"VPK1",           # Magic
    1,                 # Version
    lang_num,          # Language
    len(voice_data),   # Number of actual entries
    MAX_VOICE_ID,      # Max voice ID
    b"\x00" * 4        # Reserved
)

index_data = bytearray()
for offset, size in index_entries:
    index_data.extend(struct.pack("<II", offset, size))

with open(output_file, "wb") as f:
    f.write(header)
    f.write(index_data)
    f.write(data_blob)

total_size = len(header) + len(index_data) + len(data_blob)
print(f"  Gesamtgroesse: {total_size} Bytes ({total_size/1024:.1f} KB)")
print(f"  Voice-Daten:   {len(data_blob)} Bytes ({len(data_blob)/1024:.1f} KB)")
print(f"  Index-Tabelle: {len(index_data)} Bytes")
print(f"  Index-Eintraege: {MAX_VOICE_ID + 1}")
print(f"  Dauer geschaetzt: {len(data_blob)/8000:.1f} Sekunden")
print(f"  Flash-Kapazitaet: {MAX_VOICE_DATA} Bytes ({MAX_VOICE_DATA/1024:.1f} KB)")

if len(data_blob) > MAX_VOICE_DATA:
    excess = len(data_blob) - MAX_VOICE_DATA
    print(f"\n  *** WARNUNG: Voice-Daten ueberschreiten Flash-Kapazitaet um "
          f"{excess} Bytes ({excess/1024:.1f} KB)! ***")
    print(f"  *** Maximal {MAX_VOICE_DATA} Bytes ({MAX_VOICE_DATA/1024:.1f} KB) "
          f"passen in den Flash-Bereich 0x{FLASH_DATA_START:06X}-0x{FLASH_DATA_END:06X}. ***")
    sys.exit(1)
else:
    used_pct = len(data_blob) * 100 / MAX_VOICE_DATA
    remaining = MAX_VOICE_DATA - len(data_blob)
    print(f"  Flash-Nutzung:  {used_pct:.1f}% ({remaining/1024:.1f} KB frei)")

PYTHON_SCRIPT

    ok "Voice-Pack erstellt: $output_file"
}

# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------
main() {
    echo ""
    echo "${BOLD}============================================${RESET}"
    echo "${BOLD}  Voice Prompt Builder${RESET}"
    echo "${BOLD}  fuer Quansheng UV-K5 V3/K1${RESET}"
    echo "${BOLD}============================================${RESET}"
    echo ""

    info "Sprache: $LANG_CODE"
    info "Ausgabe: $OUTPUT_FILE"
    echo ""

    # Check dependencies
    check_dependencies
    echo ""

    # Create temp directory
    TEMP_DIR=$(mktemp -d)
    info "Temporaeres Verzeichnis: $TEMP_DIR"

    # Create output directory
    mkdir -p "$OUTPUT_DIR"

    # Generate voice prompts
    generate_voice_prompts "$LANG_CODE" "$TEMP_DIR"
    echo ""

    # Pack into .vpk
    pack_voice_data "$TEMP_DIR/raw" "$OUTPUT_FILE" "$LANG_CODE"
    echo ""

    ok "=== Voice Prompt Build abgeschlossen ==="
    echo ""
    info "Naechster Schritt: Voice-Pack auf das Geraet flashen mit:"
    info "  ./msys2_voice_flash.sh $OUTPUT_FILE"
    echo ""
}

main "$@"
