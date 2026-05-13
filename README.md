# Quansheng K5 Talking ARDF RX

Accessible receive-only fox hunting firmware for Quansheng UV-K5 radios — now supporting **both V1 and V3** hardware generations.

## Overview

This repository is for a blind-friendly "talking peiler" direction finder:

- receive-only ARDF operation (TX removed)
- simpler controls for field use
- spoken feedback (voice prompts on V3, Morse on V1+V3)
- Morse fallback where speech clips are missing
- ongoing work toward blind-accessible spectrum support

## Supported Hardware

| Feature          | V1 (UV-K5)                        | V3 (UV-K5v3 / UV-K1)              |
|-----------------|------------------------------------|------------------------------------|
| **MCU**         | DP32G030 (ARM Cortex-M0)           | PY32F071xB (ARM Cortex-M0+)       |
| **Flash**       | 64 KB                              | 128 KB                             |
| **RAM**         | 8 KB                               | 16 KB                              |
| **Audio**       | BK4819 tone generator              | 12-bit DAC + DMA + 2 MB SPI Flash |
| **Build**       | GNU Make                           | CMake + Ninja                      |
| **Identification** | White sticker under battery      | Green PCB, PY32 chip visible       |

## Repository Layout

```
├── README.md                         # This file
├── CHANGELOG.md                      # Version history
├── V3_VOICE_PROMPT_CAPABILITIES.md   # V3 audio capabilities analysis
├── msys2_build.sh                    # Unified build script (V1 + V3)
├── msys2_flash.sh                    # Unified flash script (V1 + V3)
├── firmware-v1/                      # V1 firmware (Makefile-based)
│   ├── Makefile
│   ├── app/ardf.c                    # ARDF with accessibility features
│   ├── app/menu.c                    # Morse code menu system
│   ├── k5flash.py                    # V1 serial flash tool
│   └── ...
└── firmware-v3/                      # V3 firmware (CMake-based)
    ├── CMakeLists.txt
    ├── CMakePresets.json              # Includes ARDF, ARDF-Morse, ARDF-Voice, ARDF-SAM presets
    ├── App/app/ardf.c                # ARDF with ported accessibility features
    ├── App/app/menu.c                # Ported Morse code menu system
    └── ...
```

## Building

### Quick Start (both V1 and V3)

```bash
# From the repository root:
./msys2_build.sh
# → Interactive selection: V1 or V3
# → Dependency check and auto-install
# → Output in build-output/
```

### Manual Build — V1

```bash
cd firmware-v1
make -j$(nproc)
```

### Manual Build — V3

```bash
cd firmware-v3
cmake --preset ARDF-Voice
cmake --build --preset ARDF-Voice
```

### Toolchain Requirements

| Tool                    | V1 | V3 | Install (MSYS2)                                |
|------------------------|----|----|------------------------------------------------|
| arm-none-eabi-gcc       | ✅ | ✅ | `pacman -S mingw-w64-x86_64-arm-none-eabi-gcc` |
| arm-none-eabi-newlib    | ✅ | ✅ | `pacman -S mingw-w64-x86_64-arm-none-eabi-newlib` |
| GNU Make                | ✅ |    | `pacman -S make`                                |
| CMake                   |    | ✅ | `pacman -S mingw-w64-x86_64-cmake`              |
| Ninja                   |    | ✅ | `pacman -S mingw-w64-x86_64-ninja`              |
| Python 3 + crcmod       | ✅ |    | `pacman -S mingw-w64-x86_64-python`             |

## Flashing

### Quick Start

```bash
# From the repository root:
./msys2_flash.sh
# → Interactive selection: V1 or V3
# → V1: serial bootloader (k5flash.py)
# → V3: K5TOOL / SWD / Browser flasher
```

### V1 Flash Methods

