# Audit: Verbleibende nicht-essentielle Funktionen

**Erstellt:** 2026-04-11  
**Firmware-Stand:** Nach Phase 1+2 TX-Entfernung + Scrambler-Entfernung  
**Binary-Groesse:** 48468 bytes text  
**Entscheidungsmethode:** Einkommentieren der gewuenschten OPTION

---

## So funktioniert die Steuerung

Jeder Abschnitt enthaelt Entscheidungsbloecke. Richard kommentiert die
gewuenschte Option ein (entfernt die `<!-- -->` Klammern um die Zeile):

```
<!-- ENTSCHEIDUNG: Beschreibung -->
<!-- OPTION_A: Das machen -->
<!-- OPTION_B: Das andere machen -->
```

Wird zu (Beispiel: Option A gewaehlt):

```
<!-- ENTSCHEIDUNG: Beschreibung -->
OPTION_A: Das machen
<!-- OPTION_B: Das andere machen -->
```

---

## Uebersicht: Was ist noch drin?

| # | Feature | Typ | Status | RX-relevant? |
|---|---------|-----|--------|--------------|
| 1 | TX DCS/CTCSS Menue | Menue | Sichtbar | Nein |
| 2 | PTT ID Menue | Menue | Sichtbar | Nein |
| 3 | Roger-Beep | Code+Menue | Aktiv | Nein |
| 4 | TX Offset / Shift Direction | Menue+Struct | Sichtbar | Nein |
| 5 | FrequencyReverse (R-Taste) | Feature | Aktiv | Bedingt |
| 6 | Output Power (TXP) Menue | Menue | Sichtbar | Nein |
| 7 | BK4819 TX-Treiberfunktionen | Dead Code | Kompiliert | Nein |
| 8 | DTMF TX-Funktionen | Dead Code | Kompiliert | Nein |
| 9 | DTMF Live RX Decoder | Feature | Aktiv | Ja |
| 10 | Compander | Feature | Aktiv | Ja (RX-Expander) |
| 11 | Busy Channel Lock (BCL) | Menue | Aktiv | Bedingt |
| 12 | freq_config_TX in VFO_Info_t | Struct-Feld | Vorhanden | Nein (EEPROM-Kompatibilitaet) |
| 13 | TX_OFFSET_FREQUENCY in Struct | Struct-Feld | Vorhanden | Nein (EEPROM-Kompatibilitaet) |
| 14 | OUTPUT_POWER in Struct | Struct-Feld | Vorhanden | Nein (EEPROM-Kompatibilitaet) |
| 15 | gSetting_350EN | Setting | Aktiv | Ja (RX-Bandfreigabe) |
| 16 | TX-bezogene Makefile-Flags | Config | Deaktiviert | Sicher |

---

## 1. TX DCS / TX CTCSS Menuepunkte

**Dateien:** `ui/menu.c:80-81` (MenuList Eintraege "TxDCS", "TxCTCS")  
**Dateien:** `app/menu.c` (MENU_T_DCS, MENU_T_CTCS Handler)  
**Dateien:** `ui/menu.h:39-40` (Enum-Werte)

Diese Menuepunkte erlauben das Einstellen von TX-DCS/CTCSS-Codes.
Da nicht gesendet wird, sind sie nutzlos.
RX-DCS/CTCSS (R_DCS, R_CTCS) bleiben erhalten — die werden zum Filtern
empfangener Signale benoetigt.

```
<!-- ENTSCHEIDUNG 1: TX DCS/CTCSS Menuepunkte -->
<!-- OPTION_A: Entfernen — MenuList-Eintraege, Menu-Handler und Enum-Werte loeschen -->
<!-- OPTION_B: Beibehalten (keine Aenderung) -->
```

---

## 2. PTT ID Menue

**Dateien:** `ui/menu.c:148` (MenuList "PTT ID")  
**Dateien:** `ui/menu.c:309` (gSubMenu_PTT_ID Array)  
**Dateien:** `app/menu.c:732,1300,1764` (Handler)  
**Dateien:** `radio.h:38-45` (PTT_ID_t Enum)  
**Dateien:** `radio.h:125` (DTMF_PTT_ID_TX_MODE in VFO_Info_t)

PTT ID sendet DTMF-Toene am Anfang/Ende einer Aussendung.
100% TX-only, fuer RX voellig nutzlos.

```
<!-- ENTSCHEIDUNG 2: PTT ID -->
<!-- OPTION_A: Entfernen — Menue, Enum, Handler, gSubMenu_PTT_ID loeschen.
     Struct-Feld DTMF_PTT_ID_TX_MODE auf 0 setzen statt aus EEPROM laden. -->
<!-- OPTION_B: Beibehalten (keine Aenderung) -->
```

