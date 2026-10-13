# Lean Smart Dashboard

Firmware identity: `2.16.0-dashboard2`. Extends the existing `clock` mode; Radar
and the Clock/Radar carousel retain their IDs, configuration, capacity and
refresh settings. The full theme engine remains disabled in Lean.

Task 4 validates astronomical phases and improves dark-side contrast, scanline
symmetry and primary-phase classification. See [moon validation](MOON_VALIDATION.md)
for reference dates, timezone checks, previews and current build measurements.

## Use and layout

Select **Display → Digital clock** for the standalone screen, or select Digital
clock and Radar in the carousel. **Display → Smart dashboard** controls location,
weather, prayer convention, Asr, high-latitude rule, countdown, and Hijri correction.
Dashboard off restores the simple clock. Weather off suppresses weather requests.

All content fits the existing 240×240 display with the built-in 6×8 font.
The installed font supports CP437 glyphs: `März`, `Bewölkt`, `Nächstes Gebet`
and `°C` render directly without a UTF-8 decoder or extra font library.
Prayer names are Fajr, Dhuhr, Asr, Maghrib and Isha. The calculation method stays
in web settings; the display shows only the next prayer/time and optional
`in 1 Std. 56 Min.` countdown. Temperature is rounded to whole Celsius degrees.

| Region | Content |
|---|---|
| Top, y=10–73 | Large 24-hour time, German Gregorian weekday/date |
| Middle, y=86–169 | Weather (left) and moon (right); compact conditions and size-2 moon phase labels |
| Bottom, y=178–235 | Next prayer/time, size-2 countdown, sunrise and sunset |

Text alignment uses Arduino_GFX `getTextBounds`, including font offsets.
The two-column centers are x=60 and x=180. Only horizontal section separators
remain; every element keeps at least four pixels of edge clearance.
The moon's shaded terminator uses the astronomical cycle; the larger disk is
tested for differing new/quarter/full illumination.

Hijri is absent from the display. Its pure calculation and saved correction
setting remain compatible with existing configurations and calculation tests.
Radar clamps complete callsign/route/altitude boxes to a two-pixel panel margin,
reserves the header, and tries eight positions around the unchanged aircraft
marker. Dense traffic retains the existing priority/drop behavior when labels
cannot fit without overlapping; aircraft positions, vectors and filtering are unchanged.

Rendering repaints once per minute, after a weather attempt, or on wake/settings
changes. All names and format strings are in flash; icons use drawing primitives,
without sprite storage or a framebuffer. The headless preview uses the actual
renderer and installed firmware font; it does not represent a hardware test.

## Architecture and memory

- `DashboardMath`: pure allocation-free calculations, tested natively. Uses
  single-precision math and days relative to J2000, avoiding large Julian-date
  rounding and additional double-precision library RAM tables.
- `Dashboard`: 48 B of cached prayer/sun minutes for yesterday/today/tomorrow.
  UTC-minute offsets retain events crossing local midnight. Event timestamps
  use the existing `localtime_r` timezone/DST rules when displayed. Countdown
  uses UTC elapsed time, including DST changes and yesterday's late Isha.
- `DashboardWeather`: 36 B cached data/diagnostics. A request runs only from the
  active Clock service after NTP and Wi-Fi are available. Firmware's synchronous
  mode dispatch makes Weather and Radar requests sequential, including carousel
  switches. Weather pauses while another mode or notification is active.
- `DashboardSettings`: 24 B, no additional Strings. Nested `dashboard` JSON is
  optional for older saved configs; partial updates retain omitted fields.
  Coordinate/angle inputs require finite numbers in range. Existing Wi-Fi,
  Radar, timezone and carousel values are preserved. Default dashboard is on.
  Fresh configs now default to Hannover and Europe/Berlin; a saved timezone,
  including explicitly saved UTC, still takes precedence.

## Weather

