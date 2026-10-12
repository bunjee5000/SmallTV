# SmallTV Lean clock/radar session handoff

Updated: 2026-10-11. Project: SmallTV-Mod v2.16.0, firmware identity
`2.16.0-clock1`, target `smalltv_lean` (ESP8266 / ESP-12F, 4 MB flash).

## User constraints

- Do not flash a device, push commits, or perform destructive operations without explicit user approval.
- Review and build locally; preserve the user's existing changes.
- Treat runtime free heap, largest contiguous free block, and continuation stack headroom as separate limits.
- No flashing, commits, pushes, or destructive operations were performed during this session.

## Work completed

- Reviewed `src/ClockMode.cpp`, `src/ClockMode.h`, mode registration and carousel in `src/main.cpp`, settings persistence, NTP, radar, Wi-Fi, and OTA paths.
- Found that `src/webui.html` contained clock controls but the served, gzip-compressed `src/webui.h` did not. Regenerated the header.
- Added `tools/gzip_webui.py` and invoked it from `extra_script.py` before builds. The generated header only writes when content changes. Its decompressed bytes were verified against the HTML source.
- Moved the display-pointer check to the start of `ClockMode::service()`, so an unavailable display does not mark the current minute as painted.
- Moved `clockReapply(g_settings)` after the queued boot-time OTA attempt in `setup()`. NTP's persistent allocation can fragment heap needed by the ESP8266 update's TLS connection. NTP still starts before the web portal and normal mode services; safe-mode gating is retained.
- Built `smalltv_lean` successfully, including a rebuild after the final firmware change. No compilation errors. Dependency warnings came from PubSubClient signedness and Python escape sequences in the framework's `elf2bin.py`.
- `ClockMode.h` was reviewed and left unchanged.

Current changed tracked files: `extra_script.py`, `src/ClockMode.cpp`,
`src/main.cpp`, `src/webui.h`. New files: `tools/gzip_webui.py` and this handoff.
Run `git status --short` at the next session start to check for subsequent work.

## Build and toolchain

Python and PlatformIO were absent initially. Workspace-local tools and packages
were installed under ignored `.pio/`; they are not source changes.

PowerShell build command:

```powershell
$env:PLATFORMIO_CORE_DIR = 'C:/dev/SmallTV/.pio/core'
& ./.pio/tooling/python/python.exe -m platformio run -e smalltv_lean
```

Alternatively, with an existing PlatformIO installation: `pio run -e smalltv_lean`.
Do not add an upload target.

Versions used: Python 3.12.10, PlatformIO 6.1.19, espressif8266 platform 4.2.1,
Arduino ESP8266 core 3.1.2, Xtensa compiler 10.3.0, ArduinoJson 7.4.3,
GFX Library for Arduino 1.6.9, PubSubClient 2.8.0. Library version ranges in
`platformio.ini` can resolve differently in future installations.

| Build metric | Bytes |
| --- | ---: |
| Static RAM | 48,268 / 81,920 (58.9%) |
| Program flash | 651,309 / 1,044,464 (62.4%) |
| Actual firmware binary | 655,456 |

Artifact: `.pio/build/smalltv_lean/firmware.bin`.

SHA256: `236B598E908313AD35A1A8A667656904799A2556A080A7614671D337B818F329`.

## Heap and stack limitations

The user supplied this running-device snapshot, from the firmware already on
the device, **not from the newly built local binary**:

| Runtime metric | Value |
| --- | ---: |
| Free heap | 23,296 bytes |
| Largest contiguous free block (`maxblk`) | 19,864 bytes |
| Untouched continuation stack watermark (`contstk`) | 704 bytes |
| Wi-Fi RSSI | -85 dBm |
| Uptime | 1,340 seconds |
| Radar HTTP response | 200 |
| Radar stage / error | `ok` / `false` |
| Retained / feed aircraft | 1 / 1 |
| TLS receive buffer (`tlsRx`) | 512 bytes |

Important limits and interpretation:

