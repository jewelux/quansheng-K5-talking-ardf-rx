# Quansheng K5 Talking ARDF RX

Accessible receive-only fox hunting firmware for Quansheng white sticker radios.

## Overview

This repository is for a blind-friendly "talking peiler" direction:

- receive-only ARDF operation
- simpler controls for field use
- spoken feedback
- Morse fallback where speech clips are missing
- ongoing work toward blind-accessible spectrum support

The GitHub root is intentionally kept clean for humans. The actual firmware code now lives in [firmware-source](./firmware-source).

## Repository Layout

- [README.md](./README.md): project overview and operator-facing documentation
- [CHANGELOG.md](./CHANGELOG.md): variant and release history
- [LICENSE](./LICENSE): project license
- [firmware-source](./firmware-source): full firmware tree, build scripts, and source code

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
- `STAR (*)`: currently unused (available for future functions)

### ARDF Simple Mode

- `PTT short`: snapshot / field-strength beeper (1-9 beeps)
- `UP/DOWN`: manual gain change
- `F + UP/DOWN`: frequency step or memory channel change
- `EXIT short`: active fox and remaining time
- `EXIT long`: fox frequency and modulation
- `MENU long`: ARDF-specific acoustic help
- `SIDE1 short/long`: configurable via menu (e.g. ARDF ON/OFF, gain middle, snapshot speed)
- `SIDE2 short/long`: configurable via menu (e.g. ARDF ON/OFF, gain middle, snapshot speed)

### Menu Use

- `UP/DOWN`: navigate between menu items
- `SIDE1/SIDE2`: navigate UP/DOWN (one-handed operation, accessibility)
- `MENU`: enter submenu / confirm selection
- `PTT`: confirm selection (same as MENU, one-handed operation)
- `EXIT`: leave submenu / close menu
- menu items with spoken names use voice clips where available
- menu items without spoken names use Morse output
- Morse speed can be adjusted from `15` to `70 wpm` in `5 wpm` steps
- navigating during Morse output automatically announces the newly focused item

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
cd /d C:\Users\User\Documents\__CodexFiles\GitHub\quansheng-talking-ardf-rx\firmware-source
win_make.bat
```

If Python dependencies for packed firmware are missing:

```bat
py -m pip install crcmod
cd /d C:\Users\User\Documents\__CodexFiles\GitHub\quansheng-talking-ardf-rx\firmware-source
win_make.bat
```

## Flashing

This repository does not duplicate external flashing guides. Please use the original tools directly.

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
