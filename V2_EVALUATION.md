# V2 Implementierung — Evaluation und Konfigurationsoptionen

**Erstellt:** 2026-04-12  
**Autor:** Copilot Cloud Agent im Auftrag von DO9RE  
**Zweck:** Bestandsaufnahme der V2-Features, Accessibility-Bewertung und
Entscheidungsdokument fuer kuenftige Agenten-Sitzungen

---

## 1. Was ist im Changelog als V2_0 dokumentiert?

Laut `CHANGELOG.md` umfasst V2_0:

- Klassischer Display-Spektrum-Finder (visuell)
- Erster blinden-freundlicher Audio-Spektrum-Finder (Konzept)
- Gleiche Talking-ARDF-Basis wie V1
- Experimentelle Erweiterung, kein Ersatz fuer V1

---

## 2. Implementierungsstatus

| Feature | Datei | Makefile-Flag | Status |
|---------|-------|---------------|--------|
| Display-Spektrum-Finder | `app/spectrum.c` (1300+ Zeilen) | `ENABLE_SPECTRUM ?= 0` | Code vorhanden, aktuell DEAKTIVIERT |
| Audio-Spektrum-Finder | — | — | Nur als Konzept dokumentiert, NICHT implementiert |
| Talking ARDF Basis | `app/ardf.c` | `ENABLE_ARDF ?= 1` | AKTIV, voll funktionsfaehig |
| Morse-Feedback | `app/menu.c` | `ENABLE_VOICE ?= 1` | AKTIV |
| PTT Snapshot (Feldstaerke-Beeps) | `app/ardf.c` | `ENABLE_ARDF ?= 1` | AKTIV |

### Ergebnis

- Der **Display-Spektrum-Finder** existiert als vollstaendige Implementierung
  (`app/spectrum.c`), ist aber per Makefile-Flag deaktiviert (`ENABLE_SPECTRUM ?= 0`).
  Er belegt ca. 4-6 KB Flash wenn aktiviert.

- Der **Audio-Spektrum-Finder** ist bisher nur ein Konzept/Dokumentation.
  Es existiert keine Implementierung.

---

## 3. Display-Spektrum-Finder — Detail-Analyse

### Bereits implementiert (`app/spectrum.c`)

- Echtzeit-Spektrum-Darstellung auf dem Display
- Konfigurierbarer Scan-Bereich und Bandbreite
- Peak-Erkennung und Anzeige
- Monitor-Modus zum Abhoeren erkannter Signale
- Vollstaendige Tastatur-Steuerung (UP/DOWN, SIDE1/SIDE2, STAR, F, etc.)
- AGC-Lock fuer konsistente Messungen

### Steuerung (laut Changelog)

- `F + 5`: Startet den klassischen Display-Spektrum-Finder
- `5 halten` oder `F + 5 halten`: Geplant fuer Audio-Spektrum (nicht implementiert)

### Blind-Accessibility Bewertung

| Aspekt | Bewertung | Verbesserungspotenzial |
|--------|-----------|----------------------|
| Visuelle Darstellung | Rein visuell, nicht barrierefrei | Hoch |
| Peak-Erkennung | Intern vorhanden, keine akustische Ausgabe | Koennte Peak-Frequenz morsen |
| Monitor-Modus | Audio-Feedback vorhanden (Signal abhoerbar) | Nutzbar fuer Blinde |
| Navigation | Nur visuell, keine Sprachrueckmeldung | Morse fuer Frequenz-Ansage |

---

## 4. Audio-Spektrum-Finder — Konzept-Analyse

### Was wurde im Changelog beschrieben

Ein "erster blinden-freundlicher Audio-Spektrum-Finder" ist als Konzept
dokumentiert. Die Idee: Akustische Repraesentation der Spektrum-Daten
statt visueller Darstellung.

### Moegliche Implementierungsansaetze

#### A) Ton-Frequenz-Mapping (Sonifikation)

