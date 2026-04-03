# Quansheng-K5-Talking-ARDF-RX

Accessible receive-only Fox Hunting firmware for Quansheng K5.

## Overview

This project is being developed as a simple talking direction-finding receiver for blind and visually impaired radio amateurs. The aim is to turn inexpensive Quansheng handhelds into a practical 2 m / 70 cm ARDF receiver with strong acoustic guidance, reduced complexity, and easier operation in the field.

The firmware is based on existing open-source alternative firmware work for Quansheng handheld radios and has been adapted and simplified for a receive-only, accessible ARDF use case.

See `THIRD_PARTY_NOTICES.md` for upstream references and license details.

## Target Hardware

- Quansheng radios
- Hardware version 1 devices supported by this firmware branch

## Project Goals

- RX-only firmware for safer ARDF use
- simplified controls for field use
- spoken and acoustic feedback for blind operation
- reduced menus and reduced non-essential features
- a practical base for further accessibility work

## Current Direction

This repository is focused on a talking peiler concept:

- ARDF-oriented operating flow
- compact menu set
- voice and Morse feedback
- direct acoustic status queries
- ongoing simplification for blind-friendly use

## Quick Start for Blind Operators

The most important controls are:

- `UP/DOWN`: change channel or frequency
- `0-9`: enter a channel or frequency directly
- `MENU short`: open or confirm menu items
- `MENU long`: hear the short key-help prompt
- `EXIT short`: hear a compact spoken status
- `EXIT long`: hear the current channel or frequency

In ARDF simple mode:

- `PTT short`: field-strength snapshot
- `UP/DOWN`: change gain
- `EXIT short`: hear active fox and remaining time
- `EXIT long`: hear fox frequency and modulation

If you are unsure where you are, press `EXIT long`.
If you are unsure what the main keys do, press `MENU long`.

## Basic Operation

This section describes the current operating concept of the talking ARDF build.

### Main Screen

- `0-9`: direct channel or frequency entry
- `UP/DOWN`: next channel or frequency
- `MENU short`: open the menu
- `MENU long`: play a short key-help prompt
- `EXIT short`: speak a compact status report
- `EXIT long`: speak the current channel or frequency
- `F`: enable the function layer
- `F + 2`: switch A/B
- `F + 3`: switch VFO/MR
- `F + 4`: switch modulation between FM and AM

### Spoken Feedback

- Number keys speak the entered digits
- Completed frequency entry speaks the resulting frequency
- Completed channel entry speaks the selected memory channel
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
- Menu items with spoken names use voice clips where available
- Menu items without spoken names currently use Morse output

### Morse Speed

The menu contains a `Morse speed` setting.

- Range: `15` to `70 wpm`
- Step size: `5 wpm`
- Default: `20 wpm`

### Safety and Simplification

This project is intentionally reduced for receive-only ARDF use.

- Transmit should remain disabled
- Non-essential functions have been removed or reduced
- The goal is faster, safer, and more accessible field operation

## Building

On Windows:

```bat
cd /d C:\Users\User\Documents\...........\quansheng-talking-ardf-rx
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

- Hardware version 1 web flasher: [egzumer uvtools](https://egzumer.github.io/uvtools/)
- Linux flashing tool: [nica-f/k5prog](https://github.com/nica-f/k5prog)
- Windows flashing tool: [OneOfEleven/k5prog-win](https://github.com/OneOfEleven/k5prog-win)
- Hardware version 3 / K1 web flasher reference: [armel uvtools2](https://armel.github.io/uvtools2/)

Always verify your hardware version before flashing.

## Licensing

This repository is published under the Apache License 2.0.

Please keep original copyright and license notices in files derived from upstream sources.
