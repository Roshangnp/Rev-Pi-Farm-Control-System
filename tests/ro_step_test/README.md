# ro_step_test

Holds the RO in `NORMAL_OP` at speed 0, steps the RO pump speed to 0.5, and logs the RO sensors to
a CSV at 1 Hz. It measures how fast the flow responds to a speed change. Runs on the RO RevPi
(192.168.1.20) against the real hardware. It's a standalone CMake project that pulls in the repo's
libraries with the `ro` config, so the main build doesn't compile it.

## Build and run

On the RevPi, from the repo root:

```sh
cmake -S tests/ro_step_test -B tests/ro_step_test/build
cmake --build tests/ro_step_test/build -j4 --target ro_step_test

sudo systemctl stop farm-ro
tests/ro_step_test/build/ro_step_test     # or give it a filename.csv
sudo systemctl start farm-ro
```

The test refuses to start while `farm-ro` is running: it opens port 47100 first, and exits without
touching any outputs if the port is taken.

## Sequence

1. Drive the RO to `OFF`, set the RO pump speed to 0, move to `NORMAL_OP`
2. Wait `VALVE_SETTLE_MS` (10 s) for the valves, then the feed and RO pumps start
3. Hold speed 0 for 3 minutes, counted from the pumps starting. The VFD treats a speed of 0 as 50%
   throttle, so the pump is running
4. Step to 0.5 and hold for 3 minutes
5. Set the speed to 0 and the RO to `OFF`

A run takes about 6 minutes 10 seconds. `STEP_SPEED`, `BASELINE_TIME` and `STEP_HOLD_TIME` are at
the top of `ro_step_test.cpp`.

## CSV

Same columns as `ro_pump_speed_test` (see `tests/ro_ramp_test/README.md`), so its MATLAB plot works
on this file too. Written once a second to the given filename, or
`ro_step_test_<date>_<time>.csv` in the directory the test runs from, and flushed each row. A read
that fails leaves its field empty, and the flows are always empty in the first row.

## Early stops

The test sets the speed to 0 and the RO to `OFF`, then exits, on any of these:

- Ctrl-C, SIGTERM or the SSH session closing (exit code 0)
- A VFD alarm on the feed or RO pump (exit code 1)
- A failed speed command or pump start (exit code 1)
- The RO leaving `NORMAL_OP`, e.g. set to `OFF` from the web UI (exit code 1)

## Before running

- There is no pressure limit: nothing stops the test if pressure climbs, and the transducers are
  scaled 0-100 psi. Stay nearby with Ctrl-C ready
- `feed_tank_low_float` isn't checked: nothing stops the pumps if the feed tank runs dry
- A crash or `kill -9` skips the shutdown, so the relays stay as they were until `farm-ro` starts
  and drives them to `OFF`

## Web UI while it runs

The test serves `ro` on port 47100 but not the device server, so the web UI's `ro` row keeps
working and setting it to `OFF` stops the test. The device rows show "read failed", so pressures
can't be watched there.
