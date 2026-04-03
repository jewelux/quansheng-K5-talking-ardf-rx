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

#include "app/action.h"
#include "app/app.h"
#include "app/chFrScanner.h"
#include "app/common.h"
#ifdef ENABLE_FMRADIO
	#include "app/fm.h"
#endif
#include "app/generic.h"
#include "app/main.h"
#include "app/scanner.h"

#ifdef ENABLE_SPECTRUM
#include "app/spectrum.h"
#endif

#include "audio.h"
#include "board.h"
#include "driver/bk4819.h"
#include "driver/system.h"
#include "dtmf.h"
#include "frequencies.h"
#include "misc.h"
#include "radio.h"
#include "settings.h"
#include "ui/inputbox.h"
#include "ui/ui.h"
#include <stdlib.h>

#ifdef ENABLE_ARDF
#include "app/ardf.h"
#endif

void toggle_chan_scanlist(void)
{	// toggle the selected channels scanlist setting

	if (SCANNER_IsScanning())
		return;

	if(!IS_MR_CHANNEL(gTxVfo->CHANNEL_SAVE)) {
#ifdef ENABLE_SCAN_RANGES
		gScanRangeStart = gScanRangeStart ? 0 : gTxVfo->pRX->Frequency;
		gScanRangeStop = gEeprom.VfoInfo[!gEeprom.TX_VFO].freq_config_RX.Frequency;
		if(gScanRangeStart > gScanRangeStop)
			SWAP(gScanRangeStart, gScanRangeStop);
#endif
		return;
	}
	
	if (gTxVfo->SCANLIST1_PARTICIPATION ^ gTxVfo->SCANLIST2_PARTICIPATION){
		gTxVfo->SCANLIST2_PARTICIPATION = gTxVfo->SCANLIST1_PARTICIPATION;
	} else {
		gTxVfo->SCANLIST1_PARTICIPATION = !gTxVfo->SCANLIST1_PARTICIPATION;
	}

	SETTINGS_UpdateChannel(gTxVfo->CHANNEL_SAVE, gTxVfo, true);

	gVfoConfigureMode = VFO_CONFIGURE;
	gFlagResetVfos    = true;
}

#ifdef ENABLE_VOICE
static void MAIN_QueueFixedDigitsVoice(uint16_t value, uint8_t digits)
{
	uint16_t divisor = 1U;
	uint8_t index = 0;

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
	AUDIO_PlaySingleVoice(false);
}

static void MAIN_StopVoicePlayback(void)
{
	gVoiceWriteIndex = 0;
	gVoiceReadIndex  = 0;
	gFlagPlayQueuedVoice = false;
	gCountdownToPlayNextVoice_10ms = 0;
	gAnotherVoiceID  = VOICE_ID_INVALID;
}

static void MAIN_PlayModulationCodeVoice(const ModulationMode_t modulation)
{
	uint8_t beeps = 1U;

	if (modulation == MODULATION_AM)
		beeps = 2U;
	else if (modulation == MODULATION_USB)
		beeps = 3U;

	for (uint8_t i = 0; i < beeps; i++)
	{
		AUDIO_PlayBeep(BEEP_1KHZ_60MS_OPTIONAL);
		SYSTEM_DelayMs(80);
	}
}

static void MAIN_PlayFrequencyVoice(const uint32_t frequency)
{
	const uint32_t spoken_frequency = frequency / 100U;
	uint8_t index = AUDIO_SetDigitVoice(0, (uint16_t)(spoken_frequency / 1000U));

	if ((spoken_frequency % 1000U) != 0U)
		index = AUDIO_SetDigitVoice(index, (uint16_t)(spoken_frequency % 1000U));

	(void)index;
	AUDIO_PlaySingleVoice(true);
}

