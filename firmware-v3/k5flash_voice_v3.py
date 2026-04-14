#!/usr/bin/env python3
# =============================================================================
# k5flash_voice_v3.py — Voice Pack Flasher for Quansheng UV-K5 V3/K1
# =============================================================================
#
# Flashes a .vpk voice pack file to the PY25Q16 SPI flash on V3/K1 radios.
# Uses the UART protocol to write voice data while the firmware is running.
#
# The firmware must have ENABLE_VOICE_PROMPTS enabled for the SPI flash
# write command (0x0603) to be available.
#
# Usage:
#   python3 k5flash_voice_v3.py <COM-Port> <voice_pack.vpk>
#   python3 k5flash_voice_v3.py COM3 build-output/voice_prompts_de.vpk
#   python3 k5flash_voice_v3.py /dev/ttyUSB0 voice_prompts_de.vpk
#
# Voraussetzungen:
#   pip install pyserial
#
# Lizenz: Apache-2.0
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
# Protocol constants (same as firmware UART protocol)
# =============================================================================

BAUDRATE = 38400

# XOR table for packet obfuscation (identical to firmware uart.c)
OBFUS_TBL = bytes([
    0x16, 0x6C, 0x14, 0xE6, 0x2E, 0x91, 0x0D, 0x40,
    0x21, 0x35, 0xD5, 0x40, 0x13, 0x03, 0xE9, 0x80
])

# Message types
MSG_CMD_VERSION   = 0x0514  # Request version
MSG_VERSION_RESP  = 0x0515  # Version response
MSG_READ_EEPROM   = 0x051B  # Read EEPROM
MSG_READ_RESP     = 0x051C  # Read response
MSG_WRITE_EEPROM  = 0x051D  # Write EEPROM
MSG_WRITE_RESP    = 0x051E  # Write response
MSG_WRITE_SPI     = 0x0603  # Write PY25Q16 directly
MSG_WRITE_SPI_RESP = 0x0604 # Write SPI response

# SPI Flash addresses for voice data
VOICE_INDEX_CHI = 0x14c000  # Chinese voice index table
VOICE_INDEX_ENG = 0x14c800  # English voice index table
VOICE_DATA_BASE = 0x14d000  # Voice data starts here
VOICE_FLASH_END = 0x200000  # End of PY25Q16 flash
MAX_VOICE_DATA  = VOICE_FLASH_END - VOICE_DATA_BASE  # 733,184 bytes (716 KB)

# Max chunk size per write (limited by UART buffer)
SPI_WRITE_CHUNK = 128


# =============================================================================
# CRC16 and packet helpers (same as k5flash_v3.py)
# =============================================================================

def calc_crc16(data, offset=0, size=0):
    """CRC16-CCITT calculation."""
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


def obfuscate(buf, offset=0, size=0):
    """XOR obfuscation/deobfuscation (symmetric)."""
    n = len(OBFUS_TBL)
    for i in range(size):
        buf[offset + i] ^= OBFUS_TBL[i % n]


def make_packet(msg):
    """Build a serial packet: 0xABCD + length + obfuscate(msg + CRC) + 0xDCBA."""
    msg_len = len(msg)
    if msg_len % 2 != 0:
        msg_len += 1

    buf = bytearray(8 + msg_len)
    struct.pack_into('<H', buf, 0, 0xCDAB)
    struct.pack_into('<H', buf, 2, msg_len)
    struct.pack_into('<H', buf, 6 + msg_len, 0xBADC)

    buf[4:4 + len(msg)] = msg

    crc = calc_crc16(buf, 4, msg_len)
    struct.pack_into('<H', buf, 4 + msg_len, crc)

    obfuscate(buf, 4, 2 + msg_len)

    return bytes(buf)


