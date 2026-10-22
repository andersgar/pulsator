# Pulsator

ESP32 firmware for a pump-driven party prop: a phone page starts a countdown,
the LED strip and a piezo build up a heartbeat that gets faster and faster,
and at zero the pump fires a series of spurts. There is an "Ejaculate now"
button, a Stop button, and a debug view that shows exactly what the ESP32 is
driving on every pin, so the whole thing can be rehearsed with no hardware
attached.

Contents

1. [Parts and wiring](#1-parts-and-wiring)
2. [Install PlatformIO in VS Code](#2-install-platformio-in-vs-code)
3. [Open, build and flash](#3-open-build-and-flash)
4. [Serial monitor](#4-serial-monitor)
5. [Using the phone page](#5-using-the-phone-page)
6. [Settings and how to change the LED count](#6-settings-and-how-to-change-the-led-count)
7. [What happens, phase by phase](#7-what-happens-phase-by-phase)
8. [Testing without hardware](#8-testing-without-hardware)
9. [Troubleshooting](#9-troubleshooting)
10. [Code map](#10-code-map)

---

## 1. Parts and wiring

| Part | Notes |
|------|-------|
| ESP32 devkit (CH340 USB-serial) | The board with the micro-USB and the CH340 chip, same type as the Bombe. |
| Relay module, 1 channel, 5 V coil | The common blue-relay board with VCC, GND and IN pins. Most are **active low**: IN pulled low switches the relay on. |
| 12 V pump + 12 V adapter | The pump gets its own 12 V. It never touches the ESP32's 5 V. |
| WS2812B LED strip, 5 V | Any length up to 300 LEDs. Count is a setting. |
| Passive piezo disc, 2 wires | Optional. Glue its rim to the box wall for volume. |
| 330 Ω resistor | In series with the strip's data line, at the strip end. |
| 1000 µF capacitor (optional) | Across the strip's 5 V and GND at the strip end if it is long. |
| 1N4007 diode (if the pump is driven directly) | Across the pump terminals, band to +12 V. Most relay modules already have one across the coil, this one is for the pump motor. |

### ESP32 pins

All pin numbers are the `D` numbers printed on the devkit. They are defined at
the top of `src/config.h` if they need to move.

| ESP32 pin | Goes to | Notes |
|-----------|---------|-------|
| D25 | relay module **IN** | `PIN_RELAY` |
| 5V (VIN) | relay module **VCC** | the relay coil needs 5 V |
| GND | relay module **GND** | |
| D13 | 330 Ω, then strip **DIN** | `PIN_LED_DIN` |
| D27 | piezo terminal 1 | `PIN_PIEZO_P` |
| D26 | piezo terminal 2 | `PIN_PIEZO_N`, driven in antiphase for volume |
| D2 | nothing | the devkit's own blue LED, lights whenever the pump is on |

### Relay contacts to the pump

```
12 V adapter +  ──────── relay COM
relay NO       ──────── pump +
pump −         ──────── 12 V adapter −
```

Use the **NO** (normally open) contact so the pump is off whenever the relay is
not energised, including when the ESP32 is unpowered or rebooting.

### Strip power

The strip draws up to 60 mA per LED at full white. For 60 LEDs that is more
than a USB port will give. Feed the strip's 5 V and GND from a 5 V supply or
powerbank of 2 A or more, join its GND to the ESP32's GND, and let the ESP32
supply only the data line. The firmware caps the strip at 1.5 A in software
(`STRIP_MAX_MA`), which dims white but never the pink show.

### Keep the liquid away from the electronics

Put the ESP32, relay and supplies in a separate compartment from the pump
and tubing, higher up than anything that can leak. A dripping tube onto a live
relay board ends the party early.

---

## 2. Install PlatformIO in VS Code

1. Install **Visual Studio Code** from https://code.visualstudio.com/.
2. In VS Code, open the Extensions view (Ctrl+Shift+X), search **PlatformIO IDE**, click Install.
   The first install downloads a few hundred MB and can take several minutes; the
   status bar at the bottom says "Installing PlatformIO IDE" until it is done.
   Do not open other folders while it installs. When finished, a small alien
   head icon appears in the left activity bar and a house icon in the status bar.
3. Install the **CH340 USB-serial driver** if Windows does not recognise the
   board. Plug the ESP32 in and open Device Manager: under "Ports (COM & LPT)"
   there should be "USB-SERIAL CH340 (COMx)". If it shows up with a warning
   triangle or not at all, install the driver from the WCH site
   (search "CH341SER driver"), then unplug and replug the board.

Nothing else is needed. The ESP32 platform, the toolchain and the FastLED
library are downloaded automatically the first time the project is built.

---

## 3. Open, build and flash

1. In VS Code: **File → Open Folder** and pick this `pulsator` folder, the one
   containing `platformio.ini`. VS Code may ask whether to trust the folder; say yes.
2. Wait until the status bar stops saying "PlatformIO: Loading tasks" and shows
   the build (✓), upload (→) and monitor (plug) icons.
3. Plug the ESP32 in with a **data** USB cable. Many micro-USB cables are
   charge-only and will power the board without the PC ever seeing it.
4. Click the **→ (Upload)** icon in the status bar, or press Ctrl+Alt+U.

The first build takes two to five minutes while the toolchain downloads. The
terminal should end with:

```
[find_port] ESP32 devkit (CH340) on COM7
...
Hash of data verified.
Leaving...
Hard resetting via RTS pin...
========================= [SUCCESS] ==========================
```

The COM number does not matter and is not configured anywhere: `find_port.py`
finds the CH340 by its USB id every time. If it prints
`No CH340 USB-serial device found`, the board is not on a data cable or the
driver is missing (see section 2).

If the upload hangs at `Connecting........_____....`, hold the **BOOT** button
on the devkit until the dots turn into "Writing at 0x...", then release.

---

## 4. Serial monitor

Click the **plug icon (Monitor)** in the status bar or press Ctrl+Alt+S. It
opens at 115200 baud. The board prints what it does, and takes commands: type
a letter and press Enter.

| Command | What it does |
|---------|--------------|
| `h` | help |
| `s [secs]` | start the countdown, optional length (`s 20`) |
| `c` | climax now |
| `x` | stop |
| `p [ms]` | pump test pulse, default 300 ms (blocked in dry run) |
| `l` | lights test: a white chase and two pink flashes |
| `t` | piezo sweep 400 to 3000 Hz. Note the loudest pitch and set it as the base tone on the page |
| `d` | dump the full status line |
| `D` | toggle dry run (relay never energised) |
| `F` | reset all settings to the `config.h` defaults and reboot |

**Close the monitor before uploading.** Only one program can hold the COM
port, and "port is busy" on upload almost always means the monitor is open.
In VS Code the monitor is a terminal tab; click its trash-can icon or press
Ctrl+C in it.

---

## 5. Using the phone page

1. On the phone, join Wi-Fi **Pulsator**, password **pulsator1234**
   (both in `src/config.h`). The board makes this network itself; no router needed.
2. Open **http://192.168.4.1/** . Most phones pop the page up on their own as a
   sign-in page.

The top card shows the phase, the countdown, a heartbeat that pulses at the
real beat rate, and a progress bar.

- **Start countdown** uses the seconds in the box next to it (default from settings).
- **Ejaculate now** jumps straight to the climax from any phase.
- **Stop** cuts the pump immediately and goes back to idle, from any phase.

The second card is the **debug view**: pump on/off and the actual level on the
relay pin, the strip's average colour as a swatch plus brightness and LED
count, the piezo frequency, the current beat interval in ms and bpm, which
spurt is running, how long the pump has run since boot, and the last 20 lines
of the event log. Three buttons give a 300 ms pump pulse, a lights test and a
piezo sweep. The page polls the board five times a second.

The third card, **Settings**, is described below.

---

## 6. Settings and how to change the LED count

Every setting is on the page under Settings and takes effect the moment you
press Save. They are stored in the ESP32's flash and survive power cycles.

| Setting | Default | What it does |
|---------|---------|--------------|
| Countdown (s) | 60 | buildup length used by Start countdown |
| Afterglow (s) | 10 | warm fade after the climax |
| Spurts | 7 | number of pump bursts at the climax (3 to 12) |
| Spurt strength % | 100 | scales how long each burst keeps the pump on. 50 halves them, 200 doubles them (capped at 3 s each) |
| **LED count** | 60 | how many LEDs the strip has (1 to 300) |
| Brightness | 200 | master brightness 0 to 255 |
| Piezo | on | off silences everything |
| Piezo base tone | 2800 Hz | the thump and chirp pitches are derived from this. Use `t` in the monitor to find the disc's loudest pitch |
| Relay input | active low | flip to active high if the pump runs when it should be off and vice versa |
| Dry run | off | on: the relay is never energised, everything else runs. For the bench |

**LED count, two ways to set it:**

- On the page: Settings → LED count → Save. Immediate, no reflash.
- As the compiled default: `DEFAULT_NUM_LEDS` in `src/config.h`. This only
  applies on a fresh board or after `F` in the monitor, because the saved value
  wins once it exists.

The hard limit is `MAX_LEDS` (300) in `config.h`, which sizes the LED buffer.
Raise it if the strip is longer, then rebuild.

The same rule applies to every default in `config.h`: editing it and
reflashing does **not** change a board that already has saved settings. Change
it on the page, or send `F` after flashing to overwrite the saved settings with
the new defaults.

---

## 7. What happens, phase by phase

| Phase | Pump | Lights | Piezo |
|-------|------|--------|-------|
| **IDLE** | off | slow dim pink breathing | silent |
| **BUILDUP** | off | a pink swell runs from the base of the strip to the tip on every beat; the beat starts at about 40 bpm and ends near 230 bpm, speeding up more and more toward the end; the colour drifts from deep pink toward hot white and the floor brightness rises | a short thump on every beat, pitch rising with progress; a softer second thump ("lub-dub") early on |
| **HOLD** | off | frozen at full, 1.2 s | silent |
| **CLIMAX** | on during each spurt | white flash at each spurt, then a fast wave to the tip; dim pink between spurts | a rising chirp through each spurt |
| **AFTERGLOW** | off | warm amber, slow breathing, fading out | silent |

The spurt pattern: the first burst is the longest (about 650 ms) with a short
gap after it, then each burst gets shorter and each gap longer, ending around
220 ms on and 840 ms off. Seven spurts take about seven seconds.

Pump safety, enforced in the firmware regardless of anything else:

- The relay is only ever energised inside CLIMAX.
- No single burst can exceed 3 s (`PUMP_MAX_ON_MS`).
- On boot the relay pin is set to off before anything else runs.
- Stop turns the pump off in the same loop iteration, before the page even replies.

---

## 8. Testing without hardware

With only the bare ESP32 on USB:

1. Flash, open the monitor, and watch the log: it reports every phase change
   and every spurt with its length.
2. Join the Pulsator Wi-Fi and open the page. The debug card shows pump
   on/off, the relay pin level, the strip colour it is sending and the piezo
   pitch, all live. The devkit's blue LED (D2) lights whenever the pump would be on.
3. Set Dry run to on in Settings before the pump is connected for the first
   time, run a short countdown (`s 10` or the page), and read the log. Then set
   Dry run off and use the "Pump 300 ms" button to confirm the relay clicks
   the right way. If it clicks on when it should be off, flip the Relay input
   setting.

The event log and the `d` command print the same numbers the page shows, so
everything can be checked over USB alone.

---

## 9. Troubleshooting

| Symptom | Cause and fix |
|---------|---------------|
| Upload says `No CH340 USB-serial device found` | Charge-only USB cable, or no driver. Try another cable; check Device Manager for "USB-SERIAL CH340". |
| Upload says port is busy | The serial monitor is open. Close it and upload again. |
| Upload hangs at `Connecting...` | Hold BOOT on the devkit until writing starts. |
| Board reboots when the pump starts | The pump shares the ESP32's supply. Give the pump its own 12 V adapter and only switch it through the relay contacts. |
| Relay clicks, pump does nothing | Pump wired to NC instead of NO, or the 12 V adapter is off. |
| Pump runs when it should be off | Relay input polarity. Flip "Relay input" in Settings. |
| Strip dark | Data wire on the wrong end of the strip (follow the arrows on the strip), missing 330 Ω, or strip GND not joined to ESP32 GND. |
| Strip shows wrong colours | LED count set higher than the strip; harmless. Or a different strip type: change `GRB` in `stripBegin()` in `src/main.cpp`. |
| Far end of the strip orange/dim at full white | Voltage drop. Feed 5 V at both ends, or lower Brightness. |
| Piezo quiet | Find its resonance with `t` and set the base tone; glue the rim to the box wall. |
| Phone shows no page | Make sure it stayed on the Pulsator network (phones drop networks without internet). Type http://192.168.4.1/ by hand. |
| Settings do not stick | Press Save, then watch the Event log for "settings saved". |

---

## 10. Code map

| File | What it holds |
|------|---------------|
| `platformio.ini` | board, framework, FastLED dependency, port finder hook |
| `find_port.py` | finds the CH340 by USB id at upload time |
| `src/config.h` | pins, hotspot name and password, all compiled defaults, safety limits |
| `src/settings.h/.cpp` | the settings stored in flash and their clamping |
| `src/main.cpp` | pump driver with its guards, piezo, strip effects, the phase state machine, serial console |
| `src/web.h/.cpp` | hotspot, captive portal, the page, the JSON API |
| `src/pulsator.h` | the small API between the state machine and the page |

The phase logic is at the top of `src/main.cpp` under "State machine". The
spurt pattern is `buildPlan()`, the heartbeat curve is in `runBuildup()`, and
every colour derives from the hue setting through `baseColour()` and `lightColour()`, if someone wants to
change the look.
