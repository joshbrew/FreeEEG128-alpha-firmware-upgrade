"""Create the offline release bundle with links relative to its own layout."""
import hashlib
import importlib.util
import json
import posixpath
import re
from urllib.parse import unquote, urlsplit
import zipfile

IMAGES = ('freeeeg128-factory.hex', 'freeeeg128-application.uf2',
          'freeeeg128-application.hex', 'freeeeg128-bootloader.hex')


def relocate_markdown(text, source, destinations):
    def link(match):
        url = urlsplit(match[1])
        if url.scheme or url.netloc:
            return match[0]
        target = posixpath.normpath(posixpath.join(posixpath.dirname(source), unquote(url.path))) if url.path else source
        if target not in destinations:
            raise ValueError(f'Unpackaged documentation target: {source}: {match[1]}')
        fragment = url.fragment
        if target == 'README.md' and fragment == 'swd-programming':
            fragment = 'wiring-and-entry'
        relative = posixpath.relpath(destinations[target], posixpath.dirname(destinations[source]) or '.')
        if target == source:
            relative = '' if fragment else posixpath.basename(destinations[source])
        return '](' + relative + ('#' + fragment if fragment else '') + ')'
    return re.sub(r'\]\(([^)]+)\)', link, text)


def create_release(out, root):
    """Package existing verified build outputs; does not compile or flash."""
    firmware = root / 'Firmware'
    spec = importlib.util.spec_from_file_location('freeeeg128_layout', firmware / 'tools/check_layout.py')
    layout = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(layout)
    problems = layout.check_source(root)
    if problems:
        raise ValueError('Repository layout check failed: ' + '; '.join(problems))
    manifest = json.loads((out / 'manifest.json').read_text(encoding='utf-8'))
    if set(manifest['files']) != set(IMAGES) or not manifest['board'].startswith('FreeEEG128-alpha '):
        raise ValueError('Unexpected board or image set in release manifest')
    for name in IMAGES:
        data = (out / name).read_bytes()
        if len(data) != manifest['files'][name]['bytes'] or hashlib.sha256(data).hexdigest() != manifest['files'][name]['sha256']:
            raise ValueError('Build output does not match manifest: ' + name)

    sources = {
        'README.md': 'README.md',  # Incoming root-guide links resolve to the bundled UF2 guide.
        'Firmware/UF2/README.md': 'README.md',
        'Firmware/Alpha/README.md': 'CONTROL.md',
        'Firmware/VALIDATION.md': 'VALIDATION.md',
        'Firmware/UPSTREAM.md': 'UPSTREAM.md',
        'SetupGuides/FreeEEG128_Programming_and_Updates.md': 'WIRING.md',
        'Firmware/tools/README.md': 'tools/README.md',
    }
    for name in ('freeeeg128-swd.svg', 'freeeeg128-swd.png', 'st-link-v2-reference.jpg'):
        sources['SetupGuides/images/' + name] = name
    for p in (root / 'Flash').rglob('*'):
        if p.is_file() and p.suffix in ('.md', '.cmd', '.sh'):
            relative = p.relative_to(root).as_posix()
            sources[relative] = relative

    destination = out / 'freeeeg128-uf2-release.zip'
    with zipfile.ZipFile(destination, 'w', zipfile.ZIP_DEFLATED) as archive:
        for name in (*IMAGES, 'manifest.json'):
            archive.write(out / name, name)
        for name in ('flash.py', 'flash.cmd', 'control.py', 'device.py', 'board_tools.py'):
            archive.write(firmware / 'Alpha' / name, name)
        for name in ('FreeEEG128.cmd', 'FreeEEG128.sh', 'LICENSE'):
            archive.write(root / name, name)
        for source, target in sources.items():
            if source == 'README.md' or source == 'Firmware/tools/README.md':
                continue
            path = root / source
            if path.suffix == '.md':
                text = relocate_markdown(path.read_text(encoding='utf-8'), source, sources)
                if source == 'Firmware/UF2/README.md':
                    text += '\n![FreeEEG128 SWD connections](freeeeg128-swd.svg)\n'
                archive.writestr(target, text)
            else:
                archive.write(path, target)
        for path in (firmware / 'tools').iterdir():
            if path.is_file() and path.suffix in ('.py', '.ps1', '.sh', '.cfg', '.txt', '.md'):
                name = 'tools/' + path.name
                if path.suffix == '.md':
                    archive.writestr(name, relocate_markdown(path.read_text(encoding='utf-8'),
                                                            path.relative_to(root).as_posix(), sources))
                else:
                    archive.write(path, name)
        for source, name in ((firmware / 'UF2/vendor/License.md', 'ST-USB-MSC.md'),
                             (firmware / 'UF2/vendor/UPSTREAM.txt', 'ST-USB-MSC-UPSTREAM.txt'),
                             (firmware / 'Base/licenses/ST-HAL.md', 'ST-HAL.md'),
                             (firmware / 'Base/licenses/CMSIS.txt', 'CMSIS.txt')):
            archive.write(source, 'licenses/' + name)
    problems = layout.check_release(destination)
    if problems:
        raise ValueError('Release layout check failed: ' + '; '.join(problems))
    return destination
