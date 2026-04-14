/* SAM (Software Automatic Mouth) TTS Engine – Implementation
 *
 * Formant-based speech synthesiser for PY32F071 (Cortex-M0+).
 * All arithmetic is integer-only; no floating-point.
 *
 * Architecture:
 *   1. Text → phoneme string  (text_to_phonemes)
 *   2. Phoneme string → phoneme index list  (parse_phonemes)
 *   3. Phoneme list → streaming 12-bit PCM  (render)
 *
 * Copyright 2025 – Licensed under Apache 2.0
 */

#include "driver/sam/sam.h"
#include "driver/voice.h"

#include <string.h>
#include <stdint.h>
#include <stdbool.h>

/* ====================================================================
 *  1.  PHONEME DEFINITIONS
 * ==================================================================== */

/* Phoneme indices */
enum {
    PH_SPACE = 0, /* silence / pause */
    PH_IY,  PH_IH,  PH_EH,  PH_AE,  PH_AA,  PH_AH,  PH_AO,
    PH_OH,  PH_UH,  PH_UW,  PH_AX,  PH_ER,
    /* diphthong start components are just vowels; the reciter emits two */
    PH_P,   PH_B,   PH_T,   PH_D,   PH_K,   PH_G,
    PH_F,   PH_V,   PH_TH,  PH_DH,
    PH_S,   PH_Z,   PH_SH,  PH_ZH,  PH_HH,
    PH_M,   PH_N,   PH_NG,
    PH_L,   PH_R,   PH_W,   PH_Y,
    PH_CH,  PH_JH,
    PH_COUNT
};

/* Flags */
#define F_VOICED    0x01
#define F_VOWEL     0x02
#define F_STOP      0x04
#define F_FRIC      0x08
#define F_NASAL     0x10
#define F_LIQUID    0x20

typedef struct {
    uint8_t f1_inc;   /* F1 phase-increment (8-bit, Δf ≈ 31.25 Hz) */
    uint8_t f2_inc;   /* F2 */
    uint8_t f3_inc;   /* F3 */
    uint8_t a1;       /* F1 amplitude 0-15 */
    uint8_t a2;       /* F2 amplitude 0-15 */
    uint8_t a3;       /* F3 amplitude 0-15 */
    uint8_t dur;      /* default duration in sub-frames (× SAM_FRAME_SAMPLES) */
    uint8_t flags;
} PhonemeInfo;

/* phase_increment = round(freq × 256 / 8000) = round(freq / 31.25)
 *
 * The table below stores formant centre frequencies encoded this way.
 * Amplitudes 0-15 control the contribution of each formant.
 * Durations are in sub-frames (each 20 samples = 2.5 ms @ 8 kHz). */

static const PhonemeInfo phoneme_tab[PH_COUNT] = {
/*  idx       f1   f2   f3   a1  a2  a3  dur  flags                    */
/* SPACE */ {  0,   0,   0,   0,  0,  0,  6,  0              },
/* IY    */ {  9,  73,  96,  15, 12,  5,  8,  F_VOICED|F_VOWEL },
/* IH    */ { 12,  64,  82,  15, 12,  4,  6,  F_VOICED|F_VOWEL },
/* EH    */ { 17,  59,  79,  15, 13,  5,  7,  F_VOICED|F_VOWEL },
/* AE    */ { 21,  55,  77,  15, 14,  5,  8,  F_VOICED|F_VOWEL },
/* AA    */ { 23,  35,  78,  15, 12,  4,  8,  F_VOICED|F_VOWEL },
/* AH    */ { 17,  38,  76,  15, 11,  4,  7,  F_VOICED|F_VOWEL },
/* AO    */ { 18,  27,  77,  15, 10,  4,  8,  F_VOICED|F_VOWEL },
/* OH    */ { 14,  33,  76,  15, 11,  4,  8,  F_VOICED|F_VOWEL },
/* UH    */ { 14,  33,  72,  15, 10,  3,  6,  F_VOICED|F_VOWEL },
/* UW    */ { 10,  28,  72,  15,  9,  3,  8,  F_VOICED|F_VOWEL },
/* AX    */ { 16,  47,  80,  13, 10,  3,  4,  F_VOICED|F_VOWEL },
/* ER    */ { 16,  43,  54,  15, 11,  6,  8,  F_VOICED|F_VOWEL },
/* P     */ {  0,   0,   0,   0,  0,  0,  4,  F_STOP           },
/* B     */ {  6,  35,  69,   4,  3,  1,  4,  F_VOICED|F_STOP  },
/* T     */ {  0,   0,   0,   0,  0,  0,  3,  F_STOP           },
/* D     */ {  6,  51,  83,   4,  3,  1,  3,  F_VOICED|F_STOP  },
/* K     */ {  0,   0,   0,   0,  0,  0,  4,  F_STOP           },
/* G     */ {  6,  48,  80,   4,  3,  1,  4,  F_VOICED|F_STOP  },
/* F     */ {  6,  45,  90,   2,  4,  5,  5,  F_FRIC           },
/* V     */ {  6,  45,  90,   4,  5,  5,  5,  F_VOICED|F_FRIC  },
/* TH    */ {  6,  45,  90,   1,  3,  4,  4,  F_FRIC           },
/* DH    */ {  6,  45,  90,   3,  4,  4,  4,  F_VOICED|F_FRIC  },
/* S     */ {  0,   0, 102,   0,  0, 10,  5,  F_FRIC           },
/* Z     */ {  6,  45, 102,   2,  3, 10,  5,  F_VOICED|F_FRIC  },
/* SH    */ {  0,   0,  80,   0,  0, 12,  5,  F_FRIC           },
/* ZH    */ {  6,  45,  80,   2,  3, 12,  5,  F_VOICED|F_FRIC  },
/* HH    */ {  6,  45,  90,   3,  3,  2,  3,  F_FRIC           },
/* M     */ {  9,  32,  80,  15,  4,  1,  7,  F_VOICED|F_NASAL },
/* N     */ {  9,  54,  80,  15,  4,  1,  6,  F_VOICED|F_NASAL },
/* NG    */ {  9,  80,  96,  15,  4,  1,  7,  F_VOICED|F_NASAL },
/* L     */ { 12,  42,  86,  13,  8,  3,  6,  F_VOICED|F_LIQUID},
/* R     */ { 11,  45,  54,  12,  7,  4,  6,  F_VOICED|F_LIQUID},
/* W     */ {  9,  20,  69,  13,  6,  2,  4,  F_VOICED|F_LIQUID},
/* Y     */ {  9,  70,  96,  13,  7,  3,  4,  F_VOICED|F_LIQUID},
/* CH    */ {  0,   0,  80,   0,  0,  8,  5,  F_FRIC|F_STOP    },
/* JH    */ {  6,  51,  83,   3,  3,  6,  5,  F_VOICED|F_FRIC|F_STOP},
};

