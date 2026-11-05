"""Check the approved payload and its byte-for-byte presence in the NRO."""
from hashlib import sha256
from pathlib import Path

root = Path(__file__).resolve().parent.parent
payload = (root / 'payload.txt').read_bytes()
nro = (root / 'build' / 'RecurCIV.nro').read_bytes()
assert len(payload) == 21762
assert sha256(payload).hexdigest() == '6ea468ca392558ab2f8f2384200745a8619187cebc5357d5e985fd3ffdada37d'
assert sum(bool(line.strip()) for line in payload.splitlines()) == 87
assert nro.count(payload) == 1
print('PASS: approved payload integrity, line count, and byte-for-byte NRO embedding.')
