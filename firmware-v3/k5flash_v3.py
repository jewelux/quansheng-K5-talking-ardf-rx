#!/usr/bin/env python3
# =============================================================================
# k5flash_v3.py — Quansheng UV-K5 V3/K1 Firmware-Flasher (Kommandozeile)
# =============================================================================
#
# Flasht eine .bin Firmware ueber die serielle Schnittstelle
# auf ein Quansheng UV-K5 V3 / K1 Radio (PY32F071 MCU).
#
# Basiert auf dem seriellen Bootloader-Protokoll der V3/K1 Firmware
# (muzkr/serialtool).
#
# Benutzung:
#   python3 k5flash_v3.py <COM-Port> <firmware.bin>
#   python3 k5flash_v3.py COM3 ardf_talking_k5v3_k1.bin
#   python3 k5flash_v3.py /dev/ttyUSB0 build-output/ardf_talking_k5v3_k1.bin
#
# Voraussetzungen:
#   pip install pyserial
#
# Lizenz: Apache-2.0 (wie das restliche Projekt)
# =============================================================================

import sys
import struct
import time
import os
import math

try:
    import serial
    import serial.tools.list_ports
except ImportError:
    print("FEHLER: Python-Modul 'pyserial' fehlt.")
    print("  Installation:  pip install pyserial")
    print("  Oder in MSYS2: pacman -S mingw-w64-x86_64-python-pyserial")
    sys.exit(1)


# =============================================================================
# Protokoll-Konstanten
# =============================================================================

BAUDRATE = 38400

# XOR-Tabelle fuer Paket-Obfuskierung (16 Bytes)
OBFUS_TBL = bytes.fromhex('166c14e62e910d402135d5401303e980')

# Nachrichten-Typen
MSG_NOTIFY_DEV_INFO = 0x0518
MSG_NOTIFY_BL_VER   = 0x0530
MSG_PROG_FW         = 0x0519
MSG_PROG_FW_RESP    = 0x051A

# Flash-Parameter
BLOCK_SIZE     = 256       # 256 Bytes pro Block
MAX_FW_SIZE    = 118 * 1024  # 118 KB Flash (ab 0x08002800)


# =============================================================================
# Hilfsfunktionen
# =============================================================================

def calc_crc16(data: bytes, offset: int = 0, size: int = 0) -> int:
    """CRC16-CCITT Berechnung (identisch zum Bootloader-Protokoll)."""
    crc = 0
    for i in range(size):
        b = data[offset + i] & 0xFF
        crc ^= b << 8
        for _ in range(8):
            if crc & 0x8000:
                crc = (crc << 1) ^ 0x1021
            else:
                crc = crc << 1
            crc &= 0xFFFF
    return crc


def obfuscate(buf: bytearray, offset: int = 0, size: int = 0):
    """XOR-Obfuskierung / Deobfuskierung (symmetrisch)."""
    n = len(OBFUS_TBL)
    for i in range(size):
        buf[offset + i] ^= OBFUS_TBL[i % n]


def make_packet(msg: bytes) -> bytes:
    """Baut ein serielles Paket: 0xABCD + Laenge + obfuscate(msg + CRC) + 0xDCBA."""
    msg_len = len(msg)
    if msg_len % 2 != 0:
        msg_len += 1

    buf = bytearray(8 + msg_len)
    # Header: 0xABCD (little-endian)
    struct.pack_into('<H', buf, 0, 0xCDAB)
    # Laenge
    struct.pack_into('<H', buf, 2, msg_len)
    # Footer: 0xDCBA (little-endian)
    struct.pack_into('<H', buf, 6 + msg_len, 0xBADC)

    # Nachricht kopieren
    buf[4:4 + len(msg)] = msg

    # CRC berechnen und einfuegen
    crc = calc_crc16(buf, 4, msg_len)
    struct.pack_into('<H', buf, 4 + msg_len, crc)

    # Obfuskierung
    obfuscate(buf, 4, 2 + msg_len)

    return bytes(buf)


