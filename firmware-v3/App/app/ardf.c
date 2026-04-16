/* Copyright 2024 Dennis Real
 * https://github.com/reald
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

#ifdef ENABLE_ARDF

#include "app/ardf.h"
#include "driver/bk4819.h"
#include "driver/system.h"
#include "driver/gpio.h"
#include "audio.h"
#include "misc.h"
#include "settings.h"
#include "radio.h"
#include "functions.h"
#include "app/app.h"
#include "frequencies.h"
#include "ui/main.h"
#include "ui/ui.h"
#include "ui/ardf.h"
#include "driver/st7565.h"



uint8_t ardf_gain_index[2][ARDF_NUM_FOX_MAX];
uint8_t ardf_gain_index_steps_mistune[2][ARDF_NUM_FOX_MAX];
bool    ardf_mistune_active[2][ARDF_NUM_FOX_MAX];
uint8_t ardf_neg_gain_level[2][ARDF_NUM_FOX_MAX];


// {0x03BE, -7},   //  0 .. 3 5 3 6 ..   0dB  -4dB  0dB  -3dB ..  -7dB original
#define ARDF_ORIG_GAIN_DB -7

t_ardf_gain_table ardf_gain_table[] =
{
   {0x0000, -60}, // 0: 0, -60dB
   {0x0080, -55}, // 1: 128, -55dB
   {0x0051, -50}, // 2: 81, -50dB
   {0x00B1, -45}, // 3: 177, -45dB
   {0x00F1, -40}, // 4: 241, -40dB
   {0x00DA, -35}, // 5: 218, -35dB
   {0x0065, -30}, // 6: 101, -30dB
   {0x0215, -25}, // 7: 533, -25dB
   {0x026E, -20}, // 8: 622, -20dB
   {0x028F, -15}, // 9: 655, -15dB
   {0x02BF, -10}, // 10: 703, -10dB
   {0x03DD, -5}, // 11: 989, -5dB
   {0x03FF, 0}, // 12: 1023, 0dB
};

uint32_t          gARDFTime10ms = 0;
uint32_t          gARDFFoxDuration10ms = ARDF_DEFAULT_FOX_DURATION;  /* 60s * 100 ticks per second */
uint32_t          gARDFFoxDuration10ms_corr = ARDF_DEFAULT_FOX_DURATION + (ARDF_DEFAULT_FOX_DURATION * ARDF_CLOCK_CORR_TICKS_PER_MIN)/6000;
uint8_t           gARDFNumFoxes = ARDF_DEFAULT_NUM_FOXES;
uint8_t           gARDFActiveFox = 0;
uint8_t           gARDFGainRemember = ARDF_DEFAULT_GAIN_REMEMBER; /* remember gain on VFO 1 by default. */
uint8_t           gARDFCycleEndBeep_s = ARDF_CYCLE_END_BEEP_S_DEFAULT;
bool              gARDFDFSimpleMode = false;
bool              gARDFPlayEndBeep = false;
unsigned int      gARDFRssiMax = 0; /* max rssi of last half second */
uint8_t           gARDFMemModeFreqToggleCnt_s = 0; /* toggle memory bank/frequency display every x s */
bool              gARDFRequestSaveEEPROM = true;
int16_t           gARDFClockCorrAddTicksPerMin = ARDF_CLOCK_CORR_TICKS_PER_MIN;
int8_t            gARDFMistuneFreqRaw = ARDF_GAIN_MISTUNE_HZ_DEFAULT;
uint8_t           gARDFMistuneAddGainIdxSteps = ARDF_GAIN_INDEX_ADD_STEPS_MISTUNE_DEFAULT;
uint8_t           gARDFSnapshotSpeed = ARDF_SNAPSHOT_SPEED_DEFAULT;
uint8_t           gARDFUpDownMode = ARDF_UPDOWN_GAIN;
#ifdef ARDF_ENABLE_SHOW_DEBUG_DATA
int16_t           gARDFdebug = 0;
int16_t           gARDFdebug2 = 0;
#endif



