"""Run native dashboard tests with GCC/Clang and Node; no device/network needed.

python tools/test_dashboard.py --compiler path/to/g++
PlatformIO dependencies must already be installed by the lean build.
"""
import argparse
import gzip
import os
from pathlib import Path
import re
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser()
parser.add_argument('--compiler', default='g++')
args = parser.parse_args()
os.chdir(ROOT)
out = ROOT / '.pio' / 'dashboard-tests'
out.mkdir(parents=True, exist_ok=True)
windows = os.name == 'nt'
exe = '.exe' if windows else ''
lib = out / ('math.dll' if windows else 'math.so')
base = [args.compiler, '-std=c++11', '-O2', '-Wall', '-Wextra', '-Isrc']
deps = ROOT / '.pio/libdeps/smalltv_lean'
json_include = '-I' + str(deps / 'ArduinoJson/src')
gfx_include = '-I' + str(deps / 'GFX Library for Arduino/src')
def run(command):
    subprocess.run([str(c) for c in command], check=True)
run(base + ['-shared'] + (['-static'] if windows else ['-fPIC']) + ['src/DashboardMath.cpp', '-o', lib])
run([sys.executable, 'tests/test_dashboard_math.py', lib])
weather = out / ('weather' + exe)
run(base + [json_include, 'tests/test_dashboard_weather.cpp', 'src/DashboardWeatherValue.cpp', '-o', weather])
run([weather])
render = out / ('render' + exe)
run(base + ['-Itests/support', json_include, gfx_include, 'tests/test_dashboard_render.cpp',
            'src/Dashboard.cpp', 'src/DashboardMath.cpp', '-o', render])
run([render, out / 'preview.ppm'])
run(['node', 'tests/test_dashboard_ui.js'])
header = (ROOT / 'src/webui.h').read_text()
embedded = bytes(int(v, 16) for v in re.findall(r'0x([0-9a-fA-F]{2})', header))
assert gzip.decompress(embedded) == (ROOT / 'src/webui.html').read_bytes()
print('Embedded web UI matches HTML source byte for byte: PASS')
