# TX-Funktionalitaet Entfernungs-Analyse

**Erstellt:** 2026-04-11  
**Agent:** GitHub Copilot Cloud Agent  
**Auftraggeber:** Do9RE (Richard)  
**Ziel:** Maximale Reduktion der Codebasis auf reinen Empfangs- und Peil-Betrieb (ARDF RX-only)

---

## Lesehinweise fuer die naechste Agenten-Sitzung

Diese Datei enthaelt eine vollstaendige Analyse aller verbliebenen TX-Artefakte im Code.
Jeder Abschnitt enthaelt **Entscheidungsbloecke** in Kommentarform.

**So funktioniert die Steuerung:**

```
<!-- ENTSCHEIDUNG: Beschreibung -->
<!-- OPTION_A: Beschreibung von Option A -->
<!-- OPTION_B: Beschreibung von Option B -->
```

Richard kommentiert die gewuenschte Option ein (entfernt die `<!-- -->` Klammern):

```
<!-- ENTSCHEIDUNG: Beschreibung -->
OPTION_A: Beschreibung von Option A
<!-- OPTION_B: Beschreibung von Option B -->
```

Die naechste Agenten-Sitzung liest diese Datei und fuehrt nur die einkommentierten
Optionen aus.

---

## Zusammenfassung des IST-Zustands

### Aktuelle Schutzmechanismen

| Schutzmechanismus | Ort | Wert | Status |
|---|---|---|---|
| `ENABLE_PREVENT_TX` | Makefile:43 | `1` | **AKTIV** - `TX_freq_check()` gibt immer -1 zurueck |
| `ENABLE_ARDF` | Makefile:44 | `1` | **AKTIV** - Zusaetzliche ARDF-Sperre |
| `ENABLE_VOX` | Makefile:12 | `0` | **DEAKTIVIERT** - Nicht kompiliert |
| `ENABLE_ALARM` | Makefile:13 | `0` | **DEAKTIVIERT** - Nicht kompiliert |
| `ENABLE_TX1750` | Makefile:14 | `0` | **DEAKTIVIERT** - Nicht kompiliert |
| `ENABLE_DTMF_CALLING` | Makefile:16 | `0` | **DEAKTIVIERT** - Nicht kompiliert |
| `ENABLE_TX_WHEN_AM` | Makefile:25 | `0` | **DEAKTIVIERT** - Nicht kompiliert |

### Bewertung

**RF-Aussendung ist effektiv verhindert** durch `TX_freq_check()` in `frequencies.c`.
Allerdings befinden sich noch **erhebliche Mengen an TX-Code** im Binary, die
als Dead Code Speicherplatz belegen und die Codebasis unnoetig komplex machen.

---

## TEIL 1: DIREKTE TX-FUNKTIONEN (Dead Code im Binary)

### 1.1 RADIO_PrepareTX() — Haupt-TX-Initialisierung

**Datei:** `firmware-source/radio.c`, Zeilen 930-1035  
**Aufgerufen von:** `app/app.c:803`, `app/app.c:1990`  
**Status:** Wird aufgerufen, kehrt aber sofort zurueck (TX_freq_check blockiert)

**Abhaengigkeiten bei Entfernung:**
- `app/app.c:803` ruft `RADIO_PrepareTX()` auf wenn `gFlagPrepareTX == true`
- `app/app.c:1990` prueft `gFlagPrepareTX` und ruft ebenfalls auf
- `RADIO_PrepareCssTX()` in `radio.c:1063` ist ein Wrapper

```
<!-- ENTSCHEIDUNG 1.1: RADIO_PrepareTX() und zugehoerige TX-Initialisierung -->
<!-- OPTION_A: RADIO_PrepareTX() durch leere Funktion ersetzen (Stub).
     gFlagPrepareTX bleibt bestehen, PTT-Taste zeigt "TX DISABLED".
     Aenderungen:
     - radio.c: RADIO_PrepareTX() Body durch { RADIO_SetVfoState(VFO_STATE_TX_DISABLE); return; } ersetzen
     - radio.c: RADIO_PrepareCssTX() Body leeren
     - Gesamter Code zwischen "TX is allowed" Kommentar und Funktionsende entfernen -->
<!-- OPTION_B: RADIO_PrepareTX() komplett entfernen und alle Aufrufe entfernen.
     gFlagPrepareTX Variable entfernen. PTT-Taste hat keine Wirkung mehr.
     Aenderungen:
     - radio.c: RADIO_PrepareTX() loeschen
     - radio.c: RADIO_PrepareCssTX() loeschen
     - radio.h: Deklarationen entfernen
     - app/app.c:799-805: gFlagPrepareTX Block entfernen
     - app/app.c:1989-1991: gFlagPrepareTX Block entfernen
     - misc.h/misc.c: gFlagPrepareTX Variable entfernen -->
```

### 1.2 RADIO_SetTxParameters() — TX-Hardware-Konfiguration

**Datei:** `firmware-source/radio.c`, Zeilen 768-832  
**Aufgerufen von:** `functions.c:185` (aus FUNCTION_Select bei FUNCTION_TRANSMIT)  
**Status:** Dead Code — FUNCTION_TRANSMIT wird nie erreicht

**Inhalt:** Setzt TX-Frequenz, PA-Verstaerker, TX-Compander, CTCSS/DCS TX-Codes

```
<!-- ENTSCHEIDUNG 1.2: RADIO_SetTxParameters() -->
<!-- OPTION_A: Funktion komplett entfernen.
     Aenderungen:
     - radio.c: RADIO_SetTxParameters() loeschen (Zeilen 768-832)
     - radio.h: Deklaration entfernen (falls vorhanden)
     - functions.c:185: Aufruf entfernen (im FUNCTION_TRANSMIT case) -->
<!-- OPTION_B: Funktion durch leeren Stub ersetzen (fuer Sicherheit falls irgendwo referenziert) -->
```

### 1.3 RADIO_SendCssTail() und RADIO_SendEndOfTransmission()

**Datei:** `firmware-source/radio.c`, Zeilen 1037-1080  
**Aufgerufen von:**
- `RADIO_SendEndOfTransmission()` wird aufgerufen von `APP_EndTransmission()` (app/app.c:726)
- `RADIO_SendCssTail()` wird aufgerufen von `RADIO_SendEndOfTransmission()` und `app/app.c:1236`

**Status:** Dead Code — nur erreichbar wenn TX aktiv war