void ARDF_10ms(void)
{
   static uint16_t rssimaxhold_cnt = 0;

   rssimaxhold_cnt++;

   if ( gARDFTime10ms >= gARDFFoxDuration10ms_corr )
   {
      // new fox cycle
      gARDFTime10ms = 0;
      
      if ( (gARDFActiveFox + 1) >= gARDFNumFoxes ) // gARDFNumFoxes can be 0 if timing is disabled
      {
         gARDFActiveFox = 0;
      }
      else
      {
         gARDFActiveFox++;
      }

      if ( gSetting_ARDFEnable )
      {
         // recall last gain index if needed
         ARDF_ActivateGainIndex();
      }
      
      if ( gScreenToDisplay == DISPLAY_ARDF )
      {
         // update complete screen
         UI_DisplayARDF();
      }   

   }
   else if ( (gScreenToDisplay == DISPLAY_ARDF) && ( (gARDFTime10ms % 20) == 0) )
   {
      // update most important values ~5 times per second
      if ( gARDFNumFoxes > 0 )
      {
         UI_DisplayARDF_Timer();
      }

      if ( rssimaxhold_cnt >= 80 )
      {
         // reset max level after 0.8s
         gARDFRssiMax = BK4819_GetRSSI();
      }
      UI_DisplayARDF_RSSI();

#ifdef ARDF_ENABLE_SHOW_DEBUG_DATA
      UI_DisplayARDF_Debug();
#elif defined(ENABLE_AGC_SHOW_DATA)
      UI_MAIN_PrintAGC(true);
#else
      center_line = CENTER_LINE_RSSI;

      if ( gARDFDFSimpleMode != false )
      {
         UI_DisplayARDF_RSSIBar_Simple();
      }
      else if( !(gLowBattery && !gLowBatteryConfirmed) )
      {
         UI_DisplayRSSIBar(true);
      }

#endif

   }
   else if ( (gScreenToDisplay == DISPLAY_ARDF) && ( (gARDFTime10ms % 5) == 0) )
   {
      // reduce call rate if i2c traffic is too high
      unsigned int rssi = BK4819_GetRSSI();
      if ( rssi > gARDFRssiMax )
      {
         gARDFRssiMax = rssi;
         rssimaxhold_cnt = 0;
      }
   }

}



void ARDF_500ms(void)
{
   static uint8_t u8Secnd = 0;

   if ( gSetting_ARDFEnable && gScreenToDisplay==DISPLAY_MAIN )
   {
      // switch to ardf screen
      GUI_SelectNextDisplay(DISPLAY_ARDF);
   }
   else if ( !gSetting_ARDFEnable && gScreenToDisplay==DISPLAY_ARDF )
   {
      // ARDF is off now. switch back to main screen
      GUI_SelectNextDisplay(DISPLAY_MAIN);
   }


   u8Secnd++;
   
   if ( u8Secnd >= 2 )
   {

      // update status bar every second
      gUpdateStatus = 1;
      u8Secnd = 0;

      // counter for memory mode / frequency display toggle
      gARDFMemModeFreqToggleCnt_s++;

      if ( (gScreenToDisplay==DISPLAY_ARDF)
            && (gARDFMemModeFreqToggleCnt_s == ARDF_MEM_MODE_FREQ_TOGGLE_S) )
      {
         // screen update only really necessary in memory mode
         UI_DisplayARDF_FreqCh();
      }
      else if ( (gScreenToDisplay==DISPLAY_ARDF)
                 && (gARDFMemModeFreqToggleCnt_s >= (2 * ARDF_MEM_MODE_FREQ_TOGGLE_S)) )
      {
         gARDFMemModeFreqToggleCnt_s = 0;
         // screen update only really necessary in memory mode
         // UI_DisplayARDF_FreqCh(); // frequency update would be sufficient but problems deleting pixels
         UI_DisplayARDF();
      }


      // generate fox cycle end signal

      if ( (gScreenToDisplay==DISPLAY_ARDF)
           && (gARDFNumFoxes > 0)
           && (gARDFCycleEndBeep_s != 0)
           && (ARDF_GetRestTime_s() == gARDFCycleEndBeep_s) )
      {
         gARDFPlayEndBeep = true;
         AUDIO_PlayBeep( BEEP_880HZ_60MS_DOUBLE_BEEP );
         gARDFPlayEndBeep = false;
      }

   }

   if ( gARDFRequestSaveEEPROM != false )
   {
      // save ARDF settings to eeprom
      gARDFRequestSaveEEPROM = false;
      SETTINGS_SaveARDF();
   }

}