static void MAIN_PlayMainHelpVoice(void)
{
	AUDIO_SetVoiceID(0, VOICE_ID_MENU);
	AUDIO_PlaySingleVoice(true);
	SYSTEM_DelayMs(180);

	// 1 beep: EXIT short status, 2 beeps: EXIT long current channel/frequency,
	// 3 beeps: MENU opens menu, 4 beeps: UP/DOWN change,
	// 5 beeps: VFO/MR switch, 6 beeps: direct channel/frequency entry with digits,
	// 7 beeps: F/function key
	for (uint8_t group = 1; group <= 7; group++)
	{
		AUDIO_SetDigitVoice(0, group);
		AUDIO_PlaySingleVoice(true);
		SYSTEM_DelayMs(120);

		if (group <= 2)
		{
			AUDIO_SetVoiceID(0, VOICE_ID_CANCEL);
			AUDIO_PlaySingleVoice(true);
			SYSTEM_DelayMs(120);
		}
		else if (group == 3)
		{
			AUDIO_SetVoiceID(0, VOICE_ID_MENU);
			AUDIO_PlaySingleVoice(true);
			SYSTEM_DelayMs(120);
		}

		if (group == 2)
		{
			AUDIO_SetVoiceID(0, VOICE_ID_CHANNEL_MODE);
			AUDIO_PlaySingleVoice(true);
			SYSTEM_DelayMs(120);
		}
		else if (group == 5)
		{
			AUDIO_SetVoiceID(0, VOICE_ID_CHANNEL_MODE);
			AUDIO_PlaySingleVoice(true);
			SYSTEM_DelayMs(120);
			AUDIO_SetVoiceID(0, VOICE_ID_FREQUENCY_MODE);
			AUDIO_PlaySingleVoice(true);
			SYSTEM_DelayMs(120);
		}
		else if (group == 6)
		{
			AUDIO_SetVoiceID(0, VOICE_ID_MEMORY_CHANNEL);
			AUDIO_PlaySingleVoice(true);
			SYSTEM_DelayMs(120);
			AUDIO_SetVoiceID(0, VOICE_ID_FREQUENCY_MODE);
			AUDIO_PlaySingleVoice(true);
			SYSTEM_DelayMs(120);
		}
		else if (group == 7)
		{
			AUDIO_SetVoiceID(0, VOICE_ID_FUNCTION);
			AUDIO_PlaySingleVoice(true);
			SYSTEM_DelayMs(120);
		}
		for (uint8_t i = 0; i < group; i++)
		{
			AUDIO_PlayBeep(BEEP_1KHZ_60MS_OPTIONAL);
			SYSTEM_DelayMs(80);
		}

		SYSTEM_DelayMs(220);
	}
}

static void MAIN_PlayCompactMainStatusVoice(void)
{
	uint8_t index = 0;

	if (IS_MR_CHANNEL(gTxVfo->CHANNEL_SAVE))
	{
		AUDIO_SetVoiceID(index++, VOICE_ID_CHANNEL_MODE);
		index = AUDIO_SetDigitVoice(index, (uint16_t)(gTxVfo->CHANNEL_SAVE + 1U));
	}
	else
	{
		const uint32_t spoken_frequency = gTxVfo->pRX->Frequency / 100U;

		AUDIO_SetVoiceID(index++, VOICE_ID_FREQUENCY_MODE);
		index = AUDIO_SetDigitVoice(index, (uint16_t)(spoken_frequency / 1000U));
		if ((spoken_frequency % 1000U) != 0U)
			index = AUDIO_SetDigitVoice(index, (uint16_t)(spoken_frequency % 1000U));
	}

	AUDIO_SetVoiceID(index++, VOICE_ID_FUNCTION);
	AUDIO_SetVoiceID(index, gWasFKeyPressed ? VOICE_ID_ON : VOICE_ID_OFF);
	AUDIO_PlaySingleVoice(true);
}
#endif

#if defined(ENABLE_ARDF) && defined(ENABLE_VOICE)
static void MAIN_PlayArdfGainVoice(void)
{
	MAIN_QueueFixedDigitsVoice(ARDF_Get_GainIndex(gEeprom.RX_VFO), 2);
}

static void MAIN_PlayArdfStatusVoice(void)
{
	const int32_t rest_time_s = ARDF_GetRestTime_s();
	uint8_t index;

	if (gARDFNumFoxes == 0 || rest_time_s < 0)
	{
		gAnotherVoiceID = VOICE_ID_CANCEL;
		return;
	}

	index = AUDIO_SetDigitVoice(0, (uint16_t)(gARDFActiveFox + 1));
	AUDIO_SetDigitVoice(index, (uint16_t)rest_time_s);
	AUDIO_PlaySingleVoice(true);
}

static void MAIN_PlayArdfFrequencyStatus(void)
{
	const uint32_t frequency = gRxVfo->freq_config_RX.Frequency;
	const uint8_t modulation_beeps = (uint8_t)gRxVfo->Modulation + 1U;

	MAIN_PlayFrequencyVoice(frequency);

	for (uint8_t i = 0; i < modulation_beeps; i++)
	{
		AUDIO_PlayBeep(BEEP_1KHZ_60MS_OPTIONAL);
	}
}

