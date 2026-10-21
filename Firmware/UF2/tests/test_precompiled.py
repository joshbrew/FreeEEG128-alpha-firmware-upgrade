from pathlib import Path
import hashlib
import json
import sys
import tempfile
import unittest
import zipfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from precompiled import DOWNLOADS, IMAGES, sync_bundle


def fixture(folder, cpu=80, corrupt=False):
    bundle = folder / 'input.zip'
    images = {name: (name + ': firmware').encode() for name in IMAGES}
    manifest = {'board': 'FreeEEG128-alpha STM32H743ZI', 'application_clocks': {'cpu_mhz': cpu},
                'files': {name: {'bytes': len(data), 'sha256': hashlib.sha256(data).hexdigest()}
                          for name, data in images.items()}}
    if corrupt:
        images[IMAGES[0]] += b'corruption'
    with zipfile.ZipFile(bundle, 'w') as archive:
        archive.writestr('manifest.json', json.dumps(manifest))
        for name, data in images.items():
            archive.writestr(name, data)
    return bundle


class PrecompiledTests(unittest.TestCase):
    def test_verified_downloads_and_checksums_match_the_bundle(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            bundle = fixture(root)
            destination = root / 'Precompiled'
            sync_bundle(bundle, destination)
            self.assertEqual((destination / bundle.name).exists(), False)
            self.assertEqual((destination / 'freeeeg128-uf2-release.zip').read_bytes(), bundle.read_bytes())
            lines = (destination / 'SHA256SUMS.txt').read_text().splitlines()
            self.assertEqual(len(lines), len(DOWNLOADS))
            for line in lines:
                digest, name = line.split('  ')
                self.assertEqual(hashlib.sha256((destination / name).read_bytes()).hexdigest(), digest)
            with zipfile.ZipFile(bundle) as archive:
                for name in ('freeeeg128-factory.hex', 'freeeeg128-application.uf2', 'manifest.json'):
                    self.assertEqual((destination / name).read_bytes(), archive.read(name))

    def test_corruption_does_not_replace_previous_downloads(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            destination = root / 'Precompiled'
            destination.mkdir()
            previous = destination / 'freeeeg128-factory.hex'
            previous.write_bytes(b'previous good image')
            with self.assertRaisesRegex(ValueError, 'hash/size mismatch'):
                sync_bundle(fixture(root, corrupt=True), destination)
            self.assertEqual(previous.read_bytes(), b'previous good image')
            self.assertEqual(list(destination.iterdir()), [previous])

    def test_freeeeg32_release_is_rejected_before_downloads_are_written(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            bundle = fixture(root)
            rewritten = root / 'other-board.zip'
            with zipfile.ZipFile(bundle) as source, zipfile.ZipFile(rewritten, 'w') as target:
                for name in source.namelist():
                    data = source.read(name)
                    if name == 'manifest.json':
                        manifest = json.loads(data)
                        manifest['board'] = 'FreeEEG32-beta STM32H743ZI'
                        data = json.dumps(manifest).encode()
                    target.writestr(name, data)
            with self.assertRaisesRegex(ValueError, 'another board'):
                sync_bundle(rewritten, root / 'Precompiled')
            self.assertFalse((root / 'Precompiled').exists())

    def test_duplicate_zip_member_is_rejected(self):
        import warnings
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            bundle = fixture(root)
            with warnings.catch_warnings():
                warnings.simplefilter('ignore', UserWarning)
                with zipfile.ZipFile(bundle, 'a') as archive:
                    archive.writestr('manifest.json', '{}')
            with self.assertRaisesRegex(ValueError, 'Duplicate'):
                sync_bundle(bundle, root / 'Precompiled')
            self.assertFalse((root / 'Precompiled').exists())

    def test_unexpected_image_set_is_rejected(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            bundle = root / 'incomplete.zip'
            with zipfile.ZipFile(bundle, 'w') as archive:
                archive.writestr('manifest.json', json.dumps({
                    'board': 'FreeEEG128-alpha STM32H743ZI',
                    'application_clocks': {'cpu_mhz': 80}, 'files': {}}))
            with self.assertRaisesRegex(ValueError, 'unexpected image names'):
                sync_bundle(bundle, root / 'Precompiled')
            self.assertFalse((root / 'Precompiled').exists())

    def test_other_clock_profile_is_not_published_as_default(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            with self.assertRaisesRegex(ValueError, '80 MHz'):
                sync_bundle(fixture(root, cpu=480), root / 'Precompiled')
            self.assertFalse((root / 'Precompiled').exists())


if __name__ == '__main__':
    unittest.main()
