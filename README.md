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
    config.h            every farm device, plus startup / device server functions
  src/
    config/             one config per machine: which devices are local or remote
    devices/            local devices (wired here) and remote devices (owned by another RevPi)
    HAL/                electrical_interface: one PiCtory I/O point
webUI/                  web page listing the farm's devices (host PC only, uses API/include)
notifications/          ntfy push notifications (host PC only)
user_space/src/         programs; main_<config>.cpp is built only for that config
```

Each `API/src/*` folder, `webUI/` and `notifications/` has its own README with the details.

## Machines

| Config | Machine | IP | Role |
|---|---|---|---|
| `uppertank` | Upper tank RevPi (MIO + AIO) | 192.168.1.21 | Owns the upper tank float and water level |
| `ro` | RO RevPi (2x DIO, 4x AIO, RS485 relay board) | 192.168.1.20 | Owns the RO system: flows, floats, pressures, pumps, valves |
| `host_pc` | Ubuntu PC | - | No I/O; uses every device remotely and serves the web UI |
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

int main() {
    attachAllInterfaces();   // bind this machine's I/O to PiCtory
    startDeviceServer();     // share this machine's devices with the others

    while (true) {
        bool full = false;
        if (feed_tank_high_float.readSubmerged(full) && full) feed_pump.setOff();

        serviceDeviceServer(20);   // handle remote requests; doubles as the loop sleep
    }
}
```

`feed_pump` is a local pump on the RO RevPi and a remote pump everywhere else; the code is the same.
Every call returns `false` on failure (broken wire, unreachable RevPi, ...), so always check it.

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
- The web UI (port 8080) can switch real loads. It is open to the LAN and needs the password
  from `webui_password.txt` in the repo root (git-ignored, hardcoded at build time; see
  `webUI/README.md`). It is plain HTTP, so still firewall the port to the LAN.
- The RS485 relay board has no timeout of its own: if the RevPi stops, relays hold their last state.

## Adding hardware

1. Add the device to `API/hardware_setup.csv`.
2. Declare it in `API/include/config.h`.
3. Wire it as `local_*` in the owner's config and as `remote_*` in every config that uses it
   (see `API/src/config/README.md`).
4. Rebuild the machines whose config changed.