def parse_packet(buf: bytearray):
    """
    Versucht ein Paket aus dem Puffer zu extrahieren.
    Gibt (Msg-Bytes, verbleibender-Puffer) oder (None, Puffer) zurueck.
    """
    while True:
        if len(buf) < 8:
            return None, buf

        # Start-Marker suchen
        idx = buf.find(b'\xab\xcd')
        if idx < 0:
            # Kein Start-Marker gefunden
            if buf.endswith(b'\xab'):
                buf = buf[-1:]
            else:
                buf = bytearray()
            return None, buf

        if idx > 0:
            buf = buf[idx:]

        if len(buf) < 8:
            return None, buf

        # Nachrichtenlaenge lesen
        msg_len = buf[2] | (buf[3] << 8)
        pack_end = 6 + msg_len

        if len(buf) < pack_end + 2:
            return None, buf

        # End-Marker pruefen
        if buf[pack_end] != 0xDC or buf[pack_end + 1] != 0xBA:
            buf = buf[2:]
            continue

        # Paket gefunden — deobfuskieren
        obfuscate(buf, 4, msg_len + 2)

        # Nachricht extrahieren (ohne CRC)
        msg = bytes(buf[4:pack_end - 2])

        # Puffer bereinigen
        buf = buf[pack_end + 2:]
        return msg, buf


def get_msg_type(msg: bytes) -> int:
    """Nachrichtentyp (erste 2 Bytes, Little-Endian)."""
    if len(msg) < 2:
        return 0
    return msg[0] | (msg[1] << 8)


def get_hw_le(msg: bytes, offset: int = 0) -> int:
    """16-Bit Wert lesen (Little-Endian)."""
    return msg[offset] | (msg[offset + 1] << 8)


def get_word_le(msg: bytes, offset: int = 0) -> int:
    """32-Bit Wort lesen (Little-Endian)."""
    return (msg[offset] | (msg[offset + 1] << 8) |
            (msg[offset + 2] << 16) | (msg[offset + 3] << 24))


# =============================================================================
# Serielle Kommunikation
# =============================================================================

class K5V3Radio:
    """Kommunikation mit dem Quansheng UV-K5 V3/K1 Bootloader."""

    def __init__(self, port_name: str):
        self.ser = serial.Serial(
            port=port_name,
            baudrate=BAUDRATE,
            timeout=0.02,
            write_timeout=5,
        )
        self.rx_buf = bytearray()

    def close(self):
        if self.ser and self.ser.is_open:
            self.ser.close()

    def send_msg(self, msg: bytes):
        """Sendet eine Nachricht als Paket."""
        packet = make_packet(msg)
        self.ser.write(packet)
        self.ser.flush()

    def recv_msg(self, timeout_s: float = 2.0):
        """
        Empfaengt eine Nachricht mit Timeout.
        Gibt die dekodierte Nachricht zurueck oder None.
        """
        deadline = time.time() + timeout_s

        while time.time() < deadline:
            # Verfuegbare Bytes lesen
            waiting = self.ser.in_waiting
            if waiting > 0:
                data = self.ser.read(waiting)
                self.rx_buf.extend(data)
            else:
                time.sleep(0.005)

            # Paket parsen
            msg, self.rx_buf = parse_packet(self.rx_buf)
            if msg is not None:
                return msg

        return None

    def wait_for_device(self, timeout_s: float = 30.0):
        """
        Wartet auf wiederholte DEV_INFO-Nachrichten vom Bootloader.
        Gibt (uid, bl_version) zurueck oder (None, None) bei Timeout.
        """
        last_ts = 0
        consecutive = 0
        uid = None
        bl_ver = None

        deadline = time.time() + timeout_s

        while time.time() < deadline:
            msg = self.recv_msg(timeout_s=0.5)
            if msg is None:
                consecutive = 0
                continue

            if get_msg_type(msg) != MSG_NOTIFY_DEV_INFO:
                consecutive = 0
                continue

            ts = int(time.time() * 100)
            dt = ts - last_ts
            last_ts = ts

            # Intervall pruefen (20-1000 ms)
            if dt < 5 or dt > 100:
                consecutive = 0
                sys.stdout.write('.')
                sys.stdout.flush()
                continue

            if consecutive == 0:
                # UID extrahieren (Bytes 4..19)
                if len(msg) >= 20:
                    uid = msg[4:20]

                # BL-Version extrahieren (Bytes 20..35, null-terminiert)
                if len(msg) >= 36:
                    end = msg.find(0, 20, 36)
                    if end < 0:
                        end = 36
                    bl_ver = msg[20:end].decode('ascii', errors='replace')

            consecutive += 1
            if consecutive >= 5:
                return uid, bl_ver

        return None, None

    def handshake(self, bl_ver: str = '*') -> bool:
        """
        Fuehrt den Handshake mit dem Bootloader durch.
        Sendet MSG_NOTIFY_BL_VER als Antwort auf DEV_INFO.
        """
        if len(bl_ver) > 4:
            bl_ver = bl_ver[:4]

        consecutive = 0
        deadline = time.time() + 10.0

        while time.time() < deadline:
            msg = self.recv_msg(timeout_s=1.0)
            if msg is None:
                consecutive = 0
                continue

            if get_msg_type(msg) != MSG_NOTIFY_DEV_INFO:
                consecutive = 0
                continue

            # Handshake-Nachricht senden
            hs_msg = bytearray(8)  # type(2) + len(2) + data(4)
            struct.pack_into('<H', hs_msg, 0, MSG_NOTIFY_BL_VER)
            struct.pack_into('<H', hs_msg, 2, 4)
            encoded = bl_ver.encode('ascii')[:4]
            hs_msg[4:4 + len(encoded)] = encoded
            self.send_msg(bytes(hs_msg))

            consecutive += 1
            if consecutive >= 3:
                return True

        return False

    def program_block(self, page_index: int, page_count: int,
                      image: bytes, x4: int) -> bool:
        """
        Programmiert einen 256-Byte-Block.
        Sendet MSG_PROG_FW, erwartet MSG_PROG_FW_RESP.
        """
        # Nachricht aufbauen: type(2) + data_len(2) + x4(4) +
        #                     page_index(2) + page_count(2) +
        #                     reserved(4) + data(256) = 272 bytes
        msg = bytearray(4 + 268)  # 4 header + 268 payload
        struct.pack_into('<H', msg, 0, MSG_PROG_FW)
        struct.pack_into('<H', msg, 2, 268)
        struct.pack_into('<I', msg, 4, x4)
        struct.pack_into('<H', msg, 8, page_index)
        struct.pack_into('<H', msg, 10, page_count)
        # Bytes 12..15 reserved (0)

        # Firmware-Daten kopieren
        image_off = page_index * BLOCK_SIZE
        remaining = len(image) - image_off
        chunk = min(remaining, BLOCK_SIZE)
        if chunk > 0:
            msg[16:16 + chunk] = image[image_off:image_off + chunk]

        self.send_msg(bytes(msg))

        # Auf Antwort warten
        response = self.recv_msg(timeout_s=5.0)
        if response is None:
            return False

        if get_msg_type(response) != MSG_PROG_FW_RESP:
            return False

        if len(response) < 12:
            return False

        # Fehlercode pruefen (Bytes 10..11)
        err = get_hw_le(response, 10)
        return err == 0