---

## 3. Roger-Beep

**Dateien:** `driver/bk4819.c` — BK4819_PlayRogerNormal(), BK4819_PlayRogerMDC(),
  BK4819_PlayRoger(), FSK_RogerTable[]  
**Dateien:** `driver/bk4819.h` — Deklarationen  
**Dateien:** `ui/menu.c` — MENU_ROGER MenuList-Eintrag, gSubMenu_ROGER Array  
**Dateien:** `app/menu.c` — MENU_ROGER Handler

Roger-Beep ist ein Ton am Ende einer Aussendung. 100% TX-only.

```
<!-- ENTSCHEIDUNG 3: Roger-Beep -->
<!-- OPTION_A: Entfernen — BK4819_PlayRoger*() Funktionen, FSK_RogerTable[],
     Menue-Eintraege und Handler loeschen -->
<!-- OPTION_B: Beibehalten (keine Aenderung) -->
```

---

## 4. TX Offset / Shift Direction Menue

**Dateien:** `ui/menu.c` — MENU_SFT_D ("TxODir"), MENU_OFFSET ("TxOffs") MenuList-Eintraege  
**Dateien:** `ui/menu.h` — Enum-Werte MENU_SFT_D, MENU_OFFSET  
**Dateien:** `app/menu.c` — Handler fuer diese Menues  
**Dateien:** `radio.h:97,102` — TX_OFFSET_FREQUENCY, TX_OFFSET_FREQUENCY_DIRECTION in Struct

TX Offset bestimmt die Sendefrequenz relativ zur Empfangsfrequenz
(fuer Repeater-Betrieb). Fuer reinen Empfang nutzlos.

Hinweis: Die Struct-Felder muessen fuer EEPROM-Kompatibilitaet erhalten bleiben,
aber die Menues koennen entfernt werden.

```
<!-- ENTSCHEIDUNG 4: TX Offset Menues -->
<!-- OPTION_A: Menue-Eintraege entfernen (Struct-Felder bleiben fuer EEPROM-Kompatibilitaet) -->
<!-- OPTION_B: Beibehalten (keine Aenderung) -->
```

---

## 5. FrequencyReverse (R-Taste)

**Dateien:** `radio.h:114` — bool FrequencyReverse in VFO_Info_t  
**Dateien:** `radio.c:360-365` — Swap pRX/pTX bei Aktivierung  
**Dateien:** `ui/main.c` — "R" Symbol-Anzeige

FrequencyReverse tauscht RX- und TX-Frequenz. Das erlaubt es, auf der
TX-Frequenz eines Repeaters zu hoeren. Kann fuer Monitoring nuetzlich sein,
ist aber nur sinnvoll wenn TX_OFFSET gesetzt ist.

```
<!-- ENTSCHEIDUNG 5: FrequencyReverse -->
<!-- OPTION_A: Entfernen — FrequencyReverse immer false setzen, pRX/pTX Swap entfernen,
     R-Taste-Handler anpassen -->
<!-- OPTION_B: Beibehalten — Nuetzlich fuer Repeater-Monitoring (keine Aenderung) -->
```

---

## 6. Output Power (TXP) Menue

**Dateien:** `ui/menu.c` — MENU_TXP ("TxPwr") MenuList-Eintrag, gSubMenu_TXP Array  
**Dateien:** `app/menu.c` — Handler  
**Dateien:** `radio.h:112-113` — OUTPUT_POWER, TXP_CalculatedSetting in Struct

Sendeleistung hat fuer reinen Empfang keine Bedeutung.

```
<!-- ENTSCHEIDUNG 6: Output Power Menue -->
<!-- OPTION_A: Menue-Eintrag und gSubMenu_TXP entfernen (Struct-Feld bleibt) -->
<!-- OPTION_B: Beibehalten (keine Aenderung) -->
```

---

## 7. BK4819 TX-Treiberfunktionen (Dead Code)

**Dateien:** `driver/bk4819.c` — ca. 200 Zeilen TX-Code:
- BK4819_SetupPowerAmplifier() — PA-Bias einstellen
- BK4819_PrepareTransmit() — TX-Hardware vorbereiten
- BK4819_EnableTXLink() — TX-Audio-Pfad aktivieren
- BK4819_EnterTxMute() / BK4819_ExitTxMute() — TX stumm schalten
- BK4819_EnterDTMF_TX() / BK4819_ExitDTMF_TX() — DTMF senden
- BK4819_PlayDTMF() / BK4819_PlayDTMFString() — DTMF-Toene senden
- BK4819_TransmitTone() — Ton senden
- BK4819_PlayDTMFEx() — DTMF erweitert

