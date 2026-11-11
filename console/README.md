# Farm console (`ro_console`)

Keyboard-only terminal UI for the RO RevPi and the upper tank, in the style of `raspi-config`.
It is a client of the RevPis' line servers (port 47100), like the web UI: it never touches
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
| RO-Pressure-Sensors | the 11 RO pressures (psi) |
| RO-Flow-Sensors | feed, permeate, concentrate flow (gpm), feed/permeate conductivity, CIP temperature |
| Tank-Water-Levels | feed tank and upper tank level |
| Chemical-Tank-Floats | high pH, low pH, ASA, CIP low/mid/high |
| Water-Tank-Floats | feed tank low, permeate tank high, upper tank high |
| Manual-Pump-Control | pumps On/Off (RO pump speed 0-1, irrigation pump pressure setpoint), CIP heater |
| Manual-RO-Valve-Control | the 7 RO valves Open/Close |
| RO-Control | RO state and settling, panel toggle switches, set state, run/stop a procedure, RO pump speed |

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

- Each console is its own client, so the device server's watchdog applies: when the console
  exits (Ctrl+C, SSH dropped), pumps, loads and valves that *this console* switched on go off /
  close. RO state changes and procedures are not affected. Run it inside `tmux` over SSH so a
  dropped connection doesn't end it.
- Several consoles, the web UI and the RO controller can be used at the same time; the RevPi
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