```
<!-- ENTSCHEIDUNG 1.3: RADIO_SendCssTail() und RADIO_SendEndOfTransmission() -->
<!-- OPTION_A: Beide Funktionen komplett entfernen.
     Aenderungen:
     - radio.c: RADIO_SendCssTail() loeschen
     - radio.c: RADIO_SendEndOfTransmission() loeschen
     - radio.c: RADIO_PrepareCssTX() loeschen
     - radio.h: Deklarationen entfernen
     - app/app.c:726: Aufruf in APP_EndTransmission() entfernen
     - app/app.c:1236: Aufruf entfernen
     - app/app.c:1559: Aufruf entfernen -->
<!-- OPTION_B: Durch leere Stubs ersetzen -->
```

### 1.4 APP_EndTransmission()

**Datei:** `firmware-source/app/app.c`, Zeilen 723-734  
**Aufgerufen von:** `app/generic.c` (PTT-Release), `app/app.c:1559`  
**Status:** Dead Code

```
<!-- ENTSCHEIDUNG 1.4: APP_EndTransmission() -->
<!-- OPTION_A: Funktion komplett entfernen.
     Aenderungen:
     - app/app.c: APP_EndTransmission() loeschen
     - app/generic.c: Aufrufe entfernen (PTT-Release Handler)
     - app/app.c:1559: Aufruf entfernen
     - misc.h/misc.c: gFlagEndTransmission Variable entfernen -->
<!-- OPTION_B: Durch leeren Stub ersetzen -->
```

---

## TEIL 2: TX-BEZOGENE BK4819-TREIBER-FUNKTIONEN

### 2.1 Power Amplifier Setup

**Datei:** `firmware-source/driver/bk4819.c`, Zeile 695  
**Funktion:** `BK4819_SetupPowerAmplifier(bias, frequency)`  
**Aufgerufen von:** `radio.c:812` (in RADIO_SetTxParameters — Dead Code)

```
<!-- ENTSCHEIDUNG 2.1: BK4819_SetupPowerAmplifier() -->
<!-- OPTION_A: Funktion komplett entfernen.
     Aenderungen:
     - driver/bk4819.c: BK4819_SetupPowerAmplifier() loeschen
     - driver/bk4819.h: Deklaration entfernen -->
<!-- OPTION_B: Beibehalten (wird durch Entfernung von RADIO_SetTxParameters nicht mehr aufgerufen) -->
```

### 2.2 BK4819_PrepareTransmit()

**Datei:** `firmware-source/driver/bk4819.c`, Zeile 1157  
**Aufgerufen von:** `radio.c:802` (in RADIO_SetTxParameters — Dead Code)

```
<!-- ENTSCHEIDUNG 2.2: BK4819_PrepareTransmit() -->
<!-- OPTION_A: Funktion komplett entfernen.
     Aenderungen:
     - driver/bk4819.c: BK4819_PrepareTransmit() loeschen
     - driver/bk4819.h: Deklaration entfernen -->
<!-- OPTION_B: Beibehalten -->
```

### 2.3 BK4819_EnableTXLink()

**Datei:** `firmware-source/driver/bk4819.c`, Zeile 1249  
**Aufgerufen von:** `bk4819.c:1036` (PlayTone), `bk4819.c:1235` (DTMF), `bk4819.c:1361` (TransmitTone), `bk4819.c:1735,1841` (Roger/MDC)

```
<!-- ENTSCHEIDUNG 2.3: BK4819_EnableTXLink() -->
<!-- OPTION_A: Funktion komplett entfernen.
     Abhaengig von Entscheidungen 2.4, 2.5, 2.6 — nur entfernbar wenn alle Aufrufer auch entfernt werden. -->
<!-- OPTION_B: Beibehalten (wenn Tone-Funktionen fuer RX-Feedback benoetigt werden) -->
```

### 2.4 DTMF TX-Funktionen

**Datei:** `firmware-source/driver/bk4819.c`
- `BK4819_EnterDTMF_TX()` — Zeile 1223
- `BK4819_ExitDTMF_TX()` — Zeile 1238
- `BK4819_PlayDTMF()` — Zeile 1264
- `BK4819_PlayDTMFString()` — Zeile 1308

**Aufgerufen von:** `app/dtmf.c:96-110` (DTMF_SendEndOfTransmission — Dead Code)

```
<!-- ENTSCHEIDUNG 2.4: DTMF TX-Funktionen -->
<!-- OPTION_A: Alle vier Funktionen komplett entfernen.
     Aenderungen:
     - driver/bk4819.c: BK4819_EnterDTMF_TX(), BK4819_ExitDTMF_TX(), BK4819_PlayDTMF(), BK4819_PlayDTMFString() loeschen
     - driver/bk4819.h: Deklarationen entfernen
     - app/dtmf.c: DTMF_SendEndOfTransmission() loeschen oder leeren -->
<!-- OPTION_B: Beibehalten -->
```

### 2.5 BK4819_TransmitTone()

**Datei:** `firmware-source/driver/bk4819.c`, Zeile 1333  
**Aufgerufen von:**
- `functions.c:199` — 1750Hz Ton (ALARM_STATE_TX1750 — disabled)
- `functions.c:204` — 500Hz Ton (ALARM_STATE_TXALARM — disabled)
- `app/app.c:1251` — 500Hz Ton im TX-Pfad (Dead Code)
- `app/app.c:1831` — 1750Hz Ton bei Key-Hold (Dead Code)

```
<!-- ENTSCHEIDUNG 2.5: BK4819_TransmitTone() -->
<!-- OPTION_A: Funktion komplett entfernen (alle Aufrufer sind Dead Code oder disabled).
     Aenderungen:
     - driver/bk4819.c: BK4819_TransmitTone() loeschen
     - driver/bk4819.h: Deklaration entfernen
     - functions.c:199,204: Aufrufe entfernen
     - app/app.c:1251,1831: Aufrufe entfernen -->
<!-- OPTION_B: Beibehalten (fuer potentielle RX-Audio-Feedback-Nutzung) -->
```

### 2.6 Roger-Beep-Funktionen

**Datei:** `firmware-source/driver/bk4819.c`
- `BK4819_PlayRogerNormal()` — Zeile 1718
- `BK4819_PlayRogerMDC()` — Zeile 1755
- `BK4819_PlayRoger()` — Zeile 1803
- `FSK_RogerTable[]` — Zeile 36

**Aufgerufen von:** `RADIO_SendEndOfTransmission()` in `radio.c:1053` (Dead Code)  
**Roger-Einstellung:** `gEeprom.ROGER` gelesen in `settings.c:162`, gespeichert in `settings.c:705`

