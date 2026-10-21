"""Read-only checks for the source checkout and offline release layout."""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import posixpath
import re
from urllib.parse import unquote, urlsplit
import zipfile

ACTIONS = ('Menu', 'Build', 'Build-and-Flash', 'Flash-Precompiled', 'USB-Update',
           'Configure', 'Check-Board', 'Test-Recording', 'Support-Report',
           'Backup-Settings', 'Restore-Settings', 'Install-Tools')
COMMON = {'FreeEEG128.cmd', 'FreeEEG128.sh', 'README.md', 'LICENSE'}
WRAPPERS = {f'Flash/{platform}/{action.lower() if platform == "Linux" else action}.{extension}'
            for platform, extension in (('Windows', 'cmd'), ('Linux', 'sh')) for action in ACTIONS}
TOOLS = {'launch.ps1', 'launch.sh', 'setup-windows.ps1', 'setup-linux.sh',
         'openocd-stlink.cfg', 'requirements-host.txt', 'check_layout.py', 'README.md'}
IMAGES = {'freeeeg128-factory.hex', 'freeeeg128-application.uf2',
          'freeeeg128-application.hex', 'freeeeg128-bootloader.hex'}
SOURCE_DOCS = ('README.md', 'AssemblyFiles/README.md', 'Firmware/README.md',
               'Firmware/Base/README.md', 'Firmware/Legacy/README.md', 'Firmware/VALIDATION.md',
               'Firmware/Alpha/README.md', 'Firmware/UF2/README.md', 'Firmware/UPSTREAM.md', 'OpenVIBE/README.md', 'Precompiled/README.md')


def headings(text):
    result, seen = set(), Counter()
    in_code = False
    for line in text.splitlines():
        if line.startswith(('```', '~~~')):
            in_code = not in_code
        if in_code:
            continue
        match = re.match(r'^#{1,6}\s+(.+?)\s*#*$', line)
        if match:
            base = re.sub(r'[^\w\- ]', '', match[1].lower()).replace(' ', '-')
            result.add(base + (f'-{seen[base]}' if seen[base] else ''))
            seen[base] += 1
    return result


def check_documents(files, documents):
    errors = []
    for name, text in documents.items():
        if any(marker in text for marker in ('\u00c3', '\u00c2', '\u00e2\u20ac', '\ufffd')):
            errors.append(name + ': garbled text encoding')
        for link in re.findall(r'\]\(([^)]+)\)', text):
            url = urlsplit(link)
            if url.scheme or url.netloc:
                continue
            target = posixpath.normpath(posixpath.join(posixpath.dirname(name), unquote(url.path))) if url.path else name
            if target not in files:
                errors.append(f'{name}: missing target {link}')
            elif url.fragment and target.endswith('.md') and unquote(url.fragment) not in headings(documents.get(target, '')):
                errors.append(f'{name}: missing section {link}')
    return errors


def check_source(root):
    required = COMMON | WRAPPERS | {f'Firmware/tools/{n}' for n in TOOLS} | set(SOURCE_DOCS) | {
        '.gitignore', '.gitattributes', '.github/workflows/firmware.yml', 'Firmware/UF2/precompiled.py', 'Firmware/Alpha/build.py', 'Firmware/Alpha/prepare.py',
        'Firmware/Alpha/Src/ads131_board.c', 'Firmware/Alpha/Src/clock_start.c', 'Firmware/UF2/build.py', 'Firmware/UF2/release.py',
        'Firmware/Base/STM32H743ZI_ads131_80MHz_makefile.tar.gz',
        'Firmware/Base/licenses/ST-HAL.md', 'Firmware/Base/licenses/CMSIS.txt',
        'Firmware/Legacy/STM32CubeIDE/workspace_1.5.1/STM32H743ZI_ads131_80MHz/Src/main.c',
        'AssemblyFiles/KiCAD/FreeEEG128-alpha.kicad_pcb',
        'SetupGuides/images/freeeeg128-swd.png', 'SetupGuides/images/freeeeg128-swd.svg'}
    errors = ['Missing source file: ' + n for n in sorted(required) if not (root / n).is_file()]
    for old in ('KiCAD', 'STM32CubeIDE', 'datasheets', 'IMG20211127130429.jpg',
                'STM32H743ZI_ads131_80MHz_makefile.tar.gz'):
        if (root / old).exists():
            errors.append('Old root location still present: ' + old)
    paths = [root / n for n in SOURCE_DOCS if (root / n).is_file()]
    for folder in ('Flash', 'SetupGuides', 'Firmware/tools'):
        paths.extend((root / folder).rglob('*.md'))
    documents = {p.relative_to(root).as_posix(): p.read_text(encoding='utf-8') for p in paths}
    targets = set(documents)
    for name, text in documents.items():
        for link in re.findall(r'\]\(([^)]+)\)', text):
            url = urlsplit(link)
            if url.scheme or url.netloc:
                continue
            target = posixpath.normpath(posixpath.join(posixpath.dirname(name), unquote(url.path))) if url.path else name
            if (root / target).is_file():
                targets.add(target)
    return errors + check_documents(targets, documents)


def check_release(path):
    if path.is_dir():
        files = {p.relative_to(path).as_posix() for p in path.rglob('*') if p.is_file()}
        read = lambda name: (path / name).read_bytes()
        close = lambda: None
    else:
        archive = zipfile.ZipFile(path)
        files = set(archive.namelist())
        read, close = archive.read, archive.close
        if len(files) != len(archive.namelist()):
            close()
            return ['Release ZIP contains duplicate paths']
    try:
        required = COMMON | WRAPPERS | IMAGES | {f'tools/{n}' for n in TOOLS} | {
            'CONTROL.md', 'WIRING.md', 'VALIDATION.md', 'UPSTREAM.md', 'manifest.json', 'flash.py', 'flash.cmd',
            'device.py', 'control.py', 'board_tools.py', 'freeeeg128-swd.svg', 'freeeeg128-swd.png',
            'st-link-v2-reference.jpg', 'licenses/ST-HAL.md', 'licenses/CMSIS.txt',
            'licenses/ST-USB-MSC.md', 'licenses/ST-USB-MSC-UPSTREAM.txt'}
        errors = ['Missing release file: ' + n for n in sorted(required - files)]
        if any(n.startswith('/') or '..' in n.split('/') or '\\' in n for n in files):
            errors.append('Release contains a nonportable or unsafe path')
        documents = {n: read(n).decode('utf-8') for n in files if n.endswith('.md') and not n.startswith('licenses/')}
        errors += check_documents(files, documents)
        if 'manifest.json' in files:
            manifest = json.loads(read('manifest.json'))
            if not manifest.get('board', '').startswith('FreeEEG128-alpha ') or set(manifest.get('files', {})) != IMAGES:
                errors.append('Unexpected release board or image set')
            for name in IMAGES & files & set(manifest.get('files', {})):
                data, meta = read(name), manifest['files'][name]
                if len(data) != meta['bytes'] or hashlib.sha256(data).hexdigest() != meta['sha256']:
                    errors.append('Image does not match manifest: ' + name)
        return errors
    finally:
        close()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--release', type=Path, help='Check a release ZIP or extracted directory')
    args = parser.parse_args()
    errors = check_release(args.release.resolve()) if args.release else check_source(Path(__file__).resolve().parents[2])
    if errors:
        for error in errors:
            print(error)
        raise SystemExit(1)
    print('Release layout passed.' if args.release else 'Repository layout passed.')


if __name__ == '__main__':
    main()
