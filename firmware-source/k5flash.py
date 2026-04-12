#!/usr/bin/env python3
# =============================================================================
# k5flash.py — Quansheng UV-K5 Firmware-Flasher (Kommandozeile)
# =============================================================================
#
# Flasht eine .packed.bin oder .bin Firmware ueber die serielle Schnittstelle
# auf ein Quansheng UV-K5 / UV-K6 / UV-5R Plus Radio.
#
# Basiert auf dem Bootloader-Protokoll dokumentiert in:
#   https://github.com/amnemonic/Quansheng_UV-K5_Firmware/blob/main/docs/communication.md
# und der JavaScript-Implementierung von egzumer/uvtools.
#
# Benutzung:
#   python3 k5flash.py <COM-Port> <firmware.packed.bin>
#   python3 k5flash.py COM3 firmware_uvk5_v1.packed.bin
#   python3 k5flash.py /dev/ttyUSB0 firmware_uvk5_v1.packed.bin
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

try:
    import serial
except ImportError:
    print("FEHLER: Python-Modul 'pyserial' fehlt.")
    print("  Installation:  pip install pyserial")
    print("  Oder in MSYS2: pacman -S mingw-w64-x86_64-python-pyserial")
    sys.exit(1)


# =============================================================================
# Protokoll-Konstanten
# =============================================================================

BAUDRATE = 38400

# XOR-Schluessel fuer die serielle Kommunikation (16 Bytes)
COMM_XOR_KEY = bytes.fromhex('166c14e62e910d402135d5401303e980')

# XOR-Schluessel fuer die Firmware-Datei (128 Bytes)
FW_XOR_KEY = bytes.fromhex(
    '4722c0525d574894b16060db6fe34c7c'
    'd84ad68b30ec25e04cd9007fbfe35405'
    'e93a976bb06e0cfbb11ae2c9c15647e9'
    'baf142b6675f0f96f7c93c841b26e14e'
    '3b6f66e6a06ab0bfc6a5703aba189e27'
    '1a535b71b1941e18f2d6810222fd5a28'
    '91dbba5d64c6fe86839c501c730311d6'
    'af30f42c77b27dbb3f29285722d6928b'
)

# CRC16-CCITT Lookup-Tabelle
CRC16_TAB = [
    0, 4129, 8258, 12387, 16516, 20645, 24774, 28903,
    33032, 37161, 41290, 45419, 49548, 53677, 57806, 61935,
    4657, 528, 12915, 8786, 21173, 17044, 29431, 25302,
    37689, 33560, 45947, 41818, 54205, 50076, 62463, 58334,
    9314, 13379, 1056, 5121, 25830, 29895, 17572, 21637,
    42346, 46411, 34088, 38153, 58862, 62927, 50604, 54669,
    13907, 9842, 5649, 1584, 30423, 26358, 22165, 18100,
    46939, 42874, 38681, 34616, 63455, 59390, 55197, 51132,
    18628, 22757, 26758, 30887, 2112, 6241, 10242, 14371,
    51660, 55789, 59790, 63919, 35144, 39273, 43274, 47403,
    23285, 19156, 31415, 27286, 6769, 2640, 14899, 10770,
    56317, 52188, 64447, 60318, 39801, 35672, 47931, 43802,
    27814, 31879, 19684, 23749, 11298, 15363, 3168, 7233,
    60846, 64911, 52716, 56781, 44330, 48395, 36200, 40265,
    32407, 28342, 24277, 20212, 15891, 11826, 7761, 3696,
    65439, 61374, 57309, 53244, 48923, 44858, 40793, 36728,
    37256, 33193, 45514, 41451, 53516, 49453, 61774, 57711,
    4224, 161, 12482, 8419, 20484, 16421, 28742, 24679,
    33721, 37784, 41979, 46042, 49981, 54044, 58239, 62302,
    689, 4752, 8947, 13010, 16949, 21012, 25207, 29270,
    46570, 42443, 38312, 34185, 62830, 58703, 54572, 50445,
    13538, 9411, 5280, 1153, 29798, 25671, 21540, 17413,
    42971, 47098, 34713, 38840, 59231, 63358, 50973, 55100,
    9939, 14066, 1681, 5808, 26199, 30326, 17941, 22068,
    55628, 51565, 63758, 59695, 39368, 35305, 47498, 43435,
    22596, 18533, 30726, 26663, 6336, 2273, 14466, 10403,
    52093, 56156, 60223, 64286, 35833, 39896, 43963, 48026,
    19061, 23124, 27191, 31254, 2801, 6864, 10931, 14994,
    64814, 60687, 56684, 52557, 48554, 44427, 40424, 36297,
    31782, 27655, 23652, 19525, 15522, 11395, 7392, 3265,
    61215, 65342, 53085, 57212, 44955, 49082, 36825, 40952,
    28183, 32310, 20053, 24180, 11923, 16050, 3793, 7920,
]