| Method | Tool | Notes |
|--------|------|-------|
| Serial bootloader | `k5flash.py` (included) | USB cable, PTT+Power to enter flash mode |
| Web flasher | [egzumer uvtools](https://egzumer.github.io/uvtools/) | Browser-based |

### V3 Flash Methods

| Method | Tool | Notes |
|--------|------|-------|
| K5TOOL (serial) | [K5TOOL](https://github.com/qrp73/K5TOOL) | USB cable, same as V1 procedure |
| SWD programmer | pyocd / openocd | Requires SWD adapter (ST-Link etc.) |
| Browser flasher | [UV-Tools 2](https://armel.github.io/uvtools2/) | Browser-based, easiest for V3 |

⚠️ **Always verify your hardware version before flashing!** Check the label under the battery compartment.

## Credits and Project History

This project was started by Jean, LX1WJ (JeWeLux), as a blind-accessible ARDF receiver modification for Quansheng radios and is hosted in this GitHub account.

The current firmware implementation, tuning, accessibility improvements, and ongoing technical development have been largely completed and refined by Richard, DO9RE.

This project also builds on several open-source Quansheng firmware projects listed in the Third-Party Notices below. Their work, copyright notices, and licenses remain acknowledged and respected.

## Third-Party Notices

This project is based on and informed by open-source alternative firmware work for Quansheng handheld radios.

Referenced upstream projects:

- Dennis DL9CAT — V1 ARDF base:
  [reald/uv-k5-firmware-custom](https://github.com/reald/uv-k5-firmware-custom)
- Dennis DL9CAT — V3 ARDF base (firmware-v3 source):
  [reald/uv-k1-k5v3-firmware-custom](https://github.com/reald/uv-k1-k5v3-firmware-custom) - Apache-2.0
- DualTachyon
  [DualTachyon/uv-k5-firmware](https://github.com/DualTachyon/uv-k5-firmware) - Apache-2.0
- OneOfEleven
  [OneOfEleven/uv-k5-firmware-custom](https://github.com/OneOfEleven/uv-k5-firmware-custom) - Apache-2.0
- egzumer
  [egzumer/uv-k5-firmware-custom](https://github.com/egzumer/uv-k5-firmware-custom) - Apache-2.0
- fagci
  [fagci/uv-k5-firmware-fagci-mod](https://github.com/fagci/uv-k5-firmware-fagci-mod) - Apache-2.0
- rebezhir
  [rebezhir/openquack](https://github.com/rebezhir/openquack) - Apache-2.0
- armel — V3 F4HWN firmware base:
  [armel/uv-k1-k5v3-firmware-custom](https://github.com/armel/uv-k1-k5v3-firmware-custom) - Apache-2.0
- muzkr — V3 voice driver:
  [muzkr contributions to uv-k1-k5v3-firmware-custom](https://github.com/armel/uv-k1-k5v3-firmware-custom)
- Tunas1337
  [Tunas1337/UV-K5-Modded-Firmwares](https://github.com/Tunas1337/UV-K5-Modded-Firmwares) - BSD-2-Clause
- qrp73 — K5TOOL flash utility:
  [qrp73/K5TOOL](https://github.com/qrp73/K5TOOL)

Please retain original copyright and license notices in files derived from upstream sources.

## Licensing

This repository is published under the Apache License 2.0.

---

## Complete Menu Map

The device uses a **custom menu layout** (`ENABLE_CUSTOM_MENU_LAYOUT=1`) optimised for ARDF operation.
Press **MENU** to enter the menu, use **UP/DOWN** to navigate, press **MENU** to enter a sub-menu or confirm, and **EXIT** to go back.

### Top-Level Menu Items

| # | Display | Morse Label | Description |
|---|---------|-------------|-------------|
| 1 | Step | STEP | Frequency step size |
| 2 | AMFM | AM FM | Modulation / demodulation mode |
| 3 | ARDF | ARDF | ARDF operating mode (OFF / ARDF / DF Simple) |
| 4 | NumFox | NUMBER FOX | Number of foxes in the ARDF cycle (0–10) |
| 5 | FoxDur | FOX DURATION | Duration per fox in seconds |
| 6 | ActFox | ACTIVE FOX | Currently active fox (1–NumFox) |
| 7 | TiRst | TIME RESET | Reset the ARDF cycle timer |
| 8 | GainRe | GAIN REMEMBER | Remember gain per VFO (OFF / VFO A / VFO B / BOTH) |
| 9 | EndSig | END SIGNAL | Seconds before cycle end to play warning beep (0–30) |
| 10 | SnpSpd | SNAPSHOT SPEED | DF Simple snapshot speed (1–5) |
| 11 | BackLt | BACKLIGHT | Backlight auto-off timer |
| 12 | BLMin | BACKLIGHT MINIMUM | Backlight minimum brightness (0–9) |
| 13 | BLMax | BACKLIGHT MAXIMUM | Backlight maximum brightness (1–10) |
| 14 | Beep | BEEP | Key beep on/off |
| 15 | Voice | VOICE | Voice prompt language (OFF / CHI / ENG) |
| 16 | Morse | MORSE SPEED | Morse output speed in WPM (15–70) |
| — | Reset | RESET | Factory reset (hidden, accessible via menu number) |

### Sub-Menu Options

#### Step – Frequency Step
Available steps (kHz): 0.01, 0.05, 0.1, 0.25, 0.5, 1.0, 1.25, 2.5, 5.0, 6.25, 8.33, 10.0, 12.5, 15.0, 20.0, 25.0, 50.0

#### AMFM – Modulation Mode
| Value | Description |
|-------|-------------|
| FM | Frequency Modulation |
| AM | Amplitude Modulation |
| USB | Upper Side Band |
| BYP | Bypass demodulator |
| RAW | Raw IF output |

#### ARDF – ARDF Operating Mode
| Value | Description |
|-------|-------------|
| OFF | ARDF disabled, normal receiver operation |
| ARDF | Full ARDF mode with fox cycle timing and per-fox gain memory |
| DF Simple | Simplified direction finding – continuous signal strength display |

#### NumFox – Number of Foxes
Range: **0–10** (0 = free-running, no cycle)

#### FoxDur – Fox Duration
Duration of each fox transmission period in seconds. Shown in the menu as seconds (e.g., 60 = one minute per fox).

#### ActFox – Active Fox
Select the currently active fox. Range: **1** up to the configured number of foxes.

#### TiRst – Time Reset
Confirm to reset the ARDF cycle timer to zero.

#### GainRe – Gain Remember
| Value | Description |
|-------|-------------|
| OFF | Do not remember gain settings per fox |
| VFO A | Remember gain on VFO A only |
| VFO B | Remember gain on VFO B only |
| BOTH | Remember gain on both VFOs |

#### EndSig – Cycle End Signal
Seconds before the end of a fox cycle to play a warning beep. Range: **0–30** (0 = disabled).

#### SnpSpd – Snapshot Speed
Speed of the DF Simple snapshot display update. Range: **1–5** (1 = slowest, 5 = fastest).

#### BackLt – Backlight Timer
| Value | Description |
|-------|-------------|
| OFF | Backlight always off |
| 5 sec | 5 seconds |
| 10 sec | 10 seconds |
| 20 sec | 20 seconds |
| 1 min | 1 minute |
| 2 min | 2 minutes |
| 4 min | 4 minutes |
| ON | Backlight always on |

#### BLMin / BLMax – Backlight Brightness
- **BLMin**: Minimum brightness level (0–9)
- **BLMax**: Maximum brightness level (1–10)

#### Beep – Key Beep
OFF / ON

#### Voice – Voice Prompt
| Value | Description |
|-------|-------------|
| OFF | No voice prompts |
| CHI | Chinese voice |
| ENG | English voice |

#### Morse – Morse Speed
Morse code output speed in words per minute. Range: **15–70 WPM**.

#### Reset – Factory Reset
| Value | Description |
|-------|-------------|
| VFO | Reset VFO settings only |
| ALL | Full factory reset |

### Main Screen Key Functions (ARDF Mode)

| Key | Function |
|-----|----------|
| **UP / DOWN** | Adjust receiver gain (default) or squelch level (after SIDE1 toggle) |
| **SIDE1** (directly below PTT) | Toggle UP/DOWN between **gain adjustment** and **squelch adjustment** mode (Morse feedback: "GAIN" / "SQL") |
| **PTT** | Compass mode – continuous RSSI-to-tone output while held |
| **F + UP/DOWN** | Frequency step or memory channel change |
| **MENU** | Enter menu |
| **EXIT** (short) | Announce current status via Morse/voice |
| **EXIT** (long) | Announce current VFO frequency/channel |
| **MENU** (long, no action assigned) | Play ARDF help / status information |

### Side Key Programmable Functions

The side keys (SIDE1 / SIDE2) and M-long can be assigned to any of these actions via the F1Shrt, F1Long, F2Shrt, F2Long, and M Long menu items (accessible via menu number input):

| Action | Description |
|--------|-------------|
| NONE | No action |
| FLASHLIGHT | Toggle LED flashlight |
| POWER | Power level |
| MONITOR | Open squelch (monitor) |
| SCAN | Start/stop scan |
| VOX | Toggle VOX |
| ALARM | Alarm |
| FM RADIO | FM broadcast radio |
| 1750HZ | 1750 Hz tone burst |
| LOCK KEYPAD | Toggle key lock |
| SWITCH VFO | Switch between VFO A and B |
| VFO/MR | Toggle VFO / Memory mode |
| SWITCH DEMODUL | Cycle demodulation mode (FM→AM→USB→…) |
| BLMIN TMP OFF | Temporarily turn off backlight minimum |
| SPECTRUM | Spectrum analyser |
| ARDF off/on | Toggle ARDF mode on/off |
| ARDF Set Med.Gain | Set ARDF gain to middle position |
| SnpSpd + | Increase DF Simple snapshot speed |
| SnpSpd − | Decrease DF Simple snapshot speed |

### ARDF Gain Table

The manual gain is adjusted with UP/DOWN in 17 steps:

| Index | Gain (dB) | Notes |
|-------|-----------|-------|
| 0 | −79 | Minimum gain (maximum attenuation) |
| 1 | −74 | |
| 2 | −69 | |
| 3 | −64 | |
| 4 | **−59** | Middle gain (set via SIDE key "Set Med.Gain") |
| 5 | **−54** | DF Simple default |
| 6 | −49 | |
| 7 | **−44** | ARDF default |
| 8 | −39 | |
| 9 | −34 | |
| 10 | −29 | |
| 11 | −24 | |
| 12 | −19 | |
| 13 | −14 | |
| 14 | −9 | |
| 15 | −4 | |
| 16 | 0 | Maximum gain (minimum attenuation) |
