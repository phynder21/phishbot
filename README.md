# phishbot

Servo oscillator for the **Arduino UNO R4 WiFi**. A sine-driven servo whose speed is set
either by a Modulino knob or over WiFi, with live status on the board's 12x8 LED matrix
and a web dashboard.

- Servo oscillates +/-35 degrees around neutral (90) at 0-2 Hz
- Speed set by the physical knob **or** by HTTP - whichever moved last wins
- Joins your home WiFi, or hosts its own network when that isn't in range
- LED matrix shows live status and echoes incoming commands

---

## Mac setup

Do these **in order**. Step 2 is the one that will otherwise cost you an afternoon.

### 1. PlatformIO

This is a PlatformIO project, not an Arduino IDE sketch. Install the
**PlatformIO IDE** extension in VS Code (`platformio.platformio-ide`); it is already
listed in `.vscode/extensions.json`. The Arduino IDE is *not* needed and not used.

The CLI lives at `~/.platformio/penv/bin/pio` and is **not on `$PATH`** by default.
Either use the full path or the VS Code status-bar buttons.

### 2. Allow the board to connect over USB  <-- READ THIS

On **Apple Silicon Macs running macOS Ventura or later**, macOS blocks USB accessories
until approved. The approval prompt for this board appears and is withdrawn almost
immediately - too fast to click - and **an unanswered prompt is recorded as a denial**.
macOS then never asks again; it silently refuses to connect the board forever after.

Set this before plugging the board in:

> **System Settings -> Privacy & Security -> Allow accessories to connect
> -> "Automatically When Unlocked"**

`"Ask for New Accessories"` (the macOS default) is effectively unusable with this board,
because the prompt cannot be caught. `"Always"` also works but is less safe - it permits
accessories while the Mac is locked or closed.

There is no per-device allowlist UI in macOS. The approval record exists but is
root/SIP-protected with no supported way to inspect or edit it.

See [Troubleshooting](#usb-upload-fails) for how to recognise this failure.

### 3. Credentials

Create `src/config.h` (git-ignored, never commit it):

```c
#pragma once

// Home networks, tried in order at boot. Add WIFI_SSID3/WIFI_PASS3 for a third.
#define WIFI_SSID  "your-network"
#define WIFI_PASS  "your-password"

#define WIFI_SSID2 "your-second-network"
#define WIFI_PASS2 "your-second-password"

// The board's own access point, used when none of the above are in range.
// WPA2 requires a passphrase of at least 8 characters.
#define AP_SSID    "PHISHBOT"
#define AP_PASS    "your-ap-password"
```

All five are required — `main.cpp` deliberately `#error`s rather than shipping a
default password, so that no credential of any kind lives in this repository.

---

## Build and upload

```bash
~/.platformio/penv/bin/pio run                 # build
~/.platformio/penv/bin/pio run -t upload       # build + flash
~/.platformio/penv/bin/pio device monitor      # serial, 115200
```

In VS Code: the status-bar **->** uploads, **✓** builds, the plug icon opens the monitor.

Port auto-detection works once step 2 above is done. The board appears as
`/dev/cu.usbmodem<SERIAL>2`.

---

## Networking

The board picks a mode at boot and tells you which on the LED matrix and dashboard.

**At home** - joins `WIFI_SSID`, takes a DHCP address. To find it: the matrix scrolls
the IP for 20 seconds on connect, **or press the knob** any time to redisplay it.

**In the field** - if `WIFI_SSID` isn't found within 15 seconds, the board hosts its own
access point:

```
Network:   AP_SSID   as set in src/config.h
Password:  AP_PASS   as set in src/config.h
URL:       http://192.168.4.1
```

Your phone loses internet while joined to it - it's the Arduino, not a router.

> The UNO R4 WiFi **cannot** run both modes at once. WiFiS3 hardcodes `CWMODE=1`
> (station) in `WiFi.begin()` and `CWMODE=2` (softAP) in `WiFi.beginAP()`, and never
> sets mode 3. The ESP32-S3 chip supports concurrent AP+STA, but the library does not
> expose it. Calling one mode replaces the other.

---

## HTTP API

Plain GETs, so they work from the dashboard, a browser bar, `curl`, or a phone shortcut.

| Endpoint | Effect |
|---|---|
| `/` | Dashboard |
| `/on` | Start oscillating at the current setpoint |
| `/off` | Stop; servo parks at 90 deg. Setpoint is remembered |
| `/center` | Stop, reset phase, drive to 90 deg immediately |
| `/speed?v=0..100` | Set speed. Any value > 0 also starts it |
| `/led?v=0` / `/led?v=1` | Blank / restore the LED matrix |
| `/ip` | Re-scroll the IP address on the matrix |
| `/status` | Machine-readable state (see below) |

```bash
curl "http://192.168.4.1/speed?v=50"
```

`/status` returns fields joined by `|~|`:
`speed | angle | running | source | ledOn | matrixText | network | log`

---

## Troubleshooting

### USB upload fails

**Symptom:** upload auto-detects `/dev/cu.Bluetooth-Incoming-Port` and fails with
`No device found`. No `/dev/cu.usbmodem*` port exists.

**Diagnose:**

```bash
ioreg -r -n "UNO WiFi R4 CMSIS-DAP" -l -w0 -d 3 | grep -E "^\+-o"
```

Blocked - note `!registered, !matched` and **no child interfaces**:

```
+-o UNO WiFi R4 CMSIS-DAP@01100000  <class IOUSBHostDevice, !registered, !matched, ...>
```

Healthy - `registered, matched` plus three interfaces:

```
+-o UNO WiFi R4 CMSIS-DAP@01100000  <class IOUSBHostDevice, registered, matched, ...>
  +-o TinyUSB CMSIS-DAP@0   bInterfaceClass = 3    (HID / debug)
  +-o TinyUSB CDC@1         bInterfaceClass = 2    (serial)
  +-o IOUSBHostInterface@2  bInterfaceClass = 10   (CDC data)
```

**Fix:** macOS accessory restriction - see [Mac setup step 2](#2-allow-the-board-to-connect-over-usb----read-this).

**Do not bother with** (all confirmed useless for this failure):

- *Double-tapping RESET.* RESET resets the RA4M1. The USB interface belongs to the
  ESP32-S3, which RESET cannot reach. The board's USB identity never changes.
- *Power-cycling.* The blocked state survives unplugging; it is a stored decision,
  not a stuck chip.
- *`upload_protocol = cmsis-dap`.* OpenOCD reports `unable to find a matching CMSIS-DAP
  device` because there are no interfaces to claim - not because of a version or filter
  problem. Explicit VID/PID (`0x2341 0x1002`) and the exact serial both fail identically.
- *Installing a newer OpenOCD, Arduino IDE, or arduino-cli.* None of them can attach to
  an unconfigured device either.
- *Swapping cables.* Worth one try, but a blocked device still enumerates its descriptor,
  so the data path is demonstrably fine.

### Nothing on the LED matrix

The font covers digits, `A-Z`, and `. - : %` only. Any other character renders blank.
A completely dark panel while powered means the RA4M1 isn't running - reflash it.

### Board joins the wrong network

`WIFI_SSID` not in range for 15 seconds triggers AP mode. Check `src/config.h` and watch
the serial monitor: it logs `Joined '<ssid>'` or `No '<ssid>' in range. Starting own network...`.
