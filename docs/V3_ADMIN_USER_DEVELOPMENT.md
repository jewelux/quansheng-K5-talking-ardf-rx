# V3 Admin/User mode development

Status: step 1 implemented in the `ARDF-AdminUser-Dev` build preset only.
Normal startup selects User; holding MENU alone selects Admin. The startup screen
shows the selected role and SAM announces it. Existing presets are unchanged.

**This is a startup-selection test, not a child-ready firmware:** User controls
are not restricted yet. Transmission remains disabled in both roles.

### Build and try step 1

From `firmware-v3`, with the ARM compiler, CMake, and Ninja available:

```sh
cmake --preset ARDF-AdminUser-Dev
cmake --build --preset ARDF-AdminUser-Dev
```

Output: `build/ARDF-AdminUser-Dev/ardf.AdminUser_Dev_K5v3_K1.bin`.
Use only on the supported V3/K1 hardware, following the repository flashing guide.

Hardware checks (not yet performed):

1. Power on without a key: the screen says USER MODE, followed by spoken User mode.
2. Power off. Hold MENU and switch on: release MENU at the release-keys prompt.
   The screen says ADMIN MODE, followed by spoken Admin mode.
3. Power off and restart normally: User must return without changing any setting.
4. Hold MENU for several seconds: startup must wait for release, and the held key
   must not become a normal menu action afterward.
5. Hold an unrelated key or MENU with PTT: it must not grant Admin. Existing
   special PTT startup combinations remain unchanged at this stage.
6. Confirm the indicator appears even with the normal welcome display disabled.

The role is held in RAM only. MENU is sampled three times, 20 ms apart. The
development feature refuses to compile without `ENABLE_PREVENT_TX`.

Build validation: both `ARDF-AdminUser-Dev` and `ARDF-SAM-Access` compiled and
linked successfully with Arm GNU 15.2.1, CMake 4.4.3, and Ninja 1.13.2 on Windows.
The development build uses 91,636 bytes of flash and 14,176 bytes of RAM; the
baseline uses 91,880 bytes of flash and 14,176 bytes of RAM. The simpler startup
screen accounts for the smaller development binary. A missing `<stdlib.h>`
include in the existing menu code was added for compatibility with this compiler.
These are compilation checks, not confirmation of behavior on a physical radio.

Target: Quansheng UV-K5v3 / UV-K1 (PY32F071). Development branch:
`codex/v3-admin-user-mode`, based on `d555b2f` from `origin/master`.

## Required behavior

| Function | User mode | Admin mode |
| --- | --- | --- |
| Entry | Normal power-on | Hold MENU during power-on |
| Sensitivity and volume | Adjustable | Adjustable |
| Menu access | Blocked | Allowed |
| Frequency and channel changes | Blocked | Allowed |
| Transmission | Always disabled | Configurable, default OFF |

Admin mode is selected only at startup and must not persist across power cycles.
A normal restart always returns to User mode, regardless of the previous Admin
TX setting. MENU is a convenience gate, not authentication: anyone who knows
the startup combination can enter Admin mode.

Retain existing speech/Morse accessibility feedback. Clearly identify the active
mode on screen and through the supported audio output. The analog volume knob
continues to work normally. User sensitivity changes must use the ARDF gain
control, without falling through to normal frequency tuning.

## Existing implementation points

- `firmware-v3/App/helper/boot.c`: `BOOT_GetMode()` currently checks PTT-based
  startup combinations. Detect MENU independently of PTT, debounce it, and consume
  the startup key until release so it cannot trigger a menu action afterward.
- `firmware-v3/App/app/app.c`: central key dispatch contains keypad-lock exceptions
  for side keys, held MENU, and ARDF UP/DOWN. An ordinary keypad lock is therefore
  insufficient. Filter User actions before shortcuts and configurable actions run.
- `firmware-v3/App/app/ardf.c`: reuse the existing sensitivity adjustment path.
- `firmware-v3/App/radio.c`: TX preparation rejects transmission when
  `ENABLE_PREVENT_TX` is enabled, and also rejects TX while ARDF is enabled.
- `firmware-v3/App/frequencies.c`: `TX_freq_check()` independently rejects TX for
  `ENABLE_PREVENT_TX` and active ARDF.
- `firmware-v3/CMakePresets.json`: the ARDF preset enables `ENABLE_PREVENT_TX`.
  Add a separate development preset; keep existing receive-only presets intact.

## Implementation sequence

1. Add a mode state that defaults to User before any radio initialization; sample
   MENU during startup and select Admin only after a stable detection.
2. Ensure User startup selects the intended ARDF receive screen and saved fox
   frequency, rather than restoring a menu, scanner, or other interactive mode.
3. Allow only sensitivity adjustments in User key dispatch; block menu entry,
   digits, channel/frequency changes, scanning, configurable side-key shortcuts,
   and long-press alternatives. Keep volume available through the hardware knob.
4. Add an Admin-only TX setting, initially OFF. Route TX authorization through a
   common policy: User always denies; Admin requires explicit enablement and all
   existing frequency/hardware restrictions. Audit every TX entry point, including
   PTT, VOX, alarms, tones, and data modes. Never merely bypass the PTT handler.
5. Preserve the existing ARDF TX prohibition. Admin enabling TX can apply outside
   active ARDF; allowing transmission during ARDF would require a separate decision.
6. Add mode announcements and display indication using the selected accessibility
   build. Decide and document persistence of the Admin TX preference before coding
   its EEPROM representation; normal startup must remain receive-only either way.

## Validation before supplying firmware

- Build the new V3 preset and the unchanged V3 baseline; check flash/RAM usage.
- Normal boot, MENU boot, key bounce, MENU release, and subsequent normal reboot.
- User mode: test all short/long key presses and combinations; only sensitivity
  and volume may change. Verify the saved frequency and channel remain unchanged.
- Test startup from every supported saved screen/mode and accessibility setting.
- Verify Admin menu/frequency access, TX default OFF, and explicit TX enable/disable.
- Confirm User denies TX even after a preceding Admin session enabled it.
- Exercise PTT, VOX, alarm, tone, and other compiled TX paths; verify denial at the
  radio authorization layer as well as the UI layer.
- On hardware, use appropriate RF measurement equipment and a dummy load to check
  that prohibited TX requests produce no transmission. Source review and a passing
  build alone cannot establish hardware RF behavior.

## Windows checkout

The upstream repository contains `firmware-source/nul`, a Windows-reserved name.
This checkout excludes it using sparse checkout. The tracked file is retained in
Git and must not be accidentally committed as a deletion.
