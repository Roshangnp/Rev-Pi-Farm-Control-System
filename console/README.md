# Farm console (`ro_console`)

Keyboard-only terminal UI for the whole farm, in the style of `raspi-config`.
A user-space program like `main_host_pc`: it uses every device from `config.h` and the RO from
`ro_config.h`, linked against the client config (`farm_client` / `ro_client`), where every device
is a `remote_*` on a connection to its owner's `main_*`. This machine's own RevPi is reached on
`127.0.0.1`, the other one on its LAN address, and WiseConn through this machine's own `main_*`.
So it never touches hardware or runs a controller, and needs no keys or options:

```
ro_console
```

When the network is down, what this machine owns keeps working (on the RO RevPi: its devices and
the RO); the rest goes to `---`. The header clock is local time: on the RevPis run it with
`TZ=America/Los_Angeles` (farm-console.service does).

## Tabs

| Tab | Shows |
|---|---|
| Main-Dashboard | tiles: RO state, running procedure, a Start button per procedure and one Stop; water tank floats; water levels; well 4 and irrigation pumps; WiseConn irrigation zones (ON/off, read only); WiseConn soil probes (moisture, EC, temperature per depth) |
| RO-Dashboard | tiles: RO control (as on Main-Dashboard plus settling, panel toggle switches and a state override), chemical tank floats, pressures, flows, conductivity (TDS), RO pumps, CIP heater (switch + CIP temp), valves |

Dashboard tiles are laid out in rows as wide as the screen allows (one column at 80x24, where
the dashboard scrolls to the focused control); the tile holding the focus has a heavy border. "Running" comes from `ro readProcedure` (an older RO RevPi build shows `---`).

The devices on each tab are listed in `src/console_devices.cpp` as `DEVICE(name)` (the `config.h`
name and device); add a line there to show a new one. It fails to link if the client config
(`API/src/config/client_config.cpp`) doesn't bind it. The console never schedules irrigations. Zone on/off
refreshes about every 10 s; soil probes report every 15 min (a read fails once the data is 45 min
old). Log messages go to `~/.cache/ro_console.log` instead of the screen.

Header: `RO@<host> OK` while the RO state reads, `Upper tank@<host> OK` while the upper tank high
float reads (hosts from the client config), so `DOWN` means that machine's `main_*` or the link to it.

## Keys

- Arrow keys on a dashboard go by screen position: Up/Down to the next line of controls in the
  tile, then into the tile above/below (the one under your column, kept over shorter rows),
  Left/Right to the next control on the line, then into the tile beside it (its control closest
  to your line), so a tile with only one row of buttons is never skipped. Read-only tiles are
  one stop each. Left with nothing to the left goes back to the tab list. In a text box Left/Right
  move the cursor first; an open list keeps the arrows until Enter or Esc closes it
- In the tab list: Up/Down pick a tab, Right goes into it
- Tab/Shift+Tab still walk every control in order
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
- The machine running it must be allowed through the other RevPi's firewall on port 47100 (see
  CLAUDE.md); loopback always is.
- Needs a UTF-8 terminal, at least 80x24.

## Build

Built with every `DEVICE_CONFIG` (the config only picks the addresses): `cmake --build build
--target ro_console`. No `wiseconn_keys.txt` needed. FTXUI (v6.1.9) is fetched by CMake on the first configure, so that needs
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