void ARDF_init(void)
{
   uint8_t gain_index = ARDF_GAIN_INDEX_DEFAULT;

   if ( gARDFDFSimpleMode != false )
   {
      gain_index = ARDF_GAIN_INDEX_DF_SIMPLE;
   }

   for ( uint8_t i=0; i<ARDF_NUM_FOX_MAX; i++ )
   {
      ardf_gain_index[0][i] = gain_index;
      ardf_gain_index[1][i] = gain_index;
      ardf_gain_index_steps_mistune[0][i] = 0;
      ardf_gain_index_steps_mistune[1][i] = 0;
      ardf_mistune_active[0][i] = false;
      ardf_mistune_active[1][i] = false;
      ardf_neg_gain_level[0][i] = 0;
      ardf_neg_gain_level[1][i] = 0;
   }

}



void ARDF_GainIncr(void)
{
   uint8_t vfo = gEeprom.RX_VFO;
   uint8_t activefox = gARDFActiveFox;
   
   if ( ARDF_ActVfoHasGainRemember(vfo) == false )
   {
      // do not remember fox gains on this vfo
      activefox = 0;
   }

   if ( ardf_neg_gain_level[vfo][activefox] > 0 )
   {
      // Come back from negative gain levels first
      ardf_neg_gain_level[vfo][activefox]--;
   }
   else if ( ardf_gain_index[vfo][activefox] < (sizeof(ardf_gain_table)/sizeof(t_ardf_gain_table))-1 )
   {
      ardf_gain_index[vfo][activefox]++;
   }

}



void ARDF_GainDecr(void)
{
   uint8_t vfo = gEeprom.RX_VFO;
   uint8_t activefox = gARDFActiveFox;

   if ( ARDF_ActVfoHasGainRemember(vfo) == false )
   {
      // do not remember fox gains on this vfo
      activefox = 0;
   }


   if ( ardf_gain_index[vfo][activefox] > 0 )
   {
      ardf_gain_index[vfo][activefox]--;
   }
   else if ( ardf_neg_gain_level[vfo][activefox] < ARDF_NEG_GAIN_LEVELS )
   {
      // Already at minimum RF gain, go into negative gain territory
      ardf_neg_gain_level[vfo][activefox]++;
   }

}



uint8_t ARDF_Get_GainIndex(uint8_t vfo)
{
   if ( ARDF_ActVfoHasGainRemember(vfo) == false )
   {
      // remember fox gains not on this vfo
      return ardf_gain_index[vfo][0];
   }
   else
   {
      return ardf_gain_index[vfo][gARDFActiveFox];
   }

}



bool ARDF_ActVfoHasGainRemember(uint8_t vfo)
{
   /* "OFF", 0
      "VFO A", 1
      "VFO B", 2
      "BOTH" 3 */
   
   if ( (vfo+1) & gARDFGainRemember )
   {
      return true;
   }
   else
   {
      return false;
   }

}



void ARDF_ActivateGainIndex(void)
{
   BK4819_WriteRegister( BK4819_REG_13, ardf_gain_table[ ARDF_Get_GainIndex(gEeprom.RX_VFO) ].reg_val );
   gARDFRssiMax = 0;
   gUpdateDisplay = true;
}