/* ====================================================================
 *  2.  SINE TABLE  (256 entries, signed 8-bit, one full period)
 * ==================================================================== */

static const int8_t sine_tab[256] = {
      0,   3,   6,   9,  12,  16,  19,  22,  25,  28,  31,  34,  37,  40,  43,  46,
     49,  51,  54,  57,  60,  63,  65,  68,  71,  73,  76,  78,  81,  83,  85,  88,
     90,  92,  94,  96,  98, 100, 102, 104, 106, 107, 109, 111, 112, 113, 115, 116,
    117, 118, 120, 121, 122, 122, 123, 124, 125, 125, 126, 126, 126, 127, 127, 127,
    127, 127, 127, 127, 126, 126, 126, 125, 125, 124, 123, 122, 122, 121, 120, 118,
    117, 116, 115, 113, 112, 111, 109, 107, 106, 104, 102, 100,  98,  96,  94,  92,
     90,  88,  85,  83,  81,  78,  76,  73,  71,  68,  65,  63,  60,  57,  54,  51,
     49,  46,  43,  40,  37,  34,  31,  28,  25,  22,  19,  16,  12,   9,   6,   3,
      0,  -3,  -6,  -9, -12, -16, -19, -22, -25, -28, -31, -34, -37, -40, -43, -46,
    -49, -51, -54, -57, -60, -63, -65, -68, -71, -73, -76, -78, -81, -83, -85, -88,
    -90, -92, -94, -96, -98,-100,-102,-104,-106,-107,-109,-111,-112,-113,-115,-116,
   -117,-118,-120,-121,-122,-122,-123,-124,-125,-125,-126,-126,-126,-127,-127,-127,
   -127,-127,-127,-127,-126,-126,-126,-125,-125,-124,-123,-122,-122,-121,-120,-118,
   -117,-116,-115,-113,-112,-111,-109,-107,-106,-104,-102,-100, -98, -96, -94, -92,
    -90, -88, -85, -83, -81, -78, -76, -73, -71, -68, -65, -63, -60, -57, -54, -51,
    -49, -46, -43, -40, -37, -34, -31, -28, -25, -22, -19, -16, -12,  -9,  -6,  -3,
};

/* ====================================================================
 *  3.  WORD DICTIONARY  (menu terms → phoneme sequences)
 * ==================================================================== */

/* Phoneme sequence terminator */
#define PH_END  0xFF

/* Pre-encoded phoneme sequences for common words / menu labels.
 * Each entry is { "WORD", {phoneme indices…, PH_END} }.
 * The lookup is case-insensitive (input is upper-cased first).
 *
 * Diphthongs are encoded as two consecutive vowels so that the
 * synthesiser interpolates between them naturally:
 *   /eɪ/ = EH→IH   /aɪ/ = AA→IH   /ɔɪ/ = AO→IH
 *   /oʊ/ = OH→UH   /aʊ/ = AA→UH                           */

typedef struct {
    const char    *word;
    const uint8_t  phon[16];   /* max 15 phonemes + terminator */
} DictEntry;

