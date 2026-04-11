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
| 2026-04-11   | Codex (Copilot Cloud)   | `msys2_build.sh` erstellt, `HANDOVER.md` angelegt      |

---

## Offene Aufgaben / TODOs

<!-- Neue Aufgaben oben einfuegen -->

| Prio | Aufgabe                                                       | Status     | Verantwortlich |
| ---- | ------------------------------------------------------------- | ---------- | -------------- |
| —    | (keine offenen Aufgaben)                                      | —          | —              |

---

## Bekannte Probleme / Warnungen

<!-- Probleme hier dokumentieren, damit die naechste Sitzung Bescheid weiss -->

- Keine bekannten Probleme zum aktuellen Zeitpunkt.

---

## Build-Hinweise

### Windows CMD (win_make.bat)

- Setzt feste Pfade (`C:\Program Files (x86)\Arm\GNU Toolchain ...`)
- Erwartet `py.exe` oder `python.exe` im Windows-AppData-Pfad
- Erzeugt Zeitstempel-Kopien via PowerShell

### MSYS2 (msys2_build.sh) — NEU

- Funktioniert in MinGW64, MinGW32, UCRT64 und MSYS Shells
- Prueft automatisch: `make`, `arm-none-eabi-gcc`, `arm-none-eabi-newlib`, `python3`, `pip`, `crcmod`, `git`
- Bietet interaktive Nachinstallation ueber `pacman` an
- Erzeugt Zeitstempel-Kopien wie `win_make.bat`

### Docker

- `compile-with-docker.sh` / `compile-with-docker.bat`
- Baut im Container, Ausgabe in `compiled-firmware/`

---

## Notizen zwischen Sitzungen

<!-- Freitext-Bereich fuer Hinweise, die die naechste Sitzung wissen muss -->

- Das Repository verwendet einen sauberen Root mit `firmware-source/` als Firmware-Unterordner.
- `.gitignore` schliesst Build-Artefakte (`firmware_uvk5_v1*`) bereits aus.
- Die `CHANGELOG.md` dokumentiert Varianten (`V1`, `V2_0`).
- Aenderungen an der Build-Infrastruktur sollten auch in der `README.md` Sektion "Building" reflektiert werden.

---

## Sitzungs-Protokoll

<!-- Kurzes Protokoll jeder Agenten-Sitzung — chronologisch, neueste oben -->

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