MAX_FW_SIZE = 0xEFFF   # maximale Firmware-Groesse (61439 Bytes)
BLOCK_SIZE  = 0x100     # Flash-Block-Groesse (256 Bytes)
VERSION_OFFSET = 0x2000 # Offset der Versionsinformation in gepackter Firmware
VERSION_LENGTH = 16     # Laenge der Versionsinformation


# =============================================================================
# Hilfsfunktionen
# =============================================================================

def crc16_ccitt(data: bytes) -> int:
    """Berechnet CRC16-CCITT ueber die gegebenen Daten."""
    crc = 0
    for b in data:
        crc = CRC16_TAB[((crc >> 8) ^ b) & 0xFF] ^ (crc << 8)
        crc &= 0xFFFF
    return crc


def comm_xor(data: bytes) -> bytes:
    """XOR-Kodierung/Dekodierung fuer serielle Kommunikation."""
    ba = bytearray(data)
    key_len = len(COMM_XOR_KEY)
    for i in range(len(ba)):
        ba[i] ^= COMM_XOR_KEY[i % key_len]
    return bytes(ba)


def fw_xor(data: bytes) -> bytes:
    """XOR-Kodierung/Dekodierung fuer Firmware-Dateien."""
    ba = bytearray(data)
    key_len = len(FW_XOR_KEY)
    for i in range(len(ba)):
        ba[i] ^= FW_XOR_KEY[i % key_len]
    return bytes(ba)


def build_packet(payload: bytes) -> bytes:
    """Baut ein serielles Paket: Header + Laenge + XOR(payload+CRC) + Footer."""
    crc = crc16_ccitt(payload)
    crc_bytes = struct.pack('<H', crc)
    encoded = comm_xor(payload + crc_bytes)
    length = struct.pack('<H', len(payload))
    return b'\xAB\xCD' + length + encoded + b'\xDC\xBA'


def parse_packet(raw: bytes):
    """Parst ein empfangenes Paket und gibt die dekodierte Payload zurueck."""
    if len(raw) < 8:
        return None
    if raw[0] != 0xAB or raw[1] != 0xCD:
        return None
    payload_len = raw[2] | (raw[3] << 8)
    if len(raw) < payload_len + 8:
        return None
    if raw[payload_len + 6] != 0xDC or raw[payload_len + 7] != 0xBA:
        return None
    encoded = raw[4:4 + payload_len + 2]  # payload + CRC (both XOR-ed)
    decoded = comm_xor(encoded)
    return decoded[:payload_len]  # ohne CRC zurueckgeben


def unpack_firmware(packed_data: bytes):
    """
    Entpackt eine .packed.bin Firmware.
    Gibt (raw_firmware, version_info) zurueck.
    """
    # CRC pruefen (letzte 2 Bytes)
    file_crc = packed_data[-2:]
    calc_crc = crc16_ccitt(packed_data[:-2])
    calc_crc_bytes = bytes([calc_crc & 0xFF, calc_crc >> 8])
    if file_crc != calc_crc_bytes:
        print("  WARNUNG: CRC der Firmware-Datei stimmt nicht!")
        print(f"    Erwartet: {calc_crc_bytes.hex()}, Gefunden: {file_crc.hex()}")
        print("    Die Datei koennte beschaedigt sein!")
        raise ValueError("CRC-Pruefung fehlgeschlagen — Firmware-Datei ist ungueltig.")

    # XOR-Dekodierung
    decoded = fw_xor(packed_data[:-2])

    # Version extrahieren (16 Bytes ab Offset 0x2000)
    version_info = decoded[VERSION_OFFSET:VERSION_OFFSET + VERSION_LENGTH]

    # Firmware ohne Versionsinformation zusammensetzen
    raw_fw = decoded[:VERSION_OFFSET] + decoded[VERSION_OFFSET + VERSION_LENGTH:]

    return raw_fw, version_info


