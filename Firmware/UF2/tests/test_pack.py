from pathlib import Path
import struct
import sys
import unittest
import zlib
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from pack import APP_BASE, APP_LIMIT, APP_META, BOARD, BOARD_MAGIC, FAMILY, application_image, intel_hex, uf2_file


def fixture():
    image = bytearray(1500)
    struct.pack_into('<II', image, 0, 0x20020000, APP_BASE + 0x501)
    struct.pack_into('<5I', image, 0x400, BOARD_MAGIC, 1, BOARD, APP_BASE, APP_LIMIT)
    return image


class PackageTests(unittest.TestCase):
    def test_blocks_and_manifest_match_image(self):
        image, record = application_image(fixture())
        data = uf2_file(fixture())
        self.assertEqual(len(data), len(image) * 2)
        manifest = struct.unpack('<8I', record)
        self.assertEqual(manifest[4], zlib.crc32(image))
        self.assertEqual(manifest[7], zlib.crc32(record[:28]))
        for number, offset in enumerate(range(0, len(data), 512)):
            block = data[offset:offset + 512]
            fields = struct.unpack_from('<8I', block)
            self.assertEqual(fields[2:], (0x2000, APP_BASE + number * 256, 256, number, len(image) // 256, FAMILY))
            self.assertEqual(block[288:320], record)
            self.assertEqual(block[32:288], image[number * 256:(number + 1) * 256])

    def test_wrong_board_or_unrelocated_binary_rejected(self):
        image = fixture()
        image[0x400] ^= 1
        with self.assertRaises(ValueError): application_image(image)
        image = fixture()
        struct.pack_into('<I', image, 4, 0x08000101)
        with self.assertRaises(ValueError): application_image(image)

    def test_freeeeg32_board_is_rejected(self):
        image = fixture()
        struct.pack_into('<I', image, 0x408, 0x32334546)
        with self.assertRaises(ValueError): application_image(image)

    def test_factory_hex_checksums_and_sparse_addresses(self):
        image, metadata = application_image(fixture())
        text = intel_hex([(APP_BASE, image), (APP_META, metadata)])
        upper = 0
        result = {}
        for line in text.splitlines():
            data = bytes.fromhex(line[1:]); self.assertEqual(sum(data) & 255, 0)
            if data[3] == 4: upper = int.from_bytes(data[4:6], 'big') << 16
            if data[3] == 0:
                address = upper + int.from_bytes(data[1:3], 'big')
                for i, value in enumerate(data[4:-1]): result[address + i] = value
        self.assertEqual(bytes(result[APP_BASE + i] for i in range(len(image))), image)
        self.assertEqual(bytes(result[APP_META + i] for i in range(32)), metadata)
        self.assertNotIn(0x08000000, result)

    def test_overlapping_segments_rejected(self):
        with self.assertRaises(ValueError): intel_hex([(APP_BASE, bytes(64)), (APP_BASE + 32, bytes(64))])


if __name__ == '__main__': unittest.main()