```
<!-- ENTSCHEIDUNG 2.6: Roger-Beep komplett entfernen -->
<!-- OPTION_A: Alle Roger-Funktionen und die ROGER-Einstellung entfernen.
     Aenderungen:
     - driver/bk4819.c: BK4819_PlayRogerNormal(), BK4819_PlayRogerMDC(), BK4819_PlayRoger(), FSK_RogerTable[] loeschen
     - driver/bk4819.h: BK4819_PlayRoger() Deklaration entfernen
     - settings.c:162: gEeprom.ROGER Laden entfernen
     - settings.c:705: gEeprom.ROGER Speichern entfernen
     - settings.h: ROGER Feld in EEPROM_Config_t entfernen (oder auf 0 setzen)
     - ui/menu.c: MENU_ROGER Eintraege entfernen
     - ui/menu.h: MENU_ROGER Enum-Wert entfernen -->
<!-- OPTION_B: Beibehalten (Roger-Beep ist nur Audio, sendet nicht ueber Antenne wenn TX blockiert) -->
```

---

## TEIL 3: PTT-TASTE UND KEY-HANDLER

### 3.1 GENERIC_Key_PTT() — PTT-Tasten-Handler

**Datei:** `firmware-source/app/generic.c`, Zeilen 102-228  
**Status:** Wird bei jedem PTT-Druck aufgerufen, leitet zu blockiertem RADIO_PrepareTX()

**Inhalt:**
- Zeilen 106-132: PTT-Release (beendet TX falls aktiv)
- Zeilen 135-207: PTT-Press (bereitet TX vor, wird blockiert)
- DTMF-Eingabe-Logik (Zeilen 180-200)

```
<!-- ENTSCHEIDUNG 3.1: PTT-Tasten-Handler -->
<!-- OPTION_A: GENERIC_Key_PTT() vereinfachen — nur noch "TX DISABLED" Beep abspielen.
     Die PTT-Taste wird physisch weiterhin erkannt (Hardware), aber die Software
     ignoriert den Druck und spielt optional einen Warnton.
     Aenderungen:
     - app/generic.c: GENERIC_Key_PTT() Body vereinfachen:
       if (bKeyPressed && !bKeyHeld) { AUDIO_PlayBeep(BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL); }
       return; -->
<!-- OPTION_B: GENERIC_Key_PTT() komplett entfernen und PTT-Taste entmappen.
     Aenderungen:
     - app/generic.c: GENERIC_Key_PTT() loeschen
     - app/app.c: KEY_PTT Verarbeitung entfernen
     - Alle gPttIsPressed Referenzen pruefen -->
<!-- OPTION_C: Beibehalten wie ist (PTT versucht TX, wird blockiert, zeigt Fehlermeldung).
     Vorteil: Benutzer sieht "TX DISABLED" im Display. -->
```

### 3.2 gPttIsPressed und TX-bezogene globale Flags

**Dateien:** `misc.h`, `misc.c`

| Variable | Datei | Zeile | Beschreibung |
|---|---|---|---|
| `gFlagPrepareTX` | misc.h:262 | misc.c:184 | TX-Vorbereitungs-Flag |
| `gFlagEndTransmission` | misc.h:286 | misc.c:206 | TX-Ende-Flag |
| `gTxTimerCountdown_500ms` | misc.h:209 | scheduler.c:67 | TX-Timeout-Zaehler |
| `gTxTimeoutReached` | misc.h:210 | — | TX-Timeout-Flag |
| `gTailToneEliminationCountdown_10ms` | misc.h:212 | — | Tail-Tone-Zaehler |
| `gRTTECountdown_10ms` | misc.h:285 | — | Roger-Tail-Tone-Zaehler |

```
<!-- ENTSCHEIDUNG 3.2: TX-bezogene globale Variablen -->
<!-- OPTION_A: Alle TX-Variablen entfernen (abhaengig von Entscheidung 1.1 OPTION_B).
     Aenderungen:
     - misc.h: Alle oben gelisteten extern-Deklarationen entfernen
     - misc.c: Alle oben gelisteten Variablen-Definitionen entfernen
     - scheduler.c:67: gTxTimerCountdown_500ms Dekrement entfernen
     - Alle Referenzen in app/app.c pruefen und entfernen -->
<!-- OPTION_B: Beibehalten (wenn PTT-Handler-Logik bestehen bleibt) -->
```

---

## TEIL 4: TX-DATENSTRUKTUREN IN VFO UND EEPROM

### 4.1 TX-Felder in VFO_Info_t

**Datei:** `firmware-source/radio.h`, Zeilen 83-135

Folgende Felder der VFO_Info_t Struktur betreffen ausschliesslich TX:

| Feld | Typ | Zeile | Beschreibung |
|---|---|---|---|
| `freq_config_TX` | FREQ_Config_t | 86 | TX-Frequenz, Code-Typ, Code |
| `pTX` | FREQ_Config_t* | 95 | Zeiger auf TX-Konfiguration |
| `TX_OFFSET_FREQUENCY` | uint32_t | 97 | Repeater-TX-Offset |
| `TX_OFFSET_FREQUENCY_DIRECTION` | uint8_t | 102 | Offset-Richtung |
| `OUTPUT_POWER` | uint8_t | 112 | TX-Leistungsstufe |
| `TXP_CalculatedSetting` | uint8_t | 113 | Berechneter TX-PA-Bias |
| `DTMF_PTT_ID_TX_MODE` | PTT_ID_t | 126 | PTT-ID Sende-Modus |

**ACHTUNG — RX-ABHAENGIGKEITEN:**
- `freq_config_TX` und `pTX` werden von `FrequencyReverse` Modus benutzt (siehe 4.2)
- `OUTPUT_POWER` wird in `ui/main.c:641-645,689` fuer Display-Anzeige benutzt
- `TX_OFFSET_FREQUENCY` wird in `RADIO_ApplyOffset()` berechnet (radio.c:491-508)

