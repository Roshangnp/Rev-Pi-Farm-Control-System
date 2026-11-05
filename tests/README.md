# RO RevPi tests

Programs that run on the RO RevPi (192.168.1.20) against the real hardware. They are a
standalone CMake project that pulls in the repo's libraries with the `ro` config, so the main
build doesn't compile them.

## Build

On the RevPi, from the repo root:

```sh
cmake -S tests -B tests/build && cmake --build tests/build -j4 --target ro_pump_speed_test
```

## ro_pump_speed_test

Ramps the RO pump in `NORMAL_OP` and logs the RO sensors to a CSV.

```sh
cd ~/Rev-Pi-Farm-Control-System
sudo systemctl stop farm-ro
tests/build/ro_pump_speed_test            # or give it a filename.csv
sudo systemctl start farm-ro
```

The test refuses to start while `farm-ro` is running: it opens port 47100 first, and exits
without touching any outputs if the port is taken.

### Sequence

1. Drive the RO to `OFF`, set the RO pump speed to 0, move to `NORMAL_OP`
2. Wait `VALVE_SETTLE_MS` (10 s) for the valves, then the feed and RO pumps start
3. Ramp the speed to 0.7 at 0.2/min (3.5 minutes)
4. On reaching 0.7, set the speed to 0 and the RO to `OFF`. There is no hold at 0.7

`RAMP_END_SPEED` and `RAMP_RATE_PER_MIN` are at the top of `ro_pump_speed_test.cpp`.

### CSV

Written once a second to the given filename, or `ro_pump_speed_test_<date>_<time>.csv` in the
directory the test runs from. Each row is flushed as it's written, so a crash keeps the data so far.

| Columns | Notes |
|---|---|
| `time_s` | Seconds since the test started |
| `ro_state`, `settling` | `settling` is 1 until the pumps start |
| `ro_speed_command` | Speed sent to the VFD, 0-1. Not a measured speed |
| 11 `*_pressure_psi` | Every RO pressure transducer |
| `feed_flow_gpm`, `permeate_flow_gpm`, `concentrate_flow_gpm` | Average since the last row |
| `feed_conductivity_us_cm`, `permeate_conductivity_us_cm` | The RO has conductivity sensors, no TDS sensors |

A read that fails leaves its field empty. The flows are always empty in the first row, because
the first flow read has no previous sample.

### Early stops

The test sets the speed to 0 and the RO to `OFF`, then exits, on any of these:

- Ctrl-C, SIGTERM or the SSH session closing (exit code 0)
- A VFD alarm on the feed or RO pump (exit code 1)
- A failed speed command or pump start (exit code 1)
- The RO leaving `NORMAL_OP`, e.g. set to `OFF` from the web UI (exit code 1)

### Before running

- There is no pressure limit: nothing stops the test if pressure climbs, and the transducers are
  scaled 0-100 psi. Stay nearby with Ctrl-C ready
- `feed_tank_low_float` isn't checked: nothing stops the pumps if the feed tank runs dry
- A crash or `kill -9` skips the shutdown, so the relays stay as they were until `farm-ro` starts
  and drives them to `OFF`

### Web UI while it runs

The test serves `ro` on port 47100 but not the device server, so the web UI's `ro` row keeps
working and setting it to `OFF` stops the test. The device rows show "read failed", so pressures
can't be watched there.

### Testing so far

Run on the dev PC with a fake I/O layer and a sped-up ramp: full run, Ctrl-C, port taken, VFD
alarm, and `ro setState OFF` over the network all behaved as above. Not yet run on the real RevPi.
