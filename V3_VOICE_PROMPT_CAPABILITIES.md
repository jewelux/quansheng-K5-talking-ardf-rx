# V3 Voice Prompt Capabilities — Hardware & Software Analysis

## Executive Summary

The Quansheng UV-K5v3 / UV-K1 (V3 generation) radios provide significantly
better hardware for voice prompts and audio feedback than V1 devices.
This document compares the two platforms and outlines what is now possible
for accessibility (blind-friendly ARDF) on V3 hardware.

---

## 1. Hardware Comparison

| Feature                 | V1 (DP32G030)              | V3 (PY32F071xB)              |
|------------------------|----------------------------|-------------------------------|
| **MCU**                | Ai-Thinker DP32G030        | PUYA PY32F071xB (Cortex-M0+) |
| **Flash (internal)**   | 64 KB                      | 128 KB                        |
| **RAM**                | 8 KB                       | 16 KB                         |
| **DAC**                | None (uses BK4819 tone gen)| 12-bit DAC1 with DMA          |
| **SPI Flash**          | 128 Kbit EEPROM (16 KB)    | PY25Q16 — 2 MB (16 Mbit)     |
| **Voice Chip**         | BK4819 AF tone generator   | DAC + DMA double-buffered     |
| **Audio Sample Rate**  | N/A (tones only)           | 8 kHz / 12-bit (configurable) |
| **DMA Channels**       | Limited                    | Full DMA with circular mode   |
| **RF Transceiver**     | BK4819                     | BK4819 (same chip)            |

### Key Differences

1. **12-Bit DAC with DMA**: V3 has a dedicated Digital-to-Analog Converter
   that can play actual voice samples through hardware DMA. V1 must rely on
   the BK4819 RF chip's tone generator for all audio feedback.

2. **2 MB SPI Flash**: The PY25Q16 provides 2 MB of external storage,
   compared to V1's 16 KB EEPROM. This is over 100× the storage.

3. **Double RAM/Flash**: 128 KB internal flash (vs 64 KB) and 16 KB RAM
   (vs 8 KB) provides much more room for code and data.

---

## 2. V3 Voice System Architecture

The V3 firmware implements a proper DMA-driven voice playback system:

### Components
- **`driver/voice.c`** — DAC + DMA initialization, voice buffer management
- **`driver/voice.h`** — Buffer ring API (4 × 160 samples = 640 samples)
- **`driver/py25q16.c`** — SPI flash read/write for voice data storage
- **`audio.c`** — High-level VOICE_ID scheduling and playback control

### Voice Buffer System
```
gVoiceBuf[4][160]  — 4 ring buffers, each 160 × 16-bit samples
DAC_Buf[320]       — DMA double-buffer (2 × 160 samples)
Timer TIM6         — 8 kHz trigger for DAC sample output
DMA1 Channel 3     — Circular DMA from buffer to DAC
```

### Audio Path
```
SPI Flash (PY25Q16) → gVoiceBuf[] → DAC_Buf[] → DAC1 → Audio Amplifier → Speaker
                          ↑                ↑
                    ISR fills from      DMA circular
                    SPI flash           transfer
```

### Voice Playback Flow
1. `AUDIO_PlayVoice()` queues a VOICE_ID
2. Voice data is read from SPI flash address table
3. DMA half-transfer and transfer-complete interrupts refill DAC buffer
4. DAC outputs samples at 8 kHz via TIM6 trigger

---

## 3. Storage Capacity for Voice Prompts

### V1 (EEPROM — 16 KB)
- Total: 16 KB shared with settings, calibration, channel data
- Usable for voice: ~0 KB (no voice data storage)
- Audio: Limited to BK4819 tone generation (beeps, Morse tones)

### V3 (PY25Q16 SPI Flash — 2 MB)
- Total: 2,097,152 bytes (2 MB)
- Used by firmware settings: ~64 KB (sector 0x00A000-0x00D000+)
- **Available for voice data: ~1.9 MB**

### Voice capacity calculations at 8 kHz / 8-bit mono:
| Compression | Available | Duration  |
|-------------|-----------|-----------|
| Raw 8-bit   | ~1.9 MB   | ~4 min    |
| Raw 16-bit  | ~1.9 MB   | ~2 min    |
| μ-law       | ~1.9 MB   | ~4 min    |
| ADPCM 4-bit | ~1.9 MB   | ~8 min    |
| Codec2      | ~1.9 MB   | ~16+ min  |

With ADPCM compression, **8 minutes of spoken voice prompts** is realistic.
This is enough for complete menu labels, frequency readout, and status
messages in German or English.

---

## 4. Current V1 Accessibility Features (Ported to V3)

All V1 accessibility features from the ARDF-Talking firmware have been
ported to V3:

