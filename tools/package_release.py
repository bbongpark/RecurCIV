"""Package/validate the committed source and matching NRO; never publish or replace archives."""
import argparse
import ast
import hashlib
import io
import json
from pathlib import Path, PurePosixPath
import subprocess
import time
import zipfile

ROOT = Path(__file__).resolve().parent.parent
MANIFEST = 'RELEASE-MANIFEST.json'
PAYLOAD_SHA256 = '6ea468ca392558ab2f8f2384200745a8619187cebc5357d5e985fd3ffdada37d'


def git(*args):
    return subprocess.run(['git', *args], cwd=ROOT, check=True,
                          stdout=subprocess.PIPE, stderr=subprocess.PIPE).stdout


def sha(data):
    return hashlib.sha256(data).hexdigest()


def safe_name(name):
    path = PurePosixPath(name)
    if not name or path.is_absolute() or '..' in path.parts or '\\' in name or ':' in name:
        raise ValueError(f'Unsafe ZIP path: {name}')


def zip_files(data):
    with zipfile.ZipFile(io.BytesIO(data)) as archive:
        if archive.testzip() is not None:
            raise ValueError('ZIP CRC failure')
        names = archive.namelist()
        if len(names) != len(set(names)):
            raise ValueError('Duplicate ZIP entries')
        for entry in archive.infolist():
            safe_name(entry.orig_filename)
            safe_name(entry.filename)
            if (entry.external_attr >> 16) & 0o170000 == 0o120000:
                raise ValueError(f'Symlink in ZIP: {entry.filename}')
        return {entry.filename: archive.read(entry) for entry in archive.infolist() if not entry.is_dir()}


def source_inputs():
    if git('status', '--porcelain', '--untracked-files=normal').strip():
        raise ValueError('Commit all intended changes first; packaging requires a clean source tree')
    files = zip_files(git('archive', '--format=zip', 'HEAD'))
    tracked = set(git('ls-files', '-z').decode().strip('\0').split('\0'))
    if set(files) != tracked:
        raise ValueError('git archive omits tracked corresponding source')
    bad_suffixes = {'.zip', '.log', '.o', '.obj', '.exe', '.elf', '.nro', '.nacp', '.map', '.pdb', '.pyc', '.ppm'}
    for name in files:
        path = PurePosixPath(name)
        if ('node_modules' in path.parts or '__pycache__' in path.parts or
                path.parts[0] in {'.git', 'build', 'release', 'release-ready'} or path.suffix in bad_suffixes):
            raise ValueError(f'Generated/private file tracked in source: {name}')
        if (path.name.startswith('.env') and path.name != '.env.example') or path.suffix in {'.pem', '.key'}:
            raise ValueError(f'Potential credential file tracked in source: {name}')
    version = next(ast.literal_eval(node.value) for node in ast.parse(files['build.py'])
                   .body if isinstance(node, ast.Assign) and
                   any(isinstance(target, ast.Name) and target.id == 'VERSION' for target in node.targets))
    if not isinstance(version, str) or len(version.split('.')) != 3 or not all(part.isdigit() for part in version.split('.')):
        raise ValueError('Invalid build VERSION')
    for item in json.loads(files['upstream/SOURCES.json'])['files']:
        data = files[item['path']]
        if len(data) != item['bytes'] or sha(data) != item['sha256']:
            raise ValueError(f'Upstream source/notice mismatch: {item["path"]}')
    nro = (ROOT / 'build/RecurCIV.nro').read_bytes()
    nacp = (ROOT / 'build/RecurCIV.nacp').read_bytes()
    if nro[0x10:0x14] != b'NRO0' or nro.count(nacp) != 1:
        raise ValueError('Invalid NRO or missing NACP')
    if nacp[0x3060:0x3070].split(b'\0', 1)[0].decode() != version:
        raise ValueError('Built NRO version does not match committed source')
    if sha(files['payload.txt']) != PAYLOAD_SHA256 or nro.count(files['payload.txt']) != 1:
        raise ValueError('Approved payload mismatch')
    for text in (b'Capture short press: Start/Stop', b'Holds are timed from state submission'):
        if text not in nro:
            raise ValueError(f'NRO is missing current implementation marker: {text!r}')
    link_map = (ROOT / 'build/RecurCIV.map').read_text()
    for symbol in ('capture_control_update', 'macro_app_capture', 'macro_live_record_start',
                   'appletSetRequiresCaptureButtonShortPressedMessage', 'macro_play_tick'):
        if symbol not in link_map:
            raise ValueError(f'Missing linked implementation: {symbol}')
    commit = git('rev-parse', 'HEAD').decode().strip()
    manifest = {
        'version': version, 'commit': commit,
        'tree': git('show', '-s', '--format=%T', 'HEAD').decode().strip(),
        'repository': 'https://github.com/OR3OTHUG/RecurCIV',
        'nro': {'bytes': len(nro), 'sha256': sha(nro)},
        'source_files': {name: sha(data) for name, data in sorted(files.items())},
    }
    return version, files, nro, (json.dumps(manifest, indent=2, sort_keys=True) + '\n').encode()


