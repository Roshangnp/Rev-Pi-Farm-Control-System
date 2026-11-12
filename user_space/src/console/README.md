# Farm console (`ro_console`)

Keyboard-only terminal UI for the whole farm, in the style of `raspi-config`. It runs on
emerald-server (or the dev PC) only: the RevPis are self-contained and run no console.

It is an ordinary user-space program of the `host_pc` config, like `main_host_pc`: every device
comes from `config.h` (bound in `config_host_pc.cpp`: the RevPis' devices as `remote_*` over
port 47100, the WiseConn zones and soil probes as `wiseconn_*` from the WiseConn API) and the RO
from `ro_config.h`. So anything the console can do, any host_pc program can do too.

```
ro_console        # no options: the machines are set in config_host_pc.cpp
```

On emerald-server: `~/tools/Rev-Pi-Farm-Control-System/build/ro_console`, inside tmux over SSH.

## Tabs

| Tab | Shows |
|---|---|
| Main-Dashboard | tiles: RO state, running procedure, a Start button per procedure and one Stop; water tank floats; water levels; well 4 and irrigation pumps; WiseConn irrigation zones (ON/off, read only); WiseConn soil probes (moisture, EC, temperature per depth) |
| RO-Dashboard | tiles: RO control (as on Main-Dashboard plus settling, panel toggle switches and a state override), chemical tank floats, pressures, flows, conductivity (TDS), RO pumps, CIP heater (switch + CIP temp), valves |

Dashboard tiles are laid out in rows as wide as the screen allows (one column at 80x24, where
the dashboard scrolls to the focused control); the tile holding the focus has a heavy border. "Running" comes from `readROProcedure()`. The header shows each RevPi
`OK` while it answers (RO RevPi: the RO state reads; upper tank RevPi: its high float reads).

The devices on each tile are listed in `console_devices.cpp` as `config.h` references
(`DEVICE(feed_pump)`); add a line there to show a new one. It must be bound in
`config_host_pc.cpp`, or the console fails to link. The console never schedules irrigations.
WiseConn's on/off list refreshes every 10 s; soil probes report every 15 min (a read fails once
the data is 45 min old). Log messages go to `~/.cache/ro_console.log` instead of the screen.

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
- The machine running it must be allowed through the RevPis' firewall on port 47100 (see
  CLAUDE.md), and needs `wiseconn_keys.txt` in the repo root when building (without it the
  WiseConn tiles show `---`).
- Needs a UTF-8 terminal, at least 80x24.

## Build

Only with `-DDEVICE_CONFIG=host_pc`: `cmake --build build --target ro_console`. FTXUI (v6.1.9) is
fetched by CMake on the first configure, so that needs internet access. The RevPi configs don't
build it.
