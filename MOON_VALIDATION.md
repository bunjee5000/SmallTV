# Task 4: Moon phase validation

## Findings and corrections

The Task 3 preview at 12 October 2026, 16:37 Europe/Berlin (14:37 UTC)
calculates a waxing crescent with 3.988% illumination. Its white region was
already a small right crescent. The gray unlit disk was too bright and could
be mistaken for illuminated surface. It is now near black, with a dim outline.
Inclusive scanline endpoints and symmetric pixel-center sampling remove the
old one-pixel waxing/waning asymmetry. Exact new/full have no white pixels/all
white interior pixels respectively. The compact radius remains 14 pixels.

A separate label bug was found: nearest-45-degree classification labeled the
3.263% crescent at noon on 12 October as `Neumond`. Primary labels now occupy
only +/-3.6 degrees around conjunction, quadrature and opposition. Remaining
angles use the appropriate crescent/gibbous label. Near quarters, illumination
is approximately 47–53%; near new/full, below 0.1%/above 99.9%.

## Calculation and interpretation

`dashboardMoon` computes approximate geocentric lunar longitude using the
principal lunar perturbations, subtracts the approximate solar longitude and
normalizes that elongation to a cycle in [0,1). It does **not** count days since
a fixed new moon or use the civil Hijri calendar. No elapsed lunar-age value
is calculated. The existing astronomical longitude series is unchanged.

The disk illumination approximation is `(1-cos(2*pi*cycle))/2`. This neglects
small effects of lunar latitude and finite Sun distance, and is appropriate
for the tiny icon rather than a precise ephemeris. Actual elongation drives
the shaded elliptical terminator; the label index never substitutes a generic
circle. Waxing lights the right, waning lights the left, for the requested
Northern Hemisphere convention. Sky-position rotation/parallactic angle is
not represented.

Moon calculation uses the current UTC epoch, including its time of day.
The displayed civil date and event times retain existing local timezone/DST
conversion. Changing timezone at the same instant cannot change the moon.
Berlin uses UTC+2 on 12/13 October and UTC+1 on the November references.

## Independent astronomical checks

References are archived in `tests/fixtures/usno_moon_illumination_2026.json`
with exact source URLs, local noon offsets and UTC epochs. The
[USNO API documentation](https://aa.usno.navy.mil/data/api.html) specifies that
daily phase and illumination are evaluated at local noon. Percentages are
rounded to whole percent. The existing USNO primary-event fixture covers
every primary phase throughout 2026; its maximum longitude error remains
0.110 degrees. The 16 added daily references have maximum illumination
disagreement of 0.474 percentage points.

| Local noon in Hannover | USNO description | USNO illuminated | Calculated illuminated | Dashboard label |
|---|---|---:|---:|---|
| 2026-10-03 | Waning gibbous, near last quarter | 52% | 51.707% | Letztes Viertel |
| 2026-10-06 | Waning crescent | 20% | 19.849% | Abn. Sichel |
| 2026-10-10 | New moon | 0% | 0.064% | Neumond |
| 2026-10-12 | Waxing crescent | 3% | 3.263% | Zun. Sichel |
| 2026-10-13 | Waxing crescent | 8% | 7.736% | Zun. Sichel |
| 2026-10-15 | Waxing crescent | 21% | 21.066% | Zun. Sichel |
| 2026-10-18 | Waxing crescent, near first quarter | 48% | 47.643% | Erstes Viertel |
| 2026-10-22 | Waxing gibbous | 84% | 83.705% | Zun. Mond |
| 2026-10-26 | Full moon | 100% | 99.921% | Vollmond |
| 2026-10-29 | Waning gibbous | 86% | 85.893% | Abn. Mond |
| 2026-11-01 | Waning gibbous, near last quarter | 55% | 54.526% | Abn. Mond |
| 2026-11-05 | Waning crescent | 15% | 14.695% | Abn. Sichel |

Primary events in October (USNO UTC): last quarter 03 Oct 13:25 (50%),
new moon 10 Oct 15:50 (0%), first quarter 18 Oct 16:12 (50%), full moon
26 Oct 04:12 (100%). USNO labels primary events at exact instants; dashboard
labels use the documented small windows around those instants.

The regenerated `.pio/dashboard-tests/moon-validation.md` includes all 16
daily references, their timezone offsets, primary-event values and exact
preview timestamps. At 16:37 CEST the 12/13 October previews show 3.988% and
8.787% calculated illumination respectively, both `Zun. Sichel`.

## Regression tests and artifacts

- Production icon tested at 401 phase angles: correct dominant illuminated
  side, exact mirror symmetry, new/full endpoints and rasterized illumination
  within four percentage points of the continuous geometry.
- All eight German labels and independent controlled phase icons are checked.
- Forty days sampled every ten minutes verify all classification transitions,
  wraparound and continuity; October new/full are bracketed by USNO events.
- UTC offsets -12, -7, 0, +1, +2, +5.5 and +14 exercise local midnight and
  date crossings. At an identical UTC instant the full moon column is pixel
  identical across offsets. Explicit Berlin winter/summer offsets are included;
  existing timezone/DST web and prayer regression tests still pass.
- Dashboard text retains a four-pixel margin with zero clipping or overlap.
- Existing math, weather, web UI and Radar regressions pass, including all
  1,728 Radar edge/scale/route cases and dense traffic bounds tests.

Run `tools/test_dashboard.py --compiler <native-g++>` after the Lean build.
It generates `.pio/dashboard-tests/dashboard-task4.png`, the 13 October
variant, eight full dashboard phase variants, eight controlled phase PNGs
(`moon-task4.phase-0.png` through `moon-task4.phase-7.png`) and a 480x200
comparison sheet (`moon-task4-comparison.png`) at native pixel scale.
Each individual preview is exactly 240x240. Host-only PNG/contact-sheet code
uses Python's standard library and is not part of the firmware.

## Lean build and memory

`pio run -e smalltv_lean` succeeds using the existing ESP8266 toolchain.

| Metric | Before Task 4 | After Task 4 | Change |
|---|---:|---:|---:|
| Static RAM | 49,020 B | 49,020 B | 0 B |
| Toolchain flash usage | 690,613 B | 690,869 B | +256 B |
| Firmware binary | 694,768 B | 695,024 B | +256 B |

No firmware graphics dependency, framebuffer or dynamic render-loop allocation
was added. The existing fixed buffers and graphics primitives remain in use.
Runtime free heap/stack peaks have not been measured on hardware. Weather,
prayer calculations, Radar service/geometry, Wi-Fi, NTP, HTTPS, OTA and carousel
remain unchanged by Task 4. No device flash or GitHub push was performed.

Firmware SHA-256: `7B88F51ECA77366259C2D350ED3CC91B101A1D7E43E06A79E3B7BA077405486D`.