static const DictEntry word_dict[] = {
    /* Numbers */
    {"ZERO",     {PH_Z,PH_IY,PH_R,PH_OH, PH_END}},
    {"ONE",      {PH_W,PH_AH,PH_N, PH_END}},
    {"TWO",      {PH_T,PH_UW, PH_END}},
    {"THREE",    {PH_TH,PH_R,PH_IY, PH_END}},
    {"FOUR",     {PH_F,PH_AO,PH_R, PH_END}},
    {"FIVE",     {PH_F,PH_AA,PH_IH,PH_V, PH_END}},
    {"SIX",      {PH_S,PH_IH,PH_K,PH_S, PH_END}},
    {"SEVEN",    {PH_S,PH_EH,PH_V,PH_AX,PH_N, PH_END}},
    {"EIGHT",    {PH_EH,PH_IH,PH_T, PH_END}},
    {"NINE",     {PH_N,PH_AA,PH_IH,PH_N, PH_END}},
    {"TEN",      {PH_T,PH_EH,PH_N, PH_END}},
    {"HUNDRED",  {PH_HH,PH_AH,PH_N,PH_D,PH_R,PH_AX,PH_D, PH_END}},
    {"POINT",    {PH_P,PH_AO,PH_IH,PH_N,PH_T, PH_END}},

    /* Common menu / status words */
    {"STEP",     {PH_S,PH_T,PH_EH,PH_P, PH_END}},
    {"POWER",    {PH_P,PH_AA,PH_UH,PH_ER, PH_END}},
    {"OFFSET",   {PH_AO,PH_F,PH_S,PH_EH,PH_T, PH_END}},
    {"LOCK",     {PH_L,PH_AA,PH_K, PH_END}},
    {"UNLOCK",   {PH_AH,PH_N,PH_L,PH_AA,PH_K, PH_END}},
    {"CHANNEL",  {PH_CH,PH_AE,PH_N,PH_AX,PH_L, PH_END}},
    {"SCAN",     {PH_S,PH_K,PH_AE,PH_N, PH_END}},
    {"SAVE",     {PH_S,PH_EH,PH_IH,PH_V, PH_END}},
    {"DELETE",   {PH_D,PH_IH,PH_L,PH_IY,PH_T, PH_END}},
    {"MEMORY",   {PH_M,PH_EH,PH_M,PH_ER,PH_IY, PH_END}},
    {"SQUELCH",  {PH_S,PH_K,PH_W,PH_EH,PH_L,PH_CH, PH_END}},
    {"BEEP",     {PH_B,PH_IY,PH_P, PH_END}},
    {"VOICE",    {PH_V,PH_AO,PH_IH,PH_S, PH_END}},
    {"MORSE",    {PH_M,PH_AO,PH_R,PH_S, PH_END}},
    {"VOLUME",   {PH_V,PH_AA,PH_L,PH_UW,PH_M, PH_END}},
    {"FREQUENCY",{PH_F,PH_R,PH_IY,PH_K,PH_W,PH_EH,PH_N,PH_S,PH_IY, PH_END}},
    {"MODE",     {PH_M,PH_OH,PH_D, PH_END}},
    {"RESET",    {PH_R,PH_IY,PH_S,PH_EH,PH_T, PH_END}},
    {"MENU",     {PH_M,PH_EH,PH_N,PH_UW, PH_END}},
    {"ON",       {PH_AA,PH_N, PH_END}},
    {"OFF",      {PH_AO,PH_F, PH_END}},
    {"HIGH",     {PH_HH,PH_AA,PH_IH, PH_END}},
    {"LOW",      {PH_L,PH_OH, PH_END}},
    {"MID",      {PH_M,PH_IH,PH_D, PH_END}},
    {"ROGER",    {PH_R,PH_AA,PH_JH,PH_ER, PH_END}},
    {"BATTERY",  {PH_B,PH_AE,PH_T,PH_ER,PH_IY, PH_END}},
    {"ACCESS",   {PH_AE,PH_K,PH_S,PH_EH,PH_S, PH_END}},
    {"SAM",      {PH_S,PH_AE,PH_M, PH_END}},
    {"WELCOME",  {PH_W,PH_EH,PH_L,PH_K,PH_AH,PH_M, PH_END}},
    {"BACKLIGHT",{PH_B,PH_AE,PH_K,PH_L,PH_AA,PH_IH,PH_T, PH_END}},
    {"COMPANDER",{PH_K,PH_AH,PH_M,PH_P,PH_AE,PH_N,PH_D,PH_ER, PH_END}},
    {"TIMEOUT",  {PH_T,PH_AA,PH_IH,PH_M,PH_AA,PH_UH,PH_T, PH_END}},
    {"BANDWIDTH",{PH_B,PH_AE,PH_N,PH_D,PH_W,PH_IH,PH_D,PH_TH, PH_END}},
    {"MODULATION",{PH_M,PH_AA,PH_D,PH_UW,PH_L,PH_EH,PH_IH,PH_SH,PH_AX,PH_N, PH_END}},
    {"PRIORITY", {PH_P,PH_R,PH_AA,PH_IH,PH_AO,PH_R,PH_IH,PH_T,PH_IY, PH_END}},
    {"REPEATER", {PH_R,PH_IH,PH_P,PH_IY,PH_T,PH_ER, PH_END}},
    {"MICROPHONE",{PH_M,PH_AA,PH_IH,PH_K,PH_R,PH_OH,PH_F,PH_OH,PH_N, PH_END}},

    /* Helper words */
    {"BEE",      {PH_B,PH_IY, PH_END}},
    {"SEE",      {PH_S,PH_IY, PH_END}},
    {"DEE",      {PH_D,PH_IY, PH_END}},
    {"EE",       {PH_IY, PH_END}},
    {"EFF",      {PH_EH,PH_F, PH_END}},
    {"GEE",      {PH_JH,PH_IY, PH_END}},

    /* German extensions (ä, ö, ü, ß mapped to closest phonemes) */
    {"FREQUENZ", {PH_F,PH_R,PH_EH,PH_K,PH_V,PH_EH,PH_N,PH_T,PH_S, PH_END}},
    {"KANAL",    {PH_K,PH_AH,PH_N,PH_AA,PH_L, PH_END}},
    {"PUNKT",    {PH_P,PH_UH,PH_NG,PH_K,PH_T, PH_END}},
    {"NULL",     {PH_N,PH_UH,PH_L, PH_END}},
    {"EINS",     {PH_AA,PH_IH,PH_N,PH_S, PH_END}},
    {"ZWEI",     {PH_T,PH_S,PH_V,PH_AA,PH_IH, PH_END}},
    {"DREI",     {PH_D,PH_R,PH_AA,PH_IH, PH_END}},
    {"VIER",     {PH_F,PH_IY,PH_R, PH_END}},
    {"FUENF",    {PH_F,PH_UW,PH_N,PH_F, PH_END}},
    {"SECHS",    {PH_Z,PH_EH,PH_K,PH_S, PH_END}},
    {"SIEBEN",   {PH_Z,PH_IY,PH_B,PH_AX,PH_N, PH_END}},
    {"ACHT",     {PH_AA,PH_K,PH_T, PH_END}},
    {"NEUN",     {PH_N,PH_AO,PH_IH,PH_N, PH_END}},
    {"ZEHN",     {PH_T,PH_S,PH_EH,PH_IH,PH_N, PH_END}},
};

