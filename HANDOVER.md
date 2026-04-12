# HANDOVER.md — Agenten-Synchronisation

Dieses Dokument dient als Synchronisationspunkt zwischen allen Agenten-Sitzungen,
die an diesem Repository arbeiten. JeWeLux arbeitet mit **Codex Extern** und
ggf. weiteren KI-Agenten. Jede Sitzung muss diese Datei lesen und aktualisieren.

---

## Regeln fuer alle Agenten

1. **Vor Arbeitsbeginn** diese Datei lesen.
2. **Nach jeder Aenderung** diese Datei aktualisieren (Abschnitte unten).
3. Aenderungen hier committen **zusammen** mit den eigentlichen Code-Aenderungen.
4. Bei Konflikten hat die juengere Sitzung Vorrang, muss aber aeltere Eintraege erhalten.
5. Keine Eintraege loeschen — nur als erledigt markieren oder ergaenzen.

---

## Aktueller Repository-Zustand

| Eigenschaft         | Wert                                         |
| ------------------- | -------------------------------------------- |
| Hauptbranch         | `main`                                       |
| Firmware-Quellcode  | `firmware-source/`                           |
| Build-Ausgabe       | `build-output/` (wird automatisch erstellt)  |
| Build (Windows CMD) | `firmware-source/win_make.bat`               |
| Build (MSYS2)       | `firmware-source/msys2_build.sh`             |
| Build (Docker)      | `firmware-source/compile-with-docker.sh`     |
| Firmware-Varianten  | `V1` (talking ARDF), `V2_0` (+ Spectrum)     |
| Lizenz              | Apache-2.0                                   |

---

## Zuletzt abgeschlossene Aenderungen

<!-- Neue Eintraege oben einfuegen, aelteste unten -->

| Datum (UTC)  | Agent / Sitzung         | Aenderung                                              |
| ------------ | ----------------------- | ------------------------------------------------------ |
| 2026-04-12   | Copilot Cloud Agent     | Voice-Prompt-Evaluation erstellt: `VOICE_PROMPT_EVALUATION.md` + `utils/generate_voice_prompts.sh` |
| 2026-04-12   | Copilot Cloud Agent     | MSYS2 Flash-Tool erstellt: `k5flash.py` + `msys2_flash.sh` |
| 2026-04-11   | Copilot Cloud Agent     | Unterbrochene Arbeit fortgesetzt: Compile-Error behoben, TX-Menues/Dead-Code entfernt (48468->45184 bytes) |
| 2026-04-11   | Copilot Cloud Agent     | Scrambler/Descrambler komplett entfernt (48856->48468 bytes) |
| 2026-04-11   | Copilot Cloud Agent     | Codebase-Audit erstellt (`AUDIT_REMAINING_FEATURES.md`) |
| 2026-04-11   | Copilot Cloud Agent     | TX-Code Phase 1+2: FUNCTION_TRANSMIT, TX-Variablen/Flags/Enums entfernt (52136->48856 bytes) |
| 2026-04-11   | Copilot Cloud Agent     | TX-Entfernungs-Analyse erstellt (`TX_REMOVAL_ANALYSIS.md`) |
| 2026-04-11   | Codex (Copilot Cloud)   | Build-Output nach `build-output/` verschoben, .gitignore angepasst |
| 2026-04-11   | Codex (Copilot Cloud)   | `msys2_build.sh`: PEP 668 fix (pacman+venv statt pip), MINGW64-Pflicht |
| 2026-04-11   | Codex (Copilot Cloud)   | `msys2_build.sh` erstellt, `HANDOVER.md` angelegt      |

---

## Offene Aufgaben / TODOs

<!-- Neue Aufgaben oben einfuegen -->

| Prio | Aufgabe                                                       | Status     | Verantwortlich |
| ---- | ------------------------------------------------------------- | ---------- | -------------- |
| 0    | Entscheidungen in `VOICE_PROMPT_EVALUATION.md` treffen        | Warte auf Do9RE | Do9RE |
| 1    | Entscheidungen in `AUDIT_REMAINING_FEATURES.md` treffen       | Warte auf Do9RE | Do9RE |
| 2    | Gewaehlte Optionen aus Voice-Evaluation umsetzen              | Warte auf Entscheidungen | Agent |
| 3    | Gewaehlte Optionen aus Audit umsetzen                         | Warte auf Entscheidungen | Agent |
| 4    | TX-Code Phase 3-5 (BK4819 TX, DTMF TX, Menues, etc.)         | Groesstenteils erledigt — siehe Sitzung 7 | Agent |

---

## Bekannte Probleme / Warnungen

