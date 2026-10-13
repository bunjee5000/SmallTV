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
import struct
import zlib
import ctypes
import datetime
import json
import math

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
run(base + ['-DDASHBOARD_TEST_TIMEZONES', '-Itests/support', json_include, gfx_include, 'tests/test_dashboard_render.cpp',
            'src/Dashboard.cpp', 'src/DashboardMath.cpp', '-o', render])
run([render, out / 'dashboard-task4.ppm'])
moon_render=out / ('moon-render'+exe)
run(base + ['-Itests/support', json_include, gfx_include, 'tests/test_dashboard_moon_render.cpp',
            'src/DashboardMath.cpp', '-o', moon_render])
run([moon_render, out / 'moon-task4'])
# Export every representative production-renderer fixture, without dependencies.
def chunk(kind, data):
    return struct.pack('!I', len(data)) + kind + data + struct.pack('!I', zlib.crc32(kind + data))
def save_png(path,pixels,width=240,height=240):
    assert len(pixels)==width*height*3
    stride=width*3
    rows=b''.join(b'\0'+pixels[y*stride:(y+1)*stride] for y in range(height))
    path.write_bytes(b'\x89PNG\r\n\x1a\n' +
        chunk(b'IHDR', struct.pack('!IIBBBBB', width,height,8,2,0,0,0)) +
        chunk(b'IDAT', zlib.compress(rows)) + chunk(b'IEND', b''))
for ppm in list(out.glob('dashboard-task4*.ppm'))+list(out.glob('moon-task4*.ppm')):
    header, dimensions, depth, pixels = ppm.read_bytes().split(b'\n', 3)
    assert header == b'P6' and dimensions == b'240 240' and depth == b'255'
    assert len(pixels) == 240 * 240 * 3
    save_png(ppm.with_suffix('.png'),pixels)
# 4x2 contact sheet, retaining native-resolution production icons and labels.
# Crop each phase's moon column and add padding, without rescaling its pixels.
sheet=bytearray(4*120*2*100*3)
for phase in range(8):
    pixels=(out / f'moon-task4.phase-{phase}.ppm').read_bytes().split(b'\n',3)[3]
    for y in range(90):
        source=((85+y)*240+120)*3
        target=(((phase//4)*100+y)*480+(phase%4)*120)*3
        sheet[target:target+360]=pixels[source:source+360]
save_png(out / 'moon-task4-comparison.png',sheet,480,200)
class Moon(ctypes.Structure):
    _fields_=[('index',ctypes.c_uint8),('cycle',ctypes.c_float)]
moon_math=ctypes.CDLL(str(lib.resolve()))
moon_math.dashboardMoon.argtypes=[ctypes.c_int64]
moon_math.dashboardMoon.restype=Moon
names=['Neumond','Zun. Sichel','Erstes Viertel','Zun. Mond','Vollmond','Abn. Mond','Letztes Viertel','Abn. Sichel']
report=['# Moon astronomical validation', '',
    'USNO daily references specify illumination at local noon and round it to whole percent.',
    'The dashboard reserves +/-3.6 degrees around new/quarter/full for primary labels; remaining angles describe crescents/gibbous phases.',
    'USNO names primary events at their exact instants; near-quarter references may say crescent/gibbous while the dashboard says Viertel.',
    'The icon retains continuous illumination and changes waxing/waning side at actual conjunction/opposition, independently of the label window.', '',
    '| Local date/noon | UTC offset | USNO phase | USNO lit | Dashboard label | Calculated lit |',
    '|---|---:|---|---:|---|---:|']
references=json.loads((ROOT / 'tests/fixtures/usno_moon_illumination_2026.json').read_text())
for ref in references['data']:
    moon=moon_math.dashboardMoon(ref['epoch'])
    fraction=(1-math.cos(moon.cycle*2*math.pi))/2
    report.append(f"| [{ref['date']}]({ref['source']}) | {ref['utc_offset']:+g} | {ref['phase']} | {ref['illumination']*100:.0f}% | {names[moon.index]} | {fraction*100:.3f}% |")
report+=['','## October primary events (USNO UTC)', '',
    '| UTC instant | Expected phase | Expected illumination | Calculated illumination |',
    '|---|---|---:|---:|']
phases=json.loads((ROOT / 'tests/fixtures/usno_moon_2026.json').read_text())
for ref in phases['data']:
    if ref['month']!=10: continue
    instant=datetime.datetime.fromisoformat(f"2026-10-{ref['day']:02d}T{ref['time']}").replace(tzinfo=datetime.timezone.utc)
    moon=moon_math.dashboardMoon(int(instant.timestamp()))
    expected={'New Moon':0,'Full Moon':100,'First Quarter':50,'Last Quarter':50}[ref['phase']]
    fraction=(1-math.cos(moon.cycle*2*math.pi))/2
    report.append(f"| {instant.isoformat()} | {ref['phase']} | {expected}% | {fraction*100:.3f}% |")
report+=['','## Dashboard preview timestamps (Europe/Berlin, CEST)', '',
    '| Local instant | German phase | Calculated illumination |','|---|---|---:|']
for day in [12,13]:
    instant=datetime.datetime(2026,10,day,16,37,tzinfo=datetime.timezone(datetime.timedelta(hours=2)))
    moon=moon_math.dashboardMoon(int(instant.timestamp()))
    report.append(f'| {instant.isoformat()} | {names[moon.index]} | {(1-math.cos(moon.cycle*2*math.pi))*50:.3f}% |')
(out / 'moon-validation.md').write_text('\n'.join(report)+'\n',encoding='utf-8')
print('240 x 240 dashboard PNG previews:', out)
run(['node', 'tests/test_dashboard_ui.js'])
header = (ROOT / 'src/webui.h').read_text()
embedded = bytes(int(v, 16) for v in re.findall(r'0x([0-9a-fA-F]{2})', header))
assert gzip.decompress(embedded) == (ROOT / 'src/webui.html').read_bytes()
print('Embedded web UI matches HTML source byte for byte: PASS')
