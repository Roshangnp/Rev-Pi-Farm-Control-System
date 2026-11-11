# ro_newton_test

Holds permeate flow at 30 gpm in `NORMAL_OP` by Newton iteration on the RO pump speed, and logs the
RO sensors to a CSV at 1 Hz. It tries the controller out before any of it goes into the `ro` class.
Runs on the RO RevPi (192.168.1.20) against the real hardware. It's a standalone CMake project that
pulls in the repo's libraries with the `ro` config, so the main build doesn't compile it.

## Build and run

On the RevPi, from the repo root:

```sh
cmake -S tests/ro_newton_test -B tests/ro_newton_test/build
cmake --build tests/ro_newton_test/build -j4 --target ro_newton_test

sudo systemctl stop farm-ro
tests/ro_newton_test/build/ro_newton_test     # or give it a filename.csv
sudo systemctl start farm-ro
```

The test refuses to start while `farm-ro` is running: it opens port 47100 first, and exits without
touching any outputs if the port is taken.

## Controller

The model comes from the step test, with `y` the permeate flow in pulses/min and `u` the RO pump speed:

```
y = a*u + b       a = 2210, b = 2678
r = 30 gpm * 133.17 pulses/gal = 3995 pulses/min
```

K only converts the target. The test logs and prints the same flow in pulses/min.

1. Drive the RO to `OFF`, set the RO pump speed to 0, move to `NORMAL_OP`
2. Wait `VALVE_SETTLE_MS` (10 s) for the valves, then the feed and RO pumps start
3. Hold speed 0 for 40 s while the startup flow settles
4. Feedforward: move to `u = (r - b)/a`, which is 0.596
5. Repeat for 5 minutes, counted from the feedforward move:
   - Move to the speed target at up to 0.1 speed/s
   - Wait 5 s
   - Average the flow over 5 s
   - Apply `u += (r - y)/a`, skipped if `|r - y|` is 100 pulses/min or less
6. Set the speed to 0 and the RO to `OFF`

A correction is also skipped, and the speed held, if a flow read failed in the window or the average
is below 1500 pulses/min. The speed target is clamped to 0-0.7, because 0.7 is the highest speed run
so far. A run takes about 6 minutes. The constants are at the top of `ro_newton_test.cpp`.

## Output

Each correction prints a line, so the test can be followed from the terminal:

```
time_s = 72.03: 3840 pulses/min (28.84 gpm), error 155, speed target 0.666
```

The CSV is written once a second to the given filename, or `ro_newton_test_<date>_<time>.csv` in
the directory the test runs from, and flushed each row. It has the same columns as
`ro_step_test`, so the MATLAB plot works on it too, with two more at the end:

| Column | Meaning |
|---|---|
| `speed_target` | Speed the controller is moving to. `ro_speed_command` is the speed sent to the VFD |
| `window_pulses_per_min` | Average flow used for a correction, empty on every other row |

## Early stops

The test sets the speed to 0 and the RO to `OFF`, then exits, on any of these:

- Ctrl-C, SIGTERM or the SSH session closing (exit code 0)
- Primary pressure above 250 psi, or unreadable (exit code 1)
- A VFD alarm on the feed or RO pump (exit code 1)
- A failed speed command or pump start (exit code 1)
- The RO leaving `NORMAL_OP`, e.g. set to `OFF` from the web UI (exit code 1)

## Before running

- No float is checked: nothing stops the pumps if the feed tank runs dry or the permeate tank fills
- A crash or `kill -9` skips the shutdown, so the relays stay as they were until `farm-ro` starts
  and drives them to `OFF`

## Web UI while it runs

The test serves `ro` on port 47100 but not the device server, so the web UI's `ro` row keeps
working and setting it to `OFF` stops the test. Its speed box still sends `setRoSpeed`, which this
test overwrites on its next move, so leave it alone. The device rows show "read failed".

Flow comes from each sensor's 1 s rolling average, which the test's loop updates every pass
(`update()`), so the once-a-second reads each cover the last second whoever else reads the sensors.