<!-- Probleme hier dokumentieren, damit die naechste Sitzung Bescheid weiss -->

- Keine bekannten Probleme zum aktuellen Zeitpunkt.
- (Behoben) PEP 668: `pip install` schlug in MSYS2 fehl wegen externally-managed-environment. Fix: pacman-first + venv-Fallback.
- (Behoben) Falscher Shell-Typ: UCRT64 statt MINGW64 fuehrte zu fehlenden Paketen. Fix: Skript erzwingt MINGW64.

---

## Build-Hinweise

### Windows CMD (win_make.bat)

- Setzt feste Pfade (`C:\Program Files (x86)\Arm\GNU Toolchain ...`)
- Erwartet `py.exe` oder `python.exe` im Windows-AppData-Pfad
- Verschiebt Firmware-Binaries nach `build-output/` mit Zeitstempel-Kopien

### MSYS2 (msys2_build.sh)

- **Erfordert MINGW64-Shell** — bricht mit klarer Fehlermeldung ab bei UCRT64, MINGW32 oder MSYS
- Prueft automatisch: `make`, `arm-none-eabi-gcc`, `arm-none-eabi-newlib`, `python3`, `crcmod`, `git`
- crcmod-Installation: versucht zuerst `pacman -S mingw-w64-x86_64-python-crcmod`, dann Fallback ueber temporaeres venv (PEP 668 sicher)
- Bietet interaktive Nachinstallation ueber `pacman` an
- Verschiebt Firmware-Binaries nach `build-output/` mit Zeitstempel-Kopien

### Docker

- `compile-with-docker.sh` / `compile-with-docker.bat`
- Baut im Container, Ausgabe in `compiled-firmware/`

### Firmware flashen (MSYS2)

- `msys2_flash.sh` — interaktives Wrapper-Skript
  - Prueft Python3 und pyserial, bietet Installation an
  - Startet `k5flash.py` im interaktiven Modus
- `k5flash.py` — Python-Flasher (auch standalone nutzbar)
  - Interaktiver Modus: `python3 k5flash.py` (Port und Datei werden abgefragt)
  - Kommandozeile: `python3 k5flash.py COM3 firmware_uvk5_v1.packed.bin`
  - Unterstuetzt `.packed.bin` (gepackt) und `.bin` (roh) Dateien
  - Sucht automatisch in `build-output/` nach Firmware-Dateien
  - Benoetigt `pyserial` (`pip install pyserial` oder `pacman -S mingw-w64-x86_64-python-pyserial`)
  - Radio muss im Flash-Modus sein: PTT + Einschalten (Display dunkel, LED weiss)

---

## Notizen zwischen Sitzungen

<!-- Freitext-Bereich fuer Hinweise, die die naechste Sitzung wissen muss -->

- Das Repository verwendet einen sauberen Root mit `firmware-source/` als Firmware-Unterordner.
- `.gitignore` schliesst Build-Artefakte aus: `build-output/`, `firmware_uvk5_v1*` u.a.
- Die `CHANGELOG.md` dokumentiert Varianten (`V1`, `V2_0`).
- Aenderungen an der Build-Infrastruktur sollten auch in der `README.md` Sektion "Building" reflektiert werden.

---

## Sitzungs-Protokoll

<!-- Kurzes Protokoll jeder Agenten-Sitzung — chronologisch, neueste oben -->

### 2026-04-12 (9. Sitzung) — Copilot Cloud Agent

**Auftrag:** Evaluation der Moeglichkeit, Voice Prompts zusaetzlich zur Morse-Ausgabe
zu implementieren. Analyse der Hardware-Voraussetzungen und Erstellung eines
Entscheidungsdokuments.

**Durchgefuehrt:**
- Komplette Analyse der Audio-Hardware:
  - Externer Voice-ROM-Chip (JQ8400-artig): 76 englische + 58 chinesische vorprogrammierte Clips
  - GPIO Bit-Banging Protokoll (GPIOA Pin 12/13) — nur Wiedergabe, kein Schreibzugriff
  - BK4819: Nur Tone-Generator, kein PCM/Sample-Playback
  - DP32G030: Kein DAC-Ausgang, kein Pfad fuer Software-generiertes Audio zum Lautsprecher
- Speicher-Analyse: ~15 KB Flash frei, EEPROM nicht nutzbar fuer Audio
- 5 Optionen evaluiert (A–E) mit Machbarkeit, Speicherbedarf und Empfehlung
- `VOICE_PROMPT_EVALUATION.md` erstellt mit:
  - Detaillierte Hardware-Dokumentation
  - Optionen A–E mit Bewertungen
  - Kommentierbare Entscheidungsbloecke fuer kuenftige Sitzungen
  - Technische Referenz mit Datei- und Funktionsliste
