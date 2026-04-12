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

#if !defined(ENABLE_OVERLAY)
	#include "ARMCM0.h"
#endif
#include "app/generic.h"
#include "app/common.h"
#include "app/menu.h"
#include "app/scanner.h"
#include "audio.h"
#include "board.h"
#include "bsp/dp32g030/gpio.h"
#include "driver/backlight.h"
#include "driver/bk4819.h"
#include "driver/eeprom.h"
#include "driver/gpio.h"
#include "driver/keyboard.h"
#include "driver/system.h"
#include "frequencies.h"
#include "helper/battery.h"
#include "misc.h"
#include "settings.h"
#if defined(ENABLE_OVERLAY)
	#include "sram-overlay.h"
#endif
#include "ui/inputbox.h"
#include "ui/menu.h"
#include "ui/ui.h"

#ifdef ENABLE_ARDF
#include "app/ardf.h"
#endif

#ifndef ARRAY_SIZE
	#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))
#endif

uint8_t gUnlockAllTxConfCnt;

#ifdef ENABLE_VOICE
static const char *MENU_GetMorsePattern(char ch)
{
	switch (ch)
	{
		case 'A': return ".-";
		case 'B': return "-...";
		case 'C': return "-.-.";
		case 'D': return "-..";
		case 'E': return ".";
		case 'F': return "..-.";
		case 'G': return "--.";
		case 'H': return "....";
		case 'I': return "..";
		case 'J': return ".---";
		case 'K': return "-.-";
		case 'L': return ".-..";
		case 'M': return "--";
		case 'N': return "-.";
		case 'O': return "---";
		case 'P': return ".--.";
		case 'Q': return "--.-";
		case 'R': return ".-.";
		case 'S': return "...";
		case 'T': return "-";
		case 'U': return "..-";
		case 'V': return "...-";
		case 'W': return ".--";
		case 'X': return "-..-";
		case 'Y': return "-.--";
		case 'Z': return "--..";
		case '0': return "-----";
		case '1': return ".----";
		case '2': return "..---";
		case '3': return "...--";
		case '4': return "....-";
		case '5': return ".....";
		case '6': return "-....";
		case '7': return "--...";
		case '8': return "---..";
		case '9': return "----.";
		default:  return NULL;
	}
}

static uint16_t MENU_GetMorseUnitMs(void)
{
	const uint8_t wpm = (gMorseSpeedWpm >= 15U && gMorseSpeedWpm <= 70U) ? gMorseSpeedWpm : 20U;
	uint16_t unit = 1200U / wpm;

	if (unit < 40U)
		unit = 40U;

	return unit;
}

static void MENU_StopVoicePlayback(void)
{
	gVoiceWriteIndex         = 0;
	gVoiceReadIndex          = 0;
	gFlagPlayQueuedVoice     = false;
	gCountdownToPlayNextVoice_10ms = 0;
	gAnotherVoiceID          = VOICE_ID_INVALID;
}

// Force immediate display update before blocking Morse playback
static void MENU_ForceDisplayUpdate(void)
{
	gFlagRefreshSetting   = true;
	gRequestDisplayScreen = DISPLAY_INVALID;
	UI_DisplayMenu();
}

static KEY_Code_t gMorseAbortKey = KEY_INVALID;

static bool MENU_IsAbortKeyPressed(void)
{
	KEY_Code_t key = KEYBOARD_Poll();
	if (key != KEY_INVALID)
		gMorseAbortKey = key;
	return key != KEY_INVALID;
}

static void MENU_WaitForKeyReleaseBeforeMorse(void)
{
	for (uint8_t i = 0; i < 25U; i++)
	{
		if (KEYBOARD_Poll() == KEY_INVALID)
			return;

		SYSTEM_DelayMs(10);
	}
}

static bool MENU_DelayInterruptible(uint16_t duration_ms)
{
	while (duration_ms > 0U)
	{
		const uint8_t chunk_ms = (duration_ms > 10U) ? 10U : (uint8_t)duration_ms;

		if (MENU_IsAbortKeyPressed())
			return false;

		SYSTEM_DelayMs(chunk_ms);
		duration_ms -= chunk_ms;
	}

	return true;
}

// Saved tone config for one-time setup/teardown
static uint16_t gMorseToneCfg;
static uint16_t gMorseAfGainCfg;

// One-time audio setup before a morse string
static void MENU_MorseAudioSetup(void)
{
	BK4819_EnterTxMute();
	gMorseToneCfg  = BK4819_ReadRegister(BK4819_REG_71);
	gMorseAfGainCfg = BK4819_ReadRegister(BK4819_REG_48);
	// Set a fixed AF DAC gain for consistent Morse volume
	BK4819_WriteRegister(BK4819_REG_48,
		(11u << 12) |
		( 0u << 10) |
		(58u <<  4) |
		( 8u <<  0));
	BK4819_PlayTone(880, true);
	SYSTEM_DelayMs(2);
	AUDIO_AudioPathOn();
	SYSTEM_DelayMs(60);
	// tone generator running but muted -- ready for elements
}

// One-time audio teardown after morse string
static void MENU_MorseAudioTeardown(void)
{
	BK4819_EnterTxMute();
	SYSTEM_DelayMs(20);
	AUDIO_AudioPathOff();
	SYSTEM_DelayMs(5);
	BK4819_TurnsOffTones_TurnsOnRX();
	SYSTEM_DelayMs(5);
	BK4819_WriteRegister(BK4819_REG_71, gMorseToneCfg);
	BK4819_WriteRegister(BK4819_REG_48, gMorseAfGainCfg);
}

// Play a single morse element (unmute for duration, then re-mute)
static bool MENU_PlayMorseElement(const uint16_t duration_ms)
{
	BK4819_ExitTxMute();
	if (!MENU_DelayInterruptible(duration_ms))
	{
		BK4819_EnterTxMute();
		return false;
	}
	BK4819_EnterTxMute();
	return true;
}

static const char *MENU_GetMorseLabel(const uint8_t menu_id)
{
	switch (menu_id)
	{
		case MENU_SQL:                  return "SQUELCH";
		case MENU_STEP:                 return "STEP";
		case MENU_R_DCS:                return "DCS";
		case MENU_R_CTCS:               return "CTCSS";
		case MENU_W_N:                  return "BANDWIDTH";
		case MENU_AM:                   return "AM FM";
		case MENU_COMPAND:              return "COMPAND";
		case MENU_S_ADD1:               return "SCAN LIST 1";
		case MENU_S_ADD2:               return "SCAN LIST 2";
		case MENU_MEM_CH:               return "MEMORY";
		case MENU_DEL_CH:               return "DELETE";
		case MENU_MEM_NAME:             return "NAME";
		case MENU_S_LIST:               return "SCAN LIST";
		case MENU_SLIST1:               return "LIST 1";
		case MENU_SLIST2:               return "LIST 2";
		case MENU_SC_REV:               return "SCAN RESUME";
		case MENU_MDF:                  return "DISPLAY";
		case MENU_SAVE:                 return "SAVE";
		case MENU_ABR:                  return "BACKLIGHT";
		case MENU_ABR_MIN:              return "BACKLIGHT MINIMUM";
		case MENU_ABR_MAX:              return "BACKLIGHT MAXIMUM";
		case MENU_ABR_ON_TX_RX:         return "BACKLIGHT TX RX";
		case MENU_BEEP:                 return "BEEP";
		case MENU_AUTOLK:               return "KEY LOCK";
		case MENU_STE:                  return "TAIL";
		case MENU_RP_STE:               return "TAIL RP";
		case MENU_MIC:                  return "MIC";
		case MENU_PONMSG:               return "POWER ON";
		case MENU_VOL:                  return "BATTERY";
		case MENU_BAT_TXT:              return "BATTERY TEXT";
		case MENU_TDR:                  return "RX MODE";
		case MENU_MORSE_SPEED:          return "MORSE SPEED";
#ifdef ENABLE_VOICE
		case MENU_VOICE:                return "VOICE";
#endif
		case MENU_F1SHRT:               return "F1 SHORT";
		case MENU_F1LONG:               return "F1 LONG";
		case MENU_F2SHRT:               return "F2 SHORT";
		case MENU_F2LONG:               return "F2 LONG";
		case MENU_MLONG:                return "M LONG";
		case MENU_RESET:                return "RESET";
#ifdef ENABLE_ARDF
		case MENU_ARDF:                 return "ARDF";
		case MENU_ARDF_NUMFOXES:        return "NUMBER FOX";
		case MENU_ARDF_FOXDURATION:     return "FOX DURATION";
		case MENU_ARDF_SETFOX:          return "ACTIVE FOX";
		case MENU_ARDF_TIME_RESET:      return "TIME RESET";
		case MENU_ARDF_GAIN_REMEMBER:   return "GAIN REMEMBER";
		case MENU_ARDF_CYCLE_END_BEEP:  return "END SIGNAL";
		case MENU_ARDF_SNAPSHOT_SPEED:  return "SNAPSHOT SPEED";
		case MENU_ARDF_CLOCK_CORR:      return "CLOCK CORRECT";
		case MENU_ARDF_MIST_FREQ:       return "MISTUNE FREQ";
		case MENU_ARDF_MIST_GAIN_ADD_STEPS: return "MISTUNE GAIN";
#endif
#ifdef ENABLE_AM_FIX
		case MENU_AM_FIX:               return "AM FIX";
#endif
#ifdef ENABLE_FLASHLIGHT
		// MENU_FLASHLIGHT is not in the menu enum but keeping for completeness
#endif
		default:                        return NULL;
	}
}

