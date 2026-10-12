# Lean memory review — 2026-10-12

Baseline is the working tree at the start of this task, including the existing
custom Clock and build-script changes. Historical device readings are not used.
Both builds used the local PlatformIO installation, `run -e smalltv_lean`.

| Metric | Before | After | Improvement |
|---|---:|---:|---:|
| Static RAM | 48,268 B | 47,964 B | 304 B less |
| Free heap | Not measured | Not measured | Not measured |
| Largest free block | Not measured | Not measured | Not measured |
| Peak HTTPS allocation | Not measured | Not measured | Not measured |
| Firmware binary | 655,456 B | 656,688 B | 1,232 B larger |
| Toolchain flash usage | 651,309 B | 652,537 B | 1,228 B larger |

## Changes and evidence

- Aircraft storage retains all 24 slots, but discards latitude/longitude after
  the identical projection calculation. Rendering uses distance and bearing.
  Cache shrank from 960 B to 768 B (ELF symbol `g_ac`); this saves 192 B.
  Aircraft is runtime-only, so persisted configuration is unaffected.
- Clock weekday/month tables now live in flash, with explicit `memcpy_P` reads
  into two four-byte stack buffers. ELF places both tables at flash addresses.
  This removes 144 B of RAM tables/string storage. Text remains identical.
- Radar fetch uses the retained diagnostic URL directly, eliminating the second
  caller-owned URL during the request. Heap savings depend on URL length and
  String storage; no runtime number is claimed. HTTPClient still owns its own
  necessary URL/header strings.
- Added a 32 B fixed diagnostics record, without logging or timers during a
  request. Net static reduction is 192 + 144 - 32 = 304 B.
  Serializing the added status fields uses temporary JSON memory only when
  `/api/status` is requested; the snapshots themselves allocate nothing.

## Memory consumers and retained protections

The highest-risk dynamic consumers are BearSSL's handshake, TLS record buffers,
TCP queues, and the filtered JSON document. Parsing avoids a full response String,
but still retains every filtered aircraft object before selecting the nearest 24;
JSON memory therefore grows with feed traffic rather than display capacity.
The filter document also allocates a JSON pool. Both documents are destroyed
before HTTP/socket teardown. HTTPClient and the dynamically created NetClient
are local RAII objects and are destroyed on success and error returns.

Existing MFLN probing (512/1024 B receive buffers when supported), TLS session
resumption, P-256 selection, HTTP/1.0 stream framing, connection non-reuse, and
16,000 B contiguous-block guard remain intact. The existing 4 KB receive fallback
for hosts without MFLN is an existing large-record limitation, not changed here.
Ticker cache also remains present in the lean build: ELF `g_stocks` is 2,720 B.
No features, cipher options, connection timeouts, or polling settings changed.

Clock rendering has no explicit heap allocation. It adds eight bytes of local
text buffers; it does not add recursion or large stack buffers. Radar label
decluttering still uses 24 eight-byte boxes (192 B) on the stack. JSON parsing,
printf, graphics and TLS nested calls need device stack validation; compile
success alone cannot certify stack safety.

## Device measurements to collect later

`/api/status` now includes `radar.memory`:

- `polls`: attempts since boot (including skipped/error attempts).
- `before`: free heap before entering fetch, after URL construction.
- `headers`: after GET returns, while the HTTP/client objects exist.
- `json`: after deserialization, while filter and result documents exist.
- `afterParse`: after both documents are destroyed.
- `closed`, `closedBlock`: free heap and largest block after fetch objects die.
- `minSampled`: minimum across these stage samples since boot, retained across
  mode changes. This is NOT the true minimum during HTTPS or JSON allocations.

Zero stage values mean that stage was not reached in the last attempt.
For repeated-cycle comparison, record these fields, `radar.stage`, `radar.tlsRx`,
and top-level `contstk` for initial and resumed connections, busy feeds, failures,
and mode/carousel changes. Compare `closed` and `closedBlock` across cycles.
`100 * (1 - closedBlock / closed)` is a largest-block fragmentation indicator,
not an exact allocator fragmentation measurement. The top-level status heap
readings include status-document allocations; Radar snapshots precede them.
No physical device was flashed or measured during this task.

## Validation and next steps

Lean compilation succeeded; `git diff --check` passed. Reviewed project diff,
preserving pre-existing edits in main.cpp, extra_script.py, and webui.h. Settings,
Clock/Radar carousel dispatch, labels, vectors, altitude filter, nearest ordering,
24-aircraft capacity, refresh intervals, NTP, Wi-Fi, storage, and OTA paths retain
their behavior by code review. Runtime rendering/network reliability and future
dashboard headroom still require hardware measurements.

The next substantial optimization to evaluate is bounded per-aircraft JSON
streaming. It needs fixture tests for both providers/webhooks, malformed and
truncated arrays, arbitrary field order, timeouts, and preserving stale aircraft
on parse failure. Introducing it without those tests would risk reliability.
Allocation tracing would be needed for a true TLS high-water measurement; the
lightweight stage snapshots intentionally do not claim that precision.
