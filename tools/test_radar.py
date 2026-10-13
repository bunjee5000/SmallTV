"""Build and run native tests for the production Radar parser and route labels."""
import argparse
from pathlib import Path
import subprocess
import os
import struct
import zlib

parser = argparse.ArgumentParser()
parser.add_argument('--compiler', default='g++')
args = parser.parse_args()
root = Path(__file__).resolve().parents[1]
os.chdir(root)
out = root / '.pio' / 'radar-tests'
out.mkdir(parents=True, exist_ok=True)
base = [args.compiler, '-std=c++11', '-O2', '-Wall', '-Wextra', '-DARDUINOJSON_USE_DOUBLE=0',
        '-Itests/support', '-Isrc', '-I.pio/libdeps/smalltv_lean/ArduinoJson/src']
exe = out / ('feed.exe' if os.name == 'nt' else 'feed')
subprocess.run(base + ['tests/test_radar_feed.cpp', '-o', str(exe)], check=True)
subprocess.run([str(exe)], check=True)
routes = out / ('routes.exe' if os.name == 'nt' else 'routes')
subprocess.run(base + ['tests/test_radar_routes.cpp', 'src/features/radar/RadarRouteData.cpp', '-o', str(routes)], check=True)
subprocess.run([str(routes)], check=True)
render = out / ('render.exe' if os.name == 'nt' else 'render')
gfx = '-I.pio/libdeps/smalltv_lean/GFX Library for Arduino/src'
subprocess.run(base + [gfx, 'tests/test_radar_render.cpp', 'src/features/radar/RadarMode.cpp',
                      'src/features/radar/RadarRouteData.cpp', '-o', str(render)], check=True)
subprocess.run([str(render), str(out / 'preview.ppm')], check=True)

# Export the actual renderer's pixels as a portable preview, without Pillow.
pixels = (out / 'preview.ppm').read_bytes().split(b'\n', 3)[3]
assert len(pixels) == 240 * 240 * 3
def chunk(kind, data):
    return struct.pack('!I', len(data)) + kind + data + struct.pack('!I', zlib.crc32(kind + data))
rows = b''.join(b'\0' + pixels[y*720:(y+1)*720] for y in range(240))
png = (b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('!IIBBBBB', 240, 240, 8, 2, 0, 0, 0))
       + chunk(b'IDAT', zlib.compress(rows)) + chunk(b'IEND', b''))
(out / 'preview.png').write_bytes(png)
print('Radar preview:', out / 'preview.png')