static void MAIN_PlayArdfHelpVoice(void)
{
	AUDIO_SetVoiceID(0, VOICE_ID_MENU);
	AUDIO_PlaySingleVoice(true);
	SYSTEM_DelayMs(180);

	// 1 beep: PTT snapshot, 2 beeps: EXIT short status, 3 beeps: EXIT long freq/mode, 4 beeps: UP/DOWN gain
	for (uint8_t group = 1; group <= 4; group++)
	{
		AUDIO_SetDigitVoice(0, group);
		AUDIO_PlaySingleVoice(true);
		SYSTEM_DelayMs(120);

		if (group == 2 || group == 3)
		{
			AUDIO_SetVoiceID(0, VOICE_ID_CANCEL);
			AUDIO_PlaySingleVoice(true);
			SYSTEM_DelayMs(120);
		}

		if (group == 3)
		{
			AUDIO_SetVoiceID(0, VOICE_ID_FREQUENCY_MODE);
			AUDIO_PlaySingleVoice(true);
			SYSTEM_DelayMs(120);
		}

		for (uint8_t i = 0; i < group; i++)
		{
			AUDIO_PlayBeep(BEEP_1KHZ_60MS_OPTIONAL);
			SYSTEM_DelayMs(80);
		}

		SYSTEM_DelayMs(220);
	}
}
#endif



static void processFKeyFunction(const KEY_Code_t Key, const bool beep)
{
	uint8_t Vfo = gEeprom.TX_VFO;

#ifdef ENABLE_ARDF
	int8_t Direction = 1;
	uint8_t Channel = gEeprom.ScreenChannel[Vfo];
	uint8_t Next;
#endif

	if (gScreenToDisplay == DISPLAY_MENU) {
		gBeepToPlay = BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL;
		return;
	}
	
	gBeepToPlay = BEEP_1KHZ_60MS_OPTIONAL;

	switch (Key) {
		case KEY_0:
			#ifdef ENABLE_FMRADIO
				ACTION_FM();
			#endif
			break;

		case KEY_1:
			if (!IS_FREQ_CHANNEL(gTxVfo->CHANNEL_SAVE)) {
				gWasFKeyPressed = false;
				gUpdateStatus   = true;
				gBeepToPlay     = BEEP_1KHZ_60MS_OPTIONAL;

#ifdef ENABLE_COPY_CHAN_TO_VFO
				if (!gEeprom.VFO_OPEN || gCssBackgroundScan) {
					gBeepToPlay = BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL;
					return;
				}

				if (gScanStateDir != SCAN_OFF) {
					if (gCurrentFunction != FUNCTION_INCOMING ||
						gRxReceptionMode == RX_MODE_NONE      ||
						gScanPauseDelayIn_10ms == 0)
					{	// scan is running (not paused)
						return;
					}
				}

				const uint8_t vfo = gEeprom.TX_VFO;

				if (IS_MR_CHANNEL(gEeprom.ScreenChannel[vfo]))
				{	// copy channel to VFO, then swap to the VFO

					gEeprom.ScreenChannel[vfo] = FREQ_CHANNEL_FIRST + gEeprom.VfoInfo[vfo].Band;
					gEeprom.VfoInfo[vfo].CHANNEL_SAVE = gEeprom.ScreenChannel[vfo];

					RADIO_SelectVfos();
					RADIO_ApplyOffset(gRxVfo);
					RADIO_ConfigureSquelchAndOutputPower(gRxVfo);
					RADIO_SetupRegisters(true);

					//SETTINGS_SaveChannel(channel, gEeprom.RX_VFO, gRxVfo, 1);

					gUpdateDisplay = true;
				}
#endif
				return;
			}

#ifdef ENABLE_WIDE_RX
			if(gTxVfo->Band == BAND7_470MHz && gTxVfo->pRX->Frequency < _1GHz_in_KHz) {
					gTxVfo->pRX->Frequency = _1GHz_in_KHz;
					return;
			}
#endif
			gTxVfo->Band += 1;

			if (gTxVfo->Band == BAND5_350MHz && !gSetting_350EN) {
				// skip if not enabled
				gTxVfo->Band += 1;
			} else if (gTxVfo->Band >= BAND_N_ELEM){
				// go arround if overflowed
				gTxVfo->Band = BAND1_50MHz;
			}

			gEeprom.ScreenChannel[Vfo] = FREQ_CHANNEL_FIRST + gTxVfo->Band;
			gEeprom.FreqChannel[Vfo]   = FREQ_CHANNEL_FIRST + gTxVfo->Band;

			gRequestSaveVFO            = true;
			gVfoConfigureMode          = VFO_CONFIGURE_RELOAD;

			gRequestDisplayScreen      = DISPLAY_MAIN;

			if (beep)
				gBeepToPlay = BEEP_1KHZ_60MS_OPTIONAL;

			break;

		case KEY_2:
			COMMON_SwitchVFOs();

			if (beep)
				gBeepToPlay = BEEP_1KHZ_60MS_OPTIONAL;
			break;

		case KEY_3:
			COMMON_SwitchVFOMode();

			if (beep)
				gBeepToPlay = BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL;

			break;

		case KEY_4:
			gRequestSaveChannel = 1;
			gTxVfo->Modulation = (gTxVfo->Modulation == MODULATION_AM) ? MODULATION_FM : MODULATION_AM;
			RADIO_SetModulation(gTxVfo->Modulation);
#ifdef ENABLE_VOICE
			MAIN_PlayModulationCodeVoice(gTxVfo->Modulation);
#endif
			break;

		case KEY_5:
		case KEY_6:
		case KEY_7:
		case KEY_8:
		case KEY_9:
			gWasFKeyPressed = false;
			gUpdateStatus   = true;
			if (beep)
				gBeepToPlay = BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL;
#ifdef ENABLE_VOICE
			gAnotherVoiceID = VOICE_ID_CANCEL;
#endif
			break;

#ifdef ENABLE_ARDF

		case KEY_DOWN:
			Direction = -1;
			[[fallthrough]];
						
		case KEY_UP:

                        // change frequency or station

                        // stop frequency mistuning if active
                        ARDF_StopFreqMistune();

			if (IS_FREQ_CHANNEL(Channel)) { // step/down in frequency

				const uint32_t frequency = APP_SetFrequencyByStep(gTxVfo, Direction);

				if (RX_freq_check(frequency) < 0) { // frequency not allowed
					gBeepToPlay = BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL;
					return;
				}
				gTxVfo->freq_config_RX.Frequency = frequency;
				BK4819_SetFrequency(frequency);
				gRequestSaveChannel = 1;
				return;
			}

			Next = RADIO_FindNextChannel(Channel + Direction, Direction, false, 0);
			if (Next == 0xFF)
				return;
			if (Channel == Next)
				return;
			gEeprom.MrChannel[gEeprom.TX_VFO] = Next;
			gEeprom.ScreenChannel[gEeprom.TX_VFO] = Next;

	                gRequestSaveVFO   = true;
        	        gVfoConfigureMode = VFO_CONFIGURE_RELOAD;

			break;

#endif
			
		default:
			gUpdateStatus   = true;
			gWasFKeyPressed = false;

			if (beep)
				gBeepToPlay = BEEP_1KHZ_60MS_OPTIONAL;
			break;
	}
}