[Open-Meteo](https://open-meteo.com/en/docs) current temperature/code plus timestamp,
without hourly/daily forecast arrays. HTTP/1.0 with Host was verified to return
HTTP 200 and an unchunked, close-delimited JSON response on 2026-10-12. Public
weather uses HTTP to avoid another TLS allocation peak; coordinates/data travel
unencrypted. No credentials or external proxy/PC are required. Provider changes
or networks requiring HTTPS would need another transport review.

JSON is read directly from the socket, filtered, limited to 2,048 input bytes and
nesting depth four. A total parse deadline and per-read timeout bound stalls.
Requests use the existing configured HTTP timeout and disable reuse/redirects.
The socket and both JSON documents are destroyed before rendering. A separate
256 B URL-construction frame ends before parsing; fetch's own compiler-reported
frame is 528 B. These are individual frames, **not** complete stack peaks.

Successful updates recur after 25 minutes; failures retry after five minutes,
low-heap skips after one minute. Requests need 5,000 B total heap and a 2,048 B
largest block; these conservative guards are not a hardware safety guarantee.
Failed/missing/null/invalid payloads preserve the cache. A location change clears
weather from the previous location. Observations/receipts older than 90 minutes
show `veralt.`; recent failed updates show `Neuversuch`. Unknown WMO codes show `Unbekannt`.
Open-Meteo attribution is in the web UI.

`/api/status.dashboard` exposes enabled/valid/stale, weather stage and HTTP code,
attempt count, observation timestamp, post-close heap/largest block, and minimum
sampled heap. Stage values: 0 idle, 1 OK, 2 low heap, 3 connection error,
4 HTTP error, 5 parse/validation error. Samples are not transient allocation peaks.
Existing top-level heap/largest-block/cont-stack and Radar diagnostics remain.
Status/config serialization has extra temporary JSON allocations; measure them
on the device along with requests. No permanent JSON documents are retained.

## Calendar, moon and prayer conventions

Hijri is the tabular **civil** calendar, not an official/sighting calendar. `AH~`
marks that approximation; ±1-day correction is configurable and the date changes
at local midnight. Astronomical moon illumination is calculated independently
from solar/lunar longitude with principal lunar perturbations; eight broad phase
sectors and a shaded disk are supported. It is not an ephemeris for exact phase
instants, moon sighting, eclipses, or religious month announcements.

Prayer presets follow the angle conventions documented by
[PrayTimes](https://praytimes.org/calculation): MWL 18°/17°, ISNA 15°/15°,
Egypt 19.5°/17.5°, Karachi 18°/18°, plus custom 10–25° angles. Asr supports shadow
factor one or two. Dhuhr uses solar transit and Maghrib uses sunset; no community
precautionary offsets are added. Sunrise/sunset assume a level horizon at −0.833°,
without terrain/elevation correction. Solar coordinates use
[USNO's approximate algorithm](https://aa.usno.navy.mil/faq/sun_approx).

Hannover summer twilight is handled by angle/60, half-night, or one-seventh-night
limits. The preceding/following day's solar crossing supplies the actual adjacent
night length. Adjustment flags remain in the calculation result; Task 3 removes
method/adjustment annotations from the display. “None” leaves absent twilight
unavailable. Polar absent sunrise/sunset show `--:--`; the software does not invent
sun crossings, and only available prayer crossings participate in selection.
Prayer times use the selected web calculation convention. Generic convention
tests do not establish agreement with a Hannover mosque's local timetable.

## Build measurements

Task 3 UI refinement compared with the working German layout above:

| Metric | Before Task 3 | After Task 3 | Change |
|---|---:|---:|---:|
| Static RAM | 49,020 B | 49,020 B | 0 B |
| Toolchain flash usage | 690,021 B | 690,613 B | +592 B |
| Firmware binary | 694,176 B | 694,768 B | +592 B |

No render-cycle allocations or dependencies were added. Runtime free heap,
largest block and complete stack peak require device measurements and were not
measured. HTTPS, Wi-Fi, OTA, NTP, weather/prayer calculations and Radar/carousel
service code are unchanged by Task 3.

Lean build, existing math/weather/web UI tests and Radar regressions pass.
Dashboard checks enforce no text overlap/clipping and a four-pixel panel margin,
cover all five prayers/eight phases, dates and weekdays, methods, temperature
limits, weather statuses, polar states and a double-digit-hour countdown.
Actual-renderer 240×240 PNGs are regenerated by `tools/test_dashboard.py` in
`.pio/dashboard-tests/dashboard-task3.png`, with prayer and moon variants alongside.
Current binary SHA-256: `2072CBDE99FA60EC257B710F93BBCFB59F0021294790C3B816385B1F35279CE0`.

The following measurements record the preceding German-layout implementation.

German layout and Radar placement update, compared with the unmodified local
`2.16.0-dashboard2` build using the same Lean toolchain/dependencies:

| Metric | Before | After | Change |
|---|---:|---:|---:|
| Static RAM | 49,020 B | 49,020 B | 0 B |
| Toolchain flash usage | 690,153 B | 690,021 B | -132 B |
| Firmware binary | 694,304 B | 694,176 B | -128 B |

`pio run -e smalltv_lean` passes. Native production-renderer checks cover German
calendar/weather text, all weekdays/methods, -100 to +70 C, countdown enabled/off,
weather status/disabled states, polar conditions and a full lunar cycle.
Radar passes 1,728 edge/scale/route cases with explicit callsign and altitude
visibility checks for in-range single aircraft, plus dense traffic bounds tests.
Runtime heap and hardware legibility are not measured; no flashing or push occurred.
Historical binary SHA-256: `8437BBC485A370B58F678CAD250101DFAFCD4856056AEAB1B4DBC491930D1D46`.

The measurements below record the earlier dashboard implementation.

Baseline is committed Task 1 firmware `00e38d4`, not historical device readings.
Both builds use `smalltv_lean`, ESP8266 core 3.1.2 and the existing dependencies.

| Metric | Task 1 baseline | Dashboard | Change |
|---|---:|---:|---:|
| Static RAM | 47,964 B | 48,160 B | +196 B |
| Toolchain flash usage | 652,537 B | 673,253 B | +20,716 B |
| Firmware binary | 656,688 B | 677,408 B | +20,720 B |
| Runtime free heap | Not measured | Not measured | Not measured |
| Largest free block | Not measured | Not measured | Not measured |
| Peak request allocation | Not measured | Not measured | Not measured |

The final firmware builds successfully and fits the application limit;
safe runtime headroom still requires device validation. Task 1's 768 B/24-slot
aircraft cache, TLS tuning/session/guards and 32 B Radar memory record are intact.

Artifact: `.pio/build/smalltv_lean/firmware.bin`.
Historical SHA-256: `CC2ABF6351B8CE9A2D3DBBA36DCAA5ADC71C8E841D10753C6C881C520844177C`.

## Validation

- Lean build succeeds; generated web UI decompresses byte-for-byte to HTML.
- Native production calculations: 288 prayer cases against published PrayTime
  v3.2 differ by at most one minute; source hash recorded in fixture metadata.
  Adjacent-night refinement differs intentionally from its same-date night.
- USNO equinox/solstice solar crossings pass a two-minute tolerance. All 2026
  primary moon phases pass; maximum tested longitude error is 0.110°. This is
  measured at those primary instants, not a guarantee at all times.
- 384 civil Hijri cases match ICU; correction, polar/no-twilight handling,
  next-prayer boundaries/late Isha, and DST epoch countdowns pass.
- Polar-winter Asr is unavailable when the sun stays below the horizon at
  transit. Regression tests cover both hemispheres, both shadow factors and
  all high-latitude rules, next-prayer selection, and valid polar-summer Asr.
- Weather validation accepts good/stale observations and rejects missing/null,
  invalid, future, and truncated responses without changing the previous value.
- Actual renderer on a headless adapter passes 864 month/weather/rule cases,
  additional moon phases and pending/polar states, including ASCII/bounds checks.
- Embedded UI JavaScript tests cover defaults, round-trip (including zero
  coordinates, negative correction and disabled options), carousel and Berlin DST.

Run all offline tests after a lean build:

```powershell
& ./.pio/tooling/python/python.exe tools/test_dashboard.py --compiler C:/dev/SmallTV/.pio/tooling/w64devkit/bin/g++.exe
```

Any native GCC/Clang C++11 compiler and Node can be used; no firmware dependencies
were added. Test downloads/compilers/artifacts live in ignored `.pio/`. USNO and
ICU/prayer fixtures are included; tests do not contact a provider or device.

## Hardware checklist (no flashing performed)

1. Confirm pixel legibility, panel gains/rotation, stale/retry labels, all icon
   categories, simple-clock fallback, carousel return and notification restoration.
2. Track heap, largest block and `contstk` across initial/busy Radar fetches,
   weather fetches, repeated carousel cycles, weak Wi-Fi/reconnects, and status/config
   requests. Check reset reason and recovery after failed/malformed responses.
3. Compare selected prayer convention and high-latitude rule with the community
   timetable in summer/winter. Verify midnight, year/month boundary, next prayer,
   countdown and Berlin DST changes. Confirm ±1 Hijri correction expectations.
4. Verify location/disabled settings save/reload and old-config upgrades; choose
   the matching timezone explicitly after changing coordinates.
5. Test OTA space and manual update/recovery only under separate flashing approval.
   The inherited GitHub updater still targets upstream releases, which can replace
   custom features; this task does not change that behavior.

No device flash, factory reset, commit, push or release tag was performed.