static void MENU_PlayMorseString(const char *text)
{
	const uint16_t unit_ms = MENU_GetMorseUnitMs();
	const uint16_t dash_ms = unit_ms * 3U;

	if (text == NULL)
		return;

	MENU_MorseAudioSetup();

	for (size_t i = 0; text[i] != '\0'; i++)
	{
		char ch = text[i];
		const char *pattern;

		if (MENU_IsAbortKeyPressed())
			goto done;

		if (ch >= 'a' && ch <= 'z')
			ch -= ('a' - 'A');

		if (ch == ' ')
		{
			if (!MENU_DelayInterruptible(unit_ms * 7U))
				goto done;
			continue;
		}

		pattern = MENU_GetMorsePattern(ch);
		if (pattern == NULL)
			continue;

		for (size_t j = 0; pattern[j] != '\0'; j++)
		{
			if (MENU_IsAbortKeyPressed())
				goto done;

			if (!MENU_PlayMorseElement((pattern[j] == '-') ? dash_ms : unit_ms))
				goto done;

			// intra-character gap: 1 unit
			if (pattern[j + 1] != '\0')
			{
				if (!MENU_DelayInterruptible(unit_ms))
					goto done;
			}
		}

		// inter-character gap: 3 units
		if (text[i + 1] != '\0' && text[i + 1] != ' ')
		{
			if (!MENU_DelayInterruptible(unit_ms * 3U))
				goto done;
		}
	}

done:
	MENU_MorseAudioTeardown();
}

static uint8_t MENU_AppendFixedDigitsVoice(uint8_t index, uint16_t value, uint8_t digits)
{
	uint16_t divisor = 1U;

	while (digits > 1U)
	{
		divisor *= 10U;
		digits--;
	}

	for (;;)
	{
		if (index < ARRAY_SIZE(gVoiceID))
			gVoiceID[index++] = (VOICE_ID_t)(VOICE_ID_0 + (value / divisor));
		value %= divisor;

		if (divisor == 1U)
			break;

		divisor /= 10U;
	}

	gVoiceWriteIndex = index;
	return index;
}

static void MENU_QueueFixedDigitsVoice(uint16_t value, uint8_t digits)
{
	MENU_StopVoicePlayback();
	MENU_AppendFixedDigitsVoice(0, value, digits);
	AUDIO_PlaySingleVoice(false);
}

static void MENU_PlayCurrentMenuVoice(void)
{
	MENU_StopVoicePlayback();
	gMorseAbortKey = KEY_INVALID;

	// Try voice clip first (if voice prompts are enabled)
	if (gEeprom.VOICE_PROMPT != VOICE_PROMPT_OFF)
	{
		if (MenuList[gMenuCursor].voice_id != VOICE_ID_INVALID) {
			gAnotherVoiceID = MenuList[gMenuCursor].voice_id;
			return;
		}
	}

	// Fall back to morse (always available, independent of voice prompt setting)
	{
		const char *morse_label = MENU_GetMorseLabel(MenuList[gMenuCursor].menu_id);

		if (morse_label != NULL)
		{
			MENU_WaitForKeyReleaseBeforeMorse();
			gMorseAbortKey = KEY_INVALID;
			MENU_PlayMorseString(morse_label);
			return;
		}
	}

	// Last resort: voice clip for "Menu N" (only if voice enabled)
	if (gEeprom.VOICE_PROMPT != VOICE_PROMPT_OFF)
	{
		AUDIO_SetVoiceID(0, VOICE_ID_MENU);
		AUDIO_SetDigitVoice(1, gMenuCursor + 1);
		AUDIO_PlaySingleVoice(true);
	}
}

static void MENU_PlayNumberVoice(const uint16_t value)
{
	if (gEeprom.VOICE_PROMPT == VOICE_PROMPT_OFF)
		return;

	AUDIO_SetDigitVoice(0, value);
	AUDIO_PlaySingleVoice(true);
}

static void MENU_PlayStepVoice(const uint16_t step_10hz)
{
	uint8_t index;

	if (gEeprom.VOICE_PROMPT == VOICE_PROMPT_OFF)
		return;

	MENU_StopVoicePlayback();
	index = AUDIO_SetDigitVoice(0, step_10hz / 100U);
	MENU_AppendFixedDigitsVoice(index, step_10hz % 100U, 2);
	AUDIO_PlaySingleVoice(false);
}

static void MENU_PlayValueVoice(const uint8_t menu_id, const int32_t value)
{
	if (gEeprom.VOICE_PROMPT == VOICE_PROMPT_OFF)
		return;

	switch (menu_id)
	{
		case MENU_STEP:
			MENU_PlayStepVoice(gStepFrequencyTable[FREQUENCY_GetStepIdxFromSortedIdx((uint8_t)value)]);
			break;

		case MENU_AM:
			if (value >= 0 && (unsigned int)value < ARRAY_SIZE(gModulationStr))
				MENU_PlayMorseString(gModulationStr[value]);
			break;

		case MENU_ARDF:
			// Distinguish OFF / ARDF / DF Simple via Morse
			if (value <= 0)
				MENU_PlayMorseString("OFF");
			else if (value == 1)
				MENU_PlayMorseString("ARDF");
			else
				MENU_PlayMorseString("DF");
			break;

		case MENU_BEEP:
			gAnotherVoiceID = (value <= 0) ? VOICE_ID_OFF : VOICE_ID_ON;
			break;

		case MENU_ABR:
			if (value <= 0)
			{
				gAnotherVoiceID = VOICE_ID_OFF;
				break;
			}

			if (value >= 7)
			{
				gAnotherVoiceID = VOICE_ID_ON;
				break;
			}

			if (value <= 3)
			{
				MENU_PlayNumberVoice((uint16_t[]){0, 5, 10, 20}[(uint8_t)value]);
				break;
			}

			MENU_PlayNumberVoice((uint16_t[]){0, 0, 0, 0, 1, 2, 4}[(uint8_t)value]);
			break;

		case MENU_ABR_MIN:
		case MENU_ABR_MAX:
		case MENU_SAVE:
		case MENU_ARDF_NUMFOXES:
		case MENU_ARDF_SETFOX:
		case MENU_ARDF_GAIN_REMEMBER:
		case MENU_ARDF_CYCLE_END_BEEP:
		case MENU_ARDF_SNAPSHOT_SPEED:
			MENU_PlayNumberVoice((uint16_t)value);
			break;

		case MENU_VOICE:
			if (value <= 0)
				gAnotherVoiceID = VOICE_ID_OFF;
			else
				MENU_PlayNumberVoice((uint16_t)value);
			break;

		case MENU_RESET:
			MENU_PlayNumberVoice((uint16_t)(value + 1));
			break;

		case MENU_MORSE_SPEED:
			MENU_QueueFixedDigitsVoice((uint16_t)value, 2);
			break;

		case MENU_ARDF_FOXDURATION:
			MENU_PlayNumberVoice((uint16_t)((value + 50) / 100));
			break;

		case MENU_ARDF_TIME_RESET:
			gAnotherVoiceID = VOICE_ID_CONFIRM;
			break;

		default:
			break;
	}
}
#endif

#ifdef ENABLE_F_CAL_MENU
	void writeXtalFreqCal(const int32_t value, const bool update_eeprom)
	{
		BK4819_WriteRegister(BK4819_REG_3B, 22656 + value);

		if (update_eeprom)
		{
			struct
			{
				int16_t  BK4819_XtalFreqLow;
				uint16_t EEPROM_1F8A;
				uint16_t EEPROM_1F8C;
				uint8_t  VOLUME_GAIN;
				uint8_t  DAC_GAIN;
			} __attribute__((packed)) misc;

			gEeprom.BK4819_XTAL_FREQ_LOW = value;

			// radio 1 .. 04 00 46 00 50 00 2C 0E
			// radio 2 .. 05 00 46 00 50 00 2C 0E
			//
			EEPROM_ReadBuffer(0x1F88, &misc, 8);
			misc.BK4819_XtalFreqLow = value;
			EEPROM_WriteBuffer(0x1F88, &misc);
		}
	}
#endif

void MENU_StartCssScan(void)
{
	SCANNER_Start(true);
	gUpdateStatus = true;
	gCssBackgroundScan = true;

	gRequestDisplayScreen = DISPLAY_MENU;
}

void MENU_CssScanFound(void)
{
	if(gScanCssResultType == CODE_TYPE_DIGITAL || gScanCssResultType == CODE_TYPE_REVERSE_DIGITAL) {
		gMenuCursor = UI_MENU_GetMenuIdx(MENU_R_DCS);
	}
	else if(gScanCssResultType == CODE_TYPE_CONTINUOUS_TONE) {
		gMenuCursor = UI_MENU_GetMenuIdx(MENU_R_CTCS);
	}

	MENU_ShowCurrentSetting();

	gUpdateStatus = true;
	gUpdateDisplay = true;
}

void MENU_StopCssScan(void)
{
	gCssBackgroundScan = false;

#ifdef ENABLE_VOICE
	gAnotherVoiceID       = VOICE_ID_SCANNING_STOP;
#endif
	gUpdateDisplay = true;
	gUpdateStatus = true;
}