static void MAIN_Key_DIGITS(KEY_Code_t Key, bool bKeyPressed, bool bKeyHeld)
{
	if (bKeyHeld) { // key held down
		if (bKeyPressed) {
			if ( (gScreenToDisplay == DISPLAY_MAIN) 
#ifdef ENABLE_ARDF
                             || (gScreenToDisplay == DISPLAY_ARDF)
#endif			
			)
			{
				if (gInputBoxIndex > 0) { // delete any inputted chars
					gInputBoxIndex        = 0;
					gRequestDisplayScreen = DISPLAY_MAIN;
#ifdef ENABLE_ARDF
					if ( gScreenToDisplay == DISPLAY_ARDF )
						gRequestDisplayScreen = DISPLAY_ARDF;
#endif
				}

				gWasFKeyPressed = false;
				gUpdateStatus   = true;

				processFKeyFunction(Key, false);
			}
		}
		return;
	}

	if (bKeyPressed)
	{	// key is pressed
		gBeepToPlay = BEEP_1KHZ_60MS_OPTIONAL;  // beep when key is pressed
		return;                                 // don't use the key till it's released
	}

	if (!gWasFKeyPressed) { // F-key wasn't pressed
		const uint8_t Vfo = gEeprom.TX_VFO;
		gKeyInputCountdown = key_input_timeout_500ms;
		INPUTBOX_Append(Key);
		gRequestDisplayScreen = DISPLAY_MAIN;

		#ifdef ENABLE_ARDF
		if ( gScreenToDisplay == DISPLAY_ARDF )
			gRequestDisplayScreen = DISPLAY_ARDF;
		#endif

		if (IS_MR_CHANNEL(gTxVfo->CHANNEL_SAVE)) { // user is entering channel number

			if (gInputBoxIndex != 3) {
				gRequestDisplayScreen = DISPLAY_MAIN;

				#ifdef ENABLE_ARDF
				if ( gScreenToDisplay == DISPLAY_ARDF )
					gRequestDisplayScreen = DISPLAY_ARDF;
				#endif

				return;
			}

			gInputBoxIndex = 0;

			const uint16_t Channel = ((gInputBox[0] * 100) + (gInputBox[1] * 10) + gInputBox[2]) - 1;

			if (!RADIO_CheckValidChannel(Channel, false, 0)) {
				gBeepToPlay = BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL;
				return;
			}

			#ifdef ENABLE_ARDF
				ARDF_StopFreqMistune();
			#endif

			#ifdef ENABLE_VOICE
				gAnotherVoiceID        = (VOICE_ID_t)Key;
			#endif

			gEeprom.MrChannel[Vfo]     = (uint8_t)Channel;
			gEeprom.ScreenChannel[Vfo] = (uint8_t)Channel;
			gRequestSaveVFO            = true;
			gVfoConfigureMode          = VFO_CONFIGURE_RELOAD;
#ifdef ENABLE_VOICE
			COMMON_PlayChannelVoice((uint8_t)Channel, gTxVfo->pRX->Frequency);
#endif

			return;
		}

//		#ifdef ENABLE_NOAA
//			if (!IS_NOAA_CHANNEL(gTxVfo->CHANNEL_SAVE))
//		#endif
		if (IS_FREQ_CHANNEL(gTxVfo->CHANNEL_SAVE))
		{	// user is entering a frequency

			bool isGigaF = gTxVfo->pRX->Frequency >= _1GHz_in_KHz;
			if (gInputBoxIndex < 6 + isGigaF) {
				return;
			}

			gInputBoxIndex = 0;
			uint32_t Frequency = StrToUL(INPUTBOX_GetAscii()) * 100;

			#ifdef ENABLE_ARDF
				ARDF_StopFreqMistune();
			#endif

			// clamp the frequency entered to some valid value
			if (Frequency < frequencyBandTable[0].lower) {
				Frequency = frequencyBandTable[0].lower;
			}
			else if (Frequency >= BX4819_band1.upper && Frequency < BX4819_band2.lower) {
				const uint32_t center = (BX4819_band1.upper + BX4819_band2.lower) / 2;
				Frequency = (Frequency < center) ? BX4819_band1.upper : BX4819_band2.lower;
			}
			else if (Frequency > frequencyBandTable[BAND_N_ELEM - 1].upper) {
				Frequency = frequencyBandTable[BAND_N_ELEM - 1].upper;
			}

			const FREQUENCY_Band_t band = FREQUENCY_GetBand(Frequency);

			if (gTxVfo->Band != band) {
				gTxVfo->Band               = band;
				gEeprom.ScreenChannel[Vfo] = band + FREQ_CHANNEL_FIRST;
				gEeprom.FreqChannel[Vfo]   = band + FREQ_CHANNEL_FIRST;

				SETTINGS_SaveVfoIndices();

				RADIO_ConfigureChannel(Vfo, VFO_CONFIGURE_RELOAD);
			}

			if (Frequency >= BX4819_band1.upper && Frequency < BX4819_band2.lower)
			{	// clamp the frequency to the limit
				const uint32_t center = (BX4819_band1.upper + BX4819_band2.lower) / 2;
				Frequency = (Frequency < center) ? BX4819_band1.upper - gTxVfo->StepFrequency : BX4819_band2.lower;
			}

			gTxVfo->freq_config_RX.Frequency = Frequency;
#ifdef ENABLE_VOICE
			MAIN_PlayFrequencyVoice(Frequency);
#endif

			gRequestSaveChannel = 1;
			return;

		}
		#ifdef ENABLE_NOAA
			else
			if (IS_NOAA_CHANNEL(gTxVfo->CHANNEL_SAVE))
			{	// user is entering NOAA channel
				if (gInputBoxIndex != 2) {
					#ifdef ENABLE_VOICE
						gAnotherVoiceID   = (VOICE_ID_t)Key;
					#endif
					gRequestDisplayScreen = DISPLAY_MAIN;
					return;
				}

				gInputBoxIndex = 0;

				uint8_t Channel = (gInputBox[0] * 10) + gInputBox[1];
				if (Channel >= 1 && Channel <= ARRAY_SIZE(NoaaFrequencyTable)) {
					Channel                   += NOAA_CHANNEL_FIRST;
					#ifdef ENABLE_VOICE
						gAnotherVoiceID        = (VOICE_ID_t)Key;
					#endif
					gEeprom.NoaaChannel[Vfo]   = Channel;
					gEeprom.ScreenChannel[Vfo] = Channel;
					gRequestSaveVFO            = true;
					gVfoConfigureMode          = VFO_CONFIGURE_RELOAD;
					return;
				}
			}
		#endif

		gRequestDisplayScreen = DISPLAY_MAIN;
		
		#ifdef ENABLE_ARDF
		if ( gScreenToDisplay == DISPLAY_ARDF )
			gRequestDisplayScreen = DISPLAY_ARDF;
		#endif

		gBeepToPlay           = BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL;
		return;
	}

	gWasFKeyPressed = false;
	gUpdateStatus   = true;

	processFKeyFunction(Key, true);
}