Die Spektrumdaten werden in hoerbare Toene umgewandelt:
- Jeder Frequenz-Bin wird als kurzer Ton mit proportionaler Lautstaerke gespielt
- Ein Sweep ueber das Spektrum erzeugt eine "akustische Landschaft"
- Starke Signale sind lauter, schwache leiser

**Vorteil:** Intuitiv, gibt Ueberblick ueber die gesamte Band-Aktivitaet  
**Nachteil:** Schwierig zu interpretieren, erfordert Uebung  
**Aufwand:** Mittel (BK4819 Tone-Generator nutzbar)

#### B) Peak-Morse-Ansage

Nach einem Spektrum-Scan wird der staerkste Peak identifiziert und die
Frequenz per Morse angesagt:

- Scan laeuft automatisch
- Peak-Frequenz wird als Morse-Zahl ausgegeben (z.B. "145.500")
- Peak-Staerke als Beep-Anzahl (1-9, wie PTT Snapshot)

**Vorteil:** Praezise, nutzbar mit bestehendem Morse-System  
**Nachteil:** Kein Echtzeit-Ueberblick  
**Aufwand:** Gering (baut auf vorhandenem Code auf)

#### C) Sweep-Beep (Audio-Balkendiagramm)

Ein vereinfachter Ansatz speziell fuer ARDF:

- Langsamer Sweep ueber einen konfigurierbaren Bereich
- Pro Frequenz-Bin: kurzer Beep mit Lautstaerke proportional zum RSSI
- Stille bei keinem Signal, laute Beeps bei starkem Signal

**Vorteil:** Einfach zu verstehen, ARDF-geeignet  
**Nachteil:** Langsam, nicht echtzeitfaehig  
**Aufwand:** Gering

#### D) Kompass-Modus (Audio-Peilung)

Speziell fuer ARDF DF-Modus:

- Kontinuierlicher Ton, dessen Frequenz/Lautstaerke sich mit der
  Empfangsstaerke aendert
- Beim Drehen der Antenne: hoeherer Ton = staerkeres Signal = richtige Richtung
- Kein Spektrum noetig, arbeitet auf der eingestellten Frequenz

**Vorteil:** Direktes haptisch-akustisches Peil-Feedback  
**Nachteil:** Nicht wirklich ein Spektrum-Finder  
**Aufwand:** Gering (nur RSSI → Tonhoehe/Lautstaerke Mapping)  
**Hinweis:** Teilweise bereits durch PTT-Snapshot abgedeckt

---

## 5. Makefile-Konfigurationsoptionen — Vollstaendige Uebersicht

Die folgenden Optionen koennen in `firmware-v1/Makefile` per `?= 0` (aus) oder
`?= 1` (ein) gesteuert werden. Flags ohne Relevanz fuer dieses Projekt sind
mit "Stock/Legacy" markiert.

### Aktive ARDF-relevante Optionen

```makefile
ENABLE_VOICE                  ?= 1    # Sprachausgabe und Morse-Feedback
ENABLE_ARDF                   ?= 1    # ARDF-Modus mit Fuchs-Timer und Gain-Steuerung
ENABLE_SQUELCH_MORE_SENSITIVE ?= 1    # Empfindlichere Squelch-Schwellen
ENABLE_FASTER_CHANNEL_SCAN    ?= 1    # Schnellerer Kanal-Scan
ENABLE_BYP_RAW_DEMODULATORS   ?= 1    # Bypass/Raw-Demodulatoren
ENABLE_PREVENT_TX             ?= 1    # TX vollstaendig blockiert (RX-only)
ENABLE_CUSTOM_MENU_LAYOUT     ?= 1    # Angepasstes Menue-Layout
ENABLE_NO_CODE_SCAN_TIMEOUT   ?= 1    # Kein Timeout bei Code-Scan
```

### Deaktivierte Optionen (koennen bei Bedarf eingeschaltet werden)