#define DICT_SIZE  (sizeof(word_dict) / sizeof(word_dict[0]))

/* ====================================================================
 *  4.  TEXT-TO-PHONEME CONVERTER  (simplified English rules)
 * ==================================================================== */

/* Internal phoneme buffer written by the converter, consumed by renderer */
static uint8_t  phon_buf[SAM_MAX_PHONEMES];
static uint8_t  phon_count;

/* --- helpers --------------------------------------------------------- */

static uint8_t to_upper(uint8_t c)
{
    return (c >= 'a' && c <= 'z') ? (c - 32) : c;
}

static bool is_alpha(uint8_t c)
{
    c = to_upper(c);
    return c >= 'A' && c <= 'Z';
}

static bool is_vowel_letter(uint8_t c)
{
    c = to_upper(c);
    return c == 'A' || c == 'E' || c == 'I' || c == 'O' || c == 'U';
}

static bool is_digit(uint8_t c) { return c >= '0' && c <= '9'; }

/* Emit a phoneme; silently drops if buffer full */
static void emit(uint8_t ph)
{
    if (phon_count < SAM_MAX_PHONEMES)
        phon_buf[phon_count++] = ph;
}

/* Try to look up *word* in dictionary; *len* is its length.
 * Returns true and emits phonemes on success. */
static bool dict_lookup(const char *word, uint8_t len)
{
    for (uint16_t i = 0; i < DICT_SIZE; i++)
    {
        const char *dw = word_dict[i].word;
        uint8_t j = 0;
        while (j < len && dw[j] != '\0')
        {
            if (to_upper((uint8_t)word[j]) != (uint8_t)dw[j])
                break;
            j++;
        }
        if (j == len && dw[j] == '\0')
        {
            /* Match – emit phonemes */
            const uint8_t *p = word_dict[i].phon;
            while (*p != PH_END)
                emit(*p++);
            return true;
        }
    }
    return false;
}

/* Map a single digit character to phonemes */
static void emit_digit(uint8_t ch)
{
    static const uint8_t digit_phon[][6] = {
        /* 0 */ {PH_Z,PH_IY,PH_R,PH_OH, PH_END,PH_END},
        /* 1 */ {PH_W,PH_AH,PH_N, PH_END,PH_END,PH_END},
        /* 2 */ {PH_T,PH_UW, PH_END,PH_END,PH_END,PH_END},
        /* 3 */ {PH_TH,PH_R,PH_IY, PH_END,PH_END,PH_END},
        /* 4 */ {PH_F,PH_AO,PH_R, PH_END,PH_END,PH_END},
        /* 5 */ {PH_F,PH_AA,PH_IH,PH_V, PH_END,PH_END},
        /* 6 */ {PH_S,PH_IH,PH_K,PH_S, PH_END,PH_END},
        /* 7 */ {PH_S,PH_EH,PH_V,PH_AX,PH_N, PH_END},
        /* 8 */ {PH_EH,PH_IH,PH_T, PH_END,PH_END,PH_END},
        /* 9 */ {PH_N,PH_AA,PH_IH,PH_N, PH_END,PH_END},
    };
    const uint8_t *p = digit_phon[ch - '0'];
    while (*p != PH_END)
        emit(*p++);
}