static void MAIN_Key_EXIT(bool bKeyPressed, bool bKeyHeld)
{
	if (!bKeyHeld && bKeyPressed) { // exit key pressed
		gBeepToPlay = BEEP_1KHZ_60MS_OPTIONAL;

#ifdef ENABLE_DTMF_CALLING
		if (gDTMF_CallState != DTMF_CALL_STATE_NONE && gCurrentFunction != FUNCTION_TRANSMIT)
		{	// clear CALL mode being displayed
			gDTMF_CallState = DTMF_CALL_STATE_NONE;
			gUpdateDisplay  = true;
			return;
		}
#endif

#ifdef ENABLE_FMRADIO
		if (!gFmRadioMode)
#endif
		{
			if (gScanStateDir == SCAN_OFF) {
				if (gInputBoxIndex == 0)
				{
#ifdef ENABLE_ARDF
#ifdef ENABLE_VOICE
					if (gSetting_ARDFEnable &&
						gARDFDFSimpleMode &&
						gScreenToDisplay == DISPLAY_ARDF)
					{
						MAIN_PlayArdfStatusVoice();
					}
					else if (gScreenToDisplay == DISPLAY_MAIN || gScreenToDisplay == DISPLAY_ARDF)
					{
						MAIN_PlayCompactMainStatusVoice();
					}
#endif
#endif
					return;
				}
				gInputBox[--gInputBoxIndex] = 10;

				gKeyInputCountdown = key_input_timeout_500ms;

#ifdef ENABLE_VOICE
				if (gInputBoxIndex == 0)
					gAnotherVoiceID = VOICE_ID_CANCEL;
#endif
			}
			else {
				gScanKeepResult = false;
				CHFRSCANNER_Stop();

#ifdef ENABLE_VOICE
				gAnotherVoiceID = VOICE_ID_SCANNING_STOP;
#endif
			}

			gRequestDisplayScreen = DISPLAY_MAIN;
			return;
		}

#ifdef ENABLE_FMRADIO
		ACTION_FM();
#endif
		return;
	}

	if (bKeyHeld && bKeyPressed) { // exit key held down
		if (gInputBoxIndex > 0 || gDTMF_InputBox_Index > 0 || gDTMF_InputMode)
		{	// cancel key input mode (channel/frequency entry)
			gDTMF_InputMode       = false;
			gDTMF_InputBox_Index  = 0;
			memset(gDTMF_String, 0, sizeof(gDTMF_String));
			gInputBoxIndex        = 0;
			gRequestDisplayScreen = DISPLAY_MAIN;
			gBeepToPlay           = BEEP_1KHZ_60MS_OPTIONAL;
#ifdef ENABLE_VOICE
			gAnotherVoiceID       = VOICE_ID_CANCEL;
#endif
		}
#ifdef ENABLE_ARDF
#ifdef ENABLE_VOICE
		else if (gSetting_ARDFEnable &&
				 gARDFDFSimpleMode &&
				 gScreenToDisplay == DISPLAY_ARDF)
		{
			MAIN_PlayArdfFrequencyStatus();
		}
		else
#endif
#endif
#ifdef ENABLE_VOICE
		if (gScreenToDisplay == DISPLAY_MAIN || gScreenToDisplay == DISPLAY_ARDF)
		{
			COMMON_PlayCurrentVfoVoice();
		}
#endif
	}
}