```
<!-- ENTSCHEIDUNG 4.1: TX-Felder in VFO_Info_t -->
<!-- OPTION_A: Nur reine TX-Felder entfernen, RX-relevante Felder behalten.
     ENTFERNEN:
     - OUTPUT_POWER (und TXP_CalculatedSetting)
     - DTMF_PTT_ID_TX_MODE
     BEHALTEN (wegen FrequencyReverse/Offset):
     - freq_config_TX, pTX
     - TX_OFFSET_FREQUENCY, TX_OFFSET_FREQUENCY_DIRECTION
     Aenderungen:
     - radio.h: OUTPUT_POWER, TXP_CalculatedSetting, DTMF_PTT_ID_TX_MODE entfernen
     - radio.c: RADIO_ConfigureSquelchAndOutputPower() TX-Power-Berechnung entfernen
     - ui/main.c:641-645,689: TX-Power-Anzeige entfernen oder durch festen Wert ersetzen
     - settings.c: TX-Power EEPROM-Lesen/Schreiben entfernen
     - helper/boot.c:92: OUTPUT_POWER Initialisierung entfernen -->
<!-- OPTION_B: Alle TX-Felder entfernen inklusive freq_config_TX und pTX.
     ERFORDERT vorher Entscheidung 4.2 (FrequencyReverse entfernen).
     Aenderungen:
     - radio.h: Alle TX-Felder entfernen
     - radio.c: pRX direkt auf freq_config_RX zeigen lassen (immer)
     - radio.c: RADIO_ApplyOffset() entfernen
     - Alle pTX-Referenzen durch pRX ersetzen oder entfernen
     - settings.c: Alle TX-bezogenen EEPROM-Felder nicht mehr lesen/schreiben -->
<!-- OPTION_C: Alles beibehalten (minimale Aenderungen, nur Runtime-Block) -->
```

### 4.2 FrequencyReverse Modus

**Datei:** `firmware-source/radio.c`, Zeilen 368-377

```c
if (!pVfo->FrequencyReverse) {
    pVfo->pRX = &pVfo->freq_config_RX;
    pVfo->pTX = &pVfo->freq_config_TX;
} else {
    pVfo->pRX = &pVfo->freq_config_TX;   // RX auf TX-Frequenz!
    pVfo->pTX = &pVfo->freq_config_RX;
}
```

**Erklaerung:** FrequencyReverse vertauscht RX und TX Frequenzen. Dies wird im
Repeater-Betrieb benutzt um die Eingabe- statt Ausgabe-Frequenz abzuhoeren.
Bei aktivem FrequencyReverse wird `pRX` auf `freq_config_TX` gesetzt — d.h.
der Empfaenger hoert auf der TX-Offset-Frequenz.

**RX-Relevanz:** Ein reiner Empfaenger koennte diese Funktion zum Abhoeren
von Repeater-Eingabefrequenzen nutzen. Alternativ kann der Benutzer die
Frequenz manuell einstellen.

```
<!-- ENTSCHEIDUNG 4.2: FrequencyReverse Modus -->
<!-- OPTION_A: FrequencyReverse beibehalten.
     freq_config_TX und pTX muessen dann erhalten bleiben.
     TX_OFFSET_FREQUENCY und RADIO_ApplyOffset() ebenfalls.
     Kein RX-Code muss geaendert werden. -->
<!-- OPTION_B: FrequencyReverse entfernen.
     pRX zeigt immer auf freq_config_RX. pTX und freq_config_TX koennen entfernt werden.
     Benutzer muss Repeater-Eingabefrequenz manuell einstellen.
     Aenderungen:
     - radio.h: FrequencyReverse Feld entfernen
     - radio.c:292,300: FrequencyReverse EEPROM-Lesen entfernen
     - radio.c:368-377: Reverse-Logik entfernen, pRX = &freq_config_RX immer
     - radio.c:491-508: RADIO_ApplyOffset() entfernen
     - settings.c:797: FrequencyReverse EEPROM-Speichern entfernen
     - ui/main.c:699-707: FrequencyReverse Display-Anzeige entfernen
     - radio.h: TX_OFFSET_FREQUENCY, TX_OFFSET_FREQUENCY_DIRECTION entfernen
     - radio.h: freq_config_TX, pTX entfernen -->
```

### 4.3 TX-Einstellungen in EEPROM_Config_t

**Datei:** `firmware-source/settings.h`

| Feld | Zeile | Beschreibung | RX-Nutzung? |
|---|---|---|---|
| `TX_VFO` | 146 | Gewahlter TX-VFO Index | **JA** — wird fuer RX_VFO Berechnung benutzt |
| `CROSS_BAND_RX_TX` | 173 | Cross-Band RX/TX | **NEIN** — reine TX-Funktion |
| `ROGER` | ~170 | Roger-Beep Modus | **NEIN** |
| `TX_TIMEOUT_TIMER` | ~170 | TX-Timeout | **NEIN** |
| `TAIL_TONE_ELIMINATION` | ~170 | CSS-Tail-Ton | **NEIN** |
| `DTMF_SIDE_TONE` | ~210 | DTMF Seitentoene | **NEIN** |

**Spezialfall TX_VFO:** Die Variable `gEeprom.TX_VFO` wird im gesamten Code
als "primaerer VFO Index" benutzt — auch fuer reine RX-Operationen. Sie bestimmt,
welcher VFO als `gTxVfo` gesetzt wird, und `gRxVfo` wird daraus abgeleitet.
Eine Umbenennung waere sinnvoll aber nicht zwingend noetig.

```
<!-- ENTSCHEIDUNG 4.3: TX-EEPROM-Einstellungen -->
<!-- OPTION_A: Alle reinen TX-Einstellungen auf feste Werte setzen, nicht mehr aus EEPROM laden.
     TX_VFO beibehalten (wird fuer VFO-Auswahl benutzt).
     Aenderungen:
     - settings.c: CROSS_BAND_RX_TX immer CROSS_BAND_OFF setzen, nicht aus EEPROM laden
     - settings.c: ROGER immer ROGER_MODE_OFF setzen
     - settings.c: TX_TIMEOUT_TIMER ignorieren
     - settings.c: TAIL_TONE_ELIMINATION ignorieren
     - settings.c: Schreib-Funktionen fuer diese Felder entfernen -->
<!-- OPTION_B: TX_VFO in PRIMARY_VFO umbenennen und alle TX-Einstellungen entfernen.
     Umfangreiche Umbenennung in der gesamten Codebasis.
     Aenderungen:
     - settings.h: TX_VFO -> PRIMARY_VFO umbenennen
     - Alle Referenzen auf gEeprom.TX_VFO umschreiben
     - gTxVfo -> gPrimaryVfo umbenennen (radio.c, radio.h, viele Dateien)
     - CROSS_BAND_RX_TX komplett entfernen -->
<!-- OPTION_C: Alles beibehalten -->
```

### 4.4 CROSS_BAND_RX_TX und DUAL_WATCH

**Datei:** `firmware-source/radio.c`, Zeilen 516, 522  
**Datei:** `firmware-source/settings.h`, Zeilen 55-63, 172-173

**CROSS_BAND_RX_TX** bestimmt ob auf einem VFO empfangen und auf dem anderen
gesendet wird. Fuer reinen RX-Betrieb ist dies irrelevant.