# =============================================================================
# Hauptprogramm
# =============================================================================

def print_banner():
    print()
    print("=" * 60)
    print("  Quansheng UV-K5 V3/K1 Firmware-Flasher")
    print("  (fuer MSYS2 / Kommandozeile)")
    print("=" * 60)
    print()


def print_version_warning():
    """Zeigt Warnhinweise zur Geraeteversion an."""
    print("  ┌─────────────────────────────────────────────────────┐")
    print("  │  WICHTIG: Dieses Tool ist NUR fuer V3/K1 Geraete!  │")
    print("  │                                                     │")
    print("  │  V3/K1 (PY32F071) — dieses Flash-Tool              │")
    print("  │  V1    (DP32G030) — NICHT kompatibel, k5flash.py!   │")
    print("  │                                                     │")
    print("  │  Version steht unter dem Akku / auf dem Aufkleber.  │")
    print("  │  Falsche Firmware -> Geraet funktionslos!           │")
    print("  └─────────────────────────────────────────────────────┘")
    print()


def select_device_version():
    """
    Fragt die Geraeteversion interaktiv ab.
    Gibt 'v3' zurueck wenn kompatibel, None wenn abgebrochen.
    """
    print("  Welche Hardware-Version hat dein Radio?")
    print("    1. V3 / K1 (PY32F071 MCU)")
    print("    2. V1 (Original, DP32G030 MCU)")
    print("    3. Unsicher / weiss nicht")
    print()

    choice = input("  Auswahl [1/2/3]: ").strip()

    if choice == '1':
        print()
        print("  ✓ V3/K1 erkannt — dieses Tool ist kompatibel.")
        return 'v3'

    elif choice == '2':
        print()
        print("  ╔═════════════════════════════════════════════════════╗")
        print("  ║  V1 Geraete verwenden eine andere MCU              ║")
        print("  ║  (DP32G030 statt PY32F071).                        ║")
        print("  ║                                                     ║")
        print("  ║  Bitte verwende stattdessen k5flash.py aus dem      ║")
        print("  ║  firmware-v1/ Verzeichnis.                          ║")
        print("  ╚═════════════════════════════════════════════════════╝")
        print()
        return None

    elif choice == '3':
        print()
        print("  So findest du die Version heraus:")
        print("    - Unter dem Akku auf dem Aufkleber steht z.B. 'V1', 'V3'")
        print("    - V1: schwarzes PCB, DP32G030 Chip")
        print("    - V3: gruenes PCB, PY32F071 Chip, oft als 'K1' vermarktet")
        print()
        answer = input("  Trotzdem als V3/K1 fortfahren? [j/N]: ").strip().lower()
        if answer in ('j', 'y', 'ja', 'yes'):
            return 'v3'
        return None

    else:
        print("  Ungueltige Auswahl.")
        return None


