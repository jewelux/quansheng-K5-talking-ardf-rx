# Quansheng K5 Talking ARDF RX

Accessible receive-only fox hunting firmware for Quansheng white sticker radios.

## Overview

This repository is for a blind-friendly "talking peiler" direction:

- receive-only ARDF operation
- simpler controls for field use
- spoken feedback
- Morse fallback where speech clips are missing
- ongoing work toward blind-accessible spectrum support

The GitHub root is intentionally kept clean for humans. The actual firmware code now lives in [firmware-v1](./firmware-v1).

## Repository Layout

- [README.md](./README.md): project overview and operator-facing documentation
- [CHANGELOG.md](./CHANGELOG.md): variant and release history
- [LICENSE](./LICENSE): project license
- [firmware-v1](./firmware-v1): full firmware tree, build scripts, and source code

## Firmware Variants

Two closely related variants are documented in parallel.

| Variant | Focus | Best for | Main difference |
| --- | --- | --- | --- |
| `V1` | Talking ARDF receiver | blind-first fox hunting and simple field use | reduced controls, speech and Morse, no spectrum helper |
| `V2_0` | Talking ARDF receiver plus spectrum tools | users who want the same talking core plus classic spectrum support | adds display spectrum and first audio-spectrum guidance |

## Firmware Availability

Current GitHub status:

| Variant | Documentation | Firmware asset |
| --- | --- | --- |
| `V1` | documented | baseline talking ARDF line |
| `V2_0` | documented | may still be pending as a GitHub release asset |

Why `V2_0` may not yet appear as a downloadable firmware on GitHub:

- documentation can be updated before a release asset is uploaded
- local builds and field testing can exist before public release packaging
- GitHub only lists binaries when they are uploaded as release assets or workflow artifacts

## Shared Core Controls

### Main Screen

- `0-9`: direct channel or frequency entry
- `UP/DOWN`: next channel or frequency
- `MENU short`: open the menu
- `MENU long`: play a short key-help prompt
- `EXIT short`: speak a compact status report
- `EXIT long`: speak the current operating context
- `F`: enable the function layer
- `F + 2`: switch A/B
- `F + 3`: switch VFO/MR
- `STAR (*)`: play battery percentage as Morse code (works with keypad lock on)

### ARDF Simple Mode

- `PTT short`: snapshot / field-strength beeper (1-9 beeps)
- `PTT hold`: compass mode — continuous RSSI-to-tone for direction finding (release to stop)
- `UP/DOWN`: manual gain change
- `F + UP/DOWN`: frequency step or memory channel change
- `EXIT short`: active fox and remaining time
- `EXIT long`: fox frequency and modulation
- `MENU long`: ARDF-specific acoustic help
- `SIDE1 short/long`: configurable via menu (e.g. ARDF ON/OFF, gain middle, snapshot speed)
- `SIDE2 short/long`: configurable via menu (e.g. ARDF ON/OFF, gain middle, snapshot speed)
- `STAR (*)`: play battery percentage as Morse code

### Menu Use

- `UP/DOWN`: navigate between menu items
- `SIDE1/SIDE2`: navigate UP/DOWN (one-handed operation, accessibility — works in submenus too)
- `MENU`: enter submenu / confirm selection
- `PTT`: confirm selection (same as MENU, one-handed operation)
- `EXIT`: leave submenu / close menu
- menu items with spoken names use voice clips where available
- menu items without spoken names use Morse output (works even when voice prompt is OFF)
- Morse speed can be adjusted from `15` to `70 wpm` in `5 wpm` steps
- navigating during Morse output automatically announces the newly focused item
- Voice prompts and Morse output are independent settings:
  - Voice prompt OFF: voice clips are muted but Morse continues to work
  - Morse speed adjustable independently via menu

### Side Keys (configurable via menu)

Available actions for SIDE1/SIDE2 short and long press:

- None, Flashlight, Monitor, Scan, VOX, A/B switch, VFO/MR switch
- Demodulation switch, Spectrum, Keylock
- ARDF ON/OFF, ARDF Gain Middle, ARDF Snapshot Speed UP/DOWN

## What Is Different In V2_0

- classic display spectrum finder
- first blind-friendly audio spectrum finder concept
- same talking ARDF base as `V1`
- intended as an extension, not a replacement for the simpler `V1` workflow

Spectrum controls currently documented for `V2_0`:

- `F + 5`: classic display spectrum finder
- `hold 5` or `hold F + 5`: audio spectrum finder

## Building

On Windows:

```bat
cd /d C:\Users\User\Documents\__CodexFiles\GitHub\quansheng-talking-ardf-rx\firmware-v1
win_make.bat
```

If Python dependencies for packed firmware are missing:

```bat
py -m pip install crcmod
cd /d C:\Users\User\Documents\__CodexFiles\GitHub\quansheng-talking-ardf-rx\firmware-v1
win_make.bat
```

## Flashing

**Important:** This firmware is built for V1 hardware (DP32G030 MCU) only.
V3/K1 radios (PY32F071 MCU) require different firmware and different tools.
Check the version label under the battery compartment before flashing.

### MSYS2 Flash Tool (included)

The repository includes a Python flash script in `firmware-v1/`:

```
python3 k5flash.py              (interactive mode with device version check)
python3 k5flash.py COM3 firmware.packed.bin   (command line mode)
```

The interactive mode asks for the device version and warns about incompatible hardware.

### External Tools

Recommended starting points:

- hardware version 1 web flasher: [egzumer uvtools](https://egzumer.github.io/uvtools/)
- Linux flashing tool: [nica-f/k5prog](https://github.com/nica-f/k5prog)
- Windows flashing tool: [OneOfEleven/k5prog-win](https://github.com/OneOfEleven/k5prog-win)
- hardware version 3 / K1 reference flasher: [armel uvtools2](https://armel.github.io/uvtools2/)

Always verify your hardware version before flashing.

## Third-Party Notices

This project is based on and informed by open-source alternative firmware work for Quansheng handheld radios.

Referenced upstream projects:

- Dennis DL9CAT
  [reald/uv-k5-firmware-custom](https://github.com/reald/uv-k5-firmware-custom)
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
- armel
  [armel/uv-k1-k5v3-firmware-custom](https://github.com/armel/uv-k1-k5v3-firmware-custom) - Apache-2.0
- Tunas1337
  [Tunas1337/UV-K5-Modded-Firmwares](https://github.com/Tunas1337/UV-K5-Modded-Firmwares) - BSD-2-Clause

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
