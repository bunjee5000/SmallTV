"""Regenerate the firmware's embedded UI from src/webui.html."""
import gzip
from pathlib import Path


def generate():
    root = Path(__file__).resolve().parent.parent
    data = gzip.compress((root / "src/webui.html").read_bytes(), compresslevel=9, mtime=0)
    rows = ["  " + ", ".join(f"0x{byte:02x}" for byte in data[i:i + 16]) + ","
            for i in range(0, len(data), 16)]
    header = (
        "// GENERATED FILE -- do not edit by hand.\n"
        "// Produced by tools/gzip_webui.py from src/webui.html (the source of truth).\n"
        "// Regenerate after editing the HTML: python tools/gzip_webui.py\n"
        "#pragma once\n#include <Arduino.h>\n\n"
        "static const uint8_t WEBUI_HTML_GZ[] PROGMEM = {\n"
        + "\n".join(rows) + "\n};\n"
        "static const size_t WEBUI_HTML_GZ_LEN = sizeof(WEBUI_HTML_GZ);\n"
    ).encode("ascii")
    target = root / "src/webui.h"
    if not target.exists() or target.read_bytes() != header:
        target.write_bytes(header)


if __name__ == "__main__":
    generate()