def parse_packet(buf):
    """Try to extract a packet from the buffer."""
    while True:
        if len(buf) < 8:
            return None, buf

        idx = buf.find(b'\xab\xcd')
        if idx < 0:
            if buf.endswith(b'\xab'):
                buf = buf[-1:]
            else:
                buf = bytearray()
            return None, buf

        if idx > 0:
            buf = buf[idx:]

        if len(buf) < 8:
            return None, buf

        msg_len = buf[2] | (buf[3] << 8)
        pack_end = 6 + msg_len

        if len(buf) < pack_end + 2:
            return None, buf

        if buf[pack_end] != 0xDC or buf[pack_end + 1] != 0xBA:
            buf = buf[2:]
            continue

        obfuscate(buf, 4, msg_len + 2)
        msg = bytes(buf[4:pack_end - 2])

        buf = buf[pack_end + 2:]
        return msg, buf


# =============================================================================
# Radio communication
# =============================================================================

class K5V3VoiceFlasher:
    """Communicates with the running V3 firmware for voice data flashing."""

    def __init__(self, port_name):
        self.ser = serial.Serial(
            port=port_name,
            baudrate=BAUDRATE,
            timeout=0.05,
            write_timeout=5,
        )
        self.rx_buf = bytearray()
        self.timestamp = int(time.time()) & 0xFFFFFFFF

    def close(self):
        if self.ser and self.ser.is_open:
            self.ser.close()

    def send_msg(self, msg):
        """Send a message as a packet."""
        packet = make_packet(msg)
        self.ser.write(packet)
        self.ser.flush()

    def recv_msg(self, timeout_s=2.0):
        """Receive a message with timeout."""
        deadline = time.time() + timeout_s

        while time.time() < deadline:
            waiting = self.ser.in_waiting
            if waiting > 0:
                data = self.ser.read(waiting)
                self.rx_buf.extend(data)
            else:
                time.sleep(0.01)

            msg, self.rx_buf = parse_packet(self.rx_buf)
            if msg is not None:
                return msg

        return None

    def handshake(self):
        """
        Initiate communication with the running firmware.
        Send version request and wait for response.
        """
        # Send CMD_0514 (version request)
        msg = bytearray(8)
        struct.pack_into('<H', msg, 0, MSG_CMD_VERSION)
        struct.pack_into('<H', msg, 2, 4)
        struct.pack_into('<I', msg, 4, self.timestamp)
        self.send_msg(bytes(msg))

        # Wait for response
        resp = self.recv_msg(timeout_s=3.0)
        if resp is None:
            return False

        msg_type = resp[0] | (resp[1] << 8) if len(resp) >= 2 else 0
        return msg_type == MSG_VERSION_RESP

    def write_spi_flash(self, address, data):
        """
        Write data to PY25Q16 SPI flash using CMD_0603.
        Format: Header(4) + Address(4) + Size(2) + Data[Size]
        """
        size = len(data)
        msg = bytearray(10 + size)
        struct.pack_into('<H', msg, 0, MSG_WRITE_SPI)       # Command ID
        struct.pack_into('<H', msg, 2, 6 + size)             # Data length
        struct.pack_into('<I', msg, 4, address)              # PY25Q16 address
        struct.pack_into('<H', msg, 8, size)                 # Data size
        msg[10:10 + size] = data                             # Data

        self.send_msg(bytes(msg))

        # Wait for response
        resp = self.recv_msg(timeout_s=5.0)
        if resp is None:
            return False

        msg_type = resp[0] | (resp[1] << 8) if len(resp) >= 2 else 0
        return msg_type == MSG_WRITE_SPI_RESP

    def write_spi_flash_chunked(self, base_address, data, chunk_size=SPI_WRITE_CHUNK):
        """Write large data to SPI flash in chunks."""
        total = len(data)
        written = 0

        while written < total:
            remaining = total - written
            chunk = min(remaining, chunk_size)
            address = base_address + written

            if not self.write_spi_flash(address, data[written:written + chunk]):
                return written

            written += chunk

            # Progress
            pct = written * 100 // total
            bar_len = 40
            filled = pct * bar_len // 100
            bar = '█' * filled + '░' * (bar_len - filled)
            sys.stdout.write(f'\r  [{bar}] {pct:3d}% ({written}/{total} Bytes)')
            sys.stdout.flush()

            # Small delay for SPI flash write time
            time.sleep(0.05)

        print()
        return written


# =============================================================================
# VPK file handling
# =============================================================================