```makefile
ENABLE_SPECTRUM               ?= 0    # Display-Spektrum-Finder (V2-Feature)
ENABLE_AM_FIX                 ?= 0    # AM-Empfangs-Korrektur (AGC-Fix)
ENABLE_WIDE_RX                ?= 0    # Erweiterter Empfangsbereich
ENABLE_RSSI_BAR               ?= 0    # RSSI-Balkenanzeige im Display
ENABLE_AUDIO_BAR              ?= 0    # Audio-Pegel-Balken im Display
ENABLE_FMRADIO                ?= 0    # UKW-Radio-Empfaenger
ENABLE_SCAN_RANGES            ?= 0    # Scan-Bereiche
ENABLE_COPY_CHAN_TO_VFO       ?= 0    # Kanal nach VFO kopieren
ENABLE_BLMIN_TMP_OFF          ?= 0    # Backlight temporaer aus
ENABLE_BIG_FREQ               ?= 0    # Grosse Frequenzanzeige
ENABLE_SMALL_BOLD             ?= 0    # Kleine fette Schrift
ENABLE_KEEP_MEM_NAME          ?= 0    # Kanal-Name beibehalten
ENABLE_BOOT_BEEPS             ?= 0    # Beep beim Einschalten
ENABLE_SHOW_CHARGE_LEVEL      ?= 0    # Ladepegel anzeigen
ENABLE_REVERSE_BAT_SYMBOL     ?= 0    # Batterie-Symbol umkehren
ENABLE_REDUCE_LOW_MID_TX_POWER?= 0    # (TX-bezogen, irrelevant)
```

### Entfernte / Legacy-Optionen (sollten auf 0 bleiben)

```makefile
ENABLE_UART                   ?= 0    # UART-Kommunikation (Debug)
ENABLE_AIRCOPY                ?= 0    # Frequenz-Kopie ueber Funk
ENABLE_NOAA                   ?= 0    # NOAA-Wetter-Kanaele (nur US)
ENABLE_VOX                    ?= 0    # Voice-Operated Transmit (kein TX)
ENABLE_ALARM                  ?= 0    # Alarm-Ton (kein TX)
ENABLE_TX1750                 ?= 0    # 1750 Hz Ton (kein TX)
ENABLE_PWRON_PASSWORD         ?= 0    # Einschalt-Passwort
ENABLE_DTMF_CALLING           ?= 0    # DTMF-Anruf (komplett deaktiviert)
ENABLE_FLASHLIGHT             ?= 0    # Taschenlampe
ENABLE_TX_WHEN_AM             ?= 0    # TX in AM (irrelevant)
ENABLE_F_CAL_MENU             ?= 0    # Frequenz-Kalibrierung (Hidden Menu)
ENABLE_CTCSS_TAIL_PHASE_SHIFT ?= 0    # CTCSS Tail-Elimination
```

---

## 6. Entscheidungsblock fuer kuenftige Sitzungen

Jede Option kann durch Einkommentieren der gewuenschten Zeile gewaehlt werden.

### 6.1 Display-Spektrum aktivieren?

```
<!-- ENTSCHEIDUNG: Soll der Display-Spektrum-Finder (F+5) aktiviert werden? -->
<!-- OPTION_A: Ja, ENABLE_SPECTRUM=1 setzen (ca. 4-6 KB zusaetzlicher Flash) -->
<!-- OPTION_B: Nein, bleibt deaktiviert, spart Flash-Speicher -->
```

### 6.2 Audio-Spektrum-Ansatz

```
ENTSCHEIDUNG: Welcher Audio-Spektrum-Ansatz soll implementiert werden?
<!-- OPTION_A: Ton-Frequenz-Mapping (Sonifikation) — akustische Spektrum-Landschaft -->
<!-- OPTION_B: Peak-Morse-Ansage — staerkste Frequenz nach Scan per Morse ansagen -->
<!-- OPTION_C: Sweep-Beep — langsamer Sweep mit Lautstaerke-Feedback pro Bin -->
OPTION_D: Kompass-Modus — kontinuierlicher RSSI-zu-Ton fuer Peilung
<!-- OPTION_E: Keines — Audio-Spektrum vorerst nicht implementieren -->
```

