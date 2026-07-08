RICHARD ARDF FIRMWARE - FLASHING INSTRUCTIONS
=============================================

IMPORTANT
---------
These two files were built for different hardware versions.
Never flash the V1 file onto a V3 radio or the V3 file onto a V1 radio.

V1 / DP32G030 processor:
  Richard-ARDF-V1-DP32G030.packed.bin

V3 or K1 / PY32F071 processor:
  Richard-ARDF-V3-PY32F071.bin

The V3 file was built for the same streamlined ARDF use as the V1 version:
ARDF operation, simplified menu, Morse feedback, and transmission lockout.


PREPARATION
-----------
1. Fully charge the battery.
2. Have a compatible Quansheng/Kenwood/Retevis USB programming cable Ready with CH340-driver.
3. Use Google Chrome or Microsoft Edge.
4. Confirm the radio's hardware version beyond doubt.
5. If possible, back up the channels, settings, and calibration data first.


FLASHING V1
-----------
1. Open https://egzumer.github.io/uvtools/ in your browser.
2. Switch the radio off.
3. Push both programming-cable plugs fully into the radio sockets.
4. Connect the cable to the computer.
5. Hold down the PTT button while switching the radio on.
6. Release PTT. The display must remain dark; the indicator light stays on.
7. Click Connect in the browser and select the newly detected COM port.
8. Select the firmware upload/flashing function.
9. Choose Richard-ARDF-V1-DP32G030.packed.bin.
10. Start flashing. Do not touch the radio or cable until success is reported.
11. Switch the radio off, disconnect the cable, and switch it on normally.


FLASHING V3
-----------
1. Open https://armel.github.io/uvtools2/ in your browser.
2. Switch the radio off.
3. Push both programming-cable plugs fully into the radio sockets.
4. Connect the cable to the computer.
5. Hold down the PTT button while switching the radio on.
6. Release PTT. The display must remain dark.
7. Click Connect in the browser and select the newly detected COM port.
8. Select the firmware writing/flashing function.
9. Choose Richard-ARDF-V3-PY32F071.bin.
10. Start flashing. Do not touch the radio or cable until success is reported.
11. Switch the radio off, disconnect the cable, and switch it on normally.


IF CONNECT DOES NOT WORK
------------------------
- Use Chrome or Edge, not Firefox.
- Check Windows Device Manager. Connecting the cable should add a COM port.
- Push both plugs firmly into the radio until they are fully seated.
- Switch the radio off and try again while holding PTT during power-on.
- If Windows does not show a COM port, the driver for the cable's CH340 or
  CP2102 chip is probably missing.


CHECKSUMS (SHA-256)
-------------------
V1: E74B3DE04E0572BEA3DB64EFED66A66998D4B68BBD39A2966714C1B74AEA32F1
V3: 988E46E78073D3B4A698BE67BC7405E0C8C5D788FBEE4232FC30EBBCC183F6A1

