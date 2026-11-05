"""Independent HID-stream oracle, payload integrity, optional original-generator comparison."""
from hashlib import sha256
from itertools import zip_longest
from pathlib import Path
import argparse
import re
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parent.parent
SHA = '6ea468ca392558ab2f8f2384200745a8619187cebc5357d5e985fd3ffdada37d'


def mapping():
    result = {' ': (0, 44), '\t': (0, 43)}
    for i, c in enumerate('abcdefghijklmnopqrstuvwxyz', 4):
        result[c] = (0, i)
        result[c.upper()] = (2, i)
    for c, key in zip('1234567890', range(30, 40)):
        result[c] = (0, key)
    for plain, shifted, key in zip("-=[]\\;'`,./", '_+{}|:"~<>?', (45,46,47,48,49,51,52,53,54,55,56)):
        result[plain] = (0, key)
        result[shifted] = (2, key)
    for c, key in zip('!@#$%^&*()', range(30, 40)):
        result[c] = (2, key)
    return result


def expected(lines):
    keys = mapping()
    yield (1000, 0, 0, 0)
    for _ in range(5):
        yield (50, 0, 0, 4)
        yield (550, 0, 0, 0)
    for index, line in enumerate(lines):
        yield (32, 0, 41, 0)
        yield (750, 0, 0, 0)
        yield (50, 0, 0, 4)
        yield (550, 0, 0, 0)
        previous = None
        for c in line:
            modifier, key = keys[c]
            if previous == key:
                yield (32, 0, 0, 0)
            yield (32, modifier, key, 0)
            previous = key
        yield (32, 0, 40, 0)
        yield (200, 0, 0, 0)
        yield (32, 0, 40, 0)
        if index + 1 < len(lines):
            yield (750, 0, 0, 0)
            yield (50, 0, 0, 4)
            yield (550, 0, 0, 0)
    yield (0, 0, 0, 0)


def compare(first, second):
    count = 0
    for count, (a, b) in enumerate(zip_longest(first, second), 1):
        assert a == b, f'action {count}: {a} != {b}'
    return count


def original(reference, payload):
    """Generate only a TEMPORARY macro header; neither reference nor payload is modified."""
    generator = reference / 'tools' / 'payload_to_macro.py'
    header = (reference / 'include' / 'MacroEngine.h').read_text()
    assert re.search(r'#define MACRO_START_DELAY_MS\s+1000\b', header)
    with tempfile.TemporaryDirectory(prefix='recurciv-civ-oracle-') as temp:
        output = Path(temp) / 'macro_script.h'
        subprocess.run([sys.executable, str(generator), str(payload), str(output)], check=True, capture_output=True)
        script = output.read_text().splitlines()
    names = {'HID_KEY_' + c.upper(): value[1] for c, value in mapping().items() if c.isascii() and c.isalnum()}
    names.update(dict(zip(('SPACE','TAB','MINUS','EQUAL','BRACKET_LEFT','BRACKET_RIGHT','BACKSLASH','SEMICOLON','APOSTROPHE','GRAVE','COMMA','PERIOD','SLASH','ESCAPE','ENTER'),(44,43,45,46,47,48,49,51,52,53,54,55,56,41,40))))
    names.update({'HID_KEY_' + k: v for k, v in list(names.items()) if not k.startswith('HID_KEY_')})
    yield (1000, 0, 0, 0)
    for line in script:
        if match := re.fullmatch(r'\s*WAIT\((\d+)\),', line):
            yield (int(match[1]), 0, 0, 0)
        elif match := re.fullmatch(r'\s*PAD_BTN\((\d+), SWITCH_USB_MASK_A\),', line):
            yield (int(match[1]), 0, 0, 4)
        elif match := re.fullmatch(r'\s*KEYS\((\d+), (0|KEYBOARD_MODIFIER_LEFTSHIFT), (HID_KEY_\w+), 0, 0, 0, 0, 0\),', line):
            yield (int(match[1]), 0 if match[2] == '0' else 2, names[match[3]], 0)
        elif line.strip() == 'END(),':
            yield (0, 0, 0, 0)
        elif line.strip().startswith(('KEYS(', 'PAD_', 'WAIT(', 'BOTH(', 'KEY_TIMES(')):
            raise AssertionError('unrecognized reference step: ' + line)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--reference', type=Path, help='Optional PicoSwitch directory; uses only an external temporary output')
    args = parser.parse_args()
    path = ROOT / 'payload.txt'
    raw = path.read_bytes()
    assert len(raw) == 21762 and sha256(raw).hexdigest() == SHA
    lines = raw.decode('ascii').splitlines()
    assert len(lines) == 87 and all(0 < len(line) <= 253 for line in lines)
    process = subprocess.run([str(ROOT / 'build' / 'test_civ_runner.exe'), '--trace'], cwd=ROOT, check=True, capture_output=True, text=True)
    stream = [tuple(map(int, line.split())) for line in process.stdout.splitlines()]
    count = compare(iter(stream), expected(lines))
    assert sum(s[2] == 41 for s in stream) == 87
    assert sum(s[2] == 40 for s in stream) == 174
    assert sum(s[3] == 4 for s in stream) == 178
    assert sum(s[0] for s in stream) == 993190
    if args.reference:
        assert (args.reference / 'payload.txt').read_bytes() == raw
        compare(iter(stream), original(args.reference, path))
        print('PASS: complete runner stream matches original PicoSwitch generator (plus startup/final neutral).')
    print(f'PASS: approved SHA-256, 87 exact lines and {count} ordered full-state actions match independent oracle.')


if __name__ == '__main__':
    main()