**DUAL_WATCH** ermoeglicht abwechselndes Abhoeren zweier VFOs. Dies ist eine
**reine RX-Funktion** und sollte **beibehalten** werden.

**Problem:** Die DUAL_WATCH Logik in `RADIO_SelectVfos()` (radio.c:522) verknuepft
DUAL_WATCH mit CROSS_BAND_RX_TX:
```c
gEeprom.RX_VFO = (gEeprom.CROSS_BAND_RX_TX == CROSS_BAND_OFF
    || gEeprom.DUAL_WATCH != DUAL_WATCH_OFF)
    ? gEeprom.TX_VFO : !gEeprom.TX_VFO;
```

```
<!-- ENTSCHEIDUNG 4.4: CROSS_BAND_RX_TX -->
<!-- OPTION_A: CROSS_BAND_RX_TX entfernen, DUAL_WATCH beibehalten.
     CROSS_BAND_RX_TX wird immer CROSS_BAND_OFF sein.
     Vereinfachung der VFO-Auswahl-Logik:
     Aenderungen:
     - settings.c: CROSS_BAND_RX_TX nicht aus EEPROM laden, immer CROSS_BAND_OFF
     - radio.c:516: Vereinfachen zu gCurrentVfo = gRxVfo;
     - radio.c:522: Vereinfachen zu gEeprom.RX_VFO = gEeprom.TX_VFO;
     - radio.c:149-152: CROSS_BAND Check entfernen
     - ui/status.c:139: CROSS_BAND Anzeige-Check entfernen
     - misc.h/misc.c: gBackup_CROSS_BAND_RX_TX entfernen
     - app/chFrScanner.c: initialCROSS_BAND_RX_TX entfernen -->
<!-- OPTION_B: Beide beibehalten (keine Aenderung) -->
```

---

## TEIL 5: TX-BEZOGENE MENUE-EINTRAEGE

### 5.1 Reine TX-Menue-Eintraege

**Datei:** `firmware-source/ui/menu.c`, `firmware-source/ui/menu.h`

| Menue-Eintrag | Label | Zeile (menu.c) | Funktion | RX-relevant? |
|---|---|---|---|---|
| MENU_TXP | "TxPwr" | 77 | TX-Leistungsstufe | **NEIN** |
| MENU_T_DCS | "TxDCS" | 80 | TX DCS-Code | **NEIN** |
| MENU_T_CTCS | "TxCTCS" | 81 | TX CTCSS-Code | **NEIN** |
| MENU_SFT_D | "TxODir" | 82 | TX-Offset-Richtung | Nur fuer FreqReverse |
| MENU_OFFSET | "TxOffs" | 83 | TX-Offset-Frequenz | Nur fuer FreqReverse |
| MENU_TOT | "TxTOut" | 120 | TX-Timeout | **NEIN** |
| MENU_ROGER | "Roger" | 137 | Roger-Beep | **NEIN** |
| MENU_ANI_ID | "ANI ID" | 145 | ANI-Identifikation | **NEIN** |
| MENU_UPCODE | "UPCode" | 147 | DTMF Up-Code | **NEIN** |
| MENU_DWCODE | "DWCode" | 148 | DTMF Down-Code | **NEIN** |
| MENU_PTT_ID | "PTT ID" | 149 | PTT-ID Modus | **NEIN** |
| MENU_VOX | "VOX" | 165 | Voice-TX (disabled) | **NEIN** |

### 5.2 Doppelt-genutzte Menue-Eintraege (RX und TX)

| Menue-Eintrag | Label | Beschreibung | TX-Nutzung | RX-Nutzung |
|---|---|---|---|---|
| MENU_R_DCS | "RxDCS" | RX DCS-Code | — | **JA** — Squelch |
| MENU_R_CTCS | "RxCTCS" | RX CTCSS-Code | — | **JA** — Squelch |
| MENU_SCR | "Scramb" | Scrambler | TX-Scramble | **JA** — RX-Descramble |
| MENU_CMP | "Compnd" | Compander | TX-Compress | **JA** — RX-Expand |
| MENU_D_RSP | "D Resp" | DTMF Response | TX-Auto-Reply | Teilweise RX |
| MENU_D_HOLD | "D Hold" | DTMF Hold | TX-Reply-Timer | Teilweise RX |
| MENU_D_PRE | "D Prel" | DTMF Preload | TX-Preload | **NEIN** |
| MENU_D_DCD | "D Decd" | DTMF Decode | — | **JA** — RX-Decode |
| MENU_D_LIST | "D List" | DTMF Liste | TX-Kontakte | **JA** — RX-Kontakte |
| MENU_D_LIVE | "D Live" | DTMF Live Decode | — | **JA** — RX-Live |

```
<!-- ENTSCHEIDUNG 5.1: TX-Menue-Eintraege entfernen -->
<!-- OPTION_A: Alle reinen TX-Menue-Eintraege entfernen.
     Aenderungen:
     - ui/menu.c: MenuList[] Eintraege fuer MENU_TXP, MENU_T_DCS, MENU_T_CTCS,
       MENU_TOT, MENU_ROGER, MENU_ANI_ID, MENU_UPCODE, MENU_DWCODE, MENU_PTT_ID,
       MENU_VOX entfernen
     - ui/menu.h: Entsprechende Enum-Werte entfernen
     - app/menu.c: Entsprechende case-Bloecke in MENU_AcceptSetting() und
       MENU_ShowCurrentSetting() entfernen
     - MENU_SFT_D und MENU_OFFSET: nur entfernen wenn FrequencyReverse entfernt wird (4.2)
     - MENU_D_RSP, MENU_D_HOLD, MENU_D_PRE: entfernen (TX-Reply-Funktionen) -->
<!-- OPTION_B: Menue-Eintraege beibehalten aber deaktivieren (ausgrauen/ueberspringen) -->
<!-- OPTION_C: Alles beibehalten -->
```

---

## TEIL 6: SCRAMBLER UND COMPANDER — RX-FEATURES MIT TX-KOMPONENTEN

### 6.1 Scrambler

**Dateien:** `driver/bk4819.c`, `radio.c`, `functions.c`, `ui/menu.c`

**RX-Nutzung:** Der Scrambler wird in `radio.c:670-673` und `functions.c:217-220`
fuer die **RX-Entschluesselung** eingeschaltet. Der BK4819-Chip fuehrt die
Descramble-Operation in der RX-Audio-Pipeline durch.

**TX-Nutzung:** Derselbe Scrambler wuerde bei TX die Audio-Verschluesselung
durchfuehren. Da TX blockiert ist, wird der TX-Scrambler nie aktiv.