- `firmware-source/utils/generate_voice_prompts.sh` erstellt:
  - eSpeak + FFmpeg basierter Voice-Prompt-Generator
  - MSYS2/Linux/macOS kompatibel
  - Erzeugt WAV, Raw PCM, ADPCM und C-Header
  - 3 Modi: full (alle Worte), letters (Buchstaben), test (3 Beispiele)
- HANDOVER.md aktualisiert

**Ergebnis:**
- Voice-ROM ist nicht erweiterbar (Hardware-Limitation)
- PCM-Playback unmoeglich ohne Hardware-Mod (kein DAC, kein Audio-Pfad)
- **Empfehlung:** Morse-System weiter ausbauen (Option D) — einziger rein
  firmware-basierter Ansatz
- Sekundaer: Akustische Ton-Icons (Option E) als Ergaenzung

**Keine Aenderungen an:**
- Firmware-Quellcode (nur Analyse und Dokumentation)
- Makefile oder Build-Konfiguration

### 2026-04-12 (8. Sitzung) — Copilot Cloud Agent

**Auftrag:** MSYS2-basiertes Flash-Tool erstellen, da Browser-Flasher (Firefox/Chrome)
nicht funktioniert.

**Durchgefuehrt:**
- Bootloader-Protokoll analysiert (Quellen: amnemonic/Quansheng_UV-K5_Firmware docs,
  egzumer/uvtools JavaScript-Quellcode)
- `k5flash.py` erstellt: vollstaendiger Python-Flasher fuer UV-K5/K6/5R Plus
  - Implementiert das komplette Bootloader-Protokoll (38400 baud, XOR, CRC16-CCITT)
  - Unterstuetzt `.packed.bin` und rohe `.bin` Dateien
  - Interaktiver Modus mit Port-Erkennung und Dateisuche
  - Kommandozeilen-Modus fuer Automatisierung
  - Fortschrittsbalken beim Flashen
  - Sicherheitsabfragen und ausfuehrliche Fehlermeldungen (deutsch)
- `msys2_flash.sh` erstellt: interaktives MSYS2-Wrapper-Skript
  - Prueft MINGW64-Umgebung, Python3, pyserial
  - Bietet automatische Installation fehlender Pakete
  - Treiber-Hinweise fuer CH340/CP2102 Kabel
  - Anleitung fuer Flash-Modus
- `.gitignore`: `.venv_flash` hinzugefuegt
- HANDOVER.md aktualisiert

**Keine Aenderungen an:**
- Firmware-Quellcode
- Makefile
- Bestehenden Build-Skripten

### 2026-04-11 (7. Sitzung) — Copilot Cloud Agent

**Auftrag:** Unterbrochene Arbeit fortsetzen (DTMF-Entfernung, TX-Code Phase 3-5).

**Durchgefuehrt:**
- Compile-Error behoben: `DTMF_clear_input_box()` Aufruf in `ui/ui.c` entfernt (Funktion existierte nicht mehr)
- MENU_TOT (TxTOut = TX-Timeout) aus Menue-System entfernt inkl. `gSubMenu_TOT` Array
- MENU_200TX, MENU_350TX, MENU_500TX (TX-Band-Freischaltung) aus Hidden-Menue entfernt
- TX-Ausgangsleistungs-Berechnung aus `RADIO_ConfigureSquelchAndOutputPower()` entfernt
- `FREQUENCY_CalculateOutputPower()` als Dead Code entfernt (aus `frequencies.c` und `frequencies.h`)
- Binary-Groesse: 48468 -> 45184 Bytes (3284 Bytes / 6.8% kleiner)
- HANDOVER.md aktualisiert

**Status nach dieser Sitzung:**
- TX ist komplett blockiert (ENABLE_PREVENT_TX=1, TX_freq_check gibt immer -1 zurueck)
- Alle TX-Menue-Eintraege entfernt (MENU_TXP, MENU_T_DCS, MENU_T_CTCS, MENU_SFT_D, MENU_OFFSET, MENU_TOT, MENU_ROGER, MENU_PTT_ID, MENU_BCL, MENU_200TX, MENU_350TX, MENU_500TX)
- DTMF-Calling komplett hinter #ifdef ENABLE_DTMF_CALLING (=0, nicht kompiliert)
- Scrambler/Descrambler komplett entfernt
- BK4819 TX-Funktionen (Roger, DTMF TX, PA, PrepareTransmit, TransmitTone) entfernt
- PTT-Handler auf Beep-only vereinfacht
- FUNCTION_TRANSMIT und zugehoerige State-Machine entfernt
- Verbleibende TX-Infrastruktur (FrequencyReverse, OUTPUT_POWER Struct-Feld, PTT_ID_t Enum) bleibt fuer EEPROM-Kompatibilitaet
- BK4819_EnterTxMute/ExitTxMute/EnableTXLink bleiben (werden fuer RX-Audio-Feedback/Beeps benoetigt)