def load_vpk(filepath):
    """
    Load a .vpk voice pack file.
    Returns (language, index_data, voice_data) or raises an error.
    """
    with open(filepath, 'rb') as f:
        data = f.read()

    if len(data) < 16:
        raise ValueError("VPK-Datei zu klein")

    # Parse header
    magic = data[0:4]
    if magic != b'VPK1':
        raise ValueError(f"Ungueltiger Magic: {magic}")

    version, language, num_entries, max_voice_id = struct.unpack_from('<HHHH', data, 4)
    if version != 1:
        raise ValueError(f"Unbekannte VPK-Version: {version}")

    header_size = 16
    index_size = (max_voice_id + 1) * 8
    voice_data_offset = header_size + index_size

    # Extract index table (raw 8-byte entries)
    index_data = data[header_size:header_size + index_size]

    # Extract voice data
    voice_data = data[voice_data_offset:]

    lang_names = {0: "Chinesisch", 1: "Englisch", 2: "Deutsch"}
    lang_name = lang_names.get(language, f"Unbekannt ({language})")

    print(f"  VPK-Version: {version}")
    print(f"  Sprache: {lang_name}")
    print(f"  Clips: {num_entries}")
    print(f"  Max Voice-ID: 0x{max_voice_id:02X}")
    print(f"  Index-Groesse: {len(index_data)} Bytes")
    print(f"  Voice-Daten: {len(voice_data)} Bytes ({len(voice_data)/1024:.1f} KB)")

    return language, index_data, voice_data


# =============================================================================
# Main
# =============================================================================

def print_banner():
    print()
    print("=" * 60)
    print("  Quansheng UV-K5 V3/K1 Voice Pack Flasher")
    print("  (fuer MSYS2 / Kommandozeile)")
    print("=" * 60)
    print()


def list_serial_ports():
    """List available serial ports."""
    try:
        ports = serial.tools.list_ports.comports()
        return [(p.device, p.description) for p in ports]
    except Exception:
        return []


def select_serial_port():
    """Interactively select a serial port."""
    ports = list_serial_ports()

    if not ports:
        print("  Keine seriellen Ports gefunden!")
        print("  Ist das Programmierkabel angeschlossen?")
        return None

    print("  Verfuegbare serielle Ports:")
    for i, (device, desc) in enumerate(ports):
        print(f"    {i + 1}. {device} — {desc}")
    print()

    choice = input("  Port waehlen (Nummer oder Name): ").strip()

    try:
        idx = int(choice) - 1
        if 0 <= idx < len(ports):
            return ports[idx][0]
    except ValueError:
        pass

    # Try as direct port name
    if choice:
        return choice

    return None