int MENU_GetLimits(uint8_t menu_id, int32_t *pMin, int32_t *pMax)
{
	switch (menu_id)
	{
		case MENU_SQL:
			*pMin = 0;
			*pMax = 9;
			break;

		case MENU_STEP:
			*pMin = 0;
			*pMax = STEP_N_ELEM - 1;
			break;

		case MENU_ABR:
			*pMin = 0;
			*pMax = ARRAY_SIZE(gSubMenu_BACKLIGHT) - 1;
			break;

		case MENU_ABR_MIN:
			*pMin = 0;
			*pMax = 9;
			break;

		case MENU_ABR_MAX:
			*pMin = 1;
			*pMax = 10;
			break;

		case MENU_F_LOCK:
			*pMin = 0;
			*pMax = ARRAY_SIZE(gSubMenu_F_LOCK) - 1;
			break;

		case MENU_MDF:
			*pMin = 0;
			*pMax = ARRAY_SIZE(gSubMenu_MDF) - 1;
			break;

		case MENU_TDR:
			*pMin = 0;
			*pMax = ARRAY_SIZE(gSubMenu_RXMode) - 1;
			break;

		#ifdef ENABLE_VOICE
			case MENU_VOICE:
				*pMin = 0;
				*pMax = ARRAY_SIZE(gSubMenu_VOICE) - 1;
				break;

			case MENU_MORSE_SPEED:
				*pMin = 15;
				*pMax = 70;
				break;
		#endif

		case MENU_SC_REV:
			*pMin = 0;
			*pMax = ARRAY_SIZE(gSubMenu_SC_REV) - 1;
			break;

		case MENU_PONMSG:
			*pMin = 0;
			*pMax = ARRAY_SIZE(gSubMenu_PONMSG) - 1;
			break;

		case MENU_R_DCS:
			*pMin = 0;
			*pMax = 208;
			//*pMax = (ARRAY_SIZE(DCS_Options) * 2);
			break;

		case MENU_R_CTCS:
			*pMin = 0;
			*pMax = ARRAY_SIZE(CTCSS_Options);
			break;

		case MENU_W_N:
			*pMin = 0;
			*pMax = ARRAY_SIZE(gSubMenu_W_N) - 1;
			break;

                #ifdef ENABLE_ARDF
                
		case MENU_ARDF:
			*pMin = 0;
			*pMax = ARRAY_SIZE(gSubMenu_ARDF) - 1;
			break;
                
		case MENU_ARDF_NUMFOXES:
			*pMin = 0;
			*pMax = ARDF_NUM_FOX_MAX;
			break;

		case MENU_ARDF_SETFOX:
			*pMin = 1;
			*pMax = MAX(1, gARDFNumFoxes);
			break;
		
		case MENU_ARDF_GAIN_REMEMBER:
			*pMin = 0;
			*pMax = ARRAY_SIZE(gSubMenu_ARDF_Remember_Gain) - 1;
			break;

		case MENU_ARDF_CYCLE_END_BEEP:
			*pMin = 0;
			*pMax = ARDF_CYCLE_END_BEEP_S_MAX;
			break;

		case MENU_ARDF_SNAPSHOT_SPEED:
			*pMin = 1;
			*pMax = ARDF_SNAPSHOT_SPEED_MAX;
			break;

		case MENU_ARDF_MIST_FREQ:
			*pMin = -128;
			*pMax = 127;
			break;

		case MENU_ARDF_MIST_GAIN_ADD_STEPS:
			//*pMin = 0;
			*pMax = 8;
			break;

		
                #endif

		#ifdef ENABLE_ALARM
			case MENU_AL_MOD:
				*pMin = 0;
				*pMax = ARRAY_SIZE(gSubMenu_AL_MOD) - 1;
				break;
		#endif

		case MENU_RESET:
			*pMin = 0;
			*pMax = ARRAY_SIZE(gSubMenu_RESET) - 1;
			break;

		case MENU_COMPAND:
		case MENU_ABR_ON_TX_RX:
			*pMin = 0;
			*pMax = ARRAY_SIZE(gSubMenu_RX_TX) - 1;
			break;

		#ifdef ENABLE_AM_FIX
			case MENU_AM_FIX:
		#endif
		#ifdef ENABLE_AUDIO_BAR
			case MENU_MIC_BAR:
		#endif
		case MENU_BEEP:
		case MENU_AUTOLK:
		case MENU_S_ADD1:
		case MENU_S_ADD2:
		case MENU_STE:
#ifdef ENABLE_DTMF_CALLING
		case MENU_D_DCD:
#endif
		#ifdef ENABLE_NOAA
			case MENU_NOAA_S:
		#endif
		case MENU_350EN:
			*pMin = 0;
			*pMax = ARRAY_SIZE(gSubMenu_OFF_ON) - 1;
			break;

		case MENU_AM:
			*pMin = 0;
			*pMax = ARRAY_SIZE(gModulationStr) - 1;
			break;

		#ifdef ENABLE_VOX
			case MENU_VOX:
		#endif
		case MENU_RP_STE:
			*pMin = 0;
			*pMax = 10;
			break;

		case MENU_MEM_CH:
		case MENU_DEL_CH:
		case MENU_MEM_NAME:
			*pMin = 0;
			*pMax = MR_CHANNEL_LAST;
			break;

		case MENU_SLIST1:
		case MENU_SLIST2:
			*pMin = -1;
			*pMax = MR_CHANNEL_LAST;
			break;

		case MENU_SAVE:
			*pMin = 0;
			*pMax = ARRAY_SIZE(gSubMenu_SAVE) - 1;
			break;

		case MENU_MIC:
			*pMin = 0;
			*pMax = 4;
			break;

		case MENU_S_LIST:
			*pMin = 0;
			*pMax = 2;
			break;

#ifdef ENABLE_DTMF_CALLING
		case MENU_D_RSP:
			*pMin = 0;
			*pMax = ARRAY_SIZE(gSubMenu_D_RSP) - 1;
			break;
#endif

		case MENU_BAT_TXT:
			*pMin = 0;
			*pMax = ARRAY_SIZE(gSubMenu_BAT_TXT) - 1;
			break;

#ifdef ENABLE_DTMF_CALLING
		case MENU_D_HOLD:
			*pMin = 5;
			*pMax = 60;
			break;
#endif

#ifdef ENABLE_DTMF_CALLING
		case MENU_D_LIST:
			*pMin = 1;
			*pMax = 16;
			break;
#endif
		#ifdef ENABLE_F_CAL_MENU
			case MENU_F_CALI:
				*pMin = -50;
				*pMax = +50;
				break;
		#endif

		case MENU_BATCAL:
			*pMin = 1600;
			*pMax = 2200;
			break;

		case MENU_BATTYP:
			*pMin = 0;
			*pMax = 1;
			break;

		case MENU_F1SHRT:
		case MENU_F1LONG:
		case MENU_F2SHRT:
		case MENU_F2LONG:
		case MENU_MLONG:
			*pMin = 0;
			*pMax = gSubMenu_SIDEFUNCTIONS_size-1;
			break;

		default:
			return -1;
	}

	return 0;
}

