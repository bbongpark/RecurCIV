"""Check the Windows runner rejects negative assertion/exception exit codes."""
from pathlib import Path
import os
import subprocess
import tempfile

runner = Path(__file__).with_name('run-tests.cmd').read_text()
lines = runner.splitlines()
check = 'if not "%errorlevel%"=="0" exit /b 1'
assert 'if errorlevel 1' not in runner
for i, line in enumerate(lines):
    if line.startswith('cl ') or (line.endswith('.exe') and not line.startswith('if ')):
        assert lines[i + 1] == check, line

if os.name == 'nt':
    base = Path(os.environ['LOCALAPPDATA']) / 'Temp' / 'opencode'
    with tempfile.TemporaryDirectory(prefix='recurciv-runner-', dir=base) as directory:
        script = Path(directory) / 'check.cmd'
        for code in (0, 1, -1073740791):
            script.write_text(f'@echo off\ncmd /c exit {code}\n{check}\nexit /b 0\n')
            result = subprocess.run(['cmd.exe', '/c', str(script)], check=False)
            assert result.returncode == (0 if code == 0 else 1), (code, result.returncode)
print('PASS: test runner checks every compile/run and rejects negative failure codes.')
