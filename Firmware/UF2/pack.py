"""FreeEEG128 UF2 container and sparse factory HEX generation (Python standard library)."""
import struct
import zlib

APP_BASE = 0x08020000
APP_LIMIT = 0x080E0000
APP_META = APP_LIMIT
STAGE_META = 0x081E0000
FAMILY = 0x6DB66082
BOARD = 0x38314546
LAYOUT = 1
MAGIC = 0x32474546
BOARD_MAGIC = 0x42474546


def application_image(raw):
    if not 0x420 <= len(raw) <= APP_LIMIT - APP_BASE:
        raise ValueError('Application does not fit the UF2 flash layout')
    expected = (BOARD_MAGIC, LAYOUT, BOARD, APP_BASE, APP_LIMIT)
    if struct.unpack_from('<5I', raw, 0x400) != expected:
        raise ValueError('Application is missing the FreeEEG128 UF2 board header; rebuild with the UF2 target')
    sp, entry = struct.unpack_from('<II', raw)
    if not (0x20000000 < sp <= 0x20020000 and sp % 8 == 0 and entry & 1 and APP_BASE <= entry < APP_BASE + len(raw)):
        raise ValueError('Application vectors do not match the UF2 layout')
    image = raw + b'\xff' * (-len(raw) % 256)
    crc = zlib.crc32(image)
    fields = (MAGIC, LAYOUT, BOARD, len(image), crc, len(image) ^ 0xffffffff, crc ^ 0xffffffff)
    record = struct.pack('<7I', *fields)
    record += struct.pack('<I', zlib.crc32(record))
    return image, record


def uf2_file(raw):
    image, record = application_image(raw)
    result = bytearray()
    count = len(image) // 256
    for block in range(count):
        header = struct.pack('<8I', 0x0A324655, 0x9E5D5157, 0x2000,
                             APP_BASE + block * 256, 256, block, count, FAMILY)
        result.extend(header + image[block * 256:(block + 1) * 256] + record + bytes(188) + struct.pack('<I', 0x0AB16F30))
    return bytes(result)


def intel_hex(segments):
    def record(kind, address, data):
        body = struct.pack('>BHB', len(data), address, kind) + data
        return ':' + (body + bytes([-sum(body) & 255])).hex().upper()
    lines = []
    upper = None
    end = 0
    for start, data in sorted(segments):
        if start < end or not 0x08000000 <= start < start + len(data) <= 0x08200000:
            raise ValueError('Overlapping or out-of-range HEX segment')
        end = start + len(data)
        for offset in range(0, len(data), 32):
            address = start + offset
            if address >> 16 != upper:
                upper = address >> 16
                lines.append(record(4, 0, struct.pack('>H', upper)))
            lines.append(record(0, address & 0xffff, data[offset:offset + 32]))
    lines.append(':00000001FF')
    return '\n'.join(lines) + '\n'
