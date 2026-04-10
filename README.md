# quansheng-talking-ardf-rx

Accessible receive-only Fox Hunting firmware for Quansheng white sticker radios.

## Overview

This project turns inexpensive Quansheng handhelds into a practical 2 m / 70 cm ARDF receiver with strong acoustic guidance, reduced complexity, and easier operation for blind and visually impaired radio amateurs.

The firmware is based on existing open-source Quansheng alternative firmware work and is adapted here into a receive-only, accessibility-focused ARDF build.

See [THIRD_PARTY_NOTICES.md](./THIRD_PARTY_NOTICES.md) for upstream references and license details.
See [CHANGELOG.md](./CHANGELOG.md) for variant-focused change history.

## Target Hardware

- Quansheng white sticker radios
- hardware version 1 devices supported by this branch

## Project Direction

This repository focuses on a "talking peiler" concept:

- RX-only firmware for safer ARDF use
- simplified operating flow for field use
- spoken and Morse feedback
- compact menu layout
- direct acoustic status queries
- ongoing work toward blind-friendly spectrum finding

## Firmware Variants

Two closely related variants are currently being documented in parallel.

| Variant | Focus | Best for | Main difference |
| --- | --- | --- | --- |
| `V1` | Talking ARDF receiver | blind-first fox hunting and simple field operation | reduced controls, speech and Morse, no spectrum helper |
| `V2_0` | Talking ARDF receiver plus spectrum tools | users who want the same talking core plus classic display spectrum and first audio spectrum cues | adds `F + 5` display spectrum and a first audio spectrum finder |

## Firmware Availability

Current GitHub status:

| Variant | Documentation status | Firmware asset status |
| --- | --- | --- |
| `V1` | documented | baseline talking ARDF line |
| `V2_0` | documented | not yet published here as a GitHub release asset |

Why `V2_0` may not yet appear as a downloadable firmware on GitHub:

- the variant can be documented before a release asset is uploaded
- local builds and field testing may exist before a public GitHub release is prepared
- GitHub only lists firmware binaries when they are added as release assets or workflow artifacts

## At A Glance

### `V1`

This is the simpler blind-first talking ARDF build.

- spoken status and orientation prompts
- Morse fallback for menu items without voice clips
- simplified main screen controls
- simplified ARDF workflow
- receive-only safety focus

### `V2_0`

This keeps the same talking ARDF base and adds spectrum support.

- classic display spectrum finder
- first blind-friendly audio spectrum finder
- same voice and Morse foundation as `V1`
- intended as the next experimental step, not a replacement for the simpler `V1` workflow

## Shared Core Controls

These controls are common to both variants unless stated otherwise.

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

### Spoken Feedback

- number keys speak the entered digits
- completed frequency entry speaks the resulting frequency
- completed channel entry speaks the selected memory channel
- `UP/DOWN` speaks the new channel or frequency
- `EXIT short` gives a compact spoken status
- `EXIT long` answers the question "where am I now?"
- `MENU long` gives a short acoustic help overview

### ARDF Simple Mode

In ARDF simple mode, the radio is reduced to the functions needed for practical direction finding.

- `PTT short`: snapshot / field-strength beeper
- `UP/DOWN`: manual gain change
- `EXIT short`: active fox and remaining time
- `EXIT long`: fox frequency and modulation
- `MENU long`: ARDF-specific acoustic help

### Menu Use

- `MENU short`: open the menu or confirm a menu item
- `UP/DOWN`: move through menu items or change values
- `EXIT`: leave the current menu level
- menu items with spoken names use voice clips where available
- menu items without spoken names currently use Morse output

### Morse Speed

The menu contains a `Morse speed` setting.

- range: `15` to `70 wpm`
- step size: `5 wpm`
- default: `20 wpm`

## What Is Different In `V2_0`

`V2_0` adds two spectrum-oriented entry points while keeping the talking ARDF core.

### Spectrum Controls

- `F + 5`: classic display spectrum finder
- `hold 5` or `hold F + 5`: audio spectrum finder

### Audio Spectrum Finder

The first `V2_0` audio spectrum finder is intentionally simple and field-oriented.

- it uses periodic tones instead of a spoken running commentary
- tones indicate whether the strongest peak is left, centered, or right of the current center frequency
- stronger peaks repeat faster than weaker peaks
- it is meant as a first blind-friendly search aid, not yet as a finished final interface

### Why Keep Both Variants

`V1` remains the cleaner choice when the goal is the simplest possible blind-first ARDF receiver.

`V2_0` is the experimental extension for users who also want:

- visual spectrum support on the display
- an early acoustic spectrum search mode
- a base for further blind-accessible spectrum work

## Recommended Usage

Choose `V1` if you want:

- the simpler and calmer field workflow
- the least distraction
- the strongest focus on direct ARDF operation

Choose `V2_0` if you want:

- the same talking ARDF concept
- a classic display spectrum view
- a first-generation blind-friendly spectrum search helper

## Safety and Simplification

This project is intentionally reduced for receive-only ARDF use.

- transmit should remain disabled
- non-essential functions have been removed or reduced
- the goal is faster, safer, and more accessible field operation

## Building

On Windows:

```bat
cd /d C:\Users\User\Documents\__CodexFiles\GitHub\quansheng-talking-ardf-rx
win_make.bat
```

If Python dependencies for packed firmware are missing, install them and run the build again:

```bat
py -m pip install crcmod
win_make.bat
```

## Flashing

This repository does not duplicate external flashing guides. Please use the original tools and read their documentation directly.

Recommended starting points:

- hardware version 1 web flasher: [egzumer uvtools](https://egzumer.github.io/uvtools/)
- Linux flashing tool: [nica-f/k5prog](https://github.com/nica-f/k5prog)
- Windows flashing tool: [OneOfEleven/k5prog-win](https://github.com/OneOfEleven/k5prog-win)
- hardware version 3 / K1 web flasher reference: [armel uvtools2](https://armel.github.io/uvtools2/)

Always verify your hardware version before flashing.

## Roadmap

Current and near-term accessibility work:

- keep the blind-first talking ARDF workflow solid
- improve documentation so `V1` and `V2_0` can be understood side by side
- refine the `V2_0` audio spectrum finder
- later evolve the audio spectrum helper into a stronger blind-accessible spectrum analyser concept

## Licensing

This repository is published under the Apache License 2.0.

Please keep original copyright and license notices in files derived from upstream sources.
