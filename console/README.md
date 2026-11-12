# Farm console (`ro_console`)

Keyboard-only terminal UI for the RO RevPi and the upper tank, in the style of `raspi-config`.
It is a client of the RevPis' line servers (port 47100): it never touches
hardware itself, so the same program runs on the RO RevPi's HDMI screen, or over SSH on
emerald-server or the dev PC.

```
ro_console [--ro HOST] [--upper HOST]     # defaults: --ro 192.168.1.20 --upper 192.168.1.21
```

On the RO RevPi itself use `--ro 127.0.0.1`, so the RO tabs keep working when the network is down
(only the upper tank values go to `---`).

## Tabs

| Tab | Shows |
|---|---|
| Main-Dashboard | tiles: RO state, running procedure, a Start button per procedure and one Stop; water tank floats; water levels; well 4 and irrigation pumps (WiseConn to come) |
| RO-Dashboard | tiles: RO control (as on Main-Dashboard plus settling, panel toggle switches and a state override), chemical tank floats, pressures, flows, conductivity (TDS), RO pumps, CIP heater (switch + CIP temp), valves |

Dashboard tiles are laid out in rows as wide as the screen allows (one column at 80x24, where
the dashboard scrolls to the focused tile); Up/Down walk through every tile, the focused one has a
heavy border. "Running" comes from `ro readProcedure` (an older RO RevPi build shows `---`).

The devices on each tab are listed in `src/console_devices.cpp`; add a line there to show a new
one (the name the owning RevPi registers in its device server).

## Keys

- Up/Down: pick a tab, or a control inside a tab
- Right/Left (or Tab/Shift+Tab): move between the tab list and the tab
- Enter: press a button, open a list (Enter again picks)
- Type a number in a `[    ]` box, then Right and Enter on Send
- Every change asks Yes/No first (No is selected, Esc cancels)
- Ctrl+C: quit

Values refresh twice a second. `---` in red = the read failed: the owner couldn't read the
sensor, the value is more than 3 s old (network stall), or the link is down (header shows
`DOWN`). Reads never block; a command waits up to 1.5 s for its reply.

## Things to know

- Nothing is undone when a console exits (Ctrl+C, SSH dropped): pumps, loads and valves it
  switched stay as they are (the device server has no watchdog since 2026-11-11). Manual pump
  and valve control is a precaution; the RO controller only reapplies its pumps and valves on
  its next state change, and nothing stops a manual change from fighting a running procedure.
- Several consoles and the RO controller can be used at the same time; the RevPi
  handles each request in turn.
- The machine running it must be allowed through the RevPis' firewall on port 47100 (see
  CLAUDE.md); loopback on the RO RevPi always is.
- Needs a UTF-8 terminal, at least 80x24.

## Build

Built with every `DEVICE_CONFIG` (it doesn't use the machine config): `cmake --build build
--target ro_console`. FTXUI (v6.1.9) is fetched by CMake on the first configure, so that needs
internet access.

## HDMI screen on the RO RevPi

`farm-console.service` runs it on tty1 (the HDMI console) and restarts it if it exits:

```
sudo cp console/farm-console.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable --now farm-console
```

A bigger console font helps from a distance: `sudo dpkg-reconfigure console-setup`
(e.g. Terminus, 16x32).
