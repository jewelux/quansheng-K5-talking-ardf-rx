# quansheng-talking-ardf-rx

Accessible receive-only ARDF firmware for Quansheng white sticker radios.

## Overview

This project is being developed as a simple talking direction-finding receiver for blind and visually impaired radio amateurs. The aim is to turn inexpensive Quansheng handhelds into a practical 2 m / 70 cm ARDF receiver with strong acoustic guidance, reduced complexity, and easier operation in the field.

The firmware is based on existing open-source alternative firmware work for Quansheng handheld radios and has been adapted and simplified for a receive-only, accessible ARDF use case.

See `THIRD_PARTY_NOTICES.md` for upstream references and license details.

## Target Hardware

- Quansheng white sticker radios
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

- Hardware version 1 web flasher: [egzumer uvtools](https://egzumer.github.io/uvtools/)
- Linux flashing tool: [nica-f/k5prog](https://github.com/nica-f/k5prog)
- Windows flashing tool: [OneOfEleven/k5prog-win](https://github.com/OneOfEleven/k5prog-win)
- Hardware version 3 / K1 web flasher reference: [armel uvtools2](https://armel.github.io/uvtools2/)

Always verify your hardware version before flashing.

## Licensing

This repository is published under the Apache License 2.0.

Please keep original copyright and license notices in files derived from upstream sources.
