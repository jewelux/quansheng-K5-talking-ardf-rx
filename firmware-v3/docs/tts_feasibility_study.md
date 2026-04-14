# TTS-Engine Machbarkeitsstudie für PY32F071 (Cortex-M0+)

## Zielplattform

| Parameter         | Wert                                           |
|-------------------|-------------------------------------------------|
| MCU               | PY32F071xB (ARM Cortex-M0+)                    |
| Taktfrequenz      | 48 MHz (max. 72 MHz lt. Datenblatt)             |
| Flash (intern)    | 128 KB (118 KB nutzbar nach Bootloader)         |
| SRAM              | 16 KB                                           |
| SPI-Flash extern  | PY25Q16 — 2 MB (16 Mbit), davon ~716 KB frei für Voice-Daten |
| DAC               | 12-Bit DAC Kanal 1 (PA4), via DMA bei 8 kHz     |
| FPU               | Keine (nur Integer-Arithmetik)                   |

## Zusammenfassung

Echte dynamische Text-to-Speech (beliebiger Text → Sprache in Echtzeit) ist auf
dieser Plattform **extrem eingeschränkt** aber **grundsätzlich möglich** — mit
starken Kompromissen bei Sprachqualität und Sprachabdeckung. Die realistische
Empfehlung ist ein **hybrider Ansatz**: voraufgezeichnete Clips für Standardansagen
plus ein minimaler Formant-/Phonem-Synthesizer für dynamische Inhalte (Zahlen,
Buchstaben, Frequenzen).

---

## Option 1: SAM (Software Automatic Mouth) — Empfohlen ⭐

### Beschreibung
SAM ist ein legendärer Sprachsynthesizer aus 1982 (Commodore 64, Atari), der Text
in Phoneme und dann in 8-Bit-PCM-Audio umwandelt. Die Implementierung ist
vollständig in C, ohne Floating-Point, und extrem kompakt.

### Ressourcenbedarf

| Ressource | Verbrauch              | Verfügbar | Machbar? |
|-----------|------------------------|-----------|----------|
| Flash     | ~8–16 KB Code          | 118 KB    | ✅ Ja    |
| RAM       | ~2–4 KB (Phonem-Tabellen + Puffer) | 16 KB | ✅ Ja |
| CPU       | Echtzeit bei 8 kHz auf 1 MHz möglich | 48 MHz | ✅ Ja |

### Sprachqualität
- Robotisch, retro, aber verständlich
- Englisch nativ; Deutsch mit angepasster Phonem-Tabelle möglich
- Prosodie minimal, keine natürliche Intonation

### Relevante GitHub-Projekte

| Projekt | Beschreibung | Sprache | Sterne | URL |
|---------|-------------|---------|--------|-----|
| **s-macke/SAM** | Original C-Port des 1982er SAM | C | ~2300 | https://github.com/s-macke/SAM |
| **Bakisha/STM32SAM** | SAM für STM32 (Arduino IDE), bereits Cortex-M portiert | C++ | 17 | https://github.com/Bakisha/STM32SAM |
| **earlephilhower/ESP8266SAM** | SAM für ESP8266, saubere C-Struktur | C | 349 | https://github.com/earlephilhower/ESP8266SAM |

### Portierungs-Aufwand
- **Gering** (1–3 Tage): STM32SAM kann fast direkt verwendet werden, da PY32F071
  ebenfalls Cortex-M ist. Hauptänderungen:
  - Audio-Ausgabe auf DAC1/DMA umleiten (statt PWM/I2S)
  - Arduino-Abhängigkeiten entfernen
  - Phonem-Tabelle für deutsche Umlaute erweitern
- SAM generiert 8-Bit-Samples bei konfigurierbarer Rate — passt zum bestehenden
  8-kHz-DAC-Setup über `VOICE_SAMPLES[]` Lookup-Tabelle

### Integration in Firmware
```
Text "Frequenz 145.500"
      ↓ SAM Phonem-Konverter (~200 Byte RAM)
Phoneme "FREH-KWENTS AYN FEER FUENF PUNKT FUENF NULL NULL"
      ↓ SAM Formant-Synthesizer (~2 KB RAM)
8-bit PCM Samples @ 8 kHz
      ↓ VOICE_SAMPLES[] Lookup (bestehend)
12-bit DAC via DMA (bestehend)
```