/* ---- Simple single-letter → phoneme mapping (fallback) -------------- */
static const uint8_t letter_phonemes[][3] = {
    /* A */ {PH_EH,PH_IH, PH_END},
    /* B */ {PH_B,PH_IY, PH_END},
    /* C */ {PH_S,PH_IY, PH_END},
    /* D */ {PH_D,PH_IY, PH_END},
    /* E */ {PH_IY, PH_END, PH_END},
    /* F */ {PH_EH,PH_F, PH_END},
    /* G */ {PH_JH,PH_IY, PH_END},
    /* H */ {PH_EH,PH_CH, PH_END},
    /* I */ {PH_AA,PH_IH, PH_END},
    /* J */ {PH_JH,PH_EH, PH_END},
    /* K */ {PH_K,PH_EH, PH_END},
    /* L */ {PH_EH,PH_L, PH_END},
    /* M */ {PH_EH,PH_M, PH_END},
    /* N */ {PH_EH,PH_N, PH_END},
    /* O */ {PH_OH, PH_END, PH_END},
    /* P */ {PH_P,PH_IY, PH_END},
    /* Q */ {PH_K,PH_UW, PH_END},
    /* R */ {PH_AA,PH_R, PH_END},
    /* S */ {PH_EH,PH_S, PH_END},
    /* T */ {PH_T,PH_IY, PH_END},
    /* U */ {PH_UW, PH_END, PH_END},
    /* V */ {PH_V,PH_IY, PH_END},
    /* W */ {PH_D,PH_AH,PH_B},   /* "double-u" simplified */
    /* X */ {PH_EH,PH_K,PH_S},
    /* Y */ {PH_W,PH_AA,PH_IH},
    /* Z */ {PH_Z,PH_EH,PH_D},
};

/* Spell out a single letter using NATO/letter-name phonemes */
static void emit_letter(uint8_t c)
{
    c = to_upper(c);
    if (c < 'A' || c > 'Z')
        return;
    const uint8_t *p = letter_phonemes[c - 'A'];
    for (uint8_t i = 0; i < 3 && p[i] != PH_END; i++)
        emit(p[i]);
}

/* ---- Rule-based word-to-phoneme (simplified English) ---------------- */

/* Try to convert a word using simple grapheme-to-phoneme rules.
 * *src* points to start, *len* is word length.
 * Only handles common patterns; unusual words will sound "off".       */
static void rules_word(const char *src, uint8_t len)
{
    uint8_t i = 0;
    while (i < len)
    {
        uint8_t c  = to_upper((uint8_t)src[i]);
        uint8_t cn = (i + 1 < len) ? to_upper((uint8_t)src[i + 1]) : 0;
        uint8_t cnn= (i + 2 < len) ? to_upper((uint8_t)src[i + 2]) : 0;

        /* ----- Consonant digraphs / trigraphs ----- */
        if (c == 'T' && cn == 'H') { emit(PH_TH); i += 2; continue; }
        if (c == 'S' && cn == 'H') { emit(PH_SH); i += 2; continue; }
        if (c == 'C' && cn == 'H') { emit(PH_CH); i += 2; continue; }
        if (c == 'P' && cn == 'H') { emit(PH_F);  i += 2; continue; }
        if (c == 'W' && cn == 'H') { emit(PH_W);  i += 2; continue; }
        if (c == 'N' && cn == 'G') { emit(PH_NG); i += 2; continue; }
        if (c == 'C' && cn == 'K') { emit(PH_K);  i += 2; continue; }

        /* ----- Vowel digraphs ----- */
        if (c == 'E' && cn == 'E') { emit(PH_IY); i += 2; continue; }
        if (c == 'E' && cn == 'A') { emit(PH_IY); i += 2; continue; }
        if (c == 'O' && cn == 'O') { emit(PH_UW); i += 2; continue; }
        if (c == 'O' && cn == 'U') { emit(PH_AA); emit(PH_UH); i += 2; continue; }
        if (c == 'O' && cn == 'I') { emit(PH_AO); emit(PH_IH); i += 2; continue; }
        if (c == 'O' && cn == 'Y') { emit(PH_AO); emit(PH_IH); i += 2; continue; }
        if (c == 'A' && cn == 'I') { emit(PH_EH); emit(PH_IH); i += 2; continue; }
        if (c == 'A' && cn == 'Y') { emit(PH_EH); emit(PH_IH); i += 2; continue; }
        if (c == 'A' && cn == 'W') { emit(PH_AO); i += 2; continue; }
        if (c == 'E' && cn == 'W') { emit(PH_UW); i += 2; continue; }
        if (c == 'O' && cn == 'W') { emit(PH_OH); emit(PH_UH); i += 2; continue; }
        if (c == 'I' && cn == 'G' && cnn == 'H') { emit(PH_AA); emit(PH_IH); i += 3; continue; }

        /* ----- Magic-E rule: V + C + E at end → long vowel ----- */
        if (is_vowel_letter(c) && i + 2 == len - 1
            && !is_vowel_letter(cn) && to_upper((uint8_t)src[i+2]) == 'E')
        {
            switch (c) {
                case 'A': emit(PH_EH); emit(PH_IH); break;
                case 'I': emit(PH_AA); emit(PH_IH); break;
                case 'O': emit(PH_OH); break;
                case 'U': emit(PH_UW); break;
                default:  emit(PH_IY); break;
            }
            /* emit the consonant, skip the E */
            i++;
            continue;
        }

        /* ----- Single consonants ----- */
        if (!is_vowel_letter(c))
        {
            switch (c) {
                case 'B': emit(PH_B);  break;
                case 'C': emit((cn == 'E' || cn == 'I' || cn == 'Y') ? PH_S : PH_K); break;
                case 'D': emit(PH_D);  break;
                case 'F': emit(PH_F);  break;
                case 'G': emit((cn == 'E' || cn == 'I' || cn == 'Y') ? PH_JH : PH_G); break;
                case 'H': emit(PH_HH); break;
                case 'J': emit(PH_JH); break;
                case 'K': emit(PH_K);  break;
                case 'L': emit(PH_L);  break;
                case 'M': emit(PH_M);  break;
                case 'N': emit(PH_N);  break;
                case 'P': emit(PH_P);  break;
                case 'Q': emit(PH_K);  break;
                case 'R': emit(PH_R);  break;
                case 'S': emit(PH_S);  break;
                case 'T': emit(PH_T);  break;
                case 'V': emit(PH_V);  break;
                case 'W': emit(PH_W);  break;
                case 'X': emit(PH_K); emit(PH_S); break;
                case 'Y': emit(PH_Y);  break;
                case 'Z': emit(PH_Z);  break;
                default: break;
            }
            i++;
            continue;
        }

        /* ----- Single vowels ----- */
        switch (c) {
            case 'A': emit(PH_AE); break;
            case 'E':
                /* silent final E */
                if (i == len - 1 && len > 2) { i++; continue; }
                emit(PH_EH); break;
            case 'I': emit(PH_IH); break;
            case 'O': emit(PH_AA); break;
            case 'U': emit(PH_AH); break;
            default: break;
        }
        i++;
    }
}