int32_t ARDF_GetRestTime_s(void)
{
   return (int32_t)(gARDFFoxDuration10ms - gARDFTime10ms * gARDFFoxDuration10ms/gARDFFoxDuration10ms_corr )/100;
}



int8_t ARDF_Get_GainDiff(void)
{
   return ARDF_ORIG_GAIN_DB - ardf_gain_table[ ARDF_Get_GainIndex(gEeprom.RX_VFO) ].gain_dB;
}



uint8_t ARDF_Get_NegGainLevel(uint8_t vfo)
{
   if ( ARDF_ActVfoHasGainRemember(vfo) == false )
   {
      return ardf_neg_gain_level[vfo][0];
   }
   else
   {
      return ardf_neg_gain_level[vfo][gARDFActiveFox];
   }
}



// Negative gain table: AF attenuation levels beyond minimum RF gain
// Each entry controls REG_48 (AF gain) and optionally REG_43 (IF bandwidth)
typedef struct {
    uint16_t reg48;           // AF gain register value
    uint16_t reg43_override;  // IF bandwidth override (0 = no change)
} t_ardf_neg_gain_entry;

static const t_ardf_neg_gain_entry __attribute__((unused)) ardf_neg_gain_table[ARDF_NEG_GAIN_LEVELS] =
{
    // --- Moderate AF reduction (RF gain already at minimum -60dB) ---
    { (11u << 12) | (0u << 10) | (32u << 4) | (6u << 0), 0 },     // N1
    { (11u << 12) | (1u << 10) | (20u << 4) | (4u << 0), 0 },     // N2
    { (11u << 12) | (2u << 10) | (12u << 4) | (3u << 0), 0 },     // N3

    // --- Strong AF reduction ---
    { (11u << 12) | (3u << 10) | ( 8u << 4) | (2u << 0), 0 },     // N4
    { (11u << 12) | (3u << 10) | ( 4u << 4) | (1u << 0), 0 },     // N5
    { ( 0u << 12) | (3u << 10) | ( 3u << 4) | (0u << 0), 0 },     // N6

    // --- Near-deaf: combine minimum AF with narrow IF bandwidth ---
    { ( 0u << 12) | (3u << 10) | ( 2u << 4) | (0u << 0), 0x0058 },// N7: + IF BW 1.7kHz
    { ( 0u << 12) | (3u << 10) | ( 1u << 4) | (0u << 0), 0x0058 },// N8
    { ( 0u << 12) | (3u << 10) | ( 1u << 4) | (0u << 0), 0x0018 },// N9: tightest
};



// --- Snapshot mode (PTT → signal strength beeps) ---

static uint8_t ARDF_DFSimpleAudio_dBmToBarLevel(int16_t dBm)
{
    int16_t rssi_dBm = -dBm;

    if ( rssi_dBm > 141 )
       rssi_dBm = 141;
    if ( rssi_dBm < 53 )
       rssi_dBm = 53;

    if ( rssi_dBm >= 93 )
       return map(rssi_dBm, 141, 93, 1, 9);

    return 9;
}

static uint8_t ARDF_DFSimpleAudio_GetLevelFromdBm(int16_t rssi_dBm)
{
    if ( rssi_dBm < -125 )
       return 0;

    return ARDF_DFSimpleAudio_dBmToBarLevel(rssi_dBm);
}

static void ARDF_PlaySingleSnapshotBeep(uint8_t level)
{
    const uint8_t old_beep_control = gEeprom.BEEP_CONTROL;

    gEeprom.BEEP_CONTROL = 1;

    if ( level <= 3 )
       AUDIO_PlayBeep(BEEP_440HZ_500MS);
    else if ( level <= 6 )
       AUDIO_PlayBeep(BEEP_1KHZ_60MS_OPTIONAL);
    else
       AUDIO_PlayBeep(BEEP_880HZ_60MS_DOUBLE_BEEP);

    gEeprom.BEEP_CONTROL = old_beep_control;
}