def detect_file_type(data: bytes):
    """
    Erkennt ob es sich um eine .packed.bin oder .bin (raw) Datei handelt.
    Gibt ('packed', data) oder ('raw', data) zurueck.
    """
    # .packed.bin hat eine CRC16 am Ende und ist XOR-kodiert
    # Einfache Heuristik: Pruefen ob CRC stimmt
    if len(data) > 2:
        file_crc = data[-2:]
        calc_crc = crc16_ccitt(data[:-2])
        calc_crc_bytes = bytes([calc_crc & 0xFF, calc_crc >> 8])
        if file_crc == calc_crc_bytes:
            return 'packed'
    return 'raw'


# =============================================================================
# Serielle Kommunikation
# =============================================================================

class K5Radio:
    """Kommunikation mit dem Quansheng UV-K5 Bootloader."""

    def __init__(self, port_name: str):
        self.ser = serial.Serial(
            port=port_name,
            baudrate=BAUDRATE,
            timeout=1,
            write_timeout=5,
        )

    def close(self):
        if self.ser and self.ser.is_open:
            self.ser.close()

    def send_packet(self, payload: bytes):
        """Sendet ein Paket an das Radio."""
        packet = build_packet(payload)
        self.ser.write(packet)
        self.ser.flush()

    def read_packet(self, timeout_s: float = 2.0, expected_cmd: int = None):
        """
        Liest ein Paket vom Radio.
        Gibt die dekodierte Payload zurueck oder None bei Timeout.
        """
        deadline = time.time() + timeout_s
        buf = b''

        while time.time() < deadline:
            # Verfuegbare Bytes lesen
            waiting = self.ser.in_waiting
            if waiting > 0:
                buf += self.ser.read(waiting)
            else:
                # Kurz warten
                time.sleep(0.01)
                continue

            # Nach Paketstart suchen
            while len(buf) >= 8:
                # Start-Marker suchen
                idx = buf.find(b'\xAB\xCD')
                if idx < 0:
                    buf = b''
                    break
                if idx > 0:
                    buf = buf[idx:]

                if len(buf) < 4:
                    break

                payload_len = buf[2] | (buf[3] << 8)
                total_len = payload_len + 8  # header(4) + payload + crc(2) + footer(2)

                if len(buf) < total_len:
                    break  # mehr Daten noetig

                packet_raw = buf[:total_len]
                buf = buf[total_len:]

                payload = parse_packet(packet_raw)
                if payload is None:
                    continue

                # Pruefen ob erwarteter Befehl
                if expected_cmd is not None:
                    if len(payload) >= 1 and payload[0] != expected_cmd:
                        continue  # anderes Paket, weiter suchen

                return payload

        return None

    def wait_for_bootloader(self, timeout_s: float = 10.0) -> bytes:
        """
        Wartet auf das 0x18 Bootloader-Ready-Paket.
        Das Radio sendet dieses Paket wiederholt wenn es im Flash-Modus ist.
        """
        return self.read_packet(timeout_s=timeout_s, expected_cmd=0x18)

    def send_flash_init(self, version_info: bytes) -> bytes:
        """
        Sendet den Flash-Init-Befehl (0x0530) mit Versionsinformation.
        Erwartet 0x18 Antwort zurueck.
        """
        # Paket: cmd_id(0x0530) + body_len + version_info (16 bytes)
        payload = b'\x30\x05' + struct.pack('<H', len(version_info)) + version_info
        self.send_packet(payload)
        return self.read_packet(timeout_s=5.0, expected_cmd=0x18)

    def write_flash_block(self, address: int, data: bytes, total_size: int) -> bool:
        """
        Schreibt einen 256-Byte-Block in den Flash.
        Sendet Befehl 0x0519, erwartet Antwort 0x1A.
        """
        # Block auf 256 Bytes auffuellen
        if len(data) < BLOCK_SIZE:
            data = data + b'\x00' * (BLOCK_SIZE - len(data))

        # Endadresse berechnen (aufgerundet auf naechstes Vielfaches von 0x100)
        address_final = (total_size + 0xFF) & ~0xFF
        if address_final > MAX_FW_SIZE + 1:
            raise ValueError(f"Firmware zu gross: Endadresse 0x{address_final:04X} > 0x{MAX_FW_SIZE + 1:04X}")

        # Befehlsstruktur:
        # 0x19 0x05 = cmd_id
        # 0x0C 0x01 = body_len (256 + 8 + 4 = 268 = 0x10C)
        # 0x8A 0x8D 0x9F 0x1D = Timestamp/Dummy
        # address_hi address_lo = aktuelle Adresse (Big-Endian in Bytes)
        # final_hi final_lo = letzte Adresse
        # length_hi length_lo = Block-Laenge (0x0100)
        # 0x00 0x00 = Padding
        # [256 Bytes Daten]
        payload = bytes([
            0x19, 0x05,                         # cmd_id
            0x0C, 0x01,                         # body_len
            0x8A, 0x8D, 0x9F, 0x1D,             # timestamp/dummy
            (address >> 8) & 0xFF,              # address high byte
            address & 0xFF,                     # address low byte
            (address_final >> 8) & 0xFF,        # final address high byte
            address_final & 0xFF,               # final address low byte
            (BLOCK_SIZE >> 8) & 0xFF,           # length high byte (0x01)
            BLOCK_SIZE & 0xFF,                  # length low byte (0x00)
            0x00, 0x00,                         # padding
        ]) + data

        self.send_packet(payload)

        # Auf Bestaetigung warten
        reply = self.read_packet(timeout_s=5.0, expected_cmd=0x1A)
        return reply is not None


