# HAL

The hardware abstraction layer: the only code that touches the RevPi process image.

| File | Purpose |
|---|---|
| `electronics.h/.cpp` | `electrical_interface`: one I/O point bound to a PiCtory variable |
| `analog_input.h/.cpp` | `readScaledAnalog()`: shared 4-20 mA / 0-10 V scaling for analog sensors |
| `piControl.h` | Vendored KUNBUS driver header (MIT). Do not edit |

## electrical_interface

```cpp
electrical_interface feed_pump_run(31, relay{}, "Output_Bit_1");
//                                 id   type     PiCtory variable name
```

- `attach()` looks the variable up in the process image and checks its size matches the type.
  Returns false if the name doesn't exist or the size is wrong.
- `getHardwareState()` reads the pin into the interface; works for inputs and outputs.
- `setHardwareState()` writes the interface to the pin; refused for input types.
- `get<T>()` gives the value struct, e.g. `get<relay>().value`.

Both return false on failure; nothing throws. The device id is a label from
`hardware_setup.csv` and isn't used by the code.

## Interface types

| Type | Value | PiCtory variable | Process image |
|---|---|---|---|
| `digital_in` | bool | DIO `I_N` | 1 bit |
| `digital_out` | bool | DIO `O_N` | 1 bit |
| `relay` | bool | ModbusRTU Master `Output_Bit_N` | 1 bit |
| `analog_current_in` | float mA | AIO `InputValue_N` | int16 (uA) |
| `analog_voltage_in` | float V | AIO `InputValue_N` | int16 (mV) |
| `analog_current_out` | float mA | AIO `OutputValue_N` | int16 (uA) |
| `analog_voltage_out` | float V | AIO `OutputValue_N` | int16 (mV) |
| `pwm_duty_out` | float 0-100 % | DIO `PWM_N` | uint8 |
| `pulse_in` | uint32 count | DIO `Counter_N` | uint32 |

With more than one module of a type, PiCtory adds a suffix to the second and later ones
(`InputValue_1_i07`, `Counter_1_i03`). Check names with `piTest -v <name>`.

The `electrical_interface_type` enum, the value structs and the `interface_value` variant must
stay in the same order; a `static_assert` checks the count.

## PiCtory setup

- **AIO:** multiplier and divisor must stay 1 (the code assumes mV / uA). Each input and output
  range must match what is wired, e.g. 4-20 mA for current sensors. An output with range 0 is off.
- **DIO counters:** a flow sensor input must be in counter mode (`InputMode_N`), or `Counter_N`
  stays 0.
- **DIO PWM:** the frequency is set per module, not per channel.
- **Relay board (KinCony E16v3):** ModbusRTU Master on the RS485 port, slave 10, 38400 8N1.
  Action 1 writes `Output_Bit_1..16` (FC15), action 2 reads `Input_Bit_1..16` back (FC01).
  `Modbus_Action_Status_N` = 0 means OK. After saving in PiCtory run
  `sudo systemctl restart pimodbus-master`.

## Analog scaling

`readScaledAnalog(input, min, max, value)` maps 4-20 mA or 0-10 V linearly onto `min..max`.
A current below 3.5 mA is reported as a broken wire (returns false). Values aren't clamped, so
an unwired 0-10 V input can read slightly below `min`.

## Inspecting I/O

```sh
piTest -r I_1              # read a variable
piTest -w Output_Bit_1,1   # write a variable (switches a real load)
piTest -v I_1              # show address and size
```