static uint16_t ARDF_SnapshotBeepGapMs(void)
{
    static const uint16_t gaps[ARDF_SNAPSHOT_SPEED_MAX] = {
       110, 70, 34, 14, 4
    };

    return gaps[(gARDFSnapshotSpeed > 0 ? gARDFSnapshotSpeed : 1) - 1];
}

static uint16_t ARDF_SnapshotGroupGapMs(void)
{
    static const uint16_t gaps[ARDF_SNAPSHOT_SPEED_MAX] = {
       260, 185, 120, 82, 64
    };

    return gaps[(gARDFSnapshotSpeed > 0 ? gARDFSnapshotSpeed : 1) - 1];
}

static void ARDF_PlaySnapshotSpeedFeedback(void)
{
    const uint8_t old_beep_control = gEeprom.BEEP_CONTROL;

    gARDFPlayEndBeep = true;
    gEeprom.BEEP_CONTROL = 1;

    for ( uint8_t i = 0; i < gARDFSnapshotSpeed; i++ )
    {
       AUDIO_PlayBeep(BEEP_1KHZ_60MS_OPTIONAL);
       SYSTEM_DelayMs(50);
    }

    gEeprom.BEEP_CONTROL = old_beep_control;
    gARDFPlayEndBeep = false;
    BK4819_TurnsOffTones_TurnsOnRX();
    RADIO_SetModulation(gRxVfo->Modulation);
    AUDIO_AudioPathOn();
    gEnableSpeaker = true;
    SYSTEM_DelayMs(10);
    APP_StartListening(gMonitor ? FUNCTION_MONITOR : FUNCTION_RECEIVE);
    ARDF_ActivateGainIndex();
    gARDFRssiMax = BK4819_GetRSSI();
}

void ARDF_PlaySnapshot(void)
{
    uint8_t level;
    uint8_t remaining;

    if ( !gSetting_ARDFEnable )
       return;

    level = ARDF_DFSimpleAudio_GetLevelFromdBm((gARDFRssiMax / 2) - 160);

    gARDFPlayEndBeep = true;

    if ( level == 0 )
    {
       AUDIO_PlayBeep(BEEP_500HZ_60MS_DOUBLE_BEEP);
    }
    else
    {
       remaining = level;

       while ( remaining > 0 )
       {
          const uint8_t group_count = (remaining > 3) ? 3 : remaining;

          for ( uint8_t i = 0; i < group_count; i++ )
          {
             ARDF_PlaySingleSnapshotBeep(level);
             SYSTEM_DelayMs(ARDF_SnapshotBeepGapMs());
          }

          remaining -= group_count;

          if ( remaining > 0 )
             SYSTEM_DelayMs(ARDF_SnapshotGroupGapMs());
       }
    }

    gARDFPlayEndBeep = false;
    BK4819_TurnsOffTones_TurnsOnRX();
    RADIO_SetModulation(gRxVfo->Modulation);
    AUDIO_AudioPathOn();
    gEnableSpeaker = true;
    SYSTEM_DelayMs(10);
    APP_StartListening(gMonitor ? FUNCTION_MONITOR : FUNCTION_RECEIVE);
    ARDF_ActivateGainIndex();
    gARDFRssiMax = BK4819_GetRSSI();
}

void ARDF_SnapshotSpeedIncr(void)
{
    if ( gARDFSnapshotSpeed < ARDF_SNAPSHOT_SPEED_MAX )
    {
       gARDFSnapshotSpeed++;
       gARDFRequestSaveEEPROM = true;
    }

    ARDF_PlaySnapshotSpeedFeedback();
}

void ARDF_SnapshotSpeedDecr(void)
{
    if ( gARDFSnapshotSpeed > 1 )
    {
       gARDFSnapshotSpeed--;
       gARDFRequestSaveEEPROM = true;
    }

    ARDF_PlaySnapshotSpeedFeedback();
}



// --- Compass mode (PTT held → continuous RSSI-to-tone) ---