# =============================================================================
# Hauptprogramm
# =============================================================================

def print_banner():
    print()
    print("=" * 60)
    print("  Quansheng UV-K5 Firmware-Flasher")
    print("  (fuer MSYS2 / Kommandozeile)")
    print("=" * 60)
    print()


def print_version_warning():
    """Zeigt Warnhinweise zur Geraeteversion an."""
    print("  ┌─────────────────────────────────────────────────────┐")
    print("  │  WICHTIG: Geraeteversion beachten!                  │")
    print("  │                                                     │")
    print("  │  V1 (DP32G030)  — dieses Flash-Tool ist kompatibel  │")
    print("  │  V2 (PY32F030)  — NICHT kompatibel, eigene Tools!   │")
    print("  │  V3 (PY32F071)  — NICHT kompatibel, eigene Tools!   │")
    print("  │                                                     │")
    print("  │  Version steht unter dem Akku / auf dem Aufkleber.  │")
    print("  │  Falsche Firmware -> Geraet funktionslos!           │")
    print("  └─────────────────────────────────────────────────────┘")
    print()


def select_device_version():
    """
    Fragt die Geraeteversion interaktiv ab.
    Gibt 'v1' zurueck wenn kompatibel, None wenn abgebrochen.
    """
    print("  Welche Hardware-Version hat dein Radio?")
    print("    1. V1 (Original, DP32G030 MCU)")
    print("    2. V3 / K1 (PY32F071 MCU)")
    print("    3. Unsicher / weiss nicht")
    print()

    choice = input("  Auswahl [1/2/3]: ").strip()

    if choice == '1':
        print()
        print("  ✓ V1 erkannt — dieses Tool ist kompatibel.")
        return 'v1'

    elif choice == '2':
        print()
        print("  ╔═════════════════════════════════════════════════════╗")
        print("  ║  V3/K1 Geraete verwenden eine andere MCU           ║")
        print("  ║  (PY32F071 statt DP32G030).                        ║")
        print("  ║                                                     ║")
        print("  ║  Die Firmware aus diesem Projekt ist NUR fuer V1!   ║")
        print("  ║                                                     ║")
        print("  ║  Fuer V3/K1 Firmware und Flash-Tools siehe:         ║")
        print("  ║    https://armel.github.io/uvtools2/                ║")
        print("  ║    https://github.com/armel/uv-k1-k5v3-firmware-custom  ║")
        print("  ║    https://github.com/qrp73/K5TOOL                 ║")
        print("  ╚═════════════════════════════════════════════════════╝")
        print()
        return None

    elif choice == '3':
        print()
        print("  So findest du die Version heraus:")
        print("    - Unter dem Akku auf dem Aufkleber steht z.B. 'V1', 'V3'")
        print("    - V1: schwarzes PCB, DP32G030 Chip")
        print("    - V3: gruenes PCB, PY32F071 Chip, oft als 'K1' vermarktet")
        print("    - Im Bootloader-Modus: Version ≤1.00.xx = V1, ≥1.01.xx = V3")
        print()
        print("  Hinweis: Wenn du das falsche Firmware-Image flashst,")
        print("  wird das Radio funktionslos (aber per Bootloader rettbar).")
        print()
        answer = input("  Trotzdem als V1 fortfahren? [j/N]: ").strip().lower()
        if answer in ('j', 'y', 'ja', 'yes'):
            return 'v1'
        return None

    else:
        print("  Ungueltige Auswahl.")
        return None


