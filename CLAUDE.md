# phishbot - project notes

Arduino **UNO R4 WiFi** (PlatformIO, `renesas-ra@1.9.0`). Single-file firmware:
`src/main.cpp`. Servo oscillator driven by a Modulino knob or over WiFi, with status on
the onboard 12x8 LED matrix and a web dashboard.

Setup, HTTP API and full troubleshooting live in `README.md`.

## Commands

`pio` is **not on `$PATH`**. Use the full path:

```bash
~/.platformio/penv/bin/pio run                 # build
~/.platformio/penv/bin/pio run -t upload       # flash
~/.platformio/penv/bin/pio device monitor      # serial, 115200
```

`src/config.h` holds WiFi credentials, is git-ignored, and must never be committed.

## FIRST THING TO CHECK: upload fails / no serial port

This has cost a full session once already. **Do not re-diagnose it from scratch.**

**Symptom:** upload auto-detects `/dev/cu.Bluetooth-Incoming-Port`, fails with
`No device found`; no `/dev/cu.usbmodem*` exists.

**Confirm in one command:**

```bash
ioreg -r -n "UNO WiFi R4 CMSIS-DAP" -l -w0 -d 3 | grep -E "^\+-o"
```

`!registered, !matched` with **no child interfaces** means macOS enumerated the device
descriptor but refused to configure it. Healthy shows `registered, matched` plus three
interfaces (HID class 3, CDC class 2, CDC-data class 10).

**Cause:** macOS Ventura+ on Apple Silicon, *Allow accessories to connect*. The approval
prompt for this board is withdrawn faster than a human can click, and an unanswered
prompt is recorded as a **denial** that macOS never revisits.

**Fix:** System Settings -> Privacy & Security -> Allow accessories to connect ->
**"Automatically When Unlocked"**, then replug. `"Ask for New Accessories"` cannot be
made to work with this board. There is no per-device allowlist UI; the record is
root/SIP-protected.

**Already ruled out - do not retry:** double-tap RESET (resets the RA4M1, not the
ESP32-S3 that owns USB; identity never changes), power-cycling (the denial is
persistent), `upload_protocol = cmsis-dap` (OpenOCD fails with `unable to find a
matching CMSIS-DAP device` because there are no interfaces to claim - explicit
VID/PID `0x2341 0x1002` and the exact serial both fail identically), newer OpenOCD,
Arduino IDE / arduino-cli.

## Architecture notes

- **Servo control is source-agnostic.** `activePct` is resolved from whichever input
  spoke last (`controlSource`), then converted to frequency. The oscillator itself -
  phase accumulation, `MAX_DEVIATION` scaling, clamps - is unchanged from the original
  knob-only firmware and should stay that way. Phase accumulation (not `sin(f*t)`) is
  deliberate: it stops the servo jumping when the frequency changes.
- **Everything runs in one `loop()`**, including HTTP. Any blocking there stalls the
  servo and the matrix. The request read is bounded at 1s for exactly this reason.
  Keep new work non-blocking.
- **Do not switch the matrix to `ArduinoGraphics`.** Its `endText(SCROLL_LEFT)` blocks
  for the whole scroll and would freeze the servo. The hand-rolled sliding-window
  renderer is non-blocking on purpose.
- **The font is hand-built**, 5x8, covering digits, `A-Z`, `. - : %`. Anything else
  renders blank. Extend `font[]` and `glyphIndex()` together.
- **`buildMegaBuffer()` is bounds-checked** against `MEGA_COLS` (256). It previously
  overran a 150-column buffer. Keep the guard.
- **AP mode never reports `WL_CONNECTED`** - it reports `WL_AP_LISTENING` /
  `WL_AP_CONNECTED`. Use `netReady()`, never a bare status comparison, or the server
  will never start in the field.
- **AP+STA cannot run concurrently.** WiFiS3 hardcodes `CWMODE=1` in `begin()` and
  `CWMODE=2` in `beginAP()`; mode 3 is never set. The ESP32-S3 supports it, the library
  does not expose it.
- **Network switches must be deferred out of the HTTP handler** - tearing down the
  connection mid-response makes the request appear to fail. Set a flag, act in `loop()`.