#if defined(ENABLE_SAM_TTS)
/* ---- V3 DAC-based compass mode (concurrent RX + tone) ----
 *
 * The V3 hardware has a dedicated 12-bit DAC with DMA that is separate
 * from the BK4819 RF transceiver.  This means the BK4819 can stay in
 * RX mode continuously while the MCU DAC outputs a tone through the
 * voice ring-buffer infrastructure.
 *
 * Advantage over the BK4819 time-slice approach:
 *   - No gaps in reception — RSSI is read every iteration
 *   - Smoother tone output — 12-bit sine vs BK4819 square wave
 *   - Lower latency between RSSI change and tone change
 */

#include "driver/voice.h"

/* Pre-computed 12-bit sine table (one period, 32 entries) */
static const uint16_t compass_sine_table[32] = {
    2048, 2447, 2831, 3185, 3495, 3750, 3939, 4055,
    4095, 4055, 3939, 3750, 3495, 3185, 2831, 2447,
    2048, 1648, 1264,  910,  600,  345,  156,   40,
       0,   40,  156,  345,  600,  910, 1264, 1648
};

/* Fill one voice buffer slot with a sine wave at the given frequency.
 * phase is updated in place so consecutive calls produce a continuous wave. */
static void ARDF_FillSineBuffer(uint16_t freq_hz, uint32_t *phase_acc)
{
    /* Fixed-point phase increment per sample.
     * phase_acc uses 16.16 format, table has 32 entries.
     * increment = freq_hz * 32 * 65536 / 8000 = freq_hz * 262144 / 8000
     *           = freq_hz * 32768 / 1000 */
    uint32_t phase_inc = ((uint32_t)freq_hz * 32768U) / 1000U;

    for (uint16_t i = 0; i < VOICE_BUF_LEN; i++)
    {
        uint8_t idx = (*phase_acc >> 16) & 31;
        gVoiceBuf[gVoiceBufWriteIndex][i] = compass_sine_table[idx];
        *phase_acc += phase_inc;
    }
    VOICE_BUF_ForwardWriteIndex();
    gVoiceBufLen++;
}

void ARDF_CompassMode(void)
{
    // Continuous RSSI-to-tone compass mode using V3 DAC.
    // BK4819 stays in RX mode; tone is output through DAC/DMA.
    // Runs while PTT is held; returns when PTT is released.

    #define COMPASS_FREQ_MIN_HZ   300
    #define COMPASS_FREQ_MAX_HZ   2400
    #define COMPASS_RSSI_MIN_DBM  (-130)
    #define COMPASS_RSSI_MAX_DBM  (-50)
    #define COMPASS_FREQ_SPAN     (COMPASS_FREQ_MAX_HZ - COMPASS_FREQ_MIN_HZ)
    #define COMPASS_RSSI_SPAN     (COMPASS_RSSI_MAX_DBM - COMPASS_RSSI_MIN_DBM)

    uint32_t phase = 0;

    /* Mute the BK4819 AF output so only the DAC tone is heard */
    BK4819_SetAF(BK4819_AF_MUTE);
    AUDIO_AudioPathOn();

    /* Pre-fill the voice ring buffer with initial tone */
    uint16_t rssi_raw = BK4819_GetRSSI();
    int16_t  rssi_dBm = (rssi_raw / 2) - 160;
    int16_t  freq = COMPASS_FREQ_MIN_HZ +
                    ((rssi_dBm - COMPASS_RSSI_MIN_DBM) * COMPASS_FREQ_SPAN / COMPASS_RSSI_SPAN);
    if (freq < COMPASS_FREQ_MIN_HZ)  freq = COMPASS_FREQ_MIN_HZ;
    if (freq > COMPASS_FREQ_MAX_HZ)  freq = COMPASS_FREQ_MAX_HZ;

    gVoiceBufLen = 0;
    gVoiceBufReadIndex = 0;
    gVoiceBufWriteIndex = 0;

    while (gVoiceBufLen < VOICE_BUF_CAP)
        ARDF_FillSineBuffer((uint16_t)freq, &phase);

    VOICE_Start();

    while (GPIO_IsPttPressed())
    {
        /* Read RSSI while BK4819 stays in RX mode — no time-slicing */
        rssi_raw = BK4819_GetRSSI();
        rssi_dBm = (rssi_raw / 2) - 160;

        freq = COMPASS_FREQ_MIN_HZ +
               ((rssi_dBm - COMPASS_RSSI_MIN_DBM) * COMPASS_FREQ_SPAN / COMPASS_RSSI_SPAN);
        if (freq < COMPASS_FREQ_MIN_HZ)  freq = COMPASS_FREQ_MIN_HZ;
        if (freq > COMPASS_FREQ_MAX_HZ)  freq = COMPASS_FREQ_MAX_HZ;

        /* Refill consumed voice buffer slots with updated frequency */
        if (gVoiceBufLen < VOICE_BUF_CAP)
            ARDF_FillSineBuffer((uint16_t)freq, &phase);
        else
            SYSTEM_DelayMs(5);
    }

    /* Teardown: stop DAC playback, restore audio */
    VOICE_Stop();
    gVoiceBufLen = 0;

    RADIO_SetModulation(gRxVfo->Modulation);
    AUDIO_AudioPathOn();
    gEnableSpeaker = true;
    SYSTEM_DelayMs(10);
    APP_StartListening(gMonitor ? FUNCTION_MONITOR : FUNCTION_RECEIVE);
    ARDF_ActivateGainIndex();
    gARDFRssiMax = BK4819_GetRSSI();
}

