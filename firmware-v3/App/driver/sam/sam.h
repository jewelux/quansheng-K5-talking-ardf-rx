/* SAM (Software Automatic Mouth) TTS Engine for PY32F071 (Cortex-M0+)
 *
 * Formant-based speech synthesizer adapted from the classic 1982 SAM
 * algorithm.  Integer-only, no FPU required.  Generates 8 kHz 12-bit
 * PCM samples suitable for direct DAC output.
 *
 * Copyright 2025 – Licensed under Apache 2.0
 */

#ifndef DRIVER_SAM_H
#define DRIVER_SAM_H

#include <stdint.h>
#include <stdbool.h>

/* ---- Configuration ---- */
#define SAM_SAMPLE_RATE       8000
#define SAM_FRAME_SAMPLES     20     /* samples per phoneme sub-frame  */
#define SAM_MAX_PHONEMES      128    /* max phonemes per utterance     */
#define SAM_PHONEME_BUF_SIZE  256    /* max phoneme string length      */
#define SAM_DAC_MID           2048   /* 12-bit DAC midpoint            */

/* ---- Public API ---- */

/* Initialise SAM engine (call once at boot). */
void SAM_Init(void);

/* Set speaking speed: 1 (slowest) – 9 (fastest), default 5. */
void SAM_SetSpeed(uint8_t speed);

/* Set base pitch: 1 (lowest) – 9 (highest), default 5. */
void SAM_SetPitch(uint8_t pitch);

/* Set mouth/throat character: 1 (deep) – 9 (bright), default 5.
 * Controls formant frequencies for voice timbre. */
void SAM_SetMouthThroatParam(uint8_t level);

/* Begin synthesising *text*.  Preprocesses and converts to phonemes.
 * Call SAM_FillVoiceBuffer() repeatedly afterwards.
 * Returns estimated duration in 10 ms units (0 if nothing to say). */
uint16_t SAM_StartSpeaking(const char *text);

/* Generate the next VOICE_BUF_LEN (160) samples into the voice ring
 * buffer at the current write index.  Returns true while more data is
 * available; false when the utterance is finished. */
bool SAM_FillVoiceBuffer(void);

/* True while synthesis is in progress. */
bool SAM_IsSpeaking(void);

/* Abort current synthesis. */
void SAM_Stop(void);

#endif /* DRIVER_SAM_H */
