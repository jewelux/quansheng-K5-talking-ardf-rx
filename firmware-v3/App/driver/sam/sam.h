/* SAM (Software Automatic Mouth) TTS Engine – Ported from s-macke/SAM
 *
 * Original SAM by Don't Ask Software (1982).
 * C port by Sebastian Macke (https://github.com/s-macke/SAM).
 * Adapted for PY32F071 (Cortex-M0+) by removing malloc/stdio/SDL
 * dependencies and adding streaming buffer output for DAC+DMA playback.
 *
 * Licensed under GNU GPL v2 (matching original SAM license).
 */

#ifndef DRIVER_SAM_H
#define DRIVER_SAM_H

#include <stdint.h>
#include <stdbool.h>

/* Initialise SAM engine (call once at boot). */
void SAM_Init(void);

/* Set speaking speed (default 72, lower = slower). */
void SAM_SetSpeed(uint8_t speed);

/* Set base pitch (default 64). */
void SAM_SetPitch(uint8_t pitch);

/* Set mouth/throat formant parameters (defaults 128/128). */
void SAM_SetMouth(uint8_t mouth);
void SAM_SetThroat(uint8_t throat);

/* Convert English text to SAM phoneme string.
 * Input: ASCII text string
 * Output: phoneme string written to internal buffer
 * Returns 1 on success, 0 on failure. */
int SAM_TextToPhonemes(const char *text);

/* Set phoneme string directly (for pre-encoded phonemes).
 * The string must be terminated with 0x9B. */
void SAM_SetPhonemes(const char *phonemes);

/* Process phonemes and render speech into internal buffer.
 * Call after SAM_TextToPhonemes() or SAM_SetPhonemes().
 * Returns 1 on success, 0 on failure. */
int SAM_Render(void);

/* Speak text: convenience function that calls TextToPhonemes + Render.
 * Returns 1 on success, 0 on failure. */
int SAM_SpeakText(const char *text);

/* Get pointer to rendered audio buffer (8-bit unsigned PCM, ~22050 Hz). */
const char *SAM_GetBuffer(void);

/* Get length of rendered audio in samples. */
int SAM_GetBufferLength(void);

#endif /* DRIVER_SAM_H */
