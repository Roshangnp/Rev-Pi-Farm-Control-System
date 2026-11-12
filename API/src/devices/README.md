# devices

Every device type is an abstract class in `API/include/devices.h`, with these implementations:

- `local_devices/local_*` - wired to this RevPi, built from `electrical_interface`s
- `remote_devices/remote_*` - owned by another RevPi, reached over the network
- `wiseconn_devices/wiseconn_*` - read from the WiseConn cloud (host PC only, see below)

User code only sees the base class, so a device can move between local and remote by changing
the config alone.

## Device types

All methods return `false` on failure; reads fill an out parameter.

| Type | Methods | Local wiring |
|---|---|---|
| `pump` | `setOn`, `setOff`, `setSpeed(0-1)`, `readOn`, `readSpeed`, `readAlarm`, `hasSpeedControl`, `hasAlarm` | Run: relay or digital out. Speed (optional): 0-10 V, 4-20 mA or PWM. Alarm (optional): digital in |
| `valve` | `setOpen`, `setClosed`, `readOpen` | Relay or digital out, on = open |
| `switched_load` | `setOn`, `setOff`, `readOn` | Relay or digital out (lights, heater) |
| `float_switch` | `readSubmerged` | Digital in, on = submerged |
| `pressure_transducer` | `readPressure` (psi) | 4-20 mA or 0-10 V |
| `tds_sensor` | `readTds` (ppm) | 4-20 mA or 0-10 V |
| `conductivity_sensor` | `readConductivity` (uS/cm) | 4-20 mA or 0-10 V |
| `temperature_sensor` | `readTemperature` (deg F) | 4-20 mA or 0-10 V |
| `level_sensor` | `readLevel` (units of its range) | 4-20 mA or 0-10 V |
| `flow_sensor` | `readFlow` (gpm) | DIO counter, pulses per gallon |

Behaviour worth knowing:

- Analog sensors scale linearly from the min/max given in the config. A 4-20 mA signal below
  3.5 mA is treated as a broken wire and the read fails.
- `readOn`, `readSpeed` and `readOpen` read back the output as last written, not a feedback signal.
- `pump::setSpeed` rejects values outside 0-1. Speed 0 is 4 mA / 0 V, which most VFDs treat as
  minimum speed, not stop: the run output stops the pump.
- `pump::readAlarm` fails safe: it returns true if the alarm input can't be read.
- `flow_sensor::update()` must be called every main loop pass on the RevPi that owns the sensor
  (does nothing on a remote sensor). It keeps a 1 s rolling average; `readFlow` only returns it, so
  readers never affect each other. `readFlow` fails for the first second, after a counter reset,
  if the last update couldn't read the input, or if `update()` hasn't run for 3 s.
- `push_button::readPressTotal` is the raw press counter (reads don't change it).
  `readPushButtonCount` returns presses since that object's previous call, so each machine (each
  `remote_push_button`) counts on its own; code sharing one object shares its count.
- Polarity is fixed (on = submerged / open / alarm). A normally-closed float or alarm contact
  needs an "inverted" option that doesn't exist yet.

## Remote devices

`remote_link` (`comms/src/`) is one TCP connection to another RevPi (port 47100), shared by all the `remote_*`
devices it owns (and `remote_ro`). A background thread connects (retrying every 1 s) and asks
the owner for a `snapshot` of every readable value every 0.5 s; reads answer from that cache at
once and never wait on the network. A value older than 3 s (a stall), or one the owner couldn't
read, is a failed read; when the link drops (the owner restarted) the cache is cleared and reads
fail at once. In the first 2 s after the link is created, reads wait for the first snapshot.
The snapshots also keep the owner from idling the connection out.

