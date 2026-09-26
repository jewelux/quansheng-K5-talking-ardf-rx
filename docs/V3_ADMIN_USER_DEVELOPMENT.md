# V3 Admin/User mode development

Status: requirements and implementation guide only; firmware behavior is unchanged.

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