def flash_voice_pack(port_name, vpk_path):
    """Flash a voice pack to the radio's SPI flash."""

    print(f"  Lade Voice-Pack: {vpk_path}")
    language, index_data, voice_data = load_vpk(vpk_path)
    print()

    # Determine SPI flash address for index table based on language
    if language == 0:  # Chinese
        index_addr = VOICE_INDEX_CHI
    else:  # English, German, or other
        index_addr = VOICE_INDEX_ENG

    data_addr = VOICE_DATA_BASE

    total_size = len(index_data) + len(voice_data)
    print(f"  Zieladressen:")
    print(f"    Index-Tabelle: 0x{index_addr:06X} ({len(index_data)} Bytes)")
    print(f"    Voice-Daten:   0x{data_addr:06X} ({len(voice_data)} Bytes)")
    print(f"    Gesamt:        {total_size} Bytes ({total_size/1024:.1f} KB)")
    print(f"    Verfuegbar:    {MAX_VOICE_DATA} Bytes ({MAX_VOICE_DATA/1024:.1f} KB)")

    # Validate voice data fits in flash
    if len(voice_data) > MAX_VOICE_DATA:
        excess = len(voice_data) - MAX_VOICE_DATA
        print(f"\n  FEHLER: Voice-Daten ({len(voice_data)} Bytes) ueberschreiten")
        print(f"  die Flash-Kapazitaet ({MAX_VOICE_DATA} Bytes) um {excess} Bytes!")
        print(f"  Bitte Voice-Pack mit kuerzeren Clips oder weniger Eintraegen neu erzeugen.")
        return False
    print(f"    Auslastung:    {len(voice_data)*100/MAX_VOICE_DATA:.1f}%")
    print()

    # Open serial connection
    print(f"  Oeffne seriellen Port: {port_name}")
    try:
        flasher = K5V3VoiceFlasher(port_name)
    except serial.SerialException as e:
        print(f"\n  FEHLER: Kann Port {port_name} nicht oeffnen:")
        print(f"    {e}")
        return False

    try:
        # Handshake with running firmware
        print("  Verbinde mit Firmware ...")
        print("  (Radio muss eingeschaltet und NICHT im Flash-Modus sein)")
        print()

        if not flasher.handshake():
            print("  WARNUNG: Keine Antwort auf Version-Anfrage.")
            print("  Versuche trotzdem fortzufahren ...")
            print()

        # Write index table
        print("  Schreibe Index-Tabelle ...")
        written = flasher.write_spi_flash_chunked(index_addr, index_data)
        if written < len(index_data):
            print(f"\n  FEHLER: Index-Tabelle unvollstaendig ({written}/{len(index_data)} Bytes)")
            return False
        print(f"  Index-Tabelle geschrieben: {written} Bytes")
        print()

        # Write voice data
        print("  Schreibe Voice-Daten ...")
        written = flasher.write_spi_flash_chunked(data_addr, voice_data)
        if written < len(voice_data):
            print(f"\n  FEHLER: Voice-Daten unvollstaendig ({written}/{len(voice_data)} Bytes)")
            return False
        print(f"  Voice-Daten geschrieben: {written} Bytes")
        print()

        print("  ✓ Voice-Pack erfolgreich geflasht!")
        print()
        print("  Bitte Radio neu starten, um die Voice-Prompts zu aktivieren.")
        return True

    finally:
        flasher.close()


def find_vpk_files(search_dirs):
    """Find .vpk files in search directories."""
    found = []
    for d in search_dirs:
        if os.path.isdir(d):
            for f in os.listdir(d):
                if f.endswith('.vpk'):
                    found.append(os.path.join(d, f))
    return sorted(found)


def main():
    print_banner()

    # Parse command line
    if len(sys.argv) >= 3:
        port_name = sys.argv[1]
        vpk_path = sys.argv[2]
    elif len(sys.argv) == 2:
        vpk_path = sys.argv[1]
        port_name = None
    else:
        vpk_path = None
        port_name = None

    # Find VPK file
    if vpk_path is None:
        script_dir = os.path.dirname(os.path.abspath(__file__))
        repo_root = os.path.dirname(script_dir)  # firmware-v3/ -> repo root
        search_dirs = [
            os.path.join(repo_root, "build-output"),
            repo_root,
            script_dir,
        ]
        vpk_files = find_vpk_files(search_dirs)

        if not vpk_files:
            print("  Keine .vpk Voice-Pack-Dateien gefunden!")
            print("  Bitte zuerst mit msys2_voice_build.sh erstellen.")
            sys.exit(1)

        print("  Gefundene Voice-Pack-Dateien:")
        for i, f in enumerate(vpk_files):
            size = os.path.getsize(f)
            print(f"    {i + 1}. {os.path.basename(f)} ({size / 1024:.1f} KB)")
        print()

        choice = input("  Datei waehlen (Nummer): ").strip()
        try:
            idx = int(choice) - 1
            if 0 <= idx < len(vpk_files):
                vpk_path = vpk_files[idx]
            else:
                print("  Ungueltige Auswahl.")
                sys.exit(1)
        except ValueError:
            print("  Ungueltige Eingabe.")
            sys.exit(1)

    if not os.path.isfile(vpk_path):
        print(f"  FEHLER: Datei nicht gefunden: {vpk_path}")
        sys.exit(1)

    # Select serial port
    if port_name is None:
        port_name = select_serial_port()
        if port_name is None:
            print("  Kein Port ausgewaehlt.")
            sys.exit(1)

    print()
    success = flash_voice_pack(port_name, vpk_path)
    sys.exit(0 if success else 1)


if __name__ == '__main__':
    main()