void MENU_AcceptSetting(void)
{
	int32_t        Min;
	int32_t        Max;
	FREQ_Config_t *pConfig = &gTxVfo->freq_config_RX;

	if (!MENU_GetLimits(UI_MENU_GetCurrentMenuId(), &Min, &Max))
	{
		if (gSubMenuSelection < Min) gSubMenuSelection = Min;
		else
		if (gSubMenuSelection > Max) gSubMenuSelection = Max;
	}

	switch (UI_MENU_GetCurrentMenuId())
	{
		default:
			return;

		case MENU_SQL:
			gEeprom.SQUELCH_LEVEL = gSubMenuSelection;
			gVfoConfigureMode     = VFO_CONFIGURE;
			break;

		case MENU_STEP:
			gTxVfo->STEP_SETTING = FREQUENCY_GetStepIdxFromSortedIdx(gSubMenuSelection);
			if (IS_FREQ_CHANNEL(gTxVfo->CHANNEL_SAVE))
			{
				gRequestSaveChannel = 1;
			}
			return;

		case MENU_R_DCS: {
			if (gSubMenuSelection == 0) {
				if (pConfig->CodeType == CODE_TYPE_CONTINUOUS_TONE) {
					return;
				}
				pConfig->Code = 0;
				pConfig->CodeType = CODE_TYPE_OFF;
			}
			else if (gSubMenuSelection < 105) {
				pConfig->CodeType = CODE_TYPE_DIGITAL;
				pConfig->Code = gSubMenuSelection - 1;
			}
			else {
				pConfig->CodeType = CODE_TYPE_REVERSE_DIGITAL;
				pConfig->Code = gSubMenuSelection - 105;
			}

			gRequestSaveChannel = 1;
			return;
		}
		case MENU_R_CTCS: {
			if (gSubMenuSelection == 0) {
				if (pConfig->CodeType != CODE_TYPE_CONTINUOUS_TONE) {
					return;
				}
				pConfig->Code     = 0;
				pConfig->CodeType = CODE_TYPE_OFF;
			}
			else {
				pConfig->Code     = gSubMenuSelection - 1;
				pConfig->CodeType = CODE_TYPE_CONTINUOUS_TONE;
			}

			gRequestSaveChannel = 1;
			return;
		}
		case MENU_W_N:
			gTxVfo->CHANNEL_BANDWIDTH = gSubMenuSelection;
			gRequestSaveChannel       = 1;
			return;


#ifdef ENABLE_ARDF
		
		case MENU_ARDF:

			if ( gSubMenuSelection == 2 )
			{
				// DF simple mode implies ARDF on
				gSubMenuSelection = 3;

				// DF simple settings
				gARDFNumFoxes = 0;
				gARDFGainRemember = 0;
				gEeprom.SQUELCH_LEVEL = 0;
			}

			if ( (gSubMenuSelection & 0x01) != 0 )
			{
				// an ARDF mode was switched on. make sure to use RxMode MAIN_ONLY!
				gEeprom.DUAL_WATCH = DUAL_WATCH_OFF;

				gVfoConfigureMode    = VFO_CONFIGURE;
				gFlagReconfigureVfos = true;
				gUpdateStatus        = true;
			}

			if ( ((gSetting_ARDFEnable & 0x01) + (gARDFDFSimpleMode << 1)) != gSubMenuSelection )
			{
				// value changed
				gSetting_ARDFEnable = gSubMenuSelection & 0x01;
				gARDFDFSimpleMode = (gSubMenuSelection >> 1) & 0x01;

				RADIO_SetupAGC(gRxVfo->Modulation == MODULATION_AM, false); // if gSetting_ARDFEnable is set, AGC will be switched off

				gARDFRequestSaveEEPROM = true;
			}

			break; // not return, save SQL and others, too

		case MENU_ARDF_NUMFOXES:

			if ( gARDFNumFoxes != gSubMenuSelection )
			{
				// value updated
				gARDFNumFoxes = gSubMenuSelection;

				gARDFRequestSaveEEPROM = true;
			}
#ifdef ENABLE_VOICE
			MENU_PlayValueVoice(MENU_ARDF_NUMFOXES, gARDFNumFoxes);
#endif
			return;

		case MENU_ARDF_FOXDURATION:

			if ( gARDFFoxDuration10ms != (uint32_t)gSubMenuSelection )
			{
				// value updated
				gARDFFoxDuration10ms = gSubMenuSelection;
				gARDFFoxDuration10ms_corr = (uint32_t)( (int32_t)gARDFFoxDuration10ms + ( (int32_t)gARDFFoxDuration10ms * (int32_t)gARDFClockCorrAddTicksPerMin)/6000 ); // fixme: limit to 1s

				gARDFRequestSaveEEPROM = true;
			}
#ifdef ENABLE_VOICE
			MENU_PlayValueVoice(MENU_ARDF_FOXDURATION, gARDFFoxDuration10ms);
#endif
			return;

		case MENU_ARDF_SETFOX:

			gARDFActiveFox = gSubMenuSelection - 1;
#ifdef ENABLE_VOICE
			MENU_PlayValueVoice(MENU_ARDF_SETFOX, gSubMenuSelection);
#endif

			return;

		case MENU_ARDF_TIME_RESET:

			gARDFTime10ms = 0;
#ifdef ENABLE_VOICE
			MENU_PlayValueVoice(MENU_ARDF_TIME_RESET, 0);
#endif

			return;

		case MENU_ARDF_GAIN_REMEMBER:

			if ( gARDFGainRemember != gSubMenuSelection )
			{
				// value updated
				gARDFGainRemember = gSubMenuSelection;

				uint8_t vfo = gEeprom.RX_VFO;

				if ( gSetting_ARDFEnable && (gARDFGainRemember != false) )
				{
				   // gain remember switched from off to on
				   if ( (ardf_mistune_active[vfo][0] == false) && (ardf_mistune_active[vfo][gARDFActiveFox] != false) )
				   {
				      // reenable mistuning
				      ARDF_DoMistuneFreq();
				   }
				   else if ( (ardf_mistune_active[vfo][0] != false) && (ardf_mistune_active[vfo][gARDFActiveFox] == false) )
				   {
				      // end mistuning
				      ARDF_UndoMistuneFreq();
				   }

				   ARDF_ActivateGainIndex();
				}
				else
				{
					// gain remember switched from on to off
					// just keep current mistune and gain index settings
					ardf_mistune_active[vfo][0] = ardf_mistune_active[vfo][gARDFActiveFox];
					ardf_gain_index[vfo][0] = ardf_gain_index[vfo][gARDFActiveFox];
					ardf_gain_index_steps_mistune[vfo][0] = ardf_gain_index_steps_mistune[vfo][gARDFActiveFox];
				}

				gARDFRequestSaveEEPROM = true;
			}
#ifdef ENABLE_VOICE
			MENU_PlayValueVoice(MENU_ARDF_GAIN_REMEMBER, gARDFGainRemember);
#endif

			return;

		case MENU_ARDF_CYCLE_END_BEEP:

			if ( gARDFCycleEndBeep_s != gSubMenuSelection )
			{
				// value updated
				gARDFCycleEndBeep_s = gSubMenuSelection;

				gARDFRequestSaveEEPROM = true;
			}
#ifdef ENABLE_VOICE
			MENU_PlayValueVoice(MENU_ARDF_CYCLE_END_BEEP, gARDFCycleEndBeep_s);
#endif
			return;

		case MENU_ARDF_SNAPSHOT_SPEED:

			if ( gARDFSnapshotSpeed != gSubMenuSelection )
			{
				gARDFSnapshotSpeed = gSubMenuSelection;
				gARDFRequestSaveEEPROM = true;
			}
#ifdef ENABLE_VOICE
			MENU_PlayValueVoice(MENU_ARDF_SNAPSHOT_SPEED, gARDFSnapshotSpeed);
#endif
			return;

		case MENU_ARDF_CLOCK_CORR:

			if ( gARDFClockCorrAddTicksPerMin != gSubMenuSelection )
			{
				// value updated
				gARDFClockCorrAddTicksPerMin = gSubMenuSelection;
				gARDFFoxDuration10ms_corr = (uint32_t)( (int32_t)gARDFFoxDuration10ms + ( (int32_t)gARDFFoxDuration10ms * (int32_t)gARDFClockCorrAddTicksPerMin)/6000 ); // fixme: limit to 1s

				gARDFRequestSaveEEPROM = true;
			}
			return;

		case MENU_ARDF_MIST_FREQ:

			if ( gARDFMistuneFreqRaw != gSubMenuSelection )
			{
				// value updated

				// disable old mistuning value if it is active // fixme move to if unten
				uint8_t vfo = gEeprom.RX_VFO;
				uint8_t activefox = gARDFActiveFox;

				if ( ARDF_ActVfoHasGainRemember(vfo) == false )
				{
					// do not remember fox gains on this vfo
					activefox = 0;
				}

				if ( (gSetting_ARDFEnable) && (ardf_mistune_active[vfo][activefox] != false) )
				{
					// frequency mistuning active. change to new mistune frequency
					uint32_t frequency = gTxVfo->freq_config_RX.Frequency - (gARDFMistuneFreqRaw*ARDF_MISTUNE_RES_HZ/10) + (gSubMenuSelection*ARDF_MISTUNE_RES_HZ/10);

					if ( RX_freq_check(frequency) < 0 )
					{
						// frequency not allowed
						gBeepToPlay = BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL;
						return;
					}
					gTxVfo->freq_config_RX.Frequency = frequency;
					BK4819_SetFrequency(frequency);
					// not gRequestSaveChannel = 1 because mistuning must not be saved!

					uint16_t reg = BK4819_ReadRegister(BK4819_REG_30);
					BK4819_WriteRegister(BK4819_REG_30, reg & ~BK4819_REG_30_ENABLE_VCO_CALIB);
					BK4819_WriteRegister(BK4819_REG_30, reg);
				}

				gARDFMistuneFreqRaw = gSubMenuSelection; // take over new value

				gARDFRequestSaveEEPROM = true;
			}
			return;

		case MENU_ARDF_MIST_GAIN_ADD_STEPS:

			if ( gARDFMistuneAddGainIdxSteps != gSubMenuSelection )
			{
				// value updated

				// disable old mistuning value if it is active
				ARDF_StopFreqMistune();

				// other timeslots might be out of range now. reset mistuning.
				for ( uint8_t i=0; i<ARDF_NUM_FOX_MAX; i++ )
				{
					if ( ardf_mistune_active[0][i] != false )
					{
						ardf_mistune_active[0][i] = false;
						ardf_gain_index_steps_mistune[0][i] = 0;
						ardf_gain_index[0][i] = 0;

					}

					if ( ardf_mistune_active[1][i] != false )
					{
						ardf_mistune_active[1][i] = false;
						ardf_gain_index_steps_mistune[1][i] = 0;
						ardf_gain_index[1][i] = 0;
					}
				}

				// take over new value
				gARDFMistuneAddGainIdxSteps = gSubMenuSelection; // take over new value

				gARDFRequestSaveEEPROM = true;
			}
			return;

#endif

		case MENU_MEM_CH:
			gTxVfo->CHANNEL_SAVE = gSubMenuSelection;
			#if 0
				gEeprom.MrChannel[0] = gSubMenuSelection;
			#else
				gEeprom.MrChannel[gEeprom.TX_VFO] = gSubMenuSelection;
			#endif
			gRequestSaveChannel = 2;
			gVfoConfigureMode   = VFO_CONFIGURE_RELOAD;
			gFlagResetVfos      = true;
			return;

		case MENU_MEM_NAME:
			for (int i = 9; i >= 0; i--) {
				if (edit[i] != ' ' && edit[i] != '_' && edit[i] != 0x00 && edit[i] != 0xff)
					break;
				edit[i] = ' ';
			}

			SETTINGS_SaveChannelName(gSubMenuSelection, edit);
			return;

		case MENU_SAVE:
			gEeprom.BATTERY_SAVE = gSubMenuSelection;
			break;

		#ifdef ENABLE_VOX
			case MENU_VOX:
				gEeprom.VOX_SWITCH = gSubMenuSelection != 0;
				if (gEeprom.VOX_SWITCH)
					gEeprom.VOX_LEVEL = gSubMenuSelection - 1;
				SETTINGS_LoadCalibration();
				gFlagReconfigureVfos = true;
				gUpdateStatus        = true;
				break;
		#endif

		case MENU_ABR:
			gEeprom.BACKLIGHT_TIME = gSubMenuSelection;
			break;

		case MENU_ABR_MIN:
			gEeprom.BACKLIGHT_MIN = gSubMenuSelection;
			gEeprom.BACKLIGHT_MAX = MAX(gSubMenuSelection + 1 , gEeprom.BACKLIGHT_MAX);
			break;

		case MENU_ABR_MAX:
			gEeprom.BACKLIGHT_MAX = gSubMenuSelection;
			gEeprom.BACKLIGHT_MIN = MIN(gSubMenuSelection - 1, gEeprom.BACKLIGHT_MIN);
			break;

		case MENU_ABR_ON_TX_RX:
			gSetting_backlight_on_tx_rx = gSubMenuSelection;
			break;

		case MENU_TDR:
			gEeprom.DUAL_WATCH = (gEeprom.TX_VFO + 1) * (gSubMenuSelection & 1);

			gFlagReconfigureVfos = true;
			gUpdateStatus        = true;
			break;

		case MENU_BEEP:
			gEeprom.BEEP_CONTROL = gSubMenuSelection;
			break;

		#ifdef ENABLE_VOICE
			case MENU_VOICE:
				gEeprom.VOICE_PROMPT = gSubMenuSelection;
				gUpdateStatus        = true;
				break;

			case MENU_MORSE_SPEED:
				gMorseSpeedWpm = gSubMenuSelection;
				break;
		#endif

		case MENU_SC_REV:
			gEeprom.SCAN_RESUME_MODE = gSubMenuSelection;
			break;

		case MENU_MDF:
			gEeprom.CHANNEL_DISPLAY_MODE = gSubMenuSelection;
			break;

		case MENU_AUTOLK:
			gEeprom.AUTO_KEYPAD_LOCK = gSubMenuSelection;
			gKeyLockCountdown        = 30;
			break;

		case MENU_S_ADD1:
			gTxVfo->SCANLIST1_PARTICIPATION = gSubMenuSelection;
			SETTINGS_UpdateChannel(gTxVfo->CHANNEL_SAVE, gTxVfo, true);
			gVfoConfigureMode = VFO_CONFIGURE;
			gFlagResetVfos    = true;
			return;

		case MENU_S_ADD2:
			gTxVfo->SCANLIST2_PARTICIPATION = gSubMenuSelection;
			SETTINGS_UpdateChannel(gTxVfo->CHANNEL_SAVE, gTxVfo, true);
			gVfoConfigureMode = VFO_CONFIGURE;
			gFlagResetVfos    = true;
			return;

		case MENU_STE:
			break;

		case MENU_RP_STE:
			break;

		case MENU_MIC:
			gEeprom.MIC_SENSITIVITY = gSubMenuSelection;
			SETTINGS_LoadCalibration();
			gFlagReconfigureVfos = true;
			break;

		#ifdef ENABLE_AUDIO_BAR
			case MENU_MIC_BAR:
				gSetting_mic_bar = gSubMenuSelection;
				break;
		#endif

		case MENU_COMPAND:
			gTxVfo->Compander = gSubMenuSelection;
			SETTINGS_UpdateChannel(gTxVfo->CHANNEL_SAVE, gTxVfo, true);
			gVfoConfigureMode = VFO_CONFIGURE;
			gFlagResetVfos    = true;
//			gRequestSaveChannel = 1;
			return;

		case MENU_S_LIST:
			gEeprom.SCAN_LIST_DEFAULT = gSubMenuSelection;
			break;

		#ifdef ENABLE_ALARM
			case MENU_AL_MOD:
				break;
		#endif

#ifdef ENABLE_DTMF_CALLING
		case MENU_D_RSP:
			gEeprom.DTMF_DECODE_RESPONSE = gSubMenuSelection;
			break;

		case MENU_D_HOLD:
			gEeprom.DTMF_auto_reset_time = gSubMenuSelection;
			break;
#endif

		case MENU_BAT_TXT:
			gSetting_battery_text = gSubMenuSelection;
			break;

#ifdef ENABLE_DTMF_CALLING
		case MENU_D_DCD:
			gTxVfo->DTMF_DECODING_ENABLE = gSubMenuSelection;
			DTMF_clear_RX();
			gRequestSaveChannel = 1;
			return;
#endif

#ifdef ENABLE_DTMF_CALLING
		case MENU_D_LIST:
			gDTMF_chosen_contact = gSubMenuSelection - 1;
			if (gIsDtmfContactValid)
			{
				GUI_SelectNextDisplay(DISPLAY_MAIN);
				gDTMF_InputMode       = true;
				gDTMF_InputBox_Index  = 3;
				memcpy(gDTMF_InputBox, gDTMF_ID, 4);
				gRequestDisplayScreen = DISPLAY_INVALID;
			}
			return;
#endif
		case MENU_PONMSG:
			gEeprom.POWER_ON_DISPLAY_MODE = gSubMenuSelection;
			break;

		case MENU_AM:
			gTxVfo->Modulation     = gSubMenuSelection;
			gRequestSaveChannel = 1;
			return;

		#ifdef ENABLE_AM_FIX
			case MENU_AM_FIX:
				gSetting_AM_fix = gSubMenuSelection;
				gVfoConfigureMode = VFO_CONFIGURE_RELOAD;
				gFlagResetVfos    = true;
				break;
		#endif

		#ifdef ENABLE_NOAA
			case MENU_NOAA_S:
				gEeprom.NOAA_AUTO_SCAN = gSubMenuSelection;
				gFlagReconfigureVfos   = true;
				break;
		#endif

		case MENU_DEL_CH:
			SETTINGS_UpdateChannel(gSubMenuSelection, NULL, false);
			gVfoConfigureMode = VFO_CONFIGURE_RELOAD;
			gFlagResetVfos    = true;
			return;

		case MENU_RESET:
			SETTINGS_FactoryReset(gSubMenuSelection);
			return;

		case MENU_F_LOCK:
			break;

		case MENU_350EN:
			gSetting_350EN       = gSubMenuSelection;
			gVfoConfigureMode    = VFO_CONFIGURE_RELOAD;
			gFlagResetVfos       = true;
			break;

		#ifdef ENABLE_F_CAL_MENU
			case MENU_F_CALI:
				writeXtalFreqCal(gSubMenuSelection, true);
				return;
		#endif

		case MENU_BATCAL:
		{																 // voltages are averages between discharge curves of 1600 and 2200 mAh
			// gBatteryCalibration[0] = (520ul * gSubMenuSelection) / 760;  // 5.20V empty, blinking above this value, reduced functionality below
			// gBatteryCalibration[1] = (689ul * gSubMenuSelection) / 760;  // 6.89V,  ~5%, 1 bars above this value
			// gBatteryCalibration[2] = (724ul * gSubMenuSelection) / 760;  // 7.24V, ~17%, 2 bars above this value
			gBatteryCalibration[3] =          gSubMenuSelection;         // 7.6V,  ~29%, 3 bars above this value
			// gBatteryCalibration[4] = (771ul * gSubMenuSelection) / 760;  // 7.71V, ~65%, 4 bars above this value
			// gBatteryCalibration[5] = 2300;
			SETTINGS_SaveBatteryCalibration(gBatteryCalibration);
			return;
		}

		case MENU_BATTYP:
			gEeprom.BATTERY_TYPE = gSubMenuSelection;
			break;

		case MENU_F1SHRT:
		case MENU_F1LONG:
		case MENU_F2SHRT:
		case MENU_F2LONG:
		case MENU_MLONG:
			{
				uint8_t * fun[]= {
					&gEeprom.KEY_1_SHORT_PRESS_ACTION,
					&gEeprom.KEY_1_LONG_PRESS_ACTION,
					&gEeprom.KEY_2_SHORT_PRESS_ACTION,
					&gEeprom.KEY_2_LONG_PRESS_ACTION,
					&gEeprom.KEY_M_LONG_PRESS_ACTION};
				*fun[UI_MENU_GetCurrentMenuId()-MENU_F1SHRT] = gSubMenu_SIDEFUNCTIONS[gSubMenuSelection].id;
			}
			break;

	}

	gRequestSaveSettings = true;
}

