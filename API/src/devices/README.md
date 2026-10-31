# devices

Every device type is an abstract class in `API/include/devices.h`, with two implementations:

- `local_devices/local_*` - wired to this RevPi, built from `electrical_interface`s
- `remote_devices/remote_*` - owned by another RevPi, reached over the network

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
- `flow_sensor::readFlow` returns the average since the previous call, so the first call fails.
  Every reader shares that window: a second reader (another RevPi, the web UI) shortens it.
- Polarity is fixed (on = submerged / open / alarm). A normally-closed float or alarm contact
  needs an "inverted" option that doesn't exist yet.

## Remote devices

`remote_link` is one TCP connection to another RevPi (port 47100), shared by all the `remote_*`
devices it owns. A background thread connects as soon as the link is created and pings every
0.5 s, which keeps the owner's watchdog happy. Lost connections are retried with a 1 s backoff;
connects and replies time out after 300 ms.

`device_server` runs on the owner. It is single threaded: requests are only handled inside
`poll()`, called from the owner's main loop, so they never run at the same time as local
control code.

Protocol, one line each way:

```
<device> <method> [arg]    ->    ok [value] | fail | err <reason>
feed_pump setSpeed 0.5     ->    ok
feed_tank_high_float readSubmerged  ->  ok 1
```

Method names are the C++ method names. There is no authentication; the firewall must limit
port 47100 to the other farm machines.

**Watchdog:** if a client disconnects or sends nothing for 2 s, every pump and switched load it
last commanded is turned off and every valve it last commanded is closed.

Typical cost is about 1 ms per call between RevPis. If the owner is down, calls fail after the
300 ms connect timeout, then immediately until the 1 s backoff ends.

## Adding a device type

1. `API/include/devices.h`: the abstract class.
2. `local_devices/local_<type>.h/.cpp` and `remote_devices/remote_<type>.h/.cpp`.
3. `remote_devices/device_server.h/.cpp`: an `addDevice` overload, the type in `device_ref`,
   its methods in `handleRequest`, and its safe state in `applySafeState` if it is an output.
4. `API/src/webUI/web_ui.h/.cpp`: an `addDevice` overload, the type in `device_ref`, its fields
   in `devicesJson`, and its commands in `runCommand`.