static void MAIN_Key_MENU(const bool bKeyPressed, const bool bKeyHeld)
{
	if (bKeyPressed && !bKeyHeld) // menu key pressed
		gBeepToPlay = BEEP_1KHZ_60MS_OPTIONAL;

	if (bKeyHeld) { // menu key held down (long press)
		if (bKeyPressed) { // long press MENU key

			gWasFKeyPressed = false;

			if (gScreenToDisplay == DISPLAY_MAIN) {
				if (gInputBoxIndex > 0) { // delete any inputted chars
					gInputBoxIndex        = 0;
					gRequestDisplayScreen = DISPLAY_MAIN;
				}

				gWasFKeyPressed = false;
				gUpdateStatus   = true;

#ifdef ENABLE_ARDF
#ifdef ENABLE_VOICE
				if (gSetting_ARDFEnable &&
					gARDFDFSimpleMode &&
					gEeprom.KEY_M_LONG_PRESS_ACTION == ACTION_OPT_NONE)
				{
					MAIN_PlayArdfHelpVoice();
					return;
				}
#endif
#endif
				if (gEeprom.KEY_M_LONG_PRESS_ACTION == ACTION_OPT_NONE)
				{
#ifdef ENABLE_VOICE
					MAIN_PlayMainHelpVoice();
					return;
#endif
				}
				ACTION_Handle(KEY_MENU, bKeyPressed, bKeyHeld);
			}
		}

		return;
	}

	if (!bKeyPressed && !gDTMF_InputMode) { // menu key released
		const bool bFlag = !gInputBoxIndex;
		gInputBoxIndex   = 0;

		if (bFlag) {
			if (gScanStateDir != SCAN_OFF) {
				CHFRSCANNER_Stop();
				return;
			}

			gFlagRefreshSetting = true;
			gRequestDisplayScreen = DISPLAY_MENU;
			#ifdef ENABLE_VOICE
				gAnotherVoiceID   = VOICE_ID_MENU;
			#endif
		}
		else {
			gRequestDisplayScreen = DISPLAY_MAIN;
		}
	}
}