/* ---- main text-to-phoneme entry point ---- */

static void text_to_phonemes(const char *text)
{
    phon_count = 0;
    if (!text || !*text)
        return;

    const char *p = text;

    while (*p && phon_count < SAM_MAX_PHONEMES - 2)
    {
        /* Skip whitespace / punctuation → pause */
        while (*p && !is_alpha((uint8_t)*p) && !is_digit((uint8_t)*p))
        {
            if (*p == '.' || *p == ',')
                emit(PH_SPACE);
            p++;
        }
        if (!*p)
            break;

        /* Digit sequence → speak digit by digit */
        if (is_digit((uint8_t)*p))
        {
            while (*p && is_digit((uint8_t)*p))
            {
                emit_digit((uint8_t)*p);
                p++;
                if (*p && is_digit((uint8_t)*p))
                    emit(PH_SPACE);   /* tiny gap between digits */
            }
            emit(PH_SPACE);
            continue;
        }

        /* Alphabetic word: find word boundary */
        const char *word_start = p;
        while (*p && (is_alpha((uint8_t)*p) || *p == '_' || *p == '-'))
            p++;
        uint8_t wlen = (uint8_t)(p - word_start);
        if (wlen == 0)
        { p++; continue; }

        /* Try dictionary lookup first */
        if (wlen <= 13 && dict_lookup(word_start, wlen))
        {
            emit(PH_SPACE);
            continue;
        }

        /* If word is short (1-2 letters), spell it out */
        if (wlen <= 2)
        {
            for (uint8_t j = 0; j < wlen; j++)
            {
                emit_letter((uint8_t)word_start[j]);
                if (j + 1 < wlen)
                    emit(PH_SPACE);
            }
        }
        else
        {
            /* Use rule-based converter */
            rules_word(word_start, wlen);
        }

        emit(PH_SPACE);
    }
}

/* ====================================================================
 *  5.  SYNTHESIS ENGINE  (formant-based, integer-only)
 * ==================================================================== */

static struct {
    /* phoneme list */
    uint8_t  phones[SAM_MAX_PHONEMES];
    uint8_t  durations[SAM_MAX_PHONEMES];
    uint8_t  num_phones;

    /* playback position */
    uint8_t  cur_phone;      /* current phoneme index                  */
    uint8_t  frame_count;    /* sub-frames remaining in cur phoneme    */
    uint8_t  sample_count;   /* samples remaining in current sub-frame */

    /* formant oscillators – 8-bit phase accumulators */
    uint8_t  f1_phase;
    uint8_t  f2_phase;
    uint8_t  f3_phase;

    /* pitch (F0) – 16-bit for finer resolution */
    uint16_t pitch_phase;
    uint16_t pitch_inc;      /* base pitch increment                   */

    /* current interpolated formant parameters */
    uint8_t  cur_f1, cur_f2, cur_f3;
    uint8_t  cur_a1, cur_a2, cur_a3;
    uint8_t  cur_flags;

    /* target formant parameters (for interpolation) */
    uint8_t  tgt_f1, tgt_f2, tgt_f3;
    uint8_t  tgt_a1, tgt_a2, tgt_a3;
    uint8_t  tgt_flags;

    /* configuration */
    uint8_t  speed;          /* 1-9, higher = faster */
    uint8_t  pitch;          /* 1-9, higher pitch    */

    /* LFSR for noise generation */
    uint16_t lfsr;

    bool     speaking;
} sam;

/* ---- Load parameters for the current phoneme ---- */
static void load_phoneme_params(uint8_t ph_idx, uint8_t *f1, uint8_t *f2,
                                uint8_t *f3, uint8_t *a1, uint8_t *a2,
                                uint8_t *a3, uint8_t *flags)
{
    if (ph_idx >= PH_COUNT)
        ph_idx = PH_SPACE;
    const PhonemeInfo *pi = &phoneme_tab[ph_idx];
    *f1    = pi->f1_inc;
    *f2    = pi->f2_inc;
    *f3    = pi->f3_inc;
    *a1    = pi->a1;
    *a2    = pi->a2;
    *a3    = pi->a3;
    *flags = pi->flags;
}

