# Rev-Pi-Farm-Control-System

Control software for farm equipment (pumps, valves, tanks, RO system) running on
Revolution Pi Connect 4 units. Several RevPis each own the I/O wired to them and
share their devices over the LAN, so a program on any machine can use any device
on the farm without caring where it is wired.

## Repository layout

```
CMakeLists.txt          build for every machine, picks the device config with -DDEVICE_CONFIG
API/
  hardware_setup.csv    list of every device: name, owner IP, interface type, PiCtory pin, range
  include/              the public API: what user programs include
    devices.h           device interfaces (pump, valve, float_switch, sensors, ...)
    config.h            every farm device, plus startup / device sharing functions
  src/
    config/             one config per machine: which devices are local or remote
    devices/            local devices (wired here) and remote devices (owned by another RevPi)
    HAL/                electrical_interface: one PiCtory I/O point
comms/                  line protocol between machines: line_server (port 47100) and remote_link
webUI/                  DEPRECATED web page (not built by default; replaced by console/)
console/                ro_console: keyboard-only terminal UI (RO RevPi HDMI screen, or over SSH)
notifications/          ntfy push notifications (host PC only)
user_space/src/         programs; main_<config>.cpp is built only for that config
```

Each `API/src/*` folder, `comms/`, `webUI/` and `notifications/` has its own README with the details.

## Machines

| Config | Machine | IP | Role |
|---|---|---|---|
| `uppertank` | Upper tank RevPi (MIO + AIO) | 192.168.1.21 | Owns the upper tank float and water level |
| `ro` | RO RevPi (2x DIO, 4x AIO, RS485 relay board) | 192.168.1.20 | Owns the RO system: flows, floats, pressures, pumps, valves |
| `host_pc` | Ubuntu PC | - | No I/O; uses every device remotely, sends ntfy alarm notifications |
| `test` | Test bench (same RevPi as `ro`) | 192.168.1.20 | Example wiring used by the hardware tests |

## Building

Every machine builds from the same repo; only the config changes.

```sh
cmake -B build -DDEVICE_CONFIG=<uppertank|ro|host_pc|test>
cmake --build build -j4
./build/main_<config>
```

There is no default config on purpose: building the wrong one on a RevPi would drive the wrong outputs.
On a fresh RevPi, install the tools first with `sudo apt install build-essential cmake`.

## How a program uses devices

User programs include only `config.h` and talk to devices through their interfaces:

```cpp
#include "config.h"
#include "line_server.h"

int main() {
    attachAllInterfaces();   // bind this machine's I/O to PiCtory

    line_server server;      // port 47100
    addLocalDevices(server); // share this machine's devices with the others
    server.start();

    while (true) {
        bool full = false;
        if (permeate_tank_high_float.readSubmerged(full) && full) feed_pump.setOff();

        server.poll(20);     // handle remote requests; doubles as the loop sleep
    }
}
```

`feed_pump` is a local pump on the RO RevPi and a remote pump everywhere else; the code is the same.
Every call returns `false` on failure (broken wire, unreachable RevPi, ...), so always check it.
Float switches: `readSubmerged` gives `true` when the float is submerged (water at the float),
`false` when it is dry.

## Networking and safety

- RevPis talk over TCP port 47100 (text protocol, no authentication). On each owner, open the
  port only to the machines that use its devices:
  ```sh
  sudo firewall-cmd --permanent --add-rich-rule='rule family="ipv4" source address="<client IP>" port port="47100" protocol="tcp" accept'
  sudo firewall-cmd --reload
  ```
- Watchdog: if a machine that switched an output disconnects or goes silent for 20 s, the owner
  turns its pumps and switched loads off and closes its valves. Network stalls shorter than that
  don't reset anything.
- The web UI is deprecated (2026-11-11): `main_host_pc` no longer serves it. Use `ro_console`
  (`console/README.md`).
- The RS485 relay board has no timeout of its own: if the RevPi stops, relays hold their last state.

## Adding hardware

1. Add the device to `API/hardware_setup.csv`.
2. Declare it in `API/include/config.h`.
3. Wire it as `local_*` in the owner's config and as `remote_*` in every config that uses it
   (see `API/src/config/README.md`).
4. Rebuild the machines whose config changed.