static void MENU_ClampSelection(int8_t Direction)
{
	int32_t Min;
	int32_t Max;

	if (!MENU_GetLimits(UI_MENU_GetCurrentMenuId(), &Min, &Max))
	{
		int32_t Selection = gSubMenuSelection;
		if (Selection < Min) Selection = Min;
		else
		if (Selection > Max) Selection = Max;
		gSubMenuSelection = NUMBER_AddWithWraparound(Selection, -Direction, Min, Max);
	}
}

void MENU_ShowCurrentSetting(void)
{
	switch (UI_MENU_GetCurrentMenuId())
	{
		case MENU_SQL:
			gSubMenuSelection = gEeprom.SQUELCH_LEVEL;
			break;

		case MENU_STEP:
			gSubMenuSelection = FREQUENCY_GetSortedIdxFromStepIdx(gTxVfo->STEP_SETTING);
			break;

		case MENU_RESET:
			gSubMenuSelection = 0;
			break;

		case MENU_R_DCS:
		case MENU_R_CTCS:
		{
			DCS_CodeType_t type = gTxVfo->freq_config_RX.CodeType;
			uint8_t code = gTxVfo->freq_config_RX.Code;
			int menuid = UI_MENU_GetCurrentMenuId();

			if(gScanUseCssResult) {
				gScanUseCssResult = false;
				type = gScanCssResultType;
				code = gScanCssResultCode;
			}
			if((menuid==MENU_R_CTCS) ^ (type==CODE_TYPE_CONTINUOUS_TONE)) { //not the same type
				gSubMenuSelection = 0;
				break;
			}

			switch (type) {
				case CODE_TYPE_CONTINUOUS_TONE:
				case CODE_TYPE_DIGITAL:
					gSubMenuSelection = code + 1;
					break;
				case CODE_TYPE_REVERSE_DIGITAL:
					gSubMenuSelection = code + 105;
					break;
				default:
					gSubMenuSelection = 0;
					break;
			}
		break;
		}

		case MENU_W_N:
			gSubMenuSelection = gTxVfo->CHANNEL_BANDWIDTH;
			break;


#ifdef ENABLE_ARDF

		case MENU_ARDF:
            if ( gSetting_ARDFEnable==0 )
            {
               // ARDF off (even if DF simple mode bit set)
               gSubMenuSelection = 0;
            }
            else if ( gARDFDFSimpleMode != 0 )
            {
               // ARDF on and DF simple mode
               gSubMenuSelection = 2;
            }
            else
            {
               // ARDF on without DF simple mode
               gSubMenuSelection = 1;
            }
#ifdef ENABLE_VOICE
			MENU_PlayValueVoice(MENU_ARDF, gSubMenuSelection);
#endif
			break;
			
		case MENU_ARDF_NUMFOXES:
			gSubMenuSelection = gARDFNumFoxes;
#ifdef ENABLE_VOICE
			MENU_PlayValueVoice(MENU_ARDF_NUMFOXES, gSubMenuSelection);
#endif
			break;
			
		case MENU_ARDF_FOXDURATION:
			gSubMenuSelection = gARDFFoxDuration10ms;
#ifdef ENABLE_VOICE
			MENU_PlayValueVoice(MENU_ARDF_FOXDURATION, gSubMenuSelection);
#endif
			break;

		case MENU_ARDF_SETFOX:
			gSubMenuSelection = gARDFActiveFox + 1;
#ifdef ENABLE_VOICE
			MENU_PlayValueVoice(MENU_ARDF_SETFOX, gSubMenuSelection);
#endif
			break;
		
		case MENU_ARDF_TIME_RESET:
			gSubMenuSelection = 0;
			break;
		
		case MENU_ARDF_GAIN_REMEMBER:
			gSubMenuSelection = gARDFGainRemember;
#ifdef ENABLE_VOICE
			MENU_PlayValueVoice(MENU_ARDF_GAIN_REMEMBER, gSubMenuSelection);
#endif
			break;

		case MENU_ARDF_CYCLE_END_BEEP:
			gSubMenuSelection = gARDFCycleEndBeep_s;
#ifdef ENABLE_VOICE
			MENU_PlayValueVoice(MENU_ARDF_CYCLE_END_BEEP, gSubMenuSelection);
#endif
			break;

		case MENU_ARDF_SNAPSHOT_SPEED:
			gSubMenuSelection = gARDFSnapshotSpeed;
#ifdef ENABLE_VOICE
			MENU_PlayValueVoice(MENU_ARDF_SNAPSHOT_SPEED, gSubMenuSelection);
#endif
			break;

		case MENU_ARDF_CLOCK_CORR:
			gSubMenuSelection = gARDFClockCorrAddTicksPerMin;
			break;

		case MENU_ARDF_MIST_FREQ:
			gSubMenuSelection = gARDFMistuneFreqRaw;
			break;

		case MENU_ARDF_MIST_GAIN_ADD_STEPS:
			gSubMenuSelection = gARDFMistuneAddGainIdxSteps;
			break;

#endif

		case MENU_MEM_CH:
			#if 0
				gSubMenuSelection = gEeprom.MrChannel[0];
			#else
				gSubMenuSelection = gEeprom.MrChannel[gEeprom.TX_VFO];
			#endif
			break;

		case MENU_MEM_NAME:
			gSubMenuSelection = gEeprom.MrChannel[gEeprom.TX_VFO];
			break;

		case MENU_SAVE:
			gSubMenuSelection = gEeprom.BATTERY_SAVE;
			break;

#ifdef ENABLE_VOX
		case MENU_VOX:
			gSubMenuSelection = gEeprom.VOX_SWITCH ? gEeprom.VOX_LEVEL + 1 : 0;
			break;
#endif

		case MENU_ABR:
			gSubMenuSelection = gEeprom.BACKLIGHT_TIME;
			break;

		case MENU_ABR_MIN:
			gSubMenuSelection = gEeprom.BACKLIGHT_MIN;
			break;

		case MENU_ABR_MAX:
			gSubMenuSelection = gEeprom.BACKLIGHT_MAX;
			break;

		case MENU_ABR_ON_TX_RX:
			gSubMenuSelection = gSetting_backlight_on_tx_rx;
			break;

		case MENU_TDR:
			gSubMenuSelection = (gEeprom.DUAL_WATCH != DUAL_WATCH_OFF);
			break;

		case MENU_BEEP:
			gSubMenuSelection = gEeprom.BEEP_CONTROL;
			break;

		#ifdef ENABLE_VOICE
			case MENU_VOICE:
				gSubMenuSelection = gEeprom.VOICE_PROMPT;
				break;

			case MENU_MORSE_SPEED:
				gSubMenuSelection = gMorseSpeedWpm;
				break;
		#endif

		case MENU_SC_REV:
			gSubMenuSelection = gEeprom.SCAN_RESUME_MODE;
			break;

		case MENU_MDF:
			gSubMenuSelection = gEeprom.CHANNEL_DISPLAY_MODE;
			break;

		case MENU_AUTOLK:
			gSubMenuSelection = gEeprom.AUTO_KEYPAD_LOCK;
			break;

		case MENU_S_ADD1:
			gSubMenuSelection = gTxVfo->SCANLIST1_PARTICIPATION;
			break;

		case MENU_S_ADD2:
			gSubMenuSelection = gTxVfo->SCANLIST2_PARTICIPATION;
			break;

		case MENU_STE:
			gSubMenuSelection = 0;
			break;

		case MENU_RP_STE:
			gSubMenuSelection = 0;
			break;

		case MENU_MIC:
			gSubMenuSelection = gEeprom.MIC_SENSITIVITY;
			break;

#ifdef ENABLE_AUDIO_BAR
		case MENU_MIC_BAR:
			gSubMenuSelection = gSetting_mic_bar;
			break;
#endif

		case MENU_COMPAND:
			gSubMenuSelection = gTxVfo->Compander;
			return;

		case MENU_S_LIST:
			gSubMenuSelection = gEeprom.SCAN_LIST_DEFAULT;
			break;

		case MENU_SLIST1:
			gSubMenuSelection = RADIO_FindNextChannel(0, 1, true, 0);
			break;

		case MENU_SLIST2:
			gSubMenuSelection = RADIO_FindNextChannel(0, 1, true, 1);
			break;

		#ifdef ENABLE_ALARM
			case MENU_AL_MOD:
				gSubMenuSelection = 0;
				break;
		#endif

#ifdef ENABLE_DTMF_CALLING
		case MENU_D_RSP:
			gSubMenuSelection = gEeprom.DTMF_DECODE_RESPONSE;
			break;

		case MENU_D_HOLD:
			gSubMenuSelection = gEeprom.DTMF_auto_reset_time;
			break;
#endif

		case MENU_BAT_TXT:
			gSubMenuSelection = gSetting_battery_text;
			return;

#ifdef ENABLE_DTMF_CALLING
		case MENU_D_DCD:
			gSubMenuSelection = gTxVfo->DTMF_DECODING_ENABLE;
			break;

		case MENU_D_LIST:
			gSubMenuSelection = gDTMF_chosen_contact + 1;
			break;
#endif

		case MENU_PONMSG:
			gSubMenuSelection = gEeprom.POWER_ON_DISPLAY_MODE;
			break;

		case MENU_AM:
			gSubMenuSelection = gTxVfo->Modulation;
			break;

#ifdef ENABLE_AM_FIX
		case MENU_AM_FIX:
			gSubMenuSelection = gSetting_AM_fix;
			break;
#endif
		#ifdef ENABLE_NOAA
			case MENU_NOAA_S:
				gSubMenuSelection = gEeprom.NOAA_AUTO_SCAN;
				break;
		#endif

		case MENU_DEL_CH:
			#if 0
				gSubMenuSelection = RADIO_FindNextChannel(gEeprom.MrChannel[0], 1, false, 1);
			#else
				gSubMenuSelection = RADIO_FindNextChannel(gEeprom.MrChannel[gEeprom.TX_VFO], 1, false, 1);
			#endif
			break;

		case MENU_F_LOCK:
			gSubMenuSelection = 0;
			break;

		case MENU_350EN:
			gSubMenuSelection = gSetting_350EN;
			break;

		#ifdef ENABLE_F_CAL_MENU
			case MENU_F_CALI:
				gSubMenuSelection = gEeprom.BK4819_XTAL_FREQ_LOW;
				break;
		#endif

		case MENU_BATCAL:
			gSubMenuSelection = gBatteryCalibration[3];
			break;

		case MENU_BATTYP:
			gSubMenuSelection = gEeprom.BATTERY_TYPE;
			break;

		case MENU_F1SHRT:
		case MENU_F1LONG:
		case MENU_F2SHRT:
		case MENU_F2LONG:
		case MENU_MLONG: {
			uint8_t * fun[]= {
				&gEeprom.KEY_1_SHORT_PRESS_ACTION,
				&gEeprom.KEY_1_LONG_PRESS_ACTION,
				&gEeprom.KEY_2_SHORT_PRESS_ACTION,
				&gEeprom.KEY_2_LONG_PRESS_ACTION,
				&gEeprom.KEY_M_LONG_PRESS_ACTION};
			uint8_t id = *fun[UI_MENU_GetCurrentMenuId()-MENU_F1SHRT];

			for(int i = 0; i < gSubMenu_SIDEFUNCTIONS_size; i++) {
				if(gSubMenu_SIDEFUNCTIONS[i].id==id) {
					gSubMenuSelection = i;
					break;
				}

			}
			break;
		}

		default:
			return;
	}
}