Commands (`setOn`, `setSpeed`, `setState`, ...) go over the same connection and wait up to
1.5 s for their own reply; a late reply is discarded when it arrives, and while disconnected
commands fail at once. A successful command triggers a fresh snapshot, so its readback shows up
within a round trip. The connection is only dropped after 25 s with requests outstanding and
nothing from the owner (longer than the owner's 20 s idle timeout, so short stalls are ridden
out on both ends). The timings are in `comms/src/protocol.h` (`link_timing`).

`device_server` runs on the owner, as a handler on the `line_server` (`comms/src/`) that the
owner's `main` creates: `addLocalDevices(server)` registers each local device under its name.
It is single threaded: requests are only handled inside `server.poll()`, called from the
owner's main loop, so they never run at the same time as local control code.

Protocol, one line each way:

```
<device> <method> [arg]    ->    ok [value] | fail | err <reason>
feed_pump setSpeed 0.5     ->    ok
feed_tank_low_float readSubmerged  ->  ok 1
snapshot                   ->    ok feed_pump.readOn=1;feed_flow.readFlow=4.2;cip_temp.readTemperature;...
```

A snapshot entry without `=` is a read that failed on the owner. Each `line_handler` adds its
own entries (`device_server`: every read method of every device; `ro_server`: `ro.readState`,
`ro.isSettling`).

Method names are the C++ method names. There is no authentication; the firewall must limit
port 47100 to the other farm machines.

**No watchdog** (removed 2026-11-11): a command holds after its client disconnects or goes
silent. The `line_server` still drops a client idle for 20 s, which changes nothing on the
hardware.

Reads cost microseconds (cache lookups). A command costs one round trip (~1 ms on the LAN, more
over HaLow). The RO RevPi's snapshot is about 3 KB, so each client polling it uses ~6 KB/s.

## WiseConn devices

Devices on the farm's WiseConn irrigation system, read from its cloud API
(`https://api.wiseconn.com`, `api_key` header; the scheduled-irrigation endpoints only work on this host, `apiv2.wiseconn.com` answers them with HTTP 500). Polling only sends GET requests; the only
calls that change anything are the two `scheduleIrrigation` overloads, which start real
irrigations on the live system, and `cancelIrrigation`, which deletes one. Built into every config and bound once in
`API/src/config/wiseconn_farm_devices.cpp` (needs internet): soil probes `zone_7_soil`,
`zone_8_soil`, `zone_10_3_soil`, 24 irrigation zones `valve_1_1` ... `valve_10_3`,
`overhead_1` ... `overhead_6`, `city_water_fill_valve`, and 7 flow meters `main_flow`,
`city_water_flow`, `well_1_flow` ... `well_5_flow`.

- `wiseconn_link` - one per account. A background thread fetches `GET /zones/<id>/measures`
  for each registered zone every `poll_seconds` (default 60; one HTTPS request per zone, about
  1 s each, made with the `curl` program, key passed on stdin) and caches every measure. Reads
  only look at the cache (microseconds). Once a zone's on/off is wanted it also fetches the farm's
  valve list every `hydraulics_poll_seconds` (default 10), so a zone starting or stopping shows
  within about 10 s of WiseConn knowing; a failed fetch keeps the old list for at most 1 min. A value counts while WiseConn's `lastDataDate` is within
  `max_age_minutes` (default 45; soil data arrives every 15 min). Failed polls keep old values,
  which then age out; poll failures and recoveries are logged once each.