def list_serial_ports():
    """Listet alle verfuegbaren seriellen Ports auf."""
    try:
        ports = serial.tools.list_ports.comports()
        return [(p.device, p.description) for p in ports]
    except Exception:
        return []


def flash_firmware(port_name: str, fw_path: str):
    """Flasht die Firmware auf das V3/K1 Radio."""

    # Firmware laden
    print(f"  Lade Firmware: {fw_path}")
    with open(fw_path, 'rb') as f:
        fw_data = f.read()
    print(f"  Dateigroesse: {len(fw_data)} Bytes")

    if len(fw_data) == 0:
        print("  FEHLER: Firmware-Datei ist leer!")
        return False

    # Groessencheck
    if len(fw_data) > MAX_FW_SIZE:
        print(f"\n  FEHLER: Firmware ist zu gross! ({len(fw_data)} > {MAX_FW_SIZE} Bytes)")
        return False

    print(f"  Speichernutzung: {len(fw_data) / MAX_FW_SIZE * 100:.1f}%")
    print()

    # Serielle Verbindung oeffnen
    print(f"  Oeffne seriellen Port: {port_name}")
    try:
        radio = K5V3Radio(port_name)
    except serial.SerialException as e:
        print(f"\n  FEHLER: Kann Port {port_name} nicht oeffnen:")
        print(f"    {e}")
        return False

    try:
        # Auf Bootloader warten
        print()
        print("  Warte auf Radio im Flash-Modus ...")
        print("  (Radio mit gedrueckter PTT-Taste einschalten,")
        print("   Display bleibt dunkel, LED leuchtet konstant)")
        print()

        uid, bl_ver = radio.wait_for_device(timeout_s=30.0)
        if uid is None:
            print("\n  FEHLER: Kein Bootloader erkannt!")
            print("    - Ist das Radio im Flash-Modus? (PTT + Einschalten)")
            print("    - Ist das Programmierkabel angeschlossen?")
            print("    - Stimmt der COM-Port?")
            return False

        print()
        print("  Geraet erkannt!")

        # UID anzeigen
        if uid:
            uid_str = ' '.join(f'{b:02x}' for b in uid)
            print(f"  UID: {uid_str}")

        # Bootloader-Version anzeigen
        if bl_ver:
            print(f"  Bootloader-Version: {bl_ver}")

        # Handshake
        print()
        print("  Handshake mit Bootloader ...")
        bl_ver_param = bl_ver if bl_ver else '*'
        if not radio.handshake(bl_ver_param):
            print("  FEHLER: Handshake fehlgeschlagen!")
            return False
        print("  Handshake erfolgreich!")

        # Firmware programmieren
        page_count = math.ceil(len(fw_data) / BLOCK_SIZE)
        print()
        print(f"  Flashe {len(fw_data)} Bytes in {page_count} Bloecken ...")
        print()

        x4 = int(time.time() * 100) & 0xFFFFFFFF
        retries = 0
        max_retries = 3

        for page_index in range(page_count):
            percent = (page_index + 1) / page_count * 100

            # Fortschrittsbalken
            bar_width = 40
            filled = int(bar_width * percent / 100)
            bar = '█' * filled + '░' * (bar_width - filled)
            sys.stdout.write(
                f'\r  [{bar}] {percent:5.1f}%  '
                f'Block {page_index + 1}/{page_count}'
            )
            sys.stdout.flush()

            success = radio.program_block(page_index, page_count, fw_data, x4)
            if not success:
                retries += 1
                if retries > max_retries:
                    addr = page_index * BLOCK_SIZE
                    print(
                        f"\n\n  FEHLER: Block {page_index + 1} bei "
                        f"Adresse 0x{addr:04X} fehlgeschlagen!"
                    )
                    print("    Maximale Wiederholungen erreicht.")
                    return False

                # Retry
                sys.stdout.write(f'  (Wiederholung {retries}/{max_retries})')
                sys.stdout.flush()
                page_index -= 1  # Will be incremented by loop
                time.sleep(0.1)
                continue

            retries = 0

        print()
        print()
        print("  ✓ Firmware erfolgreich geflasht!")
        print()
        print("  Das Radio startet automatisch mit der neuen Firmware.")
        print("  Falls nicht: Radio aus- und wieder einschalten.")
        print()
        return True

    except KeyboardInterrupt:
        print("\n\n  Abgebrochen durch Benutzer!")
        return False

    except Exception as e:
        print(f"\n\n  FEHLER: {e}")
        import traceback
        traceback.print_exc()
        return False

    finally:
        radio.close()


