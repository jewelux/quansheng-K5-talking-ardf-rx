# Voice Prompt Evaluation — Optionen fuer erweiterte Sprachausgabe

**Erstellt:** 2026-04-12  
**Autor:** Copilot Cloud Agent im Auftrag von DO9RE  
**Zweck:** Entscheidungsdokument fuer kuenftige Agenten-Sitzungen

---

## Inhaltsverzeichnis

1. [Hardware-Grundlagen](#1-hardware-grundlagen)
2. [Aktueller Zustand der Sprachausgabe](#2-aktueller-zustand-der-sprachausgabe)
3. [Menüpunkte ohne Sprachausgabe](#3-menuepunkte-ohne-sprachausgabe)
4. [Option A — Vorhandenen Voice-Chip erweitern/modifizieren](#option-a)
5. [Option B — PCM-Rohdaten in Flash/EEPROM ablegen und per BK4819 abspielen](#option-b)
6. [Option C — eSpeak-generierte Einzelbuchstaben/Zahlen konkateniert abspielen](#option-c)
7. [Option D — Morse-System erweitern (Fallback, bereits implementiert)](#option-d)
8. [Zusammenfassung und Empfehlung](#zusammenfassung)
9. [Entscheidungsblock](#entscheidungsblock)
10. [Shell-Script fuer Audio-Generierung (MSYS2)](#shell-script)

---

## 1. Hardware-Grundlagen <a name="1-hardware-grundlagen"></a>

### Prozessor: DP32G030 (ARM Cortex-M0)

| Ressource        | Groesse     | Nutzung aktuell                        |
| ---------------- | ----------- | -------------------------------------- |
| Interner Flash   | 64 KB       | Firmware belegt ~45 KB (von 60 KB nutzbar) |
| RAM              | 16 KB       | Stack + Variablen                      |
| EEPROM (extern)  | 8 KB (0x0000–0x1FFF) | Konfiguration, Kanaele, Kalibrierung  |

**Freier Flash:** ca. 15 KB (15.360 Bytes). Davon muss Puffer fuer kuenftige
Firmware-Erweiterungen bleiben. Realistisch nutzbar: **8–12 KB fuer Audio-Daten**.

**Freier EEPROM:** Sehr wenig. Die meisten Bereiche sind durch Kanal-Daten und
Konfiguration belegt. Die EEPROM-Schreibfunktion lehnt Adressen >= 0x2000 ab.
EEPROM ist daher **kein geeigneter Speicher fuer Audio-Daten**.

### Externer Voice-Chip (JQ8400-artig oder kompatibel)

Die aktuelle Firmware steuert einen **externen Voice-ROM-Chip** ueber zwei GPIO-Pins:

```
GPIOA Pin 12 = VOICE_0 (Clock / Latch)   — geteilt mit Keyboard!
GPIOA Pin 13 = VOICE_1 (Data)            — geteilt mit Keyboard!
```

**Protokoll:** Serielles 8-Bit-Protokoll (MSB first, Bit-Banging):
1. VOICE_0 Puls (20 ms high) = Start
2. 8 Datenbits ueber VOICE_1 getaktet mit VOICE_0 (1 ms pro Bit)
3. Der Voice-Chip spielt das zum Index gehoerende Audio-Sample ab

**Adressierung:**
- Chinesisch: VoiceID + 0x10 (Offset `VOICE_ID_CHI_BASE`)
- Englisch: VoiceID + 0x60 (Offset `VOICE_ID_ENG_BASE`)
- 58 chinesische Clips, 76 englische Clips sind im ROM vorprogrammiert
- Clip-Laengen in `VoiceClipLengthChinese[]` und `VoiceClipLengthEnglish[]` kodiert (in 10ms-Einheiten)

**Wichtig:** Dieser Chip ist ein **ROM** — die Audiodaten sind ab Werk eingebrannt
und koennen **nicht per Firmware geaendert oder erweitert** werden. Es gibt keinen
Schreibzugang zum Voice-ROM ueber die GPIO-Schnittstelle.

### BK4819 RF-Chip Audio-Faehigkeiten

Der BK4819 ist der zentrale HF-/Audio-Chip und hat einen internen DAC/Speaker-Pfad:

- **Tonerzeugung:** `BK4819_PlayTone(frequency, gain)` — erzeugt reine Sinustoene
  (wird fuer Beeps und Morse verwendet)
- **AF-Pfad-Modi:** MUTE, FM, AM, BEEP, BASEBAND (RAW/USB), CTCO, FSKO
- **Audio-Ausgang:** ueber `GPIOC Pin 4` (AUDIO_PATH) zum Lautsprecher
- **Kein PCM/Sample-Playback:** Der BK4819 hat **keine Moeglichkeit**, beliebige
  PCM-Samples abzuspielen. Er kann nur Toene, demodulierten Empfang oder Beeps ausgeben.

### Audio-Pfad-Zusammenfassung

```
Voice-ROM-Chip ──────────────────────────┐
                                          ├──▶ Audio-Mux ──▶ Verstaerker ──▶ Lautsprecher
BK4819 (Tone/RX/Beep) ──────────────────┘
                                          ▲
                                   GPIOC Pin 4
                                   (AUDIO_PATH enable)
```

Der Cortex-M0 hat **keinen eigenen DAC**. Audio-Ausgabe ist nur moeglich ueber:
1. Den Voice-ROM-Chip (vorprogrammierte Clips)
2. Den BK4819 (Toene, Beeps, RX-Audio)

---

## 2. Aktueller Zustand der Sprachausgabe <a name="2-aktueller-zustand-der-sprachausgabe"></a>

### Voice-Prompt-System (ENABLE_VOICE=1)

Bereits implementiert und aktiv:
- **76 englische Voice-Clips** im externen Voice-ROM (Zahlen 0–9, 10, 100, 11–20,
  30–90, plus Phrasen wie "Welcome", "Lock", "Scanning begin", etc.)
- Voice-Queue: bis zu 8 Voice-IDs koennen verkettet werden
- Zahlenansage: `AUDIO_SetDigitVoice()` zerlegt Werte in Einzelziffern
- Einstellung: "Voice" Menuepunkt mit Off / Chinese / English

### Morse-Accessibility-System

Fuer Menuepunkte **ohne** Voice-ROM-Clip implementiert:
- Morse-Ausgabe ueber BK4819 Tonerzeuger (880 Hz)
- Einstellbare Geschwindigkeit: 15–70 WPM
- Interruptible (Tastendruck bricht ab)
- Aktuell 13 Menuepunkte haben Morse-Labels (siehe Abschnitt 3)

### Ablauf bei Menuewechsel (`MENU_PlayCurrentMenuVoice`)

```
1. Gibt es eine voice_id != VOICE_ID_INVALID?
   → JA: Voice-ROM-Clip abspielen (z.B. "Frequency Step")
   → NEIN: Weiter zu 2.

2. Gibt es ein Morse-Label fuer dieses Menue?
   → JA: Morse-Ausgabe des Labels
   → NEIN: Weiter zu 3.

3. Fallback: "Menu" + Nummer (Voice-ROM)
```

---

## 3. Menuepunkte ohne Sprachausgabe <a name="3-menuepunkte-ohne-sprachausgabe"></a>

### Custom Menu Layout (ENABLE_CUSTOM_MENU_LAYOUT=1, aktive Konfiguration)

| Menue-Text | Voice-ROM-Clip        | Morse-Label       | Status            |
| ---------- | --------------------- | ----------------- | ----------------- |
| Step       | VOICE_ID_FREQUENCY_STEP | —               | ✅ Voice-Clip     |
| AMFM       | —                     | "AM FM"           | ✅ Morse          |
| ARDF       | —                     | "ARDF"            | ✅ Morse          |
| NumFox     | —                     | "NUMBER FOX"      | ✅ Morse          |
| FoxDur     | —                     | "FOX DURATION"    | ✅ Morse          |
| ActFox     | —                     | "ACTIVE FOX"      | ✅ Morse          |
| TiRst      | —                     | "TIME RESET"      | ✅ Morse          |
| GainRe     | —                     | "GAIN REMEMBER"   | ✅ Morse          |
| EndSig     | —                     | "END SIGNAL"      | ✅ Morse          |
| SnpSpd     | —                     | "SNAPSHOT SPEED"  | ✅ Morse          |
| BackLt     | —                     | "BACKLIGHT"       | ✅ Morse          |
| BLMin      | —                     | "BACKLIGHT MINIMUM"| ✅ Morse         |
| BLMax      | —                     | "BACKLIGHT MAXIMUM"| ✅ Morse         |
| Beep       | VOICE_ID_BEEP_PROMPT  | —                 | ✅ Voice-Clip     |
| Voice      | VOICE_ID_VOICE_PROMPT | —                 | ✅ Voice-Clip     |
| Morse      | —                     | "MORSE SPEED"     | ✅ Morse          |
| Reset      | VOICE_ID_INITIALISATION | —               | ✅ Voice-Clip     |

**Ergebnis:** Alle 17 Menuepunkte haben entweder Voice-ROM-Clip oder Morse-Ausgabe.
12 davon nutzen Morse (kein Voice-ROM-Clip verfuegbar).

---

## Option A — Vorhandenen Voice-Chip erweitern/modifizieren <a name="option-a"></a>

### Bewertung: ❌ NICHT MOEGLICH

Der externe Voice-ROM-Chip enthaelt **ab Werk eingebrannte** Audio-Clips.
Es gibt keine Moeglichkeit:
- Neue Clips hinzuzufuegen
- Bestehende Clips zu aendern
- Einen Schreibzugriff auf den Chip durchzufuehren

Das serielle GPIO-Protokoll ist **nur ein Wiedergabe-Trigger** (8-Bit Index → Abspielen).

**Die einzige Alternative waere**, den Voice-Chip physisch auszuloeten und durch
einen programmierbaren Chip (z.B. WT588D, JQ8400-FL mit SD-Karte) zu ersetzen.
Das ist eine **Hardware-Modifikation** und fuer eine reine Firmware-Loesung nicht relevant.

```
## ENTSCHEIDUNG Option A:
## [ ] Ja, Hardware-Mod durchfuehren (Voice-Chip ersetzen)
## [x] Nein, nur Firmware-Loesung (Standard)
```

---

## Option B — PCM-Rohdaten in Flash ablegen und per Software abspielen <a name="option-b"></a>

### Konzept

eSpeak oder ein anderer TTS-Engine erzeugt WAV-Dateien auf dem PC.
Diese werden mit FFmpeg in 8-Bit unsigned PCM (8000 Hz mono) konvertiert
und als const-Array im Firmware-Flash abgelegt.
Die Firmware spielt diese ueber einen **Software-PWM** oder **Bit-Banging-DAC** ab.

### Technische Herausforderungen

#### Problem 1: Kein DAC im Cortex-M0
Der DP32G030 hat **keinen analogen DAC-Ausgang**. Audio muesste ueber:
- **Software-PWM auf einem GPIO** erzeugt werden (Timer-Interrupt + Pulsweite)
- Der BK4819 BEEP-Kanal kann nur Einzelfrequenzen, kein PCM

#### Problem 2: Audio-Pfad
Der Lautsprecher haengt am BK4819-Ausgang und am Voice-Chip-Ausgang.
Ein GPIO-PWM-Signal muesste irgendwie in diesen Pfad eingespeist werden.
Es gibt **keinen direkten GPIO → Lautsprecher Pfad**.

#### Problem 3: Speicherverbrauch
Bei 8000 Hz, 8 Bit mono (niedrigste brauchbare Qualitaet):
- 1 Sekunde = 8.000 Bytes
- "ARDF" gesprochen ≈ 0,8s = 6.400 Bytes
- "Backlight" ≈ 0,7s = 5.600 Bytes
- Alle 12 fehlenden Labels ≈ 8–10 Sekunden = **64.000–80.000 Bytes**

Das uebersteigt den freien Flash (15 KB) um ein Vielfaches.

#### Kompression (ADPCM / u-Law)
| Codec      | Ratio | 10s Audio | Passt in Flash? |
| ---------- | ----- | --------- | --------------- |
| Raw 8-bit  | 1:1   | 80.000 B  | ❌ Nein         |
| u-Law 4kHz | 2:1   | 40.000 B  | ❌ Nein         |
| IMA-ADPCM  | 4:1   | 20.000 B  | ❌ Knapp        |
| LPC-10     | 26:1  | ~3.000 B  | ⚠️ Moeglich     |

LPC-10 wuerde passen, aber der **Decoder braucht ~4–6 KB Flash** fuer Code
und ~1 KB RAM fuer Zustandsspeicher — bei 16 KB Gesamt-RAM kritisch.

### Bewertung: ⚠️ THEORETISCH MOEGLICH, PRAKTISCH SEHR SCHWIERIG

- Kein DAC-Ausgang vorhanden → Software-PWM auf welchem Pin?
- Kein direkter Pfad zum Lautsprecher fuer Software-generiertes Audio
- Speicher zu knapp fuer mehr als 2–3 Woerter
- Decoder-Code verbraucht zusaetzlichen Flash und RAM
- Timer-Interrupts koennten mit bestehender Firmware kollidieren

**ABER:** Ein alternativer Ansatz waere moeglich — den **BK4819 Tone-Generator
schnell umzustimmen**, um eine Art Frequenzmodulation zu erzeugen.
Das klingt nicht wie Sprache, sondern wie ein Vokoder/Roboter. Qualitaet
waere fuer Blinde vermutlich nicht akzeptabel.

```
## ENTSCHEIDUNG Option B:
## [ ] Ja, PCM-Playback implementieren (erfordert Hardware-Analyse fuer DAC-Pfad)
## [ ] Ja, aber nur LPC-10 kodiert (spart Platz, braucht Decoder)
## [x] Nein, zu aufwaendig / kein geeigneter Audio-Pfad (Standard)
```

---

## Option C — eSpeak-Einzelbuchstaben/Zahlen konkateniert abspielen <a name="option-c"></a>

### Konzept

Statt ganzer Woerter nur **36 Grundelemente** erzeugen:
- 26 Buchstaben (A–Z)
- 10 Ziffern (0–9)

Diese werden vom Gerät per Buchstabier-Verfahren konkateniert:
"ARDF" → A, R, D, F (4 Clips nacheinander)

### Speicherberechnung

| Element        | Dauer  | Raw 8kHz 8-bit | IMA-ADPCM 4:1 |
| -------------- | ------ | -------------- | -------------- |
| 1 Buchstabe    | 0,3s   | 2.400 B        | 600 B          |
| 36 Elemente    | 10,8s  | 86.400 B       | 21.600 B       |
| 26 Buchstaben  | 7,8s   | 62.400 B       | 15.600 B       |

**Selbst nur Buchstaben in ADPCM passen nicht in den freien Flash.**

### Reduktion: Nur die noetigsten Buchstaben

Die 12 Morse-Labels verwenden diese Buchstaben:
A, B, C, D, E, F, G, H, I, K, L, M, N, O, P, R, S, T, U, X = **20 Buchstaben**

| Variante                   | ADPCM 4:1 |
| -------------------------- | ---------- |
| 20 Buchstaben × 600 B     | 12.000 B   |
| 20 Buchstaben × 400 B (4kHz) | 8.000 B |

Bei **4 kHz Samplerate, ADPCM** waeren 20 Buchstaben in ~8 KB machbar.
Das waere an der Grenze des verfuegbaren Flash.

### Bewertung: ⚠️ GLEICHES DAC-PROBLEM WIE OPTION B

Auch hier besteht das Grundproblem: Es gibt keinen Pfad, um Software-generiertes
Audio zum Lautsprecher zu bringen. Die technische Huerde ist identisch mit Option B.

**Wenn Option B geloest wird (DAC-Pfad gefunden), dann ist Option C die
speichersparendere Variante davon.**

```
## ENTSCHEIDUNG Option C:
## [ ] Ja, Buchstabier-System (nur wenn Option B DAC-Pfad geloest)
## [ ] Ja, reduziertes Set (nur noetige Buchstaben)
## [x] Nein, gleiches DAC-Problem wie Option B (Standard)
```

---

## Option D — Morse-System erweitern und verbessern <a name="option-d"></a>

### Konzept

Das bestehende Morse-System weiter ausbauen und verbessern:
- Bereits funktional fuer alle 12 Menüpunkte ohne Voice-Clip
- Kein zusaetzlicher Speicher fuer Audio-Daten noetig
- Nutzt den BK4819-Tonerzeuger (funktioniert zuverlaessig)

### Erweiterungsmoeglichkeiten

#### D1: Morse fuer Werte-Ansagen
Aktuell werden Menuewerte nur per Voice-ROM angesagt (Zahlen).
Erweitern um Morse-Ausgabe fuer:
- Aktuelle Werte (Ein/Aus, Zahlen)
- Statusmeldungen
- Frequenzansage per Morse

**Flash-Kosten:** ~200–500 Bytes zusaetzlicher Code

#### D2: Zweistufiges Feedback
- **Schnelle Identifikation:** Kurzer markanter Ton-Code pro Menue
  (z.B. 2 kurze Toene = ARDF, 3 kurze = NumFox)
- **Ausfuehrlich auf Anfrage:** Volles Morse-Label bei langem Tastendruck

**Flash-Kosten:** ~300–600 Bytes

#### D3: Phonetisches Alphabet per Morse
Statt "ARDF" als A-R-D-F morsen, "Alpha Romeo Delta Foxtrot" morsen
(laenger, aber leichter verstaendlich fuer Anfaenger)

**Flash-Kosten:** ~400–800 Bytes (Label-Strings)

#### D4: Tonfhoehen-Kodierung
Verschiedene Tonhoehen fuer verschiedene Menuekategorien:
- 600 Hz = ARDF-Menues
- 800 Hz = Display-Menues (Backlight)
- 1000 Hz = System-Menues (Reset, Voice, Morse)

**Flash-Kosten:** ~100–200 Bytes

### Bewertung: ✅ EMPFOHLEN — sofort umsetzbar, kein Hardware-Risiko

```
## ENTSCHEIDUNG Option D:
## [x] D1: Morse-Wertansagen erweitern (empfohlen)
## [ ] D2: Zweistufiges Feedback (Kurztöne + Morse auf Anfrage)
## [ ] D3: Phonetisches Alphabet
## [ ] D4: Tonhoehen-Kodierung nach Kategorie
## [ ] Alles aus D1–D4
## [ ] Keine weiteren Morse-Erweiterungen
```

---

## Option E — Alternativer Hardware-Ansatz: BK4819-Toene als Sprach-Ersatz <a name="option-e"></a>

### Konzept: "Tonsprache" / akustische Icons

Statt echter Sprache kurze, einpraegsame **Ton-Melodien** pro Menuepunkt:
- Jedes Menue bekommt eine 0,5–1s lange Tonfolge
- Wie Klingeltoene / akustische Symbole
- Blinde Benutzer lernen die Zuordnung (wie bei Blindenschrift)

### Beispiele

```
ARDF:           .-. (lang-kurz-lang, wie "A" in Morse, aber als Melodie)
Backlight:      aufsteigende Tonleiter (hell = hoch)
Number Fox:     schnelles "pip pip pip" (Anzahl)
Snapshot Speed: beschleunigendes "pip..pip.pip pip"
```

### Bewertung: ✅ SOFORT MACHBAR, ~500 Bytes Flash

Nutzt ausschliesslich den BK4819-Tonerzeuger. Kein DAC noetig.
Kann parallel zu Morse existieren.

```
## ENTSCHEIDUNG Option E:
## [ ] Ja, akustische Icons zusaetzlich zu Morse
## [x] Nein, Morse reicht (Standard)
```

---

## Zusammenfassung und Empfehlung <a name="zusammenfassung"></a>

| Option | Machbarkeit | Flash-Bedarf | Hardware-Mod noetig | Empfehlung |
| ------ | ----------- | ------------ | ------------------- | ---------- |
| A: Voice-ROM erweitern | ❌ | — | Ja (Chip-Tausch) | Nein |
| B: PCM in Flash | ⚠️ | 8–80 KB | Evtl. (DAC-Pfad) | Nein |
| C: Buchstabier-System | ⚠️ | 8–15 KB | Evtl. (DAC-Pfad) | Nur wenn B geloest |
| D: Morse erweitern | ✅ | 0,2–1 KB | Nein | **JA** |
| E: Akustische Icons | ✅ | 0,5 KB | Nein | Optional |

### Primaere Empfehlung

**Option D (Morse-Erweiterungen)** ist der einzige Weg, der:
1. Sofort implementierbar ist
2. Keinen zusaetzlichen Speicher fuer Audio-Daten braucht
3. Keine Hardware-Modifikation erfordert
4. Bereits im Code funktional ist und nur ausgebaut werden muss

### Sekundaere Empfehlung

**Option E (Akustische Icons)** als Ergaenzung — kurze Tonfolgen geben schnellere
Orientierung als volles Morse-Buchstabieren.

### Langfristige Vision

Wenn jemand den **DAC-Pfad-Hack** fuer den DP32G030 loest (z.B. PWM auf einem
unbenutzten GPIO mit externem RC-Tiefpass zum Audio-Mux), waere Option C
(Buchstabier-System mit ADPCM) die naechste Stufe.

---

## Entscheidungsblock <a name="entscheidungsblock"></a>

```
## ============================================================
## ENTSCHEIDUNGEN fuer die naechste Agenten-Sitzung
## 
## Anleitung: Kommentarzeichen (#) vor der gewuenschten Option
## entfernen. Nur EINE Option pro Block aktiv lassen.
## Die naechste Sitzung liest diesen Block und implementiert
## die gewaehlte Option.
## ============================================================

## --- HAUPT-STRATEGIE ---
## [x] STRATEGIE_MORSE: Morse-System ausbauen (Option D)
## [ ] STRATEGIE_ICONS: Akustische Ton-Icons (Option E)
## [ ] STRATEGIE_BEIDE: Morse + akustische Icons (D + E)
## [ ] STRATEGIE_PCM:   PCM-Playback erforschen (Option B/C)

## --- MORSE-ERWEITERUNGEN (nur wenn STRATEGIE_MORSE oder _BEIDE) ---
## [x] MORSE_WERTE: Menuewerte per Morse ansagen
## [ ] MORSE_ZWEISTUFIG: Kurzton + Morse auf Anfrage
## [ ] MORSE_PHONETISCH: NATO-Alphabet statt Buchstaben
## [x] MORSE_TONHOEHEN: Kategorisierte Tonhoehen

## --- AKUSTISCHE ICONS (nur wenn STRATEGIE_ICONS oder _BEIDE) ---
## [ ] ICONS_MELODIE: Kurze Melodien pro Menüpunkt
## [ ] ICONS_RHYTHMUS: Rhythmische Muster (Morse-artig, aber melodisch)

## --- ZUSAETZLICHE FEATURES ---
## [x] STATUS_MORSE: Statusmeldungen per Morse (Batterie, Frequenz)
## [ ] FREQ_MORSE: Frequenz per Morse ansagen
## [ ] ARDF_MORSE: ARDF-Status per Morse (aktiver Fuchs, Restzeit)
```

---

## Shell-Script fuer Audio-Generierung (MSYS2) <a name="shell-script"></a>

Das folgende Script ist vorbereitet fuer den Fall, dass Option B oder C
umgesetzt wird. Es erzeugt mit eSpeak und FFmpeg Rohdaten, die als
C-Header-Array in die Firmware eingebunden werden koennten.

**Datei:** `firmware-v1/utils/generate_voice_prompts.sh`

Siehe die separate Script-Datei im Repository.

---

## Technische Referenz fuer die Implementierung

### Relevante Dateien

| Datei | Beschreibung |
| ----- | ------------ |
| `audio.h` | Voice-ID Enums, Beep-Typen, Voice-Queue Prototypen |
| `audio.c` | Voice-Chip GPIO-Protokoll, Voice-Queue, Beep-Player |
| `app/menu.c` | Morse-System, Menuevoice-Logik, `MENU_PlayCurrentMenuVoice()` |
| `ui/menu.h` | `t_menu_item` Struct, Menue-ID Enums |
| `ui/menu.c` | MenuList[] Array mit Voice-IDs und Menue-Zuordnung |
| `settings.h` | `VOICE_Prompt_t` Enum, `gMorseSpeedWpm` |
| `settings.c` | EEPROM-Laden/-Speichern der Voice/Morse-Einstellungen |
| `driver/bk4819.c` | `BK4819_PlayTone()`, `BK4819_PlaySingleTone()` |
| `driver/gpio.h` | GPIO-Pin-Definitionen (VOICE_0, VOICE_1, AUDIO_PATH) |
| `firmware.ld` | Linker-Script (60K Flash, 16K RAM) |
| `Makefile` | Build-Flags (ENABLE_VOICE=1) |

### Einstiegspunkt fuer Morse-Erweiterungen

```c
// In app/menu.c — hier wird entschieden was abgespielt wird:
static void MENU_PlayCurrentMenuVoice(void)
{
    // 1. Voice-ROM Clip?
    // 2. Morse-Label? (MENU_GetMorseLabel)
    // 3. Fallback: "Menu" + Nummer

    // >>> HIER neue Logik einfuegen <<<
    // z.B. nach Morse-Label-Abspielen auch Wert morsen
}

// Morse-Labels erweitern in:
static const char *MENU_GetMorseLabel(const uint8_t menu_id)
```

### Einstiegspunkt fuer Werte-Ansagen

```c
// In app/menu.c — Werte morsen bei Menuewechsel:
// Funktionen wie MENU_PlayNumberVoice() und MENU_PlayStepVoice()
// erweitern um Morse-Fallback wenn VOICE_PROMPT != OFF
// aber kein Voice-ROM verfuegbar
```

---

## Hinweise fuer die implementierende Agenten-Sitzung

1. **Dieses Dokument lesen** und den Entscheidungsblock auswerten
2. **HANDOVER.md** aktualisieren
3. **Makefile** muss nicht geaendert werden (ENABLE_VOICE=1 bleibt)
4. **Testen:** `make clean && make` — Binary darf 60K nicht ueberschreiten
5. **Morse-Code ist bereits vollstaendig implementiert** — nur erweitern, nicht umschreiben
6. **Voice-ROM-Clips beibehalten** — sie funktionieren fuer Step, Beep, Voice, Reset
7. **RAM sparsam nutzen** — keine grossen Puffer, keine dynamische Allokation