#else /* !ENABLE_SAM_TTS — BK4819 time-slice fallback (same as V1) */

void ARDF_CompassMode(void)
{
    // Continuous RSSI-to-tone compass mode using BK4819 tone generator.
    // Uses time-slicing: alternates between RX and tone output.
    // Runs while PTT is held; returns when PTT is released.

    #define COMPASS_FREQ_MIN_HZ   300
    #define COMPASS_FREQ_MAX_HZ   2400
    #define COMPASS_RSSI_MIN_DBM  (-130)
    #define COMPASS_RSSI_MAX_DBM  (-50)
    #define COMPASS_FREQ_SPAN     (COMPASS_FREQ_MAX_HZ - COMPASS_FREQ_MIN_HZ)
    #define COMPASS_RSSI_SPAN     (COMPASS_RSSI_MAX_DBM - COMPASS_RSSI_MIN_DBM)

    #define COMPASS_TONE_MS       60
    #define COMPASS_RX_SETTLE_MS  30

    uint16_t tone_cfg = BK4819_ReadRegister(BK4819_REG_71);
    uint16_t af_gain_cfg = BK4819_ReadRegister(BK4819_REG_48);

    // Set a fixed AF DAC gain for consistent compass tone volume
    BK4819_WriteRegister(BK4819_REG_48,
       (11u << 12) |
       ( 0u << 10) |
       (58u <<  4) |
       ( 8u <<  0));

    AUDIO_AudioPathOn();

    while (GPIO_IsPttPressed())
    {
       // Phase 1: Switch to RX mode and read RSSI
       BK4819_EnterTxMute();
       BK4819_WriteRegister(BK4819_REG_70, 0);
       BK4819_WriteRegister(BK4819_REG_30, 0);
       BK4819_WriteRegister(BK4819_REG_30,
          BK4819_REG_30_ENABLE_VCO_CALIB |
          BK4819_REG_30_ENABLE_RX_LINK   |
          BK4819_REG_30_ENABLE_AF_DAC    |
          BK4819_REG_30_ENABLE_DISC_MODE |
          BK4819_REG_30_ENABLE_PLL_VCO   |
          BK4819_REG_30_ENABLE_RX_DSP);
       BK4819_SetAF(BK4819_AF_MUTE);
       SYSTEM_DelayMs(COMPASS_RX_SETTLE_MS);

       uint16_t rssi_raw = BK4819_GetRSSI();
       int16_t  rssi_dBm = (rssi_raw / 2) - 160;

       int16_t freq = COMPASS_FREQ_MIN_HZ +
                      ((rssi_dBm - COMPASS_RSSI_MIN_DBM) * COMPASS_FREQ_SPAN / COMPASS_RSSI_SPAN);
       if (freq < COMPASS_FREQ_MIN_HZ)  freq = COMPASS_FREQ_MIN_HZ;
       if (freq > COMPASS_FREQ_MAX_HZ)  freq = COMPASS_FREQ_MAX_HZ;

       // Phase 2: Play tone pulse
       BK4819_PlayTone((uint16_t)freq, true);
       SYSTEM_DelayMs(2);
       BK4819_ExitTxMute();
       SYSTEM_DelayMs(COMPASS_TONE_MS);
       BK4819_EnterTxMute();
    }

    // Teardown: restore original state
    BK4819_EnterTxMute();
    BK4819_WriteRegister(BK4819_REG_71, tone_cfg);
    BK4819_TurnsOffTones_TurnsOnRX();
    RADIO_SetModulation(gRxVfo->Modulation);
    BK4819_WriteRegister(BK4819_REG_48, af_gain_cfg);
    AUDIO_AudioPathOn();
    gEnableSpeaker = true;
    SYSTEM_DelayMs(10);
    APP_StartListening(gMonitor ? FUNCTION_MONITOR : FUNCTION_RECEIVE);
    ARDF_ActivateGainIndex();
    gARDFRssiMax = BK4819_GetRSSI();
}