---

## Option 2: LPC-basierte Synthese (Talkie) — Bedingt geeignet

### Beschreibung
Linear Predictive Coding (LPC) dekodiert vorab komprimierte Sprachdaten.
Dies ist **keine echte TTS** — alle Wörter/Phrasen müssen vorher als
LPC-Koeffizienten codiert und gespeichert werden. Dafür ist die Qualität
besser als bei Formant-Synthese.

### Ressourcenbedarf

| Ressource | Verbrauch              | Verfügbar | Machbar? |
|-----------|------------------------|-----------|----------|
| Flash     | ~4 KB Code + ~50–200 Byte/Wort | 118 KB | ✅ Ja |
| RAM       | ~1–2 KB                | 16 KB     | ✅ Ja    |
| CPU       | Minimal (Tabellen-Lookup) | 48 MHz | ✅ Ja    |

### Relevante Projekte

| Projekt | Beschreibung | URL |
|---------|-------------|-----|
| **going-digital/Talkie** | Original Arduino LPC-Bibliothek | https://github.com/going-digital/Talkie |
| **ArminJo/Talkie** | Erweiterte Version mit mehr Wörtern | https://github.com/ArminJo/Talkie |

### Einschränkungen
- **Kein dynamischer Text**: Jedes Wort muss als LPC-Daten vorliegen
- Faktisch dasselbe wie das bestehende Voice-Pack-System, nur komprimierter
- Für deutsch müssten alle Wörter/Phoneme einzeln codiert werden
- **Empfehlung**: Nur sinnvoll als Ergänzung zum bestehenden System, nicht als Ersatz

---

## Option 3: Klatt Formant-Synthesizer — Möglich mit Aufwand

### Beschreibung
Dennis Klatts Formant-Synthesizer (1980) modelliert den Vokaltrakt mit
kaskadierten Biquad-Filtern. Komplexer als SAM, aber flexibler und
natürlicher klingend.

### Ressourcenbedarf

| Ressource | Verbrauch              | Verfügbar | Machbar? |
|-----------|------------------------|-----------|----------|
| Flash     | ~12–20 KB Code         | 118 KB    | ✅ Ja    |
| RAM       | ~4–6 KB                | 16 KB     | ⚠️ Knapp |
| CPU       | Hoch (Biquad-Filter in Echtzeit) | 48 MHz | ⚠️ Grenzwertig |

### Einschränkungen
- Keine fertige Cortex-M0 Portierung bekannt
- Benötigt Fixed-Point-Arithmetik (Q15/Q31), da keine FPU
- Portierungs-Aufwand: ~1–2 Wochen
- RAM-Verbrauch für 3–4 Formant-Filter + Puffer nahe am Limit

---

## Option 4: eSpeak-ng — Nicht machbar ❌

### Beschreibung
eSpeak-ng ist ein vollwertiger TTS mit Mehrsprachunterstützung und guter
Verständlichkeit. Allerdings:

| Ressource | Verbrauch              | Verfügbar | Machbar? |
|-----------|------------------------|-----------|----------|
| Flash     | ~200–500 KB            | 118 KB    | ❌ Nein  |
| RAM       | ~32–64 KB              | 16 KB     | ❌ Nein  |
| CPU       | Hoch                   | 48 MHz    | ⚠️ Grenzwertig |

**Fazit**: Nicht portierbar auf PY32F071.

---

## Option 5: Flite (CMU Festival Lite) — Nicht machbar ❌

| Ressource | Verbrauch              | Verfügbar | Machbar? |
|-----------|------------------------|-----------|----------|
| Flash     | ~300 KB+ (nur Sprach-Daten) | 118 KB | ❌ Nein |
| RAM       | ~256 KB–1 MB           | 16 KB     | ❌ Nein  |

**Fazit**: Benötigt mindestens Cortex-M4 mit 256+ KB RAM.

---

## Empfohlener Ansatz: Hybrid (SAM + Voice-Pack)

### Architektur