static void MENU_Key_0_to_9(KEY_Code_t Key, bool bKeyPressed, bool bKeyHeld)
{
	uint8_t  Offset;
	int32_t  Min;
	int32_t  Max;
	uint16_t Value = 0;

	if (bKeyHeld || !bKeyPressed)
		return;

	gBeepToPlay = BEEP_1KHZ_60MS_OPTIONAL;

	if (UI_MENU_GetCurrentMenuId() == MENU_MEM_NAME && edit_index >= 0)
	{	// currently editing the channel name

		if (edit_index < 10)
		{
			if (Key <= KEY_9)
			{
				edit[edit_index] = '0' + Key - KEY_0;

				if (++edit_index >= 10)
				{	// exit edit
					gFlagAcceptSetting  = false;
					gAskForConfirmation = 1;
				}

				gRequestDisplayScreen = DISPLAY_MENU;
			}
		}

		return;
	}

	INPUTBOX_Append(Key);

	gRequestDisplayScreen = DISPLAY_MENU;

	if (!gIsInSubMenu) {
		switch (gInputBoxIndex) {
			case 2:
				gInputBoxIndex = 0;

				Value = (gInputBox[0] * 10) + gInputBox[1];

				if (Value > 0 && Value <= gMenuListCount) {
					gMenuCursor         = Value - 1;
					gFlagRefreshSetting = true;
					return;
				}

				if (Value <= gMenuListCount)
					break;

				gInputBox[0]   = gInputBox[1];
				gInputBoxIndex = 1;
				[[fallthrough]];
			case 1:
				Value = gInputBox[0];
				if (Value > 0 && Value <= gMenuListCount) {
					gMenuCursor         = Value - 1;
					gFlagRefreshSetting = true;
					return;
				}
				break;
		}

		gInputBoxIndex = 0;

		gBeepToPlay = BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL;
		return;
	}



#ifdef ENABLE_ARDF

	if (UI_MENU_GetCurrentMenuId() == MENU_ARDF_FOXDURATION)
	{
		uint32_t Duration10ms;

		if (gInputBoxIndex < 5) 
		{ 
			// invalid duration
			return;
		}

		Duration10ms = StrToUL( INPUTBOX_GetAscii() );
		if ( Duration10ms >= 100 )
			gSubMenuSelection = Duration10ms;

		gInputBoxIndex = 0;
		return;
	}

#endif

	if (UI_MENU_GetCurrentMenuId() == MENU_MEM_CH ||
		UI_MENU_GetCurrentMenuId() == MENU_DEL_CH ||
		UI_MENU_GetCurrentMenuId() == MENU_MEM_NAME)
	{	// enter 3-digit channel number

		if (gInputBoxIndex < 3) {
#ifdef ENABLE_VOICE
			gAnotherVoiceID   = (VOICE_ID_t)Key;
#endif
			gRequestDisplayScreen = DISPLAY_MENU;
			return;
		}

		gInputBoxIndex = 0;

		Value = ((gInputBox[0] * 100) + (gInputBox[1] * 10) + gInputBox[2]) - 1;

		if (IS_MR_CHANNEL(Value)) {
#ifdef ENABLE_VOICE
			gAnotherVoiceID = (VOICE_ID_t)Key;
#endif
			gSubMenuSelection = Value;
			return;
		}

		gBeepToPlay = BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL;
		return;
	}

	if (MENU_GetLimits(UI_MENU_GetCurrentMenuId(), &Min, &Max)) {
		gInputBoxIndex = 0;
		gBeepToPlay = BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL;
		return;
	}

	Offset = (Max >= 100) ? 3 : (Max >= 10) ? 2 : 1;

	switch (gInputBoxIndex) {
		case 1:
			Value = gInputBox[0];
			break;
		case 2:
			Value = (gInputBox[0] *  10) + gInputBox[1];
			break;
		case 3:
			Value = (gInputBox[0] * 100) + (gInputBox[1] * 10) + gInputBox[2];
			break;
	}

	if (Offset == gInputBoxIndex)
		gInputBoxIndex = 0;

	if (Value <= Max) {
		gSubMenuSelection = Value;
		return;
	}

	gBeepToPlay = BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL;
}

