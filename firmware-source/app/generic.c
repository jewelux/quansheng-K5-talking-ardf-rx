/* Copyright 2023 Dual Tachyon
 * https://github.com/DualTachyon
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 *     Unless required by applicable law or agreed to in writing, software
 *     distributed under the License is distributed on an "AS IS" BASIS,
 *     WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *     See the License for the specific language governing permissions and
 *     limitations under the License.
 */

#include <string.h>

#include "app/app.h"
#include "app/chFrScanner.h"
#include "app/common.h"

#ifdef ENABLE_FMRADIO
	#include "app/fm.h"
#endif

#include "app/generic.h"
#include "app/menu.h"
#include "app/scanner.h"
#include "audio.h"
#include "driver/keyboard.h"
#include "dtmf.h"
#include "external/printf/printf.h"
#include "functions.h"
#include "misc.h"
#include "settings.h"
#include "ui/inputbox.h"
#include "ui/ui.h"

void GENERIC_Key_F(bool bKeyPressed, bool bKeyHeld)
{
	if (gInputBoxIndex > 0) {
		if (!bKeyHeld && bKeyPressed) // short pressed
			gBeepToPlay = BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL;
		return;
	}

	if (bKeyHeld || !bKeyPressed) { // held or released
		if (bKeyHeld || bKeyPressed) { // held or pressed (cannot be held and not pressed I guess, so it checks only if HELD?)
			if (!bKeyHeld) // won't ever pass
				return;

			if (!bKeyPressed) // won't ever pass
				return;

			COMMON_KeypadLockToggle();
		}
		else { // released

			if (
#ifdef ENABLE_FMRADIO
				(gFmRadioMode && gScreenToDisplay != DISPLAY_FM) &&
#endif
#if ENABLE_ARDF
				(gScreenToDisplay != DISPLAY_ARDF) &&
#endif
				(gScreenToDisplay != DISPLAY_MAIN)
			   )
				return;

			gWasFKeyPressed = !gWasFKeyPressed; // toggle F function

			if (gWasFKeyPressed)
				gKeyInputCountdown = key_input_timeout_500ms;

#ifdef ENABLE_VOICE
			gAnotherVoiceID = gWasFKeyPressed ? VOICE_ID_FUNCTION : VOICE_ID_CANCEL;
#endif
			gUpdateStatus = true;
		}
	}
	else { // short pressed
#ifdef ENABLE_FMRADIO
		if (gScreenToDisplay != DISPLAY_FM)
#endif
		{
			gBeepToPlay = BEEP_1KHZ_60MS_OPTIONAL;
			return;
		}

#ifdef ENABLE_FMRADIO
		if (gFM_ScanState == FM_SCAN_OFF) { // not scanning
			gBeepToPlay = BEEP_1KHZ_60MS_OPTIONAL;
			return;
		}
#endif
		gBeepToPlay     = BEEP_440HZ_500MS;
		gPttWasReleased = true;
	}
}

void GENERIC_Key_PTT(bool bKeyPressed)
{
	// TX is disabled in RX-only firmware
	if (bKeyPressed && !gPttIsPressed) {
		AUDIO_PlayBeep(BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL);
	}
	gPttIsPressed = bKeyPressed;
}