- `wiseconn_soil_sensor` - one per probe: `readMoisture` (%), `readETC` (EC/salinity in
  WiseConn's raw "VIC" units, not uS/cm) and `readTemperature` (deg F), one value per depth,
  shallowest first (4, 8, 12 in). A read fails, leaving the vector unchanged, unless every depth
  is current.
- `wiseconn_flow_sensor` - one per flow meter (implements `flow_sensor`): `readFlow` (gal/min) is
  the latest value of its "Flow" measure, from its zone's poll. WiseConn updates them about every
  5 min; a read fails once the value is older than `max_age_minutes` (Well #2's meter has sent
  nothing since 2026-09-04, so it always fails). `update()` does nothing (WiseConn computes it).
- `wiseconn_irrigation` - one per WiseConn irrigation zone (implements `irrigation`).
  `readOn` is true while the zone's valve is "On" in `GET /farms/<farm_id>/hydraulics` (one
  request per poll for every valve; anything but On/Off fails). That list has no timestamps, so
  it counts for 3 poll intervals (at least 3 min) after the last good fetch.
  `scheduleIrrigation(startHour, startMinute, endHour, endMinute)` (today, farm local, end after
  start, start not passed) sends `POST /irrigations {"zoneId", "initTime", "endTime",
  "directSchedule": true}` (times like `2026-11-01T18:15:00`, no Z). `scheduleIrrigation(minutes)`
  (from now, 1 to 1440 min) sends `POST /irrigations {"zoneId", "duration"}`. Bad arguments are
  refused without contacting WiseConn. Each call blocks until WiseConn answers (about 1 s, at
  most 20 s), is never retried (no double scheduling), returns true only on a 2xx, and logs
  WiseConn's reply; a timeout means it may or may not have been created. After a success the
  link polls again at once so `readOn` catches up. Each success saves the new irrigation's id
  from WiseConn's reply (the first element of the returned array; replaces the previous one;
  lost on a restart): `readScheduledIrrigation(id)` returns it, `cancelIrrigation()` sends
  `DELETE /irrigations/<id>` and clears it on success. WiseConn only deletes one that is still
  in the future with status "Saved" or "Sending irrigation has failed"; a running one would need
  `PATCH /irrigations/<id>?action=stop` (not implemented). A running valve's status is "Operating" (counted as on), a stopped one "Off".
  (The deprecated web UI showed each zone as a read-only "on" indicator; nothing shows them now.)
- `wiseconn_keys` - parses `wiseconn_keys.txt` (repo root, git-ignored): `api_key`, `farm_id`,
  `time_zone`, optional `base_url` / `poll_seconds` / `hydraulics_poll_seconds` / `max_age_minutes`, then one `[name]`
  section per probe with `zone_id` and the measure ids for `moisture`, `etc` and `temperature`,
  an `[irrigations]` section of `name = zone id`, and a `[flows]` section of
  `name = zone id, measure id`.
  CMake builds the file in as text (`build/generated/wiseconn/`); missing or malformed: the
  devices are off and reads fail, the program still runs.
- `wiseconn_json` - minimal JSON parser (no JSON library on these machines).

Gotcha: `lastDataDate` ends in `Z` but is the farm's local time (America/Los_Angeles), so it is
read as local time. The machine running it must be in the farm's time zone; a mismatch with
`time_zone` is logged at startup, and readings then look 8 hours old and fail.

Finding ids: `GET /farms` (farm id, time zone), `GET /farms/<id>/hydraulics` (valves, their
status and the zones they feed; each irrigation zone here has exactly one valve),
`GET /farms/<id>/zones`, then
`GET /zones/<zone id>/measures` lists each measure's `id`, `name` and `sensorType`.
`GET /farms/<id>/measures` lists every measure on the farm (flow meters: `sensorType` "Flow",
unit gal/min; the main meter is listed once per zone it feeds, with the same `id`). In the
measure names, no depth suffix = 4 in, then "(8)", "(12)". Zone 8's probe also has 16, 20 and
24 in depths, not used.

## Adding a device type

1. `API/include/devices.h`: the abstract class.
2. `local_devices/local_<type>.h/.cpp` and `remote_devices/remote_<type>.h/.cpp`.
3. `remote_devices/device_server.h/.cpp`: an `addDevice` overload, the type in `device_ref`,
   its methods in `handleRequest`, and its read methods in `snapshot`.
4. (Deprecated, optional) `webUI/src/web_ui.h/.cpp` (outside the API): an `addDevice` overload, the type in `device_ref`, its fields
   in `devicesJson`, and its commands in `runCommand`.