/* ---- Advance to the next phoneme ---- */
static void advance_phoneme(void)
{
    sam.cur_phone++;
    if (sam.cur_phone >= sam.num_phones)
    {
        sam.speaking = false;
        return;
    }

    uint8_t ph = sam.phones[sam.cur_phone];
    sam.frame_count  = sam.durations[sam.cur_phone];
    sam.sample_count = SAM_FRAME_SAMPLES;

    /* Current parameters become "current"; load next as "target" for
     * smooth interpolation.                                           */
    sam.cur_f1 = sam.tgt_f1;  sam.cur_f2 = sam.tgt_f2;  sam.cur_f3 = sam.tgt_f3;
    sam.cur_a1 = sam.tgt_a1;  sam.cur_a2 = sam.tgt_a2;  sam.cur_a3 = sam.tgt_a3;
    sam.cur_flags = sam.tgt_flags;

    load_phoneme_params(ph,
                        &sam.tgt_f1, &sam.tgt_f2, &sam.tgt_f3,
                        &sam.tgt_a1, &sam.tgt_a2, &sam.tgt_a3,
                        &sam.tgt_flags);
}

/* Linear interpolation helper (8-bit) */
static inline uint8_t lerp8(uint8_t a, uint8_t b, uint8_t t)
{
    /* t in 0-255: 0 → a, 255 → b */
    return (uint8_t)((uint16_t)a + (((uint16_t)b - (uint16_t)a) * t + 128) / 256);
}

/* ---- Generate one audio sample (12-bit DAC value) ---- */
static uint16_t generate_sample(void)
{
    if (!sam.speaking)
        return SAM_DAC_MID;

    /* --- Sub-frame / phoneme advance --- */
    if (sam.sample_count == 0)
    {
        sam.sample_count = SAM_FRAME_SAMPLES;
        if (sam.frame_count > 0)
            sam.frame_count--;
        if (sam.frame_count == 0)
        {
            advance_phoneme();
            if (!sam.speaking)
                return SAM_DAC_MID;
        }
    }
    sam.sample_count--;

    /* --- Interpolation factor (0-255) within current phoneme --- */
    uint16_t total_samples = (uint16_t)sam.durations[sam.cur_phone] * SAM_FRAME_SAMPLES;
    uint16_t elapsed = total_samples - ((uint16_t)sam.frame_count * SAM_FRAME_SAMPLES + sam.sample_count);
    uint8_t  t;
    if (total_samples > 0)
        t = (uint8_t)((elapsed * 255U) / total_samples);
    else
        t = 0;

    /* Interpolate formant parameters */
    uint8_t f1 = lerp8(sam.cur_f1, sam.tgt_f1, t);
    uint8_t f2 = lerp8(sam.cur_f2, sam.tgt_f2, t);
    uint8_t f3 = lerp8(sam.cur_f3, sam.tgt_f3, t);
    uint8_t a1 = lerp8(sam.cur_a1, sam.tgt_a1, t);
    uint8_t a2 = lerp8(sam.cur_a2, sam.tgt_a2, t);
    uint8_t a3 = lerp8(sam.cur_a3, sam.tgt_a3, t);
    uint8_t fl = (t < 128) ? sam.cur_flags : sam.tgt_flags;

    /* --- Excitation source --- */
    int16_t excitation;
    if (fl & F_VOICED)
    {
        /* Voiced: glottal pulse (half-rectified sine at pitch freq) */
        sam.pitch_phase += sam.pitch_inc;
        uint8_t idx = (uint8_t)(sam.pitch_phase >> 8);
        int8_t  s   = sine_tab[idx];
        excitation = (s > 0) ? s : 0;        /* 0..127 */

        if (fl & F_FRIC)
        {
            /* Voiced fricative: mix some noise */
            sam.lfsr = (sam.lfsr >> 1) ^ (-(int16_t)(sam.lfsr & 1) & 0xB400u);
            int8_t noise = (int8_t)(sam.lfsr & 0x7F);
            excitation = (excitation + noise) >> 1;
        }
    }
    else if (fl & F_FRIC)
    {
        /* Unvoiced fricative: shaped noise */
        sam.lfsr = (sam.lfsr >> 1) ^ (-(int16_t)(sam.lfsr & 1) & 0xB400u);
        excitation = (int8_t)(sam.lfsr & 0x7F);   /* 0..127 */
    }
    else if (fl & F_STOP)
    {
        /* Unvoiced stop: brief burst then silence */
        excitation = 0;
        if (sam.frame_count <= 1)
        {
            sam.lfsr = (sam.lfsr >> 1) ^ (-(int16_t)(sam.lfsr & 1) & 0xB400u);
            excitation = (int8_t)(sam.lfsr & 0x7F);
        }
    }
    else
    {
        excitation = 0;   /* silence */
    }

    /* --- Formant oscillators --- */
    sam.f1_phase += f1;
    sam.f2_phase += f2;
    sam.f3_phase += f3;

    int16_t fsum = 0;
    if (a1) fsum += (int16_t)sine_tab[sam.f1_phase] * a1;
    if (a2) fsum += (int16_t)sine_tab[sam.f2_phase] * a2;
    if (a3) fsum += (int16_t)sine_tab[sam.f3_phase] * a3;
    /* fsum range: ±(127 × 15 × 3) = ±5715 */

    /* --- Modulation --- */
    int32_t out = ((int32_t)fsum * excitation) >> 7;
    /* max: ±5715 × 127 / 128 ≈ ±5670 */

    /* --- Scale to 12-bit DAC centered at 2048 --- */
    /* Map ±5670 → ±1500: factor ≈ 1500/5670 ≈ 68/256 */
    out = (out * 68) >> 8;
    /* Range: ±1504 → DAC 544..3552, well within 0..4095 */

    int16_t dac = SAM_DAC_MID + (int16_t)out;
    if (dac < 0)    dac = 0;
    if (dac > 4095) dac = 4095;

    return (uint16_t)dac;
}

