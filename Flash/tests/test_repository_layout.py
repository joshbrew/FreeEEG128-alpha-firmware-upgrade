"""Verify source organization and independently usable offline release packages."""
import hashlib
import importlib.util
import json
from pathlib import Path
import struct
import sys
import tempfile
import unittest
import zipfile

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'Firmware/UF2'))
from pack import APP_BASE, APP_LIMIT, APP_META, BOARD, BOARD_MAGIC, STAGE_META, application_image, intel_hex, uf2_file
from release import create_release

spec = importlib.util.spec_from_file_location('layout_checks', ROOT / 'Firmware/tools/check_layout.py')
layout = importlib.util.module_from_spec(spec)
spec.loader.exec_module(layout)


def images(out):
    """Valid minimal containers, independent of generated compiler outputs."""
    raw = bytearray(1536)
    struct.pack_into('<II', raw, 0, 0x20020000, APP_BASE + 0x501)
    struct.pack_into('<5I', raw, 0x400, BOARD_MAGIC, 1, BOARD, APP_BASE, APP_LIMIT)
    app, record = application_image(raw)
    boot = struct.pack('<II', 0x20020000, 0x08000101)
    (out / 'freeeeg128-application.uf2').write_bytes(uf2_file(raw))
    for name, segments in (
        ('application', [(APP_BASE, app), (APP_META, record), (STAGE_META, bytes(32))]),
        ('bootloader', [(0x08000000, boot)]),
        ('factory', [(0x08000000, boot), (APP_BASE, app), (APP_META, record), (STAGE_META, bytes(32))]),
    ):
        (out / f'freeeeg128-{name}.hex').write_text(intel_hex(segments), encoding='utf-8')
    manifest = {'board': 'FreeEEG128-alpha STM32H743ZI', 'files': {}}
    for name in layout.IMAGES:
        data = (out / name).read_bytes()
        manifest['files'][name] = {'bytes': len(data), 'sha256': hashlib.sha256(data).hexdigest()}
    (out / 'manifest.json').write_text(json.dumps(manifest), encoding='utf-8')


class RepositoryLayoutTests(unittest.TestCase):
    def test_checkout_has_complete_organized_layout_and_navigation(self):
        self.assertEqual(layout.check_source(ROOT), [])

    def test_release_is_complete_in_zip_and_extracted_form(self):
        with tempfile.TemporaryDirectory() as folder:
            out = Path(folder)
            images(out)
            bundle = create_release(out, ROOT)
            self.assertEqual(layout.check_release(bundle), [])
            extracted = out / 'extracted'
            with zipfile.ZipFile(bundle) as archive:
                archive.extractall(extracted)
            self.assertEqual(layout.check_release(extracted), [])
            self.assertIn('(WIRING.md)', (extracted / 'README.md').read_text(encoding='utf-8'))
            self.assertIn('(STACKING.md)', (extracted / 'README.md').read_text(encoding='utf-8'))
            self.assertIn('(CONTROL.md#usb-serial-controls)', (extracted / 'STACKING.md').read_text(encoding='utf-8'))

    def test_packaging_rejects_an_image_that_changed_after_build(self):
        with tempfile.TemporaryDirectory() as folder:
            out = Path(folder)
            images(out)
            (out / 'freeeeg128-application.uf2').write_bytes(b'changed')
            with self.assertRaisesRegex(ValueError, 'does not match manifest'):
                create_release(out, ROOT)

    def test_release_checker_catches_missing_helpers_and_image_corruption(self):
        with tempfile.TemporaryDirectory() as folder:
            out = Path(folder)
            images(out)
            bundle = create_release(out, ROOT)
            extracted = out / 'extracted'
            with zipfile.ZipFile(bundle) as archive:
                archive.extractall(extracted)
            (extracted / 'device.py').unlink()
            (extracted / 'freeeeg128-factory.hex').write_bytes(b'changed')
            errors = layout.check_release(extracted)
            self.assertIn('Missing release file: device.py', errors)
            self.assertIn('Image does not match manifest: freeeeg128-factory.hex', errors)

    def test_document_checks_detect_missing_sections_and_encoding(self):
        documents = {'README.md': '# Start\n[Next](guide.md#missing)\n',
                     'guide.md': '# Guide\nBroken \u00e2\u20ac punctuation\n'}
        errors = layout.check_documents(set(documents), documents)
        self.assertIn('README.md: missing section guide.md#missing', errors)
        self.assertIn('guide.md: garbled text encoding', errors)

    def test_heading_checks_ignore_code_and_support_duplicate_headings(self):
        self.assertEqual(layout.headings('# Setup\n```bash\n# shell comment\n```\n# Setup\n'),
                         {'setup', 'setup-1'})


if __name__ == '__main__':
    unittest.main()