static void MAIN_Key_STAR(bool bKeyPressed, bool bKeyHeld)
{
	if (gCurrentFunction == FUNCTION_TRANSMIT)
		return;
	
	if (gInputBoxIndex) {
		if (!bKeyHeld && bKeyPressed)
			gBeepToPlay = BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL;
		return;
	}

	if (bKeyHeld && !gWasFKeyPressed){ // long press
		if (!bKeyPressed) // released
			return; 

		gBeepToPlay = BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL;
#ifdef ENABLE_VOICE
		gAnotherVoiceID = VOICE_ID_CANCEL;
#endif
		return;
	}

	if (bKeyPressed) { // just pressed
		return;
	}
	
	// just released
	
	if (!gWasFKeyPressed) // pressed without the F-key
	{
		gBeepToPlay = BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL;
#ifdef ENABLE_VOICE
		gAnotherVoiceID = VOICE_ID_CANCEL;
#endif
	}
	else
	{	// with the F-key
		gWasFKeyPressed = false;

#ifdef ENABLE_NOAA
		if (IS_NOAA_CHANNEL(gTxVfo->CHANNEL_SAVE)) {
			gBeepToPlay = BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL;
			return;
		}				
#endif
		gBeepToPlay = BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL;
#ifdef ENABLE_VOICE
		gAnotherVoiceID = VOICE_ID_CANCEL;
#endif
	}
	
	gPttWasReleased = true;
	gUpdateStatus   = true;
}



