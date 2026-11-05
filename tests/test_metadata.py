"""Check the built NRO embeds the intended icon and NACP metadata."""
from pathlib import Path

root = Path(__file__).resolve().parent.parent
icon = (root / 'assets' / 'recurciv-icon.jpg').read_bytes()
nacp = (root / 'build' / 'RecurCIV.nacp').read_bytes()
nro = (root / 'build' / 'RecurCIV.nro').read_bytes()
assert icon.startswith(b'\xff\xd8') and icon.endswith(b'\xff\xd9')
assert nro.count(icon) == 1, 'custom icon is not embedded exactly once'
assert nro.count(nacp) == 1, 'generated NACP is not embedded exactly once'
assert nacp[:0x200].split(b'\0', 1)[0] == b'RecurCIV'
assert nacp[0x200:0x300].split(b'\0', 1)[0] == b'msama'
print('PASS: custom JPEG and RecurCIV NACP are embedded in the NRO.')