### 6.3 AM-Fix aktivieren?

```
ENTSCHEIDUNG: AM-Fix (AGC-Korrektur) fuer besseren AM-Empfang?
OPTION_A: Ja, ENABLE_AM_FIX=1 (verbessert AM-Empfang, braucht etwas Flash)
<!-- OPTION_B: Nein, bleibt deaktiviert -->
```

### 6.4 Erweiterter Empfangsbereich?

```
ENTSCHEIDUNG: ENABLE_WIDE_RX fuer erweiterten Empfangsbereich?
OPTION_A: Ja, alle Frequenzen empfangbar (18 MHz bis 1300 MHz)
<!-- OPTION_B: Nein, Standard-Amateurfunk-Baender -->
```

### 6.5 UKW-Radio beibehalten?

```
ENTSCHEIDUNG: UKW-Broadcast-Empfaenger (ENABLE_FMRADIO)?
<!-- OPTION_A: Ja, kann als Feldunterhaltung oder Empfangstest nuetzlich sein -->
OPTION_B: Nein, spart Flash-Speicher
```

### 6.6 RSSI-Balkenanzeige?

```
ENTSCHEIDUNG: RSSI-Balken im Display (ENABLE_RSSI_BAR)?
<!-- OPTION_A: Ja, nuetzlich fuer sehende Begleiter -->
OPTION_B: Nein, spart Flash und Display-Platz
```

### 6.7 Taschenlampe?

```
ENTSCHEIDUNG: Taschenlampen-LED (ENABLE_FLASHLIGHT)?
OPTION_A: Ja, praktisch im Feld bei Dunkelheit
<!-- OPTION_B: Nein, nicht ARDF-relevant -->
```

---

## 7. Accessibility-Verbesserungsvorschlaege

Unabhaengig von den Optionen oben gibt es Verbesserungen, die die
Blind-Accessibility weiter staerken koennten:
**DO9RE**: ich mache Sterne * vor die gewünschten Änderungen, bitte implementieren.

### 7.1 Morse-Erweiterungen

* - Alle Menueeintraege mit Morse-Labels versehen (aktuell nur ca. 13 von 40+) -> Nur wo kein Voice Prompt dazu vorhanden ist. 
- Submenu-Werte per Morse ansagen (z.B. Squelch-Level als Zahl morsen)
- Frequenz-Ansage per Morse auf Hauptbildschirm

### 7.2 Audio-Feedback

* - Unterschiedliche Beep-Toene fuer verschiedene Aktionen -> Auf und absteigende Tolnfolge für ein und ausschalten von Optionen.
  (aktuell: 500 Hz / 1 kHz, koennte differenzierter sein)
* - Batterie-Warn-Beep als unterscheidbare Tonfolge
* - Tastensperr-Feedback als eigener Ton

### 7.3 ARDF-spezifisch

* - Kontinuierlicher RSSI-Ton (Peil-Modus) statt nur PTT-Snapshot -> Geht das parallel zum empfangenen Signal hörbar? Kann der Tongenerator sweeps erzeugen oder muss immer neu gepulst werden?
- Akustisches Signal beim Fuchswechsel im Zyklus
- Gain-Index per Morse ansagen (aktuell nur Voice-Clip Nummer)

---

## 8. Zusammenfassung

| Thema | Status | Empfehlung |
|-------|--------|------------|
| Display-Spektrum | Code vorhanden, deaktiviert | Entscheidung abwarten |
| Audio-Spektrum | Nur Konzept | Peak-Morse-Ansage als einfachster Einstieg |
| Morse-System | Funktionsfaehig | Weiter ausbauen (mehr Labels, Werte morsen) |
| ARDF-Modus | Voll funktional | Kontinuierlicher Peil-Ton als Option |
| Voice-Prompts | Hardware-limitiert | Morse bleibt primaerer Feedback-Kanal |