- Radar's HTTPS guard in `src/features/radar/RadarClient.cpp` requires `platformMaxFreeBlock() >= 16000` before probing/creating the TLS client. The snapshot has only **3,864 bytes of margin** over this threshold. Passing the guard is not a guarantee that all later TLS and JSON allocations succeed.
- Free heap and largest block are different quantities. Fragmentation can prevent TLS even when total free heap looks adequate. The build's 33,652 bytes remaining after static RAM is arithmetic headroom, not measured runtime free heap.
- Wi-Fi queues, weak-link retransmissions, HTTP processing, JSON parsing, NTP, and other runtime allocations share the available heap. RSSI -85 dBm is weak; the snapshot does not establish stability over time or with larger aircraft feeds.
- Radar keeps at most 24 aircraft, a 960-byte static array in this build. Its filtered ArduinoJson document still parses the whole feed's selected fields before retaining the nearest 24; `MAX_AIRCRAFT` does **not** bound peak JSON allocation. Busy feeds can use substantially more heap than the one-aircraft snapshot.
- `ClockMode` directly allocates no heap and uses the existing display without a framebuffer or theme engine. Its global mode object is 8 bytes in the built ELF; tables, strings, and the vtable add static storage. The renderer's 32-byte text buffer and `tm` are stack locals. NTP adds persistent runtime allocation; once started, the current clock service does not stop it when the clock consumer is disabled.
- `contstk` is a watermark of stack bytes never touched since boot, verified in the ESP8266 core's `cont_get_free_stack()`. **704 bytes is limited headroom**, not an instantaneous measurement and not proof of safety for all paths. Radar's render function also has a 192-byte local label-box array; this alone does not explain the deepest watermark.
- Boot-time GitHub OTA has stronger guards in `src/OtaUpdate.cpp`: total free heap must be at least **24,896 bytes** (`16384 + 512 + 8000`) and the largest block at least **17,408 bytes** (`16384 + 1024`). The supplied runtime heap snapshot falls below that total-heap guard; the updater runs at boot, where heap availability differs.
- Manual OTA uses a different streaming path, stops UDP sockets on ESP8266, and checks flash space through the framework updater. Its successful operation cannot be inferred solely from the radar threshold or this snapshot.

## Carousel, clock, Wi-Fi, and OTA findings

- Clock is always in the mode registry. Lean compiles out HA, usage, and themes; ticker, radar, and notifications remain enabled by default.
- Clock settings serialize/deserialize, and NTP starts for standalone clock or a clock-enabled carousel even if night mode is disabled.
- Clock renders local time and date using the configured POSIX timezone, once per minute. The header and footer are fixed `HANNOVER` and `RADAR + CLOCK`.
- Clock `wake()` resets its minute marker; radar `wake()` requests repaint without resetting cached aircraft. Alternation therefore restores each screen even within the same minute.
- Only the active mode's service runs. Radar polling pauses during the clock dwell; on return, a due fetch can occur before the repaint. Synchronous network work can extend carousel dwell and delay servicing the web UI.
- Configure Carousel with only Digital clock and Radar selected for the requested pairing. The existing all-unchecked case falls back to a registry mode; with clock first and no NTP consumer enabled, it can show `Syncing time...`. This edge case was identified but not changed.
- Existing saved carousel configurations missing `carouselClock` inherit its new default `true`; upgrading can add the clock to their rotation.
- Wi-Fi STA reconnect, saved-network failover, AP setup display, and safe-mode web recovery remain structurally compatible. The clock itself does not block for NTP synchronization.
- The supplied status reported a valid local time, `synced: true`, `night: true`, `nightHeld: false`, and `clockFresh: false`. This combination is expected: freshness expires after five minutes, but night mode remains latched until the scheduled window ends.
- Flash layout remains `eagle.flash.4m1m.ld`. The binary fits this build's application limit. Actual OTA success also depends on the running device's layout, available update space, memory, and network; no upload was tested.
- The GitHub updater still points at the official lean release asset. A newer official firmware update can replace the custom clock addition. There is no unattended update demonstrated here; the update is initiated through the web UI.

## Next-session checks

1. Read this file and inspect current changes before editing. The local binary has not been flashed or hardware-tested by the assistant.
2. If the user explicitly approves flashing, confirm the running device's flash layout and available OTA space before upload. Keep that approval separate from permission to build.
3. On an approved device test, observe repeated `/api/status` snapshots across clock/radar switches, larger aircraft feeds, weak Wi-Fi, reconnects, and web UI use. Track both `heap` and `maxblk`, plus `contstk`, radar stage, and reset reason; one snapshot cannot establish absence of leaks or fragmentation.
4. Verify clock repaint on return, date/timezone changes, NTP synchronization after reconnect, and restoration after notifications.
5. If memory is insufficient, consider reducing radar range or using a LAN webhook that filters the feed before the ESP8266 parses it. Evaluate peak JSON memory before changing the static aircraft limit or adding buffers/caches.
6. Preserve the compact clock implementation and avoid introducing framebuffers, theme engines, or unnecessary persistent allocations into Lean.
