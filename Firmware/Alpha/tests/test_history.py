import importlib.util
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

BETA = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(BETA))
spec = importlib.util.spec_from_file_location('history_build', BETA / 'build.py')
build = importlib.util.module_from_spec(spec)
spec.loader.exec_module(build)


class HistoryTests(unittest.TestCase):
    def test_successful_build_archives_do_not_replace_prior_builds(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            source = root / 'Firmware/Alpha'
            source.mkdir(parents=True)
            output = root / 'output'
            output.mkdir()
            (output / 'firmware.zip').write_bytes(b'first firmware')
            (output / 'manifest.json').write_text('{"build_id":"first"}')
            with patch.object(build, 'HERE', source):
                first = build.archive_build(output, ['firmware.zip', 'manifest.json'], 80, '0123456789abcdef', 'uf2')
                (output / 'firmware.zip').write_bytes(b'second firmware')
                second = build.archive_build(output, ['firmware.zip', 'manifest.json'], 80, '0123456789abcdef', 'uf2')
            self.assertNotEqual(first, second)
            self.assertEqual((first / 'firmware.zip').read_bytes(), b'first firmware')
            self.assertEqual((second / 'firmware.zip').read_bytes(), b'second firmware')
            self.assertIn('80mhz-0123456789ab', first.name)


if __name__ == '__main__': unittest.main()
