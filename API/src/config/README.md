# config

One file per machine. Exactly one is compiled, chosen with `cmake -DDEVICE_CONFIG=<name>`
(`config_<name>.cpp`).

| File | Machine | Local devices | Remote devices | Device server | Web UI |
|---|---|---|---|---|---|
| `config_uppertank.cpp` | Upper tank RevPi, 192.168.1.195 | 2 | - | yes | yes |
| `config_ro.cpp` | RO RevPi, 192.168.1.116 | 41 | - | yes | no |
| `config_host_pc.cpp` | Ubuntu PC | - | all 43 | no | yes, localhost only |
| `config_test.cpp` | Test bench, 192.168.1.116 | 6 examples | - | yes | yes |

## What a config does

`API/include/config.h` declares every device on the farm as a reference to its interface:

```cpp
extern pump& feed_pump;
```

A config defines those references for the devices its machine uses, and implements the
functions declared in `config.h`:

| Function | Purpose |
|---|---|
| `attachAllInterfaces()` | Binds every local `electrical_interface` to its PiCtory variable. Returns false if any pin is missing or the wrong size |
| `startDeviceServer()` / `serviceDeviceServer(ms)` | Shares the local devices on port 47100 |

A config only defines the devices its machine uses. A program that uses a device its config
doesn't define fails to link (`undefined reference to 'feed_pump'`), so mistakes show up at
build time, not at runtime.

## Layout of a config

```cpp
namespace {

// 1. Electrical interfaces: (device id, interface type, PiCtory pin)
electrical_interface ro_pump_run(32, relay{}, "Output_Bit_2");
electrical_interface ro_pump_speed(46, analog_current_out{}, "OutputValue_1");
electrical_interface ro_pump_alarm(16, digital_in{}, "I_11");

// 2. Local devices built from those interfaces
local_pump local_ro_pump(&ro_pump_speed, &ro_pump_run, &ro_pump_alarm);

// 3. Remote devices: one remote_link per owner, then one proxy per device
remote_link upper_tank_revpi("192.168.1.195");
remote_level_sensor remote_upper_tank_water_level(upper_tank_revpi, "upper_tank_water_level");

device_server server;
}

// 4. Public names from config.h
pump& ro_pump = local_ro_pump;
level_sensor& upper_tank_water_level = remote_upper_tank_water_level;
```

Everything except the public references lives in an anonymous namespace, so user programs
can't reach around the interfaces.

## Adding a device

1. Add a row to `API/hardware_setup.csv`.
2. Declare it in `config.h` under its owner's section.
3. In the owner's config: add the `electrical_interface`, the `local_*` device, the public
   reference, the interface in `attachAllInterfaces()`, and `server.addDevice("name", ...)`.
4. In every other config that uses it: add a `remote_*` device on the owner's `remote_link`
   and the public reference. Add it to `addAllDevices()` in `user_space/src/main_host_pc.cpp`
   if the web page should show it.
5. Rebuild those machines. Machines whose config didn't change don't need rebuilding.

Device names are the snake_case of the CSV name and must be identical on the owner and on
every client, since the client asks the owner for the device by name.

## Adding a machine

1. Create `config_<name>.cpp` defining its devices and the five functions above (copy the
   closest existing config).
2. Add `user_space/src/main_<name>.cpp`; it is only built when `DEVICE_CONFIG=<name>`.
3. On each RevPi that owns devices the new machine uses, open port 47100 to its IP.