static void MENU_Key_EXIT(bool bKeyPressed, bool bKeyHeld)
{
	if (bKeyHeld || !bKeyPressed)
		return;

	gBeepToPlay = BEEP_1KHZ_60MS_OPTIONAL;

	if (!gCssBackgroundScan)
	{
		/* Backlight related menus set full brightness. Set it back to the configured value,
		   just in case we are exiting from one of them. */
		BACKLIGHT_TurnOn();

		if (gIsInSubMenu)
		{
			gAskForConfirmation = 0;
			gIsInSubMenu        = false;
			gInputBoxIndex      = 0;
			gFlagRefreshSetting = true;

			#ifdef ENABLE_VOICE
				gAnotherVoiceID = VOICE_ID_CANCEL;
			#endif

			// ***********************

			gRequestDisplayScreen = DISPLAY_MENU;
			return;
		}

        #ifdef ENABLE_ARDF
            if ( gSetting_ARDFEnable )
            {
                gRequestDisplayScreen = DISPLAY_ARDF;
            }
			else
			{
				gRequestDisplayScreen = DISPLAY_MAIN;
			}
        #else
            gRequestDisplayScreen = DISPLAY_MAIN;
        #endif

        #ifdef ENABLE_VOICE
            COMMON_PlayCurrentVfoVoice();
        #endif
		
		if (gEeprom.BACKLIGHT_TIME == 0) // backlight set to always off
		{
			BACKLIGHT_TurnOff();	// turn the backlight OFF
		}
	}
	else
	{
		MENU_StopCssScan();

		#ifdef ENABLE_VOICE
			gAnotherVoiceID   = VOICE_ID_SCANNING_STOP;
		#endif

		gRequestDisplayScreen = DISPLAY_MENU;
	}

	gPttWasReleased = true;
}

static void MENU_Key_MENU(const bool bKeyPressed, const bool bKeyHeld)
{
	if (bKeyHeld || !bKeyPressed)
		return;

	gBeepToPlay           = BEEP_1KHZ_60MS_OPTIONAL;
	gRequestDisplayScreen = DISPLAY_MENU;

	if (!gIsInSubMenu)
	{
#ifdef ENABLE_DTMF_CALLING
        if (UI_MENU_GetCurrentMenuId() == MENU_ANI_ID)
            return;
#endif
		#if 1
			if (UI_MENU_GetCurrentMenuId() == MENU_DEL_CH || UI_MENU_GetCurrentMenuId() == MENU_MEM_NAME)
				if (!RADIO_CheckValidChannel(gSubMenuSelection, false, 0))
					return;  // invalid channel
		#endif

		gAskForConfirmation = 0;
		gIsInSubMenu        = true;

//		if (UI_MENU_GetCurrentMenuId() != MENU_D_LIST)
		{
			gInputBoxIndex      = 0;
			edit_index          = -1;
		}

		// Play the current value directly (skip menu title — user already heard it)
		#ifdef ENABLE_VOICE
			MENU_ShowCurrentSetting();
			MENU_ForceDisplayUpdate();
			MENU_PlayValueVoice(UI_MENU_GetCurrentMenuId(), gSubMenuSelection);
		#endif

		return;
	}

	if (UI_MENU_GetCurrentMenuId() == MENU_MEM_NAME)
	{
		if (edit_index < 0)
		{	// enter channel name edit mode
			if (!RADIO_CheckValidChannel(gSubMenuSelection, false, 0))
				return;

			SETTINGS_FetchChannelName(edit, gSubMenuSelection);

			// pad the channel name out with '_'
			edit_index = strlen(edit);
			while (edit_index < 10)
				edit[edit_index++] = '_';
			edit[edit_index] = 0;
			edit_index = 0;  // 'edit_index' is going to be used as the cursor position

			// make a copy so we can test for change when exiting the menu item
			memcpy(edit_original, edit, sizeof(edit_original));

			return;
		}
		else
		if (edit_index >= 0 && edit_index < 10)
		{	// editing the channel name characters

			if (++edit_index < 10)
				return;	// next char

			// exit
			gFlagAcceptSetting  = false;
			gAskForConfirmation = 0;
			if (memcmp(edit_original, edit, sizeof(edit_original)) == 0) {
				// no change - drop it
				gIsInSubMenu = false;
			}
		}
	}

	// exiting the sub menu

	if (gIsInSubMenu)
	{
		if (UI_MENU_GetCurrentMenuId() == MENU_RESET  ||
			UI_MENU_GetCurrentMenuId() == MENU_MEM_CH ||
			UI_MENU_GetCurrentMenuId() == MENU_DEL_CH ||
			UI_MENU_GetCurrentMenuId() == MENU_MEM_NAME)
		{
			switch (gAskForConfirmation)
			{
				case 0:
					gAskForConfirmation = 1;
					break;

				case 1:
					gAskForConfirmation = 2;

					UI_DisplayMenu();

					if (UI_MENU_GetCurrentMenuId() == MENU_RESET)
					{
						#ifdef ENABLE_VOICE
							AUDIO_SetVoiceID(0, VOICE_ID_CONFIRM);
							AUDIO_PlaySingleVoice(true);
						#endif

						MENU_AcceptSetting();

						#if defined(ENABLE_OVERLAY)
							overlay_FLASH_RebootToBootloader();
						#else
							NVIC_SystemReset();
						#endif
					}

					gFlagAcceptSetting  = true;
					gIsInSubMenu        = false;
					gAskForConfirmation = 0;
			}
		}
		else
		{
			gFlagAcceptSetting = true;
			gIsInSubMenu       = false;
		}
	}

	SCANNER_Stop();

	#ifdef ENABLE_VOICE
		gAnotherVoiceID = VOICE_ID_CONFIRM;
	#endif

	gInputBoxIndex = 0;
}

static void MENU_Key_STAR(const bool bKeyPressed, const bool bKeyHeld)
{
	if (bKeyHeld || !bKeyPressed)
		return;

	gBeepToPlay = BEEP_1KHZ_60MS_OPTIONAL;

	if (UI_MENU_GetCurrentMenuId() == MENU_MEM_NAME && edit_index >= 0)
	{	// currently editing the channel name

		if (edit_index < 10)
		{
			edit[edit_index] = '-';

			if (++edit_index >= 10)
			{	// exit edit
				gFlagAcceptSetting  = false;
				gAskForConfirmation = 1;
			}

			gRequestDisplayScreen = DISPLAY_MENU;
		}

		return;
	}

	RADIO_SelectVfos();

	#ifdef ENABLE_NOAA
		if (!IS_NOAA_CHANNEL(gRxVfo->CHANNEL_SAVE) && gRxVfo->Modulation == MODULATION_FM)
	#else
		if (gRxVfo->Modulation ==  MODULATION_FM)
	#endif
	{
		if ((UI_MENU_GetCurrentMenuId() == MENU_R_CTCS || UI_MENU_GetCurrentMenuId() == MENU_R_DCS) && gIsInSubMenu)
		{	// scan CTCSS or DCS to find the tone/code of the incoming signal
			if (!SCANNER_IsScanning())
				MENU_StartCssScan();
			else
				MENU_StopCssScan();
		}

		gPttWasReleased = true;
		return;
	}

	gBeepToPlay = BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL;
}