**Dateien:** `driver/bk4819.h` — Deklarationen

Diese Funktionen werden nie aufgerufen (TX-Pfad ist entfernt),
belegen aber Platz im Binary.

Hinweis: BK4819_SetupPowerAmplifier(0, 0) wird in radio.c:555 aufgerufen
um den PA auszuschalten. Dieser Aufruf muesste durch direkten Register-Write
ersetzt werden.

```
<!-- ENTSCHEIDUNG 7: BK4819 TX-Funktionen -->
<!-- OPTION_A: Alle oben genannten TX-Funktionen entfernen.
     PA-Abschaltung durch direkten Register-Write ersetzen.
     Spart ca. 200+ Bytes im Binary. -->
<!-- OPTION_B: Beibehalten (keine Aenderung, harmloser Dead Code) -->
```

---

## 8. DTMF TX-Funktionen (Dead Code)

**Dateien:** `app/dtmf.c` — DTMF_SendEndOfTransmission(), DTMF_Reply()  
**Dateien:** `app/dtmf.h` — Deklarationen, gDTMF_IsTx Variable

Diese Funktionen senden DTMF-Toene waehrend TX. Sie werden nicht aufgerufen
(TX-Pfad entfernt), belegen aber Platz.

Der DTMF RX-Decoder (gDTMF_RX_live) ist NICHT betroffen und bleibt erhalten.

```
<!-- ENTSCHEIDUNG 8: DTMF TX-Funktionen -->
<!-- OPTION_A: DTMF_SendEndOfTransmission(), DTMF_Reply(), gDTMF_IsTx entfernen.
     DTMF RX-Decoder bleibt erhalten. -->
<!-- OPTION_B: Beibehalten (keine Aenderung) -->
```

---

## 9. DTMF Live RX Decoder

**Dateien:** `app/dtmf.c` — gDTMF_RX_live[], DTMF_GetCharacter()  
**Dateien:** `ui/main.c` — Anzeige des decodierten DTMF auf dem Display

Empfaengt und decodiert eingehende DTMF-Toene. Zeigt sie auf dem Display an.
Dies ist eine reine RX-Funktion und nützlich zum Identifizieren von
Signalen.

```
<!-- ENTSCHEIDUNG 9: DTMF RX Decoder -->
<!-- INFO: Wird empfohlen beizubehalten (reine RX-Funktion) -->
<!-- OPTION_A: Beibehalten (empfohlen, keine Aenderung) -->
<!-- OPTION_B: Entfernen — DTMF-Decoder komplett deaktivieren,
     spart etwas Platz aber verliert Signal-Identifikation -->
```

---

## 10. Compander (RX-Expander)

**Dateien:** `driver/bk4819.c:877-950` — BK4819_SetCompander()  
**Dateien:** `ui/menu.c:97` — MENU_COMPAND MenuList-Eintrag

Der Compander hat 4 Modi:
- Mode 0: AUS
- Mode 1: Nur TX (Kompression) — fuer RX nutzlos
- Mode 2: Nur RX (Expansion) — **nuetzlich fuer RX**
- Mode 3: TX + RX

Der RX-Expander verbessert die Audioqualitaet bei schwachen Signalen.

```
<!-- ENTSCHEIDUNG 10: Compander -->
<!-- INFO: RX-Expander ist nuetzlich. Empfehlung: Beibehalten -->
<!-- OPTION_A: Beibehalten (empfohlen, keine Aenderung) -->
<!-- OPTION_B: TX-Kompressor-Code entfernen, nur RX-Expander behalten -->
<!-- OPTION_C: Komplett entfernen -->
```

---

## 11. Busy Channel Lock (BCL)

**Dateien:** `radio.h:127` — BUSY_CHANNEL_LOCK in VFO_Info_t  
**Dateien:** `ui/menu.c` — MENU_BCL MenuList-Eintrag  
**Dateien:** `app/menu.c` — Handler

BCL verhindert Senden auf belegtem Kanal. Da nicht gesendet wird,
ist es technisch nutzlos. Der Menuepunkt ist aber harmlos.

```
<!-- ENTSCHEIDUNG 11: Busy Channel Lock -->
<!-- OPTION_A: Menue-Eintrag entfernen (Struct-Feld bleibt fuer EEPROM) -->
<!-- OPTION_B: Beibehalten (harmlos, keine Aenderung) -->
```

---

## 12-14. VFO_Info_t Struct-Felder (TX-bezogen)

