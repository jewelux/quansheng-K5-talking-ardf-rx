#include "app/chFrScanner.h"
#include "audio.h"
#include "functions.h"
#include "misc.h"
#include "settings.h"
#include "ui/ui.h"

#ifdef ENABLE_ARDF
#include "app/ardf.h"
#endif

#ifdef ENABLE_VOICE
void COMMON_PlayChannelVoice(const uint8_t channel_save, const uint32_t frequency)
{
    if (gEeprom.VOICE_PROMPT == VOICE_PROMPT_OFF)
        return;

    if (IS_MR_CHANNEL(channel_save))
    {
        AUDIO_SetVoiceID(0, VOICE_ID_CHANNEL_MODE);
        AUDIO_SetDigitVoice(1, channel_save + 1);
        gAnotherVoiceID = (VOICE_ID_t)0xFE;
        return;
    }

    AUDIO_SetVoiceID(0, VOICE_ID_FREQUENCY_MODE);
    AUDIO_SetDigitVoice(1, (uint16_t)(frequency / 100000U));
    AUDIO_SetDigitVoice(2, (uint16_t)((frequency / 100U) % 1000U));
    gAnotherVoiceID = (VOICE_ID_t)0xFE;
}

void COMMON_PlayCurrentVfoVoice(void)
{
    COMMON_PlayChannelVoice(gTxVfo->CHANNEL_SAVE, gTxVfo->pRX->Frequency);
}
#endif

void COMMON_KeypadLockToggle() 
{

    if (gScreenToDisplay != DISPLAY_MENU &&
        gCurrentFunction != FUNCTION_TRANSMIT)
    {	// toggle the keyboad lock

        #ifdef ENABLE_VOICE
            gAnotherVoiceID = gEeprom.KEY_LOCK ? VOICE_ID_UNLOCK : VOICE_ID_LOCK;
        #endif

        gEeprom.KEY_LOCK = !gEeprom.KEY_LOCK;

        gRequestSaveSettings = true;
    }
}

void COMMON_SwitchVFOs()
{
#ifdef ENABLE_SCAN_RANGES    
    gScanRangeStart = 0;
#endif

#ifdef ENABLE_ARDF
//fixme: can be used with gain remember off

    // vfo switch. undo mistune frequncy shift if active. only necessary if gain remember is active.
    if ( (gSetting_ARDFEnable) && (ARDF_ActVfoHasGainRemember(gEeprom.RX_VFO) != false)
        && (ardf_mistune_active[gEeprom.RX_VFO][gARDFActiveFox] != false) )
    {
        ARDF_UndoMistuneFreq(); // only undo the frequency shift
    }
#endif

    gEeprom.TX_VFO ^= 1;

    if (gEeprom.CROSS_BAND_RX_TX != CROSS_BAND_OFF)
        gEeprom.CROSS_BAND_RX_TX = gEeprom.TX_VFO + 1;
    if (gEeprom.DUAL_WATCH != DUAL_WATCH_OFF)
        gEeprom.DUAL_WATCH = gEeprom.TX_VFO + 1;

#ifdef ENABLE_ARDF
// fixme. has to be activated later

    // vfo switch. restore mistune frequncy shift if active. only necessary if gain remember is active.
/*    if ( (gSetting_ARDFEnable) && (ARDF_ActVfoHasGainRemember(gEeprom.RX_VFO) != false)
        && (ardf_mistune_active[gEeprom.RX_VFO][gARDFActiveFox] != false) )
    {
        ARDF_DoMistuneFreq(); // only do the frequency shift
    }*/
#endif

    gRequestSaveSettings  = 1;
    gFlagReconfigureVfos  = true;
    gScheduleDualWatch = true;

    gRequestDisplayScreen = DISPLAY_MAIN;

#ifdef ENABLE_ARDF
    if ( gScreenToDisplay == DISPLAY_ARDF )
        gRequestDisplayScreen = DISPLAY_ARDF;
#endif

#ifdef ENABLE_VOICE
    COMMON_PlayCurrentVfoVoice();
#endif

}

void COMMON_SwitchVFOMode()
{
#ifdef ENABLE_NOAA
    if (gEeprom.VFO_OPEN && !IS_NOAA_CHANNEL(gTxVfo->CHANNEL_SAVE))
#else
    if (gEeprom.VFO_OPEN)
#endif
    {

#ifdef ENABLE_ARDF
        ARDF_StopFreqMistune();
#endif

        if (IS_MR_CHANNEL(gTxVfo->CHANNEL_SAVE))
        {	// swap to frequency mode
            gEeprom.ScreenChannel[gEeprom.TX_VFO] = gEeprom.FreqChannel[gEeprom.TX_VFO];
            #ifdef ENABLE_VOICE
                COMMON_PlayChannelVoice(gEeprom.FreqChannel[gEeprom.TX_VFO], gTxVfo->pRX->Frequency);
            #endif
            gRequestSaveVFO            = true;
            gVfoConfigureMode          = VFO_CONFIGURE_RELOAD;
            return;
        }

        uint8_t Channel = RADIO_FindNextChannel(gEeprom.MrChannel[gEeprom.TX_VFO], 1, false, 0);
        if (Channel != 0xFF)
        {	// swap to channel mode
            gEeprom.ScreenChannel[gEeprom.TX_VFO] = Channel;
            #ifdef ENABLE_VOICE
                COMMON_PlayChannelVoice(Channel, gTxVfo->pRX->Frequency);
            #endif
            gRequestSaveVFO     = true;
            gVfoConfigureMode   = VFO_CONFIGURE_RELOAD;
            return;
        }
    }
}
