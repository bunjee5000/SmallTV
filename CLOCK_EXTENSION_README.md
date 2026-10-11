# SmallTV-Mod 2.16.0 Lean + built-in digital clock (source patch)

This archive is source code, NOT ready-to-flash firmware. It was prepared from the supplied v2.16.0 source archive and has NOT been compiled or hardware-tested.

## What changed
- New built-in `clock` display mode using the existing GFX drawing helpers and NTP service.
- `Display -> Mode -> Digital clock (built-in)` can select the clock.
- `Display -> Mode -> Carousel` offers `Digital clock` alongside `Radar`; select only those two to alternate.
- Clock paints Hannover, local time (HH:MM), and date, in the configured timezone. The renderer updates once a minute.
- NTP service now starts for Clock mode or a clock-enabled carousel, even when night mode is off.
- Firmware version identifier changed to `2.16.0-clock1` for identification.

## Compile on a development computer
1. Install VS Code and the PlatformIO IDE extension (requires internet access to download ESP8266 framework and Arduino libraries).
2. Open this folder as a PlatformIO project.
3. Run `pio run -e smalltv_lean` or use `PlatformIO: Build` with environment `smalltv_lean`.
4. Only if the build succeeds: find `.pio/build/smalltv_lean/firmware.bin`.
5. Before OTA upload, verify binary size fits the device's OTA partition and keep a recovery path. Export existing settings locally from the System tab, but that export contains plaintext Wi-Fi passwords. Firmware flashing can brick the device.
6. Device System -> Manual update (OTA) -> select `firmware.bin` -> Upload & flash. Do NOT use the loader again.
7. In Display: set timezone Europe/Berlin; Mode = Carousel; check Digital clock and Radar, uncheck Ticker and others; set switch interval to 30 seconds; Save settings.

## Important
- Standard theme engine remains disabled as in Lean. This is a fixed lightweight clock, not an `.stheme` theme.
- Lean radar behavior is unchanged, but use cannot be guaranteed until tested on actual hardware.
- Automatic GitHub firmware upgrades may revert this modification to an official build; avoid using Update now.
- Custom code was NOT compiled in the preparation environment: PlatformIO packages were unavailable due to network access restrictions.