**Keine Aenderungen an:**
- Build-Skripten
- EEPROM-Layout (Struct-Felder bleiben fuer Kompatibilitaet)
- FrequencyReverse-Funktion (RX-relevant fuer Repeater-Eingabe)

### 2026-04-11 (4. Sitzung) — Copilot Cloud Agent

**Auftrag:** Analyse aller verbliebenen TX-Funktionalitaet in der Firmware-Codebasis.
Erstellung einer Dokumentation mit Entscheidungsbloecken fuer systematische TX-Code-Entfernung.

**Durchgefuehrt:**
- Systematische Analyse aller .c und .h Dateien auf TX-bezogene Artefakte
- Identifikation von 12 Kategorien verbliebener TX-Funktionalitaet
- Identifikation von RX-Features die TX-Code nutzen (Scrambler, Compander, FrequencyReverse)
- `TX_REMOVAL_ANALYSIS.md` erstellt mit ein-/auskommentierbaren Entscheidungsbloecken
- `HANDOVER.md` aktualisiert

**Ergebnis:**
- TX ist effektiv durch ENABLE_PREVENT_TX und TX_freq_check() blockiert
- Erhebliche Mengen Dead TX-Code im Binary (Funktionen, Variablen, Menuepunkte)
- Scrambler: RX-Descramble benoetigt gleiche BK4819-Funktionen wie TX-Scramble
- Compander: RX-Expander und TX-Compressor teilen sich BK4819_SetCompander()
- FrequencyReverse: Nutzt freq_config_TX und pTX fuer Repeater-Eingabe-Abhoeren
- Dokumentation wartet auf Entscheidungen von Do9RE

**Keine Aenderungen an:**
- Bestehendem Quellcode (nur Analyse, keine Code-Modifikationen)
- Makefile
- Build-Skripten

### 2026-04-11 (3. Sitzung) — Codex (Copilot Cloud Agent)

**Auftrag:** Build-Output in separaten Ordner `build-output/` verschieben.

**Durchgefuehrt:**
- `msys2_build.sh` `do_build()`: Binaries werden nach `../build-output/` verschoben statt in `firmware-source/` zu verbleiben
- `win_make.bat`: gleiche Aenderung (Binaries nach `..\build-output\`)
- `.gitignore`: `build-output/` hinzugefuegt
- `HANDOVER.md` aktualisiert

**Keine Aenderungen an:**
- Makefile (baut weiterhin in-place, Skripte verschieben danach)
- Bestehendem Quellcode

### 2026-04-11 (2. Sitzung) — Codex (Copilot Cloud Agent)

**Auftrag:** PEP 668 fix fuer crcmod, MINGW64-Shell erzwingen.

**Durchgefuehrt:**
- `check_msys2_env` umgebaut: bricht bei UCRT64/MINGW32/MSYS ab mit erklaerenden Fehlermeldungen
- `pip_install` ersetzt durch `pip_install_crcmod`: versucht pacman (`mingw-w64-x86_64-python-crcmod`), Fallback auf lokales venv
- `check_pip_crcmod` vereinfacht (kein separater pip-Check mehr)
- Alle `case`-Bloecke fuer MSYSTEM entfernt (MINGW64 ist jetzt Pflicht)
- `.gitignore`: `firmware-source/.venv_build` hinzugefuegt
- `HANDOVER.md` aktualisiert

**Keine Aenderungen an:**
- Bestehendem Quellcode oder Makefile

### 2026-04-11 — Codex (Copilot Cloud Agent)

**Auftrag:** MSYS2-Build-Skript erstellen, Handover-Datei anlegen.

**Durchgefuehrt:**
- `win_make_bat_starten.txt` und alle Build-Skripte analysiert
- `firmware-source/msys2_build.sh` erstellt:
  - Abhaengigkeitspruefung (make, ARM toolchain, newlib, Python 3, pip, crcmod, git)
  - Interaktive Nachinstallation ueber pacman/pip
  - Farbige Terminalausgabe
  - Automatische Zeitstempel-Kopien
  - Funktioniert in allen MSYS2-Shell-Varianten
- `HANDOVER.md` (diese Datei) angelegt

**Keine Aenderungen an:**
- Bestehendem Quellcode
- Makefile
- win_make.bat
- README.md (ggf. in Folgesitzung ergaenzen)