**Funktionen:**
- `BK4819_EnableScramble(Type)` — Setzt Scramble-Frequenz (bk4819.c:882) — **RX benoetigt**
- `BK4819_DisableScramble()` — Deaktiviert Scramble (bk4819.c:876) — **RX benoetigt**
- `BK4819_SetScrambleFrequencyControlWord()` — Setzt Scramble-Freq (bk4819.c:1823)

**Analyse:** Der Scrambler des BK4819 arbeitet bidirektional — dieselbe
Registereinstellung gilt fuer RX-Descramble UND TX-Scramble.
Die Scrambler-Funktionen muessen fuer RX-Descramble erhalten bleiben.
Es gibt keinen separaten "TX-Scramble" Code der entfernt werden koennte.

```
<!-- ENTSCHEIDUNG 6.1: Scrambler -->
<!-- OPTION_A: Scrambler vollstaendig beibehalten (wird fuer RX-Descramble benoetigt).
     Keine Aenderungen noetig. -->
<!-- OPTION_B: Scrambler komplett entfernen (wenn RX-Descramble nicht benoetigt wird).
     Aenderungen:
     - driver/bk4819.c: BK4819_EnableScramble(), BK4819_DisableScramble(), BK4819_SetScrambleFrequencyControlWord() loeschen
     - driver/bk4819.h: Deklarationen entfernen
     - radio.c:670-673: Scrambler-Aktivierung entfernen
     - functions.c:217-220: Scrambler-Aktivierung entfernen
     - radio.h: SCRAMBLING_TYPE aus VFO_Info_t entfernen
     - misc.h/misc.c: gSetting_ScrambleEnable entfernen
     - ui/menu.c/h: MENU_SCR entfernen
     - app/menu.c: Scrambler-Menueverarbeitung entfernen -->
```

### 6.2 Compander

**Dateien:** `driver/bk4819.c:890-955`, `radio.c:707,800`

**RX-Nutzung:** In `radio.c:707` (RADIO_SetupRegisters — RX-Setup):
```c
BK4819_SetCompander((gRxVfo->Modulation == MODULATION_FM && gRxVfo->Compander >= 2)
    ? gRxVfo->Compander : 0);
```
Compander Modus 2 = RX-only Expander, Modus 3 = TX+RX.

**TX-Nutzung:** In `radio.c:800` (RADIO_SetTxParameters — Dead Code):
```c
BK4819_SetCompander((gRxVfo->Modulation == MODULATION_FM
    && (gRxVfo->Compander == 1 || gRxVfo->Compander >= 3))
    ? gRxVfo->Compander : 0);
```
Compander Modus 1 = TX-only Compressor, Modus 3 = TX+RX.

**Analyse:** `BK4819_SetCompander()` konfiguriert BEIDE Seiten (Compressor REG_29
und Expander REG_28) in einem Aufruf. Die Logik in radio.c:707 aktiviert den
Expander (RX-Seite) nur bei Modus >= 2.

**Problem:** Compander Modus 1 (TX-only) und die TX-Seite von Modus 3 koennten
aus `BK4819_SetCompander()` entfernt werden. Aber die Funktion setzt ohnehin
nur Register, und TX ist blockiert, also besteht kein Risiko.

```
<!-- ENTSCHEIDUNG 6.2: Compander -->
<!-- OPTION_A: Compander beibehalten, TX-Compressor-Logik in BK4819_SetCompander() entfernen.
     Nur der RX-Expander (REG_28) wird konfiguriert.
     Compander Modus 1 (TX-only) wird ignoriert.
     Aenderungen:
     - driver/bk4819.c: BK4819_SetCompander(): compress_ratio immer 0 setzen (kein TX-Compressor)
     - radio.c:800: TX-Compander-Aufruf entfernen (ist in RADIO_SetTxParameters, wird eh entfernt) -->
<!-- OPTION_B: Compander vollstaendig beibehalten (einschliesslich TX-Logik in Register-Setup).
     Kein Risiko da TX blockiert. -->
<!-- OPTION_C: Compander komplett entfernen.
     Aenderungen:
     - driver/bk4819.c: BK4819_SetCompander(), BK4819_CompanderEnabled() loeschen
     - radio.h: Compander Feld aus VFO_Info_t entfernen
     - radio.c:707: Compander-Setup entfernen
     - ui/main.c: Compander-Symbol entfernen -->
```

---

## TEIL 7: FUNCTION_TRANSMIT STATE UND ZUSTANDSMASCHINE

### 7.1 FUNCTION_TRANSMIT Enum-Wert

**Datei:** `firmware-source/functions.h`, Zeile 25

```c
typedef enum {
    FUNCTION_FOREGROUND = 0,
    FUNCTION_TRANSMIT,       // <-- dieser Wert
    FUNCTION_MONITOR,
    FUNCTION_INCOMING,
    FUNCTION_RECEIVE,
    FUNCTION_POWER_SAVE,
    FUNCTION_BAND_SCOPE,
} FUNCTION_Type_t;
```

**Problem:** `FUNCTION_TRANSMIT` wird an vielen Stellen in `if`-Abfragen und
`switch`-Anweisungen referenziert (ca. 25 Stellen). Das Entfernen des Enum-Werts
wuerde die Nummerierung aller folgenden Werte aendern, was Bugs verursachen koennte
wenn diese Werte irgendwo als Integer gespeichert sind.

**Referenzen in der Codebasis:**
- `functions.c:96,251` — State-Machine switch/case
- `app/app.c:411,620,770,799,819,835,1018,1178,1213,1416,1523,1533,1674`
- `ui/status.c:51`, `ui/main.c:134,160,191,433,495,532,742`
- `helper/battery.c:178`, `scheduler.c:89,94,100,111`

Die meisten Referenzen sind Guards: `if (gCurrentFunction != FUNCTION_TRANSMIT)`.
Da `gCurrentFunction` nie auf `FUNCTION_TRANSMIT` gesetzt wird (TX ist blockiert),
evaluieren diese Guards immer zu `true` und sind funktional irrelevant.

