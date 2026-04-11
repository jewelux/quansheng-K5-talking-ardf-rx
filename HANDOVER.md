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
| 2026-04-11   | Copilot Cloud Agent     | TX-Entfernungs-Analyse erstellt (`TX_REMOVAL_ANALYSIS.md`) |
| 2026-04-11   | Codex (Copilot Cloud)   | Build-Output nach `build-output/` verschoben, .gitignore angepasst |
| 2026-04-11   | Codex (Copilot Cloud)   | `msys2_build.sh`: PEP 668 fix (pacman+venv statt pip), MINGW64-Pflicht |
| 2026-04-11   | Codex (Copilot Cloud)   | `msys2_build.sh` erstellt, `HANDOVER.md` angelegt      |

---

## Offene Aufgaben / TODOs

<!-- Neue Aufgaben oben einfuegen -->

| Prio | Aufgabe                                                       | Status     | Verantwortlich |
| ---- | ------------------------------------------------------------- | ---------- | -------------- |
| 1    | TX-Code Entfernung gemaess `TX_REMOVAL_ANALYSIS.md`           | Warte auf Entscheidungen von Do9RE | Do9RE / Agent |
| —    | (keine weiteren offenen Aufgaben)                             | —          | —              |

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
