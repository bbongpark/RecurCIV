"""Exercise archive safety/CRC checks without publishing or touching release assets."""
import io
from pathlib import Path
import runpy
import warnings
import zipfile

root = Path(__file__).resolve().parent.parent
package = runpy.run_path(str(root / 'tools/package_release.py'), run_name='package_test')


def archive(entries):
    data = io.BytesIO()
    with warnings.catch_warnings():
        warnings.simplefilter('ignore', UserWarning)
        with zipfile.ZipFile(data, 'w', compression=zipfile.ZIP_STORED) as target:
            for name, contents in entries:
                target.writestr(name, contents)
    return data.getvalue()


def rejected(data):
    try:
        package['zip_files'](data)
    except (ValueError, zipfile.BadZipFile):
        return
    raise AssertionError('Unsafe or corrupt archive accepted')


assert package['zip_files'](archive([('switch/RecurCIV/RecurCIV.nro', b'nro'), ('COPYING', b'notice')])) == {
    'switch/RecurCIV/RecurCIV.nro': b'nro', 'COPYING': b'notice'}
for name in ('../outside', '/absolute', 'C:/absolute', 'folder/../../outside'):
    rejected(archive([(name, b'bad')]))
# ZipInfo normalizes backslashes when writing on Windows; inject the raw name
# into both ZIP headers so the reader's original-name validation is exercised.
rejected(archive([('folder/outside', b'bad')]).replace(b'folder/outside', b'folder\\outside'))
rejected(archive([('duplicate', b'a'), ('duplicate', b'b')]))
link = zipfile.ZipInfo('symlink')
link.external_attr = 0o120777 << 16
rejected(archive([(link, b'../outside')]))
corrupt = bytearray(archive([('entry', b'unique-payload-for-crc-check')]))
corrupt[corrupt.index(b'unique-payload-for-crc-check')] ^= 1
rejected(bytes(corrupt))
print('PASS: release ZIP path safety, duplicate/symlink rejection and CRC integrity checks.')