def interactive_mode():
    """Interaktiver Modus: Port und Datei werden abgefragt."""
    print_banner()
    print_version_warning()

    # Geraeteversion abfragen
    version = select_device_version()
    if version is None:
        print("  Flash-Vorgang abgebrochen.")
        return

    print()

    # Serielle Ports auflisten
    ports = list_serial_ports()
    if ports:
        print("  Verfuegbare serielle Ports:")
        for i, (dev, desc) in enumerate(ports, 1):
            print(f"    {i}. {dev}  ({desc})")
        print()

        while True:
            choice = input(
                "  Port waehlen (Nummer oder Name, z.B. COM3): "
            ).strip()
            if not choice:
                print("  Abgebrochen.")
                return

            try:
                idx = int(choice) - 1
                if 0 <= idx < len(ports):
                    port_name = ports[idx][0]
                    break
                else:
                    print("  Ungueltige Nummer.")
                    continue
            except ValueError:
                port_name = choice
                break
    else:
        print("  Keine seriellen Ports gefunden!")
        print("  (Ist das Programmierkabel angeschlossen?)")
        print()
        port_name = input(
            "  Port manuell eingeben (z.B. COM3 oder /dev/ttyUSB0): "
        ).strip()
        if not port_name:
            print("  Abgebrochen.")
            return

    print(f"\n  Gewaehlter Port: {port_name}")
    print()

    # Firmware-Datei suchen — im build-output Verzeichnis
    script_dir = os.path.dirname(os.path.abspath(__file__))
    build_dir = os.path.join(script_dir, '..', 'build-output')
    build_dir = os.path.normpath(build_dir)

    candidates = []

    # build-output/ durchsuchen
    if os.path.isdir(build_dir):
        for f in sorted(os.listdir(build_dir), reverse=True):
            full = os.path.join(build_dir, f)
            if f.endswith('.bin') and os.path.isfile(full):
                candidates.append(full)

    # Auch build/*/  durchsuchen (CMake build output)
    build_cmake = os.path.join(script_dir, 'build')
    if os.path.isdir(build_cmake):
        for preset_dir in sorted(os.listdir(build_cmake)):
            preset_path = os.path.join(build_cmake, preset_dir)
            if os.path.isdir(preset_path):
                for f in sorted(os.listdir(preset_path)):
                    full = os.path.join(preset_path, f)
                    if f.endswith('.bin') and os.path.isfile(full):
                        candidates.append(full)

    if candidates:
        print("  Firmware-Dateien gefunden:")
        for i, path in enumerate(candidates, 1):
            size = os.path.getsize(path)
            # Relativen Pfad anzeigen
            try:
                rel = os.path.relpath(path, script_dir)
            except ValueError:
                rel = path
            print(f"    {i}. {rel}  ({size} Bytes)")
        print()

        while True:
            choice = input("  Datei waehlen (Nummer oder Pfad): ").strip()
            if not choice:
                print("  Abgebrochen.")
                return

            try:
                idx = int(choice) - 1
                if 0 <= idx < len(candidates):
                    fw_path = candidates[idx]
                    break
                else:
                    print("  Ungueltige Nummer.")
                    continue
            except ValueError:
                fw_path = choice
                break
    else:
        print("  Keine Firmware-Dateien gefunden.")
        print("  (Zuerst die Firmware mit CMake bauen)")
        fw_path = input("  Pfad zur Firmware-Datei eingeben: ").strip()
        if not fw_path:
            print("  Abgebrochen.")
            return

    if not os.path.isfile(fw_path):
        print(f"\n  FEHLER: Datei nicht gefunden: {fw_path}")
        return

    print(f"\n  Gewahlte Firmware: {os.path.basename(fw_path)}")
    print()

    # Sicherheitsabfrage
    print("  ╔═══════════════════════════════════════════════════════╗")
    print("  ║  ACHTUNG: Das Flashen ueberschreibt die Firmware     ║")
    print("  ║  auf dem Radio unwiderruflich!                       ║")
    print("  ║                                                      ║")
    print("  ║  Stelle sicher, dass:                                ║")
    print("  ║    - Das Radio mit dem USB-Kabel verbunden ist       ║")
    print("  ║    - Das Radio im Flash-Modus ist                    ║")
    print("  ║      (PTT gedrueckt halten + einschalten)            ║")
    print("  ║    - Der Akku ausreichend geladen ist                ║")
    print("  ╚═══════════════════════════════════════════════════════╝")
    print()
    confirm = input("  Firmware jetzt flashen? [j/N]: ").strip().lower()
    if confirm not in ('j', 'y', 'ja', 'yes'):
        print("  Abgebrochen.")
        return

    print()
    print("-" * 60)
    success = flash_firmware(port_name, fw_path)
    print("-" * 60)

    if success:
        print("\n  Alles erledigt! 73!")
    else:
        print("\n  Flash-Vorgang fehlgeschlagen.")
        print("  Keine Panik — das Radio kann erneut geflasht werden,")
        print("  solange der Bootloader intakt ist.")
        print("  Einfach nochmal im Flash-Modus starten und erneut versuchen.")

    print()