static void MAIN_Key_UP_DOWN(bool bKeyPressed, bool bKeyHeld, int8_t Direction)
{
	uint8_t Channel = gEeprom.ScreenChannel[gEeprom.TX_VFO];

#ifdef ENABLE_ARDF

	if ( gSetting_ARDFEnable && bKeyPressed )
	{
		/* ARDF: process only short press */

		if ( gWasFKeyPressed )
		{
			/* ARDF mode: F + UP/Down: frequency step or memory change  */
			if ( Direction == 1 )
			{
		   		processFKeyFunction(KEY_UP, false);
			}
			else
			{
				processFKeyFunction(KEY_DOWN, false);
			}
			gWasFKeyPressed = 0;
   			gARDFMemModeFreqToggleCnt_s = 0;

			return;

		}

		// ARDF: adjust manual gain

#ifdef ENABLE_VOICE
		MAIN_StopVoicePlayback();
#endif

		if ( Direction == 1 )
		{
			ARDF_GainIncr();
		}
		else
		{
			ARDF_GainDecr();
		}

		ARDF_ActivateGainIndex();

#ifdef ENABLE_VOICE
		MAIN_PlayArdfGainVoice();
#endif
	
		return;
	}

#endif

	if (bKeyHeld || !bKeyPressed) { // key held or released
		if (gInputBoxIndex > 0)
			return; // leave if input box active

		if (!bKeyPressed) {
			if (!bKeyHeld || IS_FREQ_CHANNEL(Channel))
				return;
			// if released long button press and not in freq mode
#ifdef ENABLE_VOICE
			COMMON_PlayCurrentVfoVoice();
#endif
			return;
		}
	}
	else { // short pressed
		if (gInputBoxIndex > 0) {
			gBeepToPlay = BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL;
			return;
		}
		gBeepToPlay = BEEP_1KHZ_60MS_OPTIONAL;
	}

	if (gScanStateDir == SCAN_OFF) {
#ifdef ENABLE_NOAA
		if (!IS_NOAA_CHANNEL(Channel))
#endif
		{
			uint8_t Next;
			if (IS_FREQ_CHANNEL(Channel)) { // step/down in frequency

				const uint32_t frequency = APP_SetFrequencyByStep(gTxVfo, Direction);

				if (RX_freq_check(frequency) < 0) { // frequency not allowed
					gBeepToPlay = BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL;
					return;
				}
				gTxVfo->freq_config_RX.Frequency = frequency;
				BK4819_SetFrequency(frequency);
				BK4819_RX_TurnOn();
				gRequestSaveChannel = 1;
#ifdef ENABLE_VOICE
				COMMON_PlayChannelVoice(gEeprom.ScreenChannel[gEeprom.TX_VFO], frequency);
#endif
				return;
			}

			Next = RADIO_FindNextChannel(Channel + Direction, Direction, false, 0);
			if (Next == 0xFF)
				return;
			if (Channel == Next)
				return;
			gEeprom.MrChannel[gEeprom.TX_VFO] = Next;
			gEeprom.ScreenChannel[gEeprom.TX_VFO] = Next;

			if (!bKeyHeld) {
#ifdef ENABLE_VOICE
				COMMON_PlayChannelVoice(Next, gTxVfo->pRX->Frequency);
#endif
			}
		}
#ifdef ENABLE_NOAA
		else {
			Channel = NOAA_CHANNEL_FIRST + NUMBER_AddWithWraparound(gEeprom.ScreenChannel[gEeprom.TX_VFO] - NOAA_CHANNEL_FIRST, Direction, 0, 9);
			gEeprom.NoaaChannel[gEeprom.TX_VFO] = Channel;
			gEeprom.ScreenChannel[gEeprom.TX_VFO] = Channel;
#ifdef ENABLE_VOICE
			COMMON_PlayChannelVoice(Channel, gTxVfo->pRX->Frequency);
#endif
		}
#endif

		gRequestSaveVFO   = true;
		gVfoConfigureMode = VFO_CONFIGURE_RELOAD;
		return;
	}

	// jump to the next channel
	CHFRSCANNER_Start(false, Direction);
	gScanPauseDelayIn_10ms = 1;
	gScheduleScanListen = false;

	gPttWasReleased = true;
}



void MAIN_ProcessKeys(KEY_Code_t Key, bool bKeyPressed, bool bKeyHeld)
{
#ifdef ENABLE_FMRADIO
	if (gFmRadioMode && Key != KEY_PTT && Key != KEY_EXIT) {
		if (!bKeyHeld && bKeyPressed)
			gBeepToPlay = BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL;
		return;
	}
#endif

	if (gDTMF_InputMode && bKeyPressed && !bKeyHeld) {
		const char Character = DTMF_GetCharacter(Key);
		if (Character != 0xFF)
		{	// add key to DTMF string
			DTMF_Append(Character);
			gKeyInputCountdown    = key_input_timeout_500ms;
			gRequestDisplayScreen = DISPLAY_MAIN;
			gPttWasReleased       = true;
			gBeepToPlay           = BEEP_1KHZ_60MS_OPTIONAL;
			return;
		}
	}

	// TODO: ???
//	if (Key > KEY_PTT)
//	{
//		Key = KEY_SIDE2;      // what's this doing ???
//	}

	switch (Key) {
		case KEY_0...KEY_9:
			MAIN_Key_DIGITS(Key, bKeyPressed, bKeyHeld);
			break;
		case KEY_MENU:
			MAIN_Key_MENU(bKeyPressed, bKeyHeld);
			break;
		case KEY_UP:
			MAIN_Key_UP_DOWN(bKeyPressed, bKeyHeld, 1);
			break;
		case KEY_DOWN:
			MAIN_Key_UP_DOWN(bKeyPressed, bKeyHeld, -1);
			break;
		case KEY_EXIT:
			MAIN_Key_EXIT(bKeyPressed, bKeyHeld);
			break;
		case KEY_STAR:
			MAIN_Key_STAR(bKeyPressed, bKeyHeld);
			break;
		case KEY_F:
			GENERIC_Key_F(bKeyPressed, bKeyHeld);
			break;
		case KEY_PTT:
			GENERIC_Key_PTT(bKeyPressed);
			break;
		default:
			if (!bKeyHeld && bKeyPressed)
				gBeepToPlay = BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL;
			break;
	}
}
