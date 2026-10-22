# PlatformIO extra script: pick the ESP32 devkit's serial port by its USB id.
#
# The devkit's CH340 USB-serial chip (VID 1A86, PID 7523) moves between COM
# numbers (COM8 one day, COM9 the next), and this machine also has several
# Bluetooth COM ports that PlatformIO would otherwise try and hang on.
# So: find the CH340 at build time and hand that port to the uploader, and
# fail with a clear message when the board is not plugged in.
#
# The monitor target (pio run -t monitor, "Upload and Monitor" in VS Code)
# does not read the SCons environment, but PlatformIO's own auto-detection
# prefers a port with a known USB id over Bluetooth ports, so leaving
# monitor_port unset in platformio.ini makes it land on the same device.

Import("env")  # noqa: F821  (SCons injects this)

from serial.tools import list_ports

CH340 = (0x1A86, 0x7523)
targets = [str(t) for t in COMMAND_LINE_TARGETS]  # noqa: F821

def find_devkit():
    for p in list_ports.comports():
        if p.vid == CH340[0] and p.pid == CH340[1]:
            return p.device
    return None

if any(t in ("upload", "monitor") for t in targets):
    port = find_devkit()
    if port:
        env.Replace(UPLOAD_PORT=port)
        print(f"[find_port] ESP32 devkit (CH340) on {port}")
    else:
        others = ", ".join(p.device for p in list_ports.comports()) or "none"
        print("[find_port] No CH340 USB-serial device found. Is the board on a DATA cable?")
        print(f"[find_port] Serial ports present: {others}")
        env.Exit(1)