```
┌─────────────────────────────────────┐
│         Accessibility Layer         │
│                                     │
│  Modus:  Morse │ Voice │ SAM-TTS   │
│             │       │       │       │
│             ▼       ▼       ▼       │
│          BK4819   DAC/DMA  DAC/DMA  │
│          Tone     Voice    SAM      │
│          Gen.     Pack     Synth    │
└─────────────────────────────────────┘
```

### Phase 1: SAM-Basis (Sofort umsetzbar)
1. **SAM C-Code** aus `s-macke/SAM` oder `Bakisha/STM32SAM` portieren
2. Arduino-Abhängigkeiten entfernen, `main.c`-Style C verwenden
3. Audio-Ausgabe auf bestehenden DAC1/DMA-Pfad umleiten
4. Phonem-Tabelle für Deutsch (Umlaute ä/ö/ü/ß) erweitern
5. Integration als dritter Accessibility-Modus (`ACCESS_MODE_SAM`)

### Phase 2: Wort-Verkettung (Kurz danach)
1. Häufigste Menü-Begriffe als SAM-Phonem-Strings in Flash speichern
2. Zahlen 0–9, "Punkt", "MHz", "Kanal" als Phonem-Sequenzen
3. Dynamische Frequenz-Ansage: `"145.500"` → SAM-Phoneme für jede Ziffer

### Phase 3: Optimierung (Optional)
1. SAM-Ausgabe durch einfachen Tiefpassfilter glätten (1 Biquad, ~20 Byte RAM)
2. Sprechgeschwindigkeit anpassbar machen
3. Eventuell Klatt-Filter als Alternative für natürlicheren Klang

### Geschätzter RAM-Verbrauch (Phase 1)

| Komponente                  | Bytes  |
|-----------------------------|--------|
| SAM Phonem-Tabelle          | ~800   |
| SAM Formant-Puffer          | ~1200  |
| Audio-Ausgabepuffer (2×160) | 640    |
| Phonem-String-Puffer        | 128    |
| **Gesamt**                  | **~2,8 KB** |

Verfügbar: 16 KB − bestehender Firmware-Verbrauch (~10 KB) ≈ **6 KB frei** → passt.

### Geschätzter Flash-Verbrauch (Phase 1)

| Komponente              | Bytes  |
|-------------------------|--------|
| SAM Synthese-Code       | ~8000  |
| Text-zu-Phonem Regeln   | ~4000  |
| Deutsche Phonem-Regeln  | ~2000  |
| **Gesamt**              | **~14 KB** |

Verfügbar: 118 KB − bestehende Firmware (~90 KB) ≈ **28 KB frei** → passt.

---

## Konkrete Schritte für die nächste Agenten-Sitzung

1. **Repository klonen**: `git clone https://github.com/s-macke/SAM.git`
2. **Relevante C-Dateien extrahieren**: `sam.c`, `reciter.c`, `render.c`, `debug.c`
3. **Arduino/OS-Abhängigkeiten entfernen** (stdio, stdlib ersetzen)
4. **Audio-Ausgabe umleiten**: SAM-Puffer → `gVoiceBuf[]` → DAC/DMA
5. **Neuen Modus hinzufügen**: `ACCESS_MODE_SAM` in `misc.h`
6. **Integration in Menü-Ansage**: In `MENU_PlayMorseForCurrentItem()` als
   dritte Option neben Morse und Voice-Pack
7. **Deutsche Phonem-Tabelle**: `reciter.c` Regeltabelle um ä→EH, ö→ER, ü→UE erweitern
8. **Testen**: Frequenz-Ansage, Menü-Namen, Zahlen

---

## Quellenverzeichnis

1. SAM Original C-Port: https://github.com/s-macke/SAM
2. STM32 SAM Port: https://github.com/Bakisha/STM32SAM
3. ESP8266 SAM: https://github.com/earlephilhower/ESP8266SAM
4. Talkie LPC: https://github.com/going-digital/Talkie
5. PY32F071 Datenblatt: https://www.py32.org/en/mcu/PY32F07x
6. PY32F071 Reference Manual: https://download.py32.org/ReferenceManual/en/PY32F07X%20Reference%20Manual%20v0.1_EN.pdf