def list_serial_ports():
    """Listet alle verfuegbaren seriellen Ports auf."""
    try:
        import serial.tools.list_ports
        ports = serial.tools.list_ports.comports()
        return [(p.device, p.description) for p in ports]
    except ImportError:
        return []


def flash_firmware(port_name: str, fw_path: str):
    """Flasht die Firmware auf das Radio."""

    # Firmware laden
    print(f"  Lade Firmware: {fw_path}")
    with open(fw_path, 'rb') as f:
        fw_data = f.read()
    print(f"  Dateigroesse: {len(fw_data)} Bytes")

    # Dateityp erkennen
    file_type = detect_file_type(fw_data)

    if file_type == 'packed':
        print("  Dateityp: gepackte Firmware (.packed.bin)")
        raw_fw, version_info = unpack_firmware(fw_data)
        print(f"  Version: {version_info.split(b'\x00')[0].decode('ascii', errors='replace')}")
    else:
        print("  Dateityp: rohe Firmware (.bin)")
        raw_fw = fw_data
        # Standard-Version verwenden (Wildcard '*' fuer Kompatibilitaet)
        version_info = b'*FOXRX\x00' + b'\x00' * 9

    print(f"  Firmware-Groesse: {len(raw_fw)} Bytes")

    # Groessencheck
    if len(raw_fw) > MAX_FW_SIZE:
        print(f"\n  FEHLER: Firmware ist zu gross! ({len(raw_fw)} > {MAX_FW_SIZE} Bytes)")
        return False

    print(f"  Speichernutzung: {len(raw_fw) / MAX_FW_SIZE * 100:.1f}%")
    print()

    # Serielle Verbindung oeffnen
    print(f"  Oeffne seriellen Port: {port_name}")
    try:
        radio = K5Radio(port_name)
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

        response = radio.wait_for_bootloader(timeout_s=30.0)
        if response is None:
            print("  FEHLER: Kein Bootloader erkannt!")
            print("    - Ist das Radio im Flash-Modus? (PTT + Einschalten)")
            print("    - Ist das Programmierkabel angeschlossen?")
            print("    - Stimmt der COM-Port?")
            return False

        print("  Bootloader erkannt!")

        # Bootloader-Version anzeigen
        if len(response) > 0x14 + 7:
            bl_version = response[0x14:0x14 + 7]
            try:
                bl_str = bl_version.decode('ascii', errors='replace').strip('\x00')
                print(f"  Bootloader-Version: {bl_str}")
            except Exception:
                pass

        # Flash-Modus initialisieren
        print("  Initialisiere Flash-Modus ...")
        init_response = radio.send_flash_init(version_info)
        if init_response is None:
            print("  FEHLER: Flash-Initialisierung fehlgeschlagen!")
            return False

        # Versionscheck (wie im Web-Flasher)
        if len(init_response) > 0x14:
            # Radio akzeptiert Wildcard '*' immer
            if version_info[0] != 0x2A:  # nicht '*'
                if init_response[0x14] != version_info[0]:
                    print(f"\n  WARNUNG: Versions-Mismatch!")
                    print(f"    Radio: {chr(init_response[0x14])}")
                    print(f"    Firmware: {chr(version_info[0])}")
                    answer = input("    Trotzdem fortfahren? [j/N]: ").strip().lower()
                    if answer not in ('j', 'y', 'ja', 'yes'):
                        print("  Abgebrochen.")
                        return False

        # Firmware flashen
        print()
        total_blocks = (len(raw_fw) + BLOCK_SIZE - 1) // BLOCK_SIZE
        print(f"  Flashe {len(raw_fw)} Bytes in {total_blocks} Bloecken ...")
        print()

        for i in range(0, len(raw_fw), BLOCK_SIZE):
            block = raw_fw[i:i + BLOCK_SIZE]
            block_num = i // BLOCK_SIZE + 1
            percent = (i + BLOCK_SIZE) / len(raw_fw) * 100
            if percent > 100:
                percent = 100

            # Fortschrittsbalken
            bar_width = 40
            filled = int(bar_width * min(percent, 100) / 100)
            bar = '█' * filled + '░' * (bar_width - filled)
            sys.stdout.write(f'\r  [{bar}] {percent:5.1f}%  Block {block_num}/{total_blocks}')
            sys.stdout.flush()

            success = radio.write_flash_block(i, block, len(raw_fw))
            if not success:
                print(f"\n\n  FEHLER: Block {block_num} bei Adresse 0x{i:04X} fehlgeschlagen!")
                print("    Das Radio hat den Schreibbefehl nicht bestaetigt.")
                return False

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
            choice = input("  Port waehlen (Nummer oder Name, z.B. COM3): ").strip()
            if not choice:
                print("  Abgebrochen.")
                return

            # Nummer?
            try:
                idx = int(choice) - 1
                if 0 <= idx < len(ports):
                    port_name = ports[idx][0]
                    break
                else:
                    print("  Ungueltige Nummer.")
                    continue
            except ValueError:
                # Direkte Eingabe des Portnamens
                port_name = choice
                break
    else:
        print("  Keine seriellen Ports gefunden!")
        print("  (Ist das Programmierkabel angeschlossen?)")
        print()
        port_name = input("  Port manuell eingeben (z.B. COM3 oder /dev/ttyUSB0): ").strip()
        if not port_name:
            print("  Abgebrochen.")
            return

    print(f"\n  Gewaehlter Port: {port_name}")
    print()

    # Firmware-Datei suchen
    # Zuerst im build-output Verzeichnis suchen
    script_dir = os.path.dirname(os.path.abspath(__file__))
    build_dir = os.path.join(script_dir, '..', 'build-output')
    build_dir = os.path.normpath(build_dir)

    candidates = []

    # build-output/ durchsuchen
    if os.path.isdir(build_dir):
        for f in sorted(os.listdir(build_dir), reverse=True):
            if f.endswith('.packed.bin'):
                candidates.append(os.path.join(build_dir, f))
            elif f.endswith('.bin') and not f.endswith('.packed.bin'):
                candidates.append(os.path.join(build_dir, f))

    # Aktuelles Verzeichnis durchsuchen
    for f in sorted(os.listdir('.'), reverse=True):
        full = os.path.abspath(f)
        if full not in [os.path.abspath(c) for c in candidates]:
            if f.endswith('.packed.bin') or (f.endswith('.bin') and 'firmware' in f.lower()):
                candidates.append(full)

    if candidates:
        print("  Gefundene Firmware-Dateien:")
        for i, path in enumerate(candidates, 1):
            size = os.path.getsize(path)
            name = os.path.basename(path)
            print(f"    {i}. {name}  ({size} Bytes)")
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


def main():
    if len(sys.argv) == 3:
        # Kommandozeilen-Modus: k5flash.py <port> <firmware>
        port_name = sys.argv[1]
        fw_path = sys.argv[2]

        if not os.path.isfile(fw_path):
            print(f"FEHLER: Datei nicht gefunden: {fw_path}")
            sys.exit(1)

        print_banner()
        success = flash_firmware(port_name, fw_path)
        sys.exit(0 if success else 1)

    elif len(sys.argv) == 1:
        # Interaktiver Modus
        interactive_mode()

    else:
        print("Benutzung:")
        print(f"  {sys.argv[0]} <COM-Port> <firmware.packed.bin>")
        print(f"  {sys.argv[0]}                                   (interaktiver Modus)")
        print()
        print("Beispiele:")
        print(f"  {sys.argv[0]} COM3 firmware_uvk5_v1.packed.bin")
        print(f"  {sys.argv[0]} /dev/ttyUSB0 build-output/firmware_uvk5_v1.packed.bin")
        sys.exit(1)


if __name__ == '__main__':
    main()