**Betrifft:** `radio.h` VFO_Info_t Struct:
- freq_config_TX (Zeile 86) — TX-Frequenz-Konfiguration
- pTX Pointer (Zeile 95) — TX-Frequenz-Pointer
- TX_OFFSET_FREQUENCY (Zeile 97) — TX-Offset
- TX_OFFSET_FREQUENCY_DIRECTION (Zeile 102) — TX-Offset-Richtung
- OUTPUT_POWER (Zeile 112) — Sendeleistung
- TXP_CalculatedSetting (Zeile 113) — Berechnete TX-Leistung
- DTMF_PTT_ID_TX_MODE (Zeile 125) — PTT-ID Modus

Diese Felder werden aus dem EEPROM geladen und gespeichert.
Das EEPROM-Format ist festgelegt — eine Aenderung wuerde die
Kompatibilitaet mit bestehenden Kanaleinstellungen brechen.

```
<!-- ENTSCHEIDUNG 12: VFO_Info_t TX-Struct-Felder -->
<!-- OPTION_A: Beibehalten fuer EEPROM-Kompatibilitaet (empfohlen, keine Aenderung) -->
<!-- OPTION_B: Felder entfernen und EEPROM-Lese/Schreib-Routinen anpassen.
     ACHTUNG: Bricht Kompatibilitaet mit bestehenden Kanal-Speicherungen!
     Gespeicherte Kanaele muessen danach neu programmiert werden. -->
```

---

## 15. gSetting_350EN (350 MHz Bandfreigabe)

**Dateien:** `misc.h/c` — gSetting_350EN Variable  
**Dateien:** `settings.c` — EEPROM laden/speichern  
**Dateien:** `radio.c` — Band-Check

Kontrolliert ob das 350 MHz Band **empfangen** werden darf.
Dies ist eine reine RX-Funktion und sollte beibehalten werden.

```
<!-- ENTSCHEIDUNG 15: 350 MHz Bandfreigabe -->
<!-- INFO: RX-relevant, wird empfohlen beizubehalten -->
<!-- OPTION_A: Beibehalten (empfohlen, keine Aenderung) -->
```

---

## 16. TX-bezogene Makefile-Flags

**Datei:** `Makefile` Zeilen 1-55

Bereits korrekt konfiguriert:
- ENABLE_VOX = 0 ✓
- ENABLE_ALARM = 0 ✓
- ENABLE_TX1750 = 0 ✓
- ENABLE_DTMF_CALLING = 0 ✓
- ENABLE_TX_WHEN_AM = 0 ✓
- ENABLE_PREVENT_TX = 1 ✓
- ENABLE_AIRCOPY = 0 ✓

```
<!-- ENTSCHEIDUNG 16: Makefile-Flags -->
<!-- INFO: Alle TX-Flags sind korrekt deaktiviert. Keine Aenderung noetig. -->
<!-- OPTION_A: Beibehalten (keine Aenderung) -->
```

---

## Zusammenfassung der Empfehlungen

| # | Feature | Empfehlung | Ersparnis |
|---|---------|-----------|-----------|
| 1 | TX DCS/CTCSS Menue | Entfernen | ~50 Bytes |
| 2 | PTT ID | Entfernen | ~80 Bytes |
| 3 | Roger-Beep | Entfernen | ~150 Bytes |
| 4 | TX Offset Menues | Entfernen | ~50 Bytes |
| 5 | FrequencyReverse | Beibehalten | — |
| 6 | TXP Menue | Entfernen | ~40 Bytes |
| 7 | BK4819 TX-Funktionen | Entfernen | ~200+ Bytes |
| 8 | DTMF TX-Funktionen | Entfernen | ~100 Bytes |
| 9 | DTMF RX Decoder | Beibehalten | — |
| 10 | Compander | Beibehalten | — |
| 11 | BCL Menue | Entfernen | ~30 Bytes |
| 12 | Struct-Felder | Beibehalten | — |
| 15 | 350EN | Beibehalten | — |
| 16 | Makefile-Flags | Beibehalten | — |

**Geschaetzte Gesamtersparnis bei Entfernung aller empfohlenen Punkte:** ~700+ Bytes

---

## Bisherige Aenderungen (Referenz)

| Phase | Beschreibung | Ergebnis |
|-------|-------------|----------|
| Baseline | Vor allen Aenderungen | 52136 bytes |
| Phase 1 | FUNCTION_TRANSMIT + TX-Funktionen | 49680 bytes (-2456) |
| Phase 2 | TX-Variablen, Flags, Enums | 48856 bytes (-824) |
| Scrambler | Scrambler/Descrambler entfernt | 48468 bytes (-388) |
| **Gesamt** | | **-3668 bytes (-7.0%)** |