def install_files(version, files, nro, manifest):
    notices = {name: data for name, data in files.items() if name.startswith(('licences/', 'docs/'))}
    for name in ('COPYING', 'LICENSE', 'LICENSING.md', 'CREDITS.md', 'README.md', 'CHANGELOG.md'):
        notices[name] = files[name]
    for name in ('assets/Manrope-OFL.txt', 'assets/Outfit-OFL.txt', 'third_party/libnx-LICENSE'):
        notices['licences/' + PurePosixPath(name).name] = files[name]
    notices['RecurCIV.nro'] = nro
    notices[MANIFEST] = manifest
    commit = json.loads(manifest)['commit']
    notices['SOURCE.txt'] = (
        f'RecurCIV v{version} - complete corresponding source\n\n'
        f'Commit: {commit}\n'
        f'https://github.com/OR3OTHUG/RecurCIV/tree/{commit}\n'
        f'https://github.com/OR3OTHUG/RecurCIV/releases/download/v{version}/RecurCIV-v{version}-source.zip\n\n'
        'The corresponding source ZIP is supplied beside the install ZIP at no additional charge.\n'
        'The combined distribution is GPL-3.0-only; retain COPYING and all original notices.\n'
    ).encode()
    return {'switch/RecurCIV/' + name: data for name, data in notices.items()}


def write_zip(path, files):
    stamp = time.gmtime(max(int(git('show', '-s', '--format=%ct', 'HEAD')), 315532800))[:6]
    with zipfile.ZipFile(path, 'x', compression=zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
        for name, data in sorted(files.items()):
            safe_name(name)
            entry = zipfile.ZipInfo(name, stamp)
            entry.external_attr = 0o100644 << 16
            entry.compress_type = zipfile.ZIP_DEFLATED
            archive.writestr(entry, data)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--verify-only', action='store_true')
    parser.add_argument('--output', type=Path, default=ROOT / 'release-ready')
    args = parser.parse_args()
    version, source, nro, manifest = source_inputs()
    prefix = f'RecurCIV-v{version}/'
    source_zip = args.output / f'RecurCIV-v{version}-source.zip'
    install_zip = args.output / f'RecurCIV-v{version}.zip'
    expected_source = {prefix + name: data for name, data in source.items()}
    expected_source[prefix + MANIFEST] = manifest
    expected_install = install_files(version, source, nro, manifest)
    if not args.verify_only:
        if source_zip.exists() or install_zip.exists():
            raise ValueError('Refusing to replace an existing release archive; use --verify-only')
        args.output.mkdir(parents=True, exist_ok=True)
        write_zip(source_zip, expected_source)
        write_zip(install_zip, expected_install)
    for path, expected in ((source_zip, expected_source), (install_zip, expected_install)):
        actual = zip_files(path.read_bytes())
        if actual != expected:
            raise ValueError(f'Archive differs from committed source/release build: {path}')
        print(f'PASS: {path.name}: {len(actual)} files, {path.stat().st_size} bytes, SHA-256 {sha(path.read_bytes())}')
    print(f'PASS: v{version}, commit {json.loads(manifest)["commit"]}, NRO {len(nro)} bytes, SHA-256 {sha(nro)}')


if __name__ == '__main__':
    main()