def select_port_interactive():
    """Fragt interaktiv nach dem seriellen Port."""
    ports = list_serial_ports()
    if ports:
        print("  Verfuegbare serielle Ports:")
        for i, (dev, desc) in enumerate(ports, 1):
            print(f"    {i}. {dev}  ({desc})")
        print()

        while True:
            choice = input(
                "  Port waehlen (Nummer oder Name, z.B. COM3): "
            ).strip()
            if not choice:
                return None

            try:
                idx = int(choice) - 1
                if 0 <= idx < len(ports):
                    return ports[idx][0]
                else:
                    print("  Ungueltige Nummer.")
                    continue
            except ValueError:
                return choice
    else:
        print("  Keine seriellen Ports gefunden!")
        print("  (Ist das Programmierkabel angeschlossen?)")
        print()
        port_name = input(
            "  Port manuell eingeben (z.B. COM3 oder /dev/ttyUSB0): "
        ).strip()
        return port_name if port_name else None


def main():
    if len(sys.argv) == 3:
        # Kommandozeilen-Modus: k5flash_v3.py <port> <firmware>
        port_name = sys.argv[1]
        fw_path = sys.argv[2]

        if not os.path.isfile(fw_path):
            print(f"FEHLER: Datei nicht gefunden: {fw_path}")
            sys.exit(1)

        print_banner()
        success = flash_firmware(port_name, fw_path)
        sys.exit(0 if success else 1)

    elif len(sys.argv) == 2:
        # Halbautomatisch: k5flash_v3.py <firmware>
        # (Firmware vorgegeben, Port wird interaktiv abgefragt)
        fw_path = sys.argv[1]

        if not os.path.isfile(fw_path):
            print(f"FEHLER: Datei nicht gefunden: {fw_path}")
            sys.exit(1)

        print_banner()
        print(f"  Firmware: {os.path.basename(fw_path)}")
        print()

        port_name = select_port_interactive()
        if not port_name:
            print("  Abgebrochen.")
            sys.exit(0)

        print(f"\n  Gewaehlter Port: {port_name}")
        print()

        success = flash_firmware(port_name, fw_path)
        sys.exit(0 if success else 1)

    elif len(sys.argv) == 1:
        # Interaktiver Modus
        interactive_mode()

    else:
        print("Benutzung:")
        print(f"  {sys.argv[0]} <COM-Port> <firmware.bin>")
        print(f"  {sys.argv[0]} <firmware.bin>              (Port interaktiv)")
        print(f"  {sys.argv[0]}                            (interaktiver Modus)")
        print()
        print("Beispiele:")
        print(f"  {sys.argv[0]} COM3 ardf_talking_k5v3_k1.bin")
        print(f"  {sys.argv[0]} /dev/ttyUSB0 build-output/ardf_talking_k5v3_k1.bin")
        print(f"  {sys.argv[0]} ardf_talking_k5v3_k1.bin")
        sys.exit(1)


if __name__ == '__main__':
    main()