/* ====================================================================
 *  6.  PUBLIC API
 * ==================================================================== */

void SAM_Init(void)
{
    memset(&sam, 0, sizeof(sam));
    sam.speed   = 5;
    sam.pitch   = 5;
    sam.lfsr    = 0x1234;
    sam.speaking = false;

    /* Default pitch: ~130 Hz (male voice)
     * pitch_inc = freq × 65536 / 8000 = 130 × 8.192 = 1065 */
    sam.pitch_inc = 1065;
}

void SAM_SetSpeed(uint8_t speed)
{
    if (speed < 1) speed = 1;
    if (speed > 9) speed = 9;
    sam.speed = speed;
}

void SAM_SetPitch(uint8_t pitch)
{
    if (pitch < 1) pitch = 1;
    if (pitch > 9) pitch = 9;
    sam.pitch = pitch;

    /* Map 1-9 → ~80-220 Hz
     * pitch_inc = freq × 65536 / 8000 = freq × 8.192
     * We approximate: base 655 (80 Hz) + step ~123 per level */
    sam.pitch_inc = 655 + (uint16_t)(pitch - 1) * 123;
}

uint16_t SAM_StartSpeaking(const char *text)
{
    sam.speaking = false;

    if (!text || !*text)
        return 0;

    /* Convert text to phoneme sequence */
    text_to_phonemes(text);

    if (phon_count == 0)
        return 0;

    /* Copy phonemes and compute durations */
    sam.num_phones = phon_count;
    uint16_t total_frames = 0;

    /* Duration scale: speed 1 = 180%, speed 5 = 100%, speed 9 = 50%
     * factor = 200 - speed×20  (in percent, range 180..20) */
    uint8_t dur_pct = 220 - sam.speed * 24;
    if (dur_pct < 30) dur_pct = 30;

    for (uint8_t i = 0; i < phon_count; i++)
    {
        sam.phones[i] = phon_buf[i];
        uint8_t ph = phon_buf[i];
        uint8_t base_dur = (ph < PH_COUNT) ? phoneme_tab[ph].dur : 4;
        uint8_t dur = (uint8_t)(((uint16_t)base_dur * dur_pct + 50) / 100);
        if (dur < 1) dur = 1;
        sam.durations[i] = dur;
        total_frames += dur;
    }

    /* Initialise synthesis state */
    sam.cur_phone    = 0;
    sam.frame_count  = sam.durations[0];
    sam.sample_count = SAM_FRAME_SAMPLES;

    sam.f1_phase = 0;
    sam.f2_phase = 0;
    sam.f3_phase = 0;
    sam.pitch_phase = 0;

    /* Set initial and target params from first phoneme */
    load_phoneme_params(sam.phones[0],
                        &sam.cur_f1, &sam.cur_f2, &sam.cur_f3,
                        &sam.cur_a1, &sam.cur_a2, &sam.cur_a3,
                        &sam.cur_flags);
    sam.tgt_f1 = sam.cur_f1;
    sam.tgt_f2 = sam.cur_f2;
    sam.tgt_f3 = sam.cur_f3;
    sam.tgt_a1 = sam.cur_a1;
    sam.tgt_a2 = sam.cur_a2;
    sam.tgt_a3 = sam.cur_a3;
    sam.tgt_flags = sam.cur_flags;

    sam.speaking = true;

    /* Estimated duration in 10 ms units:
     * total_frames × SAM_FRAME_SAMPLES / (SAM_SAMPLE_RATE / 100) */
    uint16_t duration_10ms = (total_frames * SAM_FRAME_SAMPLES * 100U)
                             / SAM_SAMPLE_RATE;
    if (duration_10ms < 5)
        duration_10ms = 5;

    return duration_10ms;
}

bool SAM_FillVoiceBuffer(void)
{
    if (!sam.speaking)
        return false;

    /* Bail out if the ring buffer is full */
    if (gVoiceBufLen >= VOICE_BUF_CAP)
        return true;   /* still speaking, but buffer full */

    /* Generate VOICE_BUF_LEN samples into the current write slot */
    uint16_t *dst = gVoiceBuf[gVoiceBufWriteIndex];
    for (uint16_t i = 0; i < VOICE_BUF_LEN; i++)
    {
        dst[i] = generate_sample();
    }

    VOICE_BUF_ForwardWriteIndex();
    gVoiceBufLen++;

    return sam.speaking;
}

bool SAM_IsSpeaking(void)
{
    return sam.speaking;
}

void SAM_Stop(void)
{
    sam.speaking = false;
}
