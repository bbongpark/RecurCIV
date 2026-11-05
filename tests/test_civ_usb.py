"""Compare actual USB-posted report pairs with the independent Phase 1 oracle."""
from hashlib import sha256
from pathlib import Path
import re
import subprocess
from test_civ_runner import SHA, compare, expected

ROOT = Path(__file__).resolve().parent.parent
payload = (ROOT / 'payload.txt').read_bytes()
assert sha256(payload).hexdigest() == SHA
process = subprocess.run([str(ROOT / 'build' / 'test_civ_usb.exe'), '--trace'],
                         cwd=ROOT, check=True, capture_output=True, text=True)
stream = [tuple(map(int, line.split())) for line in process.stdout.splitlines()]
count = compare(iter(stream), expected(payload.decode('ascii').splitlines()))
assert count == 23688

# Structural production-main regression checks, in addition to executable USB
# tests. Strip comments/strings, retaining braces and offsets for scope checks.
main = (ROOT / 'source' / 'main.c').read_text()
code = re.sub(r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"',
              lambda m: ' ' * len(m[0]), main, flags=re.S)


def scope(marker):
    start = code.index('{', code.index(marker))
    depth = 1
    end = start + 1
    while depth:
        depth += (code[end] == '{') - (code[end] == '}')
        end += 1
    return start, end


def inside(span, text):
    start, end = span
    assert text in code[start:end], text


guard = scope('if(!civ_usb_owned() && view.screen!=SCREEN_CIV_PLACEHOLDER)')
for writer in ('caps_toggle_tick(now)', 'sending_tick(now)', 'live_step(',
               'compose_open()', 'gamepad_test_state(SWITCH_GAMEPAD_A)',
               'controller_route_tick(', 'perform(action,now)',
               'keyboard_idle_due(now)', 'keyboard_send(0,0,now)'):
    inside(guard, writer)
assert code.index('keyboard_poll(now);gamepad_poll(now);civ_usb_tick(now);') < guard[0]
assert code.index('framebufferBegin(', guard[1]) > guard[1]
assert 'svcSleepThread' not in code[guard[0]:guard[1]]
inside(scope('static void perform('), 'if(civ_usb_owned()) return;')
eligibility = scope('static bool civ_can_start(')
for condition in ('sender.phase==TX_NONE', 'caps_toggle.phase==TX_NONE',
                  '!compose_requested', '!compose_input_lock', '!quit_requested',
                  '!gamepad_button_test.active', 'view.screen!=SCREEN_LIVE',
                  'view.screen!=SCREEN_CONTROLLER', 'view.screen!=SCREEN_DIAGNOSTICS'):
    inside(eligibility, condition)
assert 'if(civ_usb_owned()) civ_usb_shutdown();' in code
inside(scope('static void perform('), 'civ_route_action(&civ_route,&view,action,now,civ_can_start())')
assert 'civ_route_input(&civ_route,&view,' in code[guard[1]:]
for path in (ROOT / 'source').glob('*.c'):
    if path.name in ('civ_usb.c', 'civ_route.c'):
        continue
    source = re.sub(r'/\*.*?\*/|//[^\n]*', '', path.read_text(), flags=re.S)
    assert not re.search(r'\bciv_usb_start\s*\(', source), path
route = (ROOT / 'source' / 'civ_route.c').read_text()
assert route.count('civ_usb_start()') == 1
assert 'action==ACTION_CIV_START && !v->civ.requested' in route
print(f'PASS: {count} actual USB report pairs match independent oracle; production ownership guards and explicit-only UI activation verified.')