```
<!-- ENTSCHEIDUNG 7.1: FUNCTION_TRANSMIT -->
<!-- OPTION_A: FUNCTION_TRANSMIT Enum-Wert beibehalten aber nie setzen.
     Alle Checks "!= FUNCTION_TRANSMIT" sind immer true = harmlos.
     Minimale Aenderung, kein Risiko.
     In functions.c:251 den case FUNCTION_TRANSMIT durch break; ersetzen.
     FUNCTION_NOP Mapping in app/app.c:411 beibehalten. -->
<!-- OPTION_B: FUNCTION_TRANSMIT komplett entfernen und alle Referenzen bereinigen.
     VORSICHT: Enum-Nummerierung aendert sich!
     Aenderungen:
     - functions.h: FUNCTION_TRANSMIT entfernen
     - Alle ca. 25 Referenzen anpassen oder entfernen
     - functions.c: case FUNCTION_TRANSMIT entfernen
     - app/app.c:411: FUNCTION_TRANSMIT Mapping entfernen
     - Alle "!= FUNCTION_TRANSMIT" Guards entfernen (immer true)
     - Alle "== FUNCTION_TRANSMIT" Bloecke entfernen (immer false = Dead Code) -->
```

### 7.2 VFO_STATE_TX_DISABLE

**Datei:** `firmware-source/radio.h`

```c
typedef enum {
    VFO_STATE_NORMAL = 0,
    VFO_STATE_BUSY,
    VFO_STATE_BAT_LOW,
    VFO_STATE_TX_DISABLE,     // <-- dieser Wert
    VFO_STATE_TIMEOUT,
    VFO_STATE_ALARM,
    VFO_STATE_VOLTAGE_HIGH,
} VfoState_t;
```

**Status:** Wird aktiv benutzt in `radio.c:960` wenn TX blockiert wird.
Wird auch in UI-Code fuer Anzeige benutzt.

```
<!-- ENTSCHEIDUNG 7.2: VFO_STATE_TX_DISABLE -->
<!-- OPTION_A: Beibehalten (wird fuer "TX DISABLED" Display-Anzeige benutzt).
     Wenn PTT gedrueckt wird, zeigt das Geraet weiterhin "TX DISABLED" an. -->
<!-- OPTION_B: Entfernen (nur wenn PTT-Handler komplett entfernt wird, Entscheidung 3.1 OPTION_B).
     VFO_STATE_TIMEOUT und VFO_STATE_ALARM koennen ebenfalls entfernt werden. -->
```

---

## TEIL 8: UART-REGISTERZUGRIFF (SICHERHEITSRISIKO)

### 8.1 BK4819 Register-Lesen/Schreiben ueber UART

**Datei:** `firmware-source/app/uart.c`, Zeilen 437-472  
**Makefile-Flag:** `ENABLE_UART_RW_BK_REGS = 0` (Zeile 48 — **DEAKTIVIERT**)

**Status:** Diese Funktion ist durch Compile-Flag deaktiviert und nicht im Binary.

**Potentielles Risiko:** Wenn aktiviert, koennte ueber die serielle Schnittstelle
direkt auf BK4819-Register geschrieben werden, einschliesslich:
- `BK4819_REG_36` (PA-Verstaerker)
- `BK4819_REG_30` (TX DSP Enable)

```
<!-- ENTSCHEIDUNG 8.1: UART Register-Zugriff -->
<!-- OPTION_A: ENABLE_UART_RW_BK_REGS dauerhaft auf 0 lassen (ist bereits so).
     Kein Handlungsbedarf. -->
<!-- OPTION_B: UART Register-Zugriff Code komplett entfernen (auch die #ifdef Bloecke).
     Aenderungen:
     - app/uart.c: #ifdef ENABLE_UART_RW_BK_REGS Bloecke komplett loeschen
     - Makefile: ENABLE_UART_RW_BK_REGS Option entfernen -->
```

### 8.2 UART CMD_052D (EEPROM Write)

**Datei:** `firmware-source/app/uart.c`, Zeile 357  
**Status:** Aktiv wenn ENABLE_UART=0 (UART ist deaktiviert in Makefile:6)

```
<!-- ENTSCHEIDUNG 8.2: UART komplett -->
<!-- OPTION_A: ENABLE_UART auf 0 belassen (ist bereits so). Kein Handlungsbedarf. -->
<!-- OPTION_B: UART-Code komplett entfernen fuer maximale Sicherheit.
     ACHTUNG: Entfernt auch Firmware-Upload-Moeglichkeit!
     Nur ausfuehren wenn sicher ist, dass kein serieller Zugriff benoetigt wird. -->
```

---

## TEIL 9: ALARM-FUNKTIONALITAET

### 9.1 AlarmState und Alarm-Handler

**Dateien:** `misc.h:248`, `misc.c:169`, `functions.c:159-220`, `app/action.c:456`  
**Makefile-Flags:** `ENABLE_ALARM=0`, `ENABLE_TX1750=0` (beide deaktiviert)

**Status:** Alarm-Code ist grossteils durch `#ifdef` geschuetzt und nicht kompiliert.
Die Variable `gAlarmState` existiert aber in misc.c und wird in einigen
nicht-ifdef-geschuetzten Stellen referenziert.

```
<!-- ENTSCHEIDUNG 9.1: Alarm-Funktionalitaet -->
<!-- OPTION_A: Alarm-Variable und nicht-ifdef-geschuetzte Referenzen entfernen.
     Aenderungen:
     - misc.h: gAlarmState extern-Deklaration entfernen
     - misc.c: gAlarmState Definition entfernen
     - radio.c:955-956: #if defined(ENABLE_ALARM) Block bereinigen
     - radio.c:986: gAlarmState = ALARM_STATE_OFF entfernen
     - ui/main.c: Alarm-State-Checks entfernen
     - Alle nicht-ifdef-geschuetzten gAlarmState Referenzen entfernen -->
<!-- OPTION_B: Beibehalten (hat keine TX-Wirkung da ENABLE_ALARM=0) -->
```

---

## TEIL 10: DTMF-SYSTEM (EMPFANG UND SENDEN)

### 10.1 DTMF RX-Decode (behalten)

**Dateien:** `app/dtmf.c`, `driver/bk4819.c`

Die DTMF-Decodierung (Empfang von DTMF-Toenen) ist eine reine RX-Funktion und
wird durch den BK4819-Chip in Hardware durchgefuehrt. Diese Funktionalitaet
sollte beibehalten werden:
- `BK4819_GetDTMF_5TONE_Code()` — DTMF-Erkennung (RX)
- DTMF_FindContact() — Kontakt-Suche
- DTMF Live Decode — Anzeige empfangener DTMF-Toene

### 10.2 DTMF TX-Antwort-Logik (entfernen)

**Dateien:** `app/dtmf.c`, `functions.c:190`

**Funktion:** `DTMF_Reply()` in `functions.c:190` wuerde auf empfangene DTMF-Codes
automatisch antworten (TX). Da TX blockiert ist, ist dies Dead Code.