static void MENU_Key_UP_DOWN(bool bKeyPressed, bool bKeyHeld, int8_t Direction)
{
	uint8_t VFO;
	uint8_t Channel;
	bool    bCheckScanList;

	if (UI_MENU_GetCurrentMenuId() == MENU_MEM_NAME && gIsInSubMenu && edit_index >= 0)
	{	// change the character
		if (bKeyPressed && edit_index < 10 && Direction != 0)
		{
			const char   unwanted[] = "$%&!\"':;?^`|{}";
			char         c          = edit[edit_index] + Direction;
			unsigned int i          = 0;
			while (i < sizeof(unwanted) && c >= 32 && c <= 126)
			{
				if (c == unwanted[i++])
				{	// choose next character
					c += Direction;
					i = 0;
				}
			}
			edit[edit_index] = (c < 32) ? 126 : (c > 126) ? 32 : c;

			gRequestDisplayScreen = DISPLAY_MENU;
		}
		return;
	}

	if (!bKeyHeld)
	{
		if (!bKeyPressed)
			return;

		gBeepToPlay = BEEP_1KHZ_60MS_OPTIONAL;

		gInputBoxIndex = 0;
	}
	else if (!bKeyPressed)
		return;

	if (SCANNER_IsScanning())
		return;

	if (!gIsInSubMenu) {
#ifdef ENABLE_VOICE
		MENU_StopVoicePlayback();
#endif
		gMenuCursor = NUMBER_AddWithWraparound(gMenuCursor, -Direction, 0, gMenuListCount - 1);

		gFlagRefreshSetting = true;

		gRequestDisplayScreen = DISPLAY_MENU;

		#ifdef ENABLE_VOICE
			MENU_ForceDisplayUpdate();
			MENU_PlayCurrentMenuVoice();

			// if morse was interrupted by a navigation key, advance and re-announce
			while (gMorseAbortKey == KEY_UP || gMorseAbortKey == KEY_DOWN
			       || gMorseAbortKey == KEY_SIDE1 || gMorseAbortKey == KEY_SIDE2)
			{
				Direction = (gMorseAbortKey == KEY_UP || gMorseAbortKey == KEY_SIDE1) ? 1 : -1;
				gMorseAbortKey = KEY_INVALID;

				// wait for key release before advancing
				while (KEYBOARD_Poll() != KEY_INVALID)
					SYSTEM_DelayMs(10);

				gMenuCursor = NUMBER_AddWithWraparound(gMenuCursor, -Direction, 0, gMenuListCount - 1);
				gFlagRefreshSetting   = true;
				gRequestDisplayScreen = DISPLAY_MENU;
				MENU_ForceDisplayUpdate();
				MENU_PlayCurrentMenuVoice();
			}
		#endif

		if (UI_MENU_GetCurrentMenuId() != MENU_ABR
			&& UI_MENU_GetCurrentMenuId() != MENU_ABR_MIN
			&& UI_MENU_GetCurrentMenuId() != MENU_ABR_MAX
			&& gEeprom.BACKLIGHT_TIME == 0) // backlight always off and not in the backlight menu
		{
			BACKLIGHT_TurnOff();
		}

		return;
	}

	if (UI_MENU_GetCurrentMenuId() == MENU_MORSE_SPEED) {
		int32_t next = gSubMenuSelection + (Direction * 5);

		if (next > 70)
			next = 15;
		else if (next < 15)
			next = 70;

		gSubMenuSelection = next;
		gRequestDisplayScreen = DISPLAY_MENU;
#ifdef ENABLE_VOICE
		MENU_StopVoicePlayback();
		MENU_ForceDisplayUpdate();
		MENU_PlayValueVoice(UI_MENU_GetCurrentMenuId(), gSubMenuSelection);
#endif
		return;
	}


#ifdef ENABLE_ARDF

	if (UI_MENU_GetCurrentMenuId() == MENU_ARDF_FOXDURATION) 
	{
		int32_t duration = (Direction * 10) + gSubMenuSelection;
		if (duration <= 99999) 
		{
			if (duration < 100)
				duration = 99999;
		}
		else
		{
			duration = 100;
		}

		gSubMenuSelection     = duration;
		gRequestDisplayScreen = DISPLAY_MENU;
#ifdef ENABLE_VOICE
		MENU_ForceDisplayUpdate();
		MENU_PlayValueVoice(UI_MENU_GetCurrentMenuId(), gSubMenuSelection);
#endif
		return;
	}

	if (UI_MENU_GetCurrentMenuId() == MENU_ARDF_CLOCK_CORR ) 
	{
		int16_t correction = Direction + gSubMenuSelection;
		if (correction <= 500) 
		{
			if (correction < -500)
				correction = -500;
		}
		else
		{
			correction = 500;
		}

		gSubMenuSelection     = correction;
		gRequestDisplayScreen = DISPLAY_MENU;
#ifdef ENABLE_VOICE
		MENU_ForceDisplayUpdate();
		MENU_PlayValueVoice(UI_MENU_GetCurrentMenuId(), gSubMenuSelection);
#endif
		return;
	}

#endif

	VFO = 0;

	switch (UI_MENU_GetCurrentMenuId())
	{
		case MENU_DEL_CH:
		case MENU_MEM_NAME:
			bCheckScanList = false;
			break;

		case MENU_SLIST2:
			VFO = 1;
			[[fallthrough]];
		case MENU_SLIST1:
			bCheckScanList = true;
			break;

		default:
			MENU_ClampSelection(Direction);
			gRequestDisplayScreen = DISPLAY_MENU;
#ifdef ENABLE_VOICE
			MENU_ForceDisplayUpdate();
			MENU_PlayValueVoice(UI_MENU_GetCurrentMenuId(), gSubMenuSelection);
#endif
			return;
	}

	Channel = RADIO_FindNextChannel(gSubMenuSelection + Direction, Direction, bCheckScanList, VFO);
	if (Channel != 0xFF)
		gSubMenuSelection = Channel;

	gRequestDisplayScreen = DISPLAY_MENU;
#ifdef ENABLE_VOICE
	MENU_ForceDisplayUpdate();
	MENU_PlayValueVoice(UI_MENU_GetCurrentMenuId(), gSubMenuSelection);
#endif
}

void MENU_PlayMorseNumber(uint16_t number)
{
	char buf[6];
	uint8_t pos = 0;

	if (number == 0) {
		buf[0] = '0';
		buf[1] = '\0';
	} else {
		char tmp[6];
		while (number > 0 && pos < 5) {
			tmp[pos++] = '0' + (number % 10);
			number /= 10;
		}
		for (uint8_t i = 0; i < pos; i++)
			buf[i] = tmp[pos - 1 - i];
		buf[pos] = '\0';
	}

	gMorseAbortKey = KEY_INVALID;
	MENU_WaitForKeyReleaseBeforeMorse();
	gMorseAbortKey = KEY_INVALID;
	MENU_PlayMorseString(buf);
}

void MENU_ProcessKeys(KEY_Code_t Key, bool bKeyPressed, bool bKeyHeld)
{
	switch (Key) {
		case KEY_0...KEY_9:
			MENU_Key_0_to_9(Key, bKeyPressed, bKeyHeld);
			break;
		case KEY_MENU:
			MENU_Key_MENU(bKeyPressed, bKeyHeld);
			break;
		case KEY_UP:
		case KEY_SIDE1:  // side key 1 = UP for one-handed operation
			MENU_Key_UP_DOWN(bKeyPressed, bKeyHeld,  1);
			break;
		case KEY_DOWN:
		case KEY_SIDE2:  // side key 2 = DOWN for one-handed operation
			MENU_Key_UP_DOWN(bKeyPressed, bKeyHeld, -1);
			break;
		case KEY_EXIT:
			MENU_Key_EXIT(bKeyPressed, bKeyHeld);
			break;
		case KEY_STAR:
			MENU_Key_STAR(bKeyPressed, bKeyHeld);
			break;
		case KEY_F:
			if (UI_MENU_GetCurrentMenuId() == MENU_MEM_NAME && edit_index >= 0)
			{	// currently editing the channel name
				if (!bKeyHeld && bKeyPressed)
				{
					gBeepToPlay = BEEP_1KHZ_60MS_OPTIONAL;
					if (edit_index < 10)
					{
						edit[edit_index] = ' ';
						if (++edit_index >= 10)
						{	// exit edit
							gFlagAcceptSetting  = false;
							gAskForConfirmation = 1;
						}
						gRequestDisplayScreen = DISPLAY_MENU;
					}
				}
				break;
			}

			GENERIC_Key_F(bKeyPressed, bKeyHeld);
			break;
		case KEY_PTT:
			MENU_Key_MENU(bKeyPressed, bKeyHeld);  // PTT confirms menu entries (one-handed operation)
			break;
		default:
			if (!bKeyHeld && bKeyPressed)
				gBeepToPlay = BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL;
			break;
	}

	if (gScreenToDisplay == DISPLAY_MENU)
	{
		if (UI_MENU_GetCurrentMenuId() == MENU_VOL ||
			#ifdef ENABLE_F_CAL_MENU
				UI_MENU_GetCurrentMenuId() == MENU_F_CALI ||
		    #endif
			UI_MENU_GetCurrentMenuId() == MENU_BATCAL)
		{
			gMenuCountdown = menu_timeout_long_500ms;
		}
		else
		{
			gMenuCountdown = menu_timeout_500ms;
		}
	}
}