| Feature                | V1 Status | V3 Status | Notes                          |
|------------------------|-----------|-----------|--------------------------------|
| Morse code menu labels | ✅ Done   | ✅ Ported | MENU_PlayMorseString()         |
| Morse speed control    | ✅ Done   | ✅ Ported | MENU_MORSE_SPEED, 15-70 WPM   |
| ARDF snapshot (PTT)    | ✅ Done   | ✅ Ported | 1-9 beeps for RSSI level       |
| ARDF compass mode      | ✅ Done   | ✅ Ported | PTT hold → RSSI tone sweep    |
| Negative gain N1-N9    | ✅ Done   | ✅ Ported | AF attenuation below min RF    |
| Frequency mistune      | ✅ Done   | ✅ Ported | Fine detuning for close range  |
| Snapshot speed control  | ✅ Done   | ✅ Ported | Adjustable beep rate           |
| TX removal             | ✅ Done   | ✅ Ported | ENABLE_PREVENT_TX compile flag |
| Battery Morse announce | ✅ Done   | 🔧 TODO  | Needs V3 battery API adapter   |

---

## 5. New Possibilities on V3 Hardware

### 5.1 Spoken Voice Prompts (Instead of Morse)
With the DAC+DMA voice system and 2 MB SPI flash, V3 can play actual
spoken words instead of Morse code:

- **Menu item names**: "Squelch", "Step", "Bandwidth" etc.
- **Frequency readout**: "One four five point five zero zero"
- **Status messages**: "Battery low", "Scanning", "ARDF mode"
- **RSSI level**: "Signal three" instead of 3 beeps
- **Fox number**: "Fox two" instead of Morse "2"

### 5.2 Multi-Language Support
The 2 MB flash can store voice packs for multiple languages.
With ADPCM compression (~500 bytes/second), a complete German
voice pack for all menu items + numbers could fit in ~200 KB,
leaving room for English and other languages.

### 5.3 Enhanced ARDF Audio Feedback
- **Spoken gain level**: "Gain minus thirty" or "N5"
- **Fox cycle countdown**: "Ten seconds" instead of beep
- **Directional tone sweep**: Use 12-bit DAC for smoother tones
  than the BK4819 tone generator

### 5.4 Variable Tone Quality
The 12-bit DAC can produce:
- Smoother sine waves for compass mode
- Complex waveforms (sawtooth, triangle) for different signal types
- Polyphonic tones for richer audio feedback
- Volume-faded tones for more natural audio transitions

---

## 6. Commit History Review — V1 Features & V3 Applicability

### Key V1 Commits and V3 Impact:

| V1 Feature Commit                     | V3 Impact                                    |
|---------------------------------------|----------------------------------------------|
| Morse code menu system                | Ported. V3 can additionally use spoken voice  |
| ARDF snapshot beeps                   | Ported. V3 DAC could produce richer tones     |
| Compass mode RSSI-to-tone             | Ported. V3 DAC gives smoother frequency sweep |
| Negative gain N1-N9                   | Ported directly. Same BK4819 registers        |
| Frequency mistune                     | Ported directly. Same BK4819 API              |
| TX removal (ENABLE_PREVENT_TX)        | Already supported in V3 ARDF preset           |
| Snapshot speed control                | Ported directly                               |
| Battery Morse announcement            | Needs adaptation to V3 battery API            |
| UP/DOWN key mode switching            | Ported. Same keyboard API                     |

---

## 7. Recommendations for Future V3 Development

### Short-Term (Next Version)
1. **Record German voice prompts** for all ARDF menu items
2. **Create voice pack builder** tool to compress and flash voice data
3. **Implement VOICE_PlayWord()** using DAC playback for spoken labels
4. **Add battery voltage readout** using V3 ADC capabilities

### Medium-Term
5. **Full frequency readout** in spoken digits
6. **RSSI level spoken feedback** instead of beep count
7. **Multi-language voice pack** support (DE/EN switchable)
8. **Higher-quality compass tone** using 12-bit DAC sine table

### Long-Term
9. **Text-to-speech engine** (Codec2 or similar) for dynamic text
10. **Custom voice recording** directly on the radio via SPI flash

---

## 8. Technical Notes

### V3 Memory Map (PY25Q16 SPI Flash)
```
0x000000 - 0x00A000  : Calibration, channel memory
0x00A000 - 0x00D000  : Settings, VFO state
0x00D000 - 0x00E000  : ARDF settings
0x00E000 - 0x200000  : Available for voice data (~1.9 MB)
```

### V3 Build Configuration for Voice
The ARDF-Talking preset enables `ENABLE_VOICE=true` which activates:
- Voice driver (DAC + DMA initialization)
- Voice playback scheduling in audio.c
- Voice-related menu items

### Pin Differences V1 vs V3
| Function       | V1 GPIO                | V3 GPIO                    |
|---------------|------------------------|----------------------------|
| PTT           | GPIOC Pin check        | GPIO_IsPttPressed()        |
| Audio Path    | GPIO_SetBit/ClearBit   | GPIO_EnableAudioPath()     |
| Speaker       | gEnableSpeaker flag    | gEnableSpeaker flag (same) |

---

*Document created for the Quansheng UV-K5 ARDF Talking project*
*Author: DO9RE (Richard)*
*Date: 2026-04-13*