**Variablen:**
- `gDTMF_ReplyState` — Antwort-Zustand (misc.h, misc.c, mehrere Referenzen)
- `gDTMF_CallState` — Anruf-Zustand
- `gDTMF_IsTx` — DTMF-TX-Flag
- `gDTMF_TxStopCountdown_500ms` — TX-Stop-Timer

```
<!-- ENTSCHEIDUNG 10.1: DTMF TX-Antwort-System -->
<!-- OPTION_A: DTMF TX-Reply entfernen, RX-Decode beibehalten.
     Aenderungen:
     - app/dtmf.c: DTMF_SendEndOfTransmission() loeschen
     - app/dtmf.c: DTMF_Reply() stub-en oder loeschen
     - functions.c:190: DTMF_Reply() Aufruf entfernen
     - misc.h/misc.c: gDTMF_ReplyState, gDTMF_IsTx, gDTMF_TxStopCountdown_500ms entfernen
     - DTMF_CallState vereinfachen (nur noch NONE und RECEIVED behalten)
     - ui/main.c: DTMF-TX-Anzeigen ("CALL OUT", gDTMF_IsTx) entfernen -->
<!-- OPTION_B: DTMF-System komplett beibehalten (kein Risiko da TX blockiert) -->
```

---

## TEIL 11: DISPLAY/UI TX-ANZEIGEN

### 11.1 TX-bezogene UI-Elemente in ui/main.c

**Datei:** `firmware-source/ui/main.c`

| Element | Zeilen | Beschreibung |
|---|---|---|
| TX-Power Anzeige | 641-645, 689 | "L", "M", "H" TX-Leistung |
| TX-Frequenz | 535 | Zeigt pTX->Frequency |
| FrequencyReverse Symbol | 699-707 | "R" Symbol bei Reverse |
| TX-Status | 134, 160, 191, 433, 495, 532, 742 | Checks auf FUNCTION_TRANSMIT |
| DTMF Call-State | 378-404 | "CALL OUT"/"CALL FRM" Anzeige |
| Scrambler Symbol | 727 | Scramble-Indikator |
| Compander Symbol | 548-550, 625 | Compander-Indikator |
| Mic Bar | 742 | Mikrofon-Pegel-Balken (nur TX) |

### 11.2 TX-Status-Anzeige in ui/status.c

**Datei:** `firmware-source/ui/status.c`, Zeile 51

```c
if (gCurrentFunction == FUNCTION_TRANSMIT) {
    // "TX" Status-Anzeige
}
```

```
<!-- ENTSCHEIDUNG 11.1: TX-Display-Elemente -->
<!-- OPTION_A: Alle TX-bezogenen UI-Elemente entfernen.
     Aenderungen:
     - ui/main.c:535: pTX->Frequency Anzeige entfernen
     - ui/main.c:641-645: TX-Power Buchstabe entfernen
     - ui/main.c:689: TX-Power Berechnung entfernen
     - ui/main.c:742: Mic-Bar entfernen
     - ui/main.c: Alle FUNCTION_TRANSMIT Checks in if-Anweisungen entfernen
     - ui/main.c: DTMF "CALL OUT" Anzeige entfernen
     - ui/status.c:51: TX-Status-Anzeige entfernen -->
<!-- OPTION_B: Nur Mic-Bar und "TX" Status entfernen, Rest beibehalten.
     TX-Power Anzeige koennte als "Info was gespeichert ist" beibehalten werden.
     FrequencyReverse Anzeige ist RX-relevant (wenn beibehalten). -->
<!-- OPTION_C: Alles beibehalten -->
```

---

## TEIL 12: ZUSAMMENFASSENDE EMPFEHLUNG

### Minimale Aenderungen (konservativ)

Nur Runtime-Blocker aktiv, kein Code entfernt. **Aktueller Zustand.**
- Vorteil: Keine Risiken, keine Bugs
- Nachteil: Grosser Binary, unnoetig komplexer Code

### Moderate Aenderungen (empfohlen)

Entfernt toten TX-Code, behaelt aber VFO-Struktur und FrequencyReverse:
- RADIO_PrepareTX() -> Stub (1.1 OPTION_A)
- RADIO_SetTxParameters() entfernen (1.2 OPTION_A)
- SendCssTail/SendEndOfTransmission entfernen (1.3 OPTION_A)
- APP_EndTransmission() entfernen (1.4 OPTION_A)
- BK4819 TX-Funktionen entfernen (2.1-2.6 OPTION_A)
- PTT vereinfachen (3.1 OPTION_A)
- TX-Variablen entfernen (3.2 OPTION_A)
- OUTPUT_POWER/PTT_ID entfernen (4.1 OPTION_A)
- FrequencyReverse beibehalten (4.2 OPTION_A)
- CROSS_BAND entfernen (4.4 OPTION_A)
- TX-Menuees entfernen (5.1 OPTION_A)
- Scrambler beibehalten (6.1 OPTION_A)
- Compander TX-Seite entfernen (6.2 OPTION_A)
- FUNCTION_TRANSMIT beibehalten (7.1 OPTION_A)
- UART belassen (8.1 OPTION_A)
- Alarm entfernen (9.1 OPTION_A)
- DTMF-TX-Reply entfernen (10.1 OPTION_A)
- TX-UI entfernen (11.1 OPTION_A oder OPTION_B)

### Maximale Aenderungen (aggressiv)

Entfernt allen TX-Code einschliesslich Datenstrukturen:
- Alle OPTION_B Varianten
- FrequencyReverse entfernen
- FUNCTION_TRANSMIT Enum entfernen
- Umfangreiche Code-Bereinigung

---

## Anweisungen fuer die naechste Agenten-Sitzung

1. Lies diese Datei vollstaendig.
2. Identifiziere alle einkommentierten (nicht in `<!-- -->` eingeschlossenen) OPTION-Zeilen.
3. Fuehre **nur** die einkommentierten Optionen aus.
4. Arbeite die Aenderungen in der Reihenfolge der Abschnitte ab (1.1 vor 1.2, etc.).
5. Beachte Abhaengigkeiten zwischen Entscheidungen (z.B. 4.2 beeinflusst 4.1).
6. Baue den Code nach jeder groesseren Aenderungsgruppe und stelle sicher, dass er kompiliert.
7. Markiere erledigte Abschnitte als `[ERLEDIGT]` in dieser Datei.
8. Aktualisiere HANDOVER.md mit den durchgefuehrten Aenderungen.

---

**Ende der Analyse**

73, Agent an Do9RE — die Codebasis ist bereit fuer deine Entscheidungen!