#endif /* ENABLE_SAM_TTS */



// --- Frequency mistune for close-range fox hunting ---

void ARDF_DoMistuneFreq(void)
{
    uint32_t frequency = gTxVfo->freq_config_RX.Frequency + (gARDFMistuneFreqRaw*ARDF_MISTUNE_RES_HZ/10);

    if ( RX_freq_check(frequency) < 0 )
    {
       gBeepToPlay = BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL;
       return;
    }

    gTxVfo->freq_config_RX.Frequency = frequency;
    BK4819_SetFrequency(frequency);

    uint16_t reg = BK4819_ReadRegister(BK4819_REG_30);
    BK4819_WriteRegister(BK4819_REG_30, reg & ~BK4819_REG_30_ENABLE_VCO_CALIB);
    BK4819_WriteRegister(BK4819_REG_30, reg);

    return;
}

void ARDF_UndoMistuneFreq(void)
{
    uint32_t frequency = gTxVfo->freq_config_RX.Frequency - (gARDFMistuneFreqRaw*ARDF_MISTUNE_RES_HZ/10);

    if ( RX_freq_check(frequency) < 0 )
    {
       gBeepToPlay = BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL;
       return;
    }
    gTxVfo->freq_config_RX.Frequency = frequency;
    BK4819_SetFrequency(frequency);

    uint16_t reg = BK4819_ReadRegister(BK4819_REG_30);
    BK4819_WriteRegister(BK4819_REG_30, reg & ~BK4819_REG_30_ENABLE_VCO_CALIB);
    BK4819_WriteRegister(BK4819_REG_30, reg);

    return;
}

void ARDF_StopFreqMistune(void)
{
    uint8_t vfo = gEeprom.RX_VFO;
    uint8_t activefox = gARDFActiveFox;

    if ( ARDF_ActVfoHasGainRemember(vfo) == false )
    {
       activefox = 0;
    }

    if ( (gSetting_ARDFEnable) && (ardf_mistune_active[vfo][activefox] != false) )
    {
       ARDF_UndoMistuneFreq();

       ardf_mistune_active[vfo][activefox] = false;
       ardf_gain_index[vfo][activefox] = 0;
       ardf_gain_index_steps_mistune[vfo][activefox] = 0;
       ardf_neg_gain_level[vfo][activefox] = 0;
    }

    return;
}


#endif
