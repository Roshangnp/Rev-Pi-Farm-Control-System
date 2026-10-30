#include "electronics.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <type_traits>

#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "piControl.h"

namespace {

// AIO values are stored as mV / uA with PiCtory multiplier = divisor = 1
constexpr float MILLI_PER_UNIT = 1000.0f;

template <typename T, typename... Ts>
constexpr bool is_one_of = (std::is_same_v<T, Ts> || ...);

template <typename T>
constexpr bool is_bit_type = is_one_of<T, digital_in, digital_out, relay>;
template <typename T>
constexpr bool is_analog_in_type = is_one_of<T, analog_current_in, analog_voltage_in>;
template <typename T>
constexpr bool is_analog_out_type = is_one_of<T, analog_current_out, analog_voltage_out>;
template <typename T>
constexpr bool is_output_type = is_one_of<T, digital_out, relay, analog_current_out,
                                          analog_voltage_out, pwm_duty_out>;

// Size in bits of the process image variable each interface type maps to
template <typename T>
constexpr uint16_t expected_bit_length() {
    if constexpr (is_bit_type<T>) return 1;
    else if constexpr (is_analog_in_type<T> || is_analog_out_type<T>) return 16;
    else if constexpr (std::is_same_v<T, pwm_duty_out>) return 8;
    else if constexpr (std::is_same_v<T, pulse_in>) return 32;
    else static_assert(sizeof(T) == 0, "no process image size for this interface type");
}

int process_image() {
    static int fd = open(PICONTROL_DEVICE, O_RDWR);
    return fd;
}

bool read_bit(uint16_t address, uint8_t bit, bool& value) {
    SPIValue v{};
    v.i16uAddress = address;
    v.i8uBit = bit;
    if (ioctl(process_image(), KB_GET_VALUE, &v) < 0) return false;
    value = v.i8uValue != 0;
    return true;
}

bool write_bit(uint16_t address, uint8_t bit, bool value) {
    SPIValue v{};
    v.i16uAddress = address;
    v.i8uBit = bit;
    v.i8uValue = value ? 1 : 0;
    return ioctl(process_image(), KB_SET_VALUE, &v) >= 0;
}

// Process image is little endian, same as the RevPi CPU
template <typename T>
bool read_bytes(uint16_t address, T& value) {
    return pread(process_image(), &value, sizeof(T), address) == static_cast<ssize_t>(sizeof(T));
}

template <typename T>
bool write_bytes(uint16_t address, T value) {
    return pwrite(process_image(), &value, sizeof(T), address) == static_cast<ssize_t>(sizeof(T));
}

int16_t to_raw_analog(float value) {
    float raw = std::round(value * MILLI_PER_UNIT);
    return static_cast<int16_t>(std::clamp(raw, -32768.0f, 32767.0f));
}

}  // namespace

bool electrical_interface::attach() {
    attached_ = false;
    if (process_image() < 0) return false;

    SPIVariable var{};
    if (pin_name_.size() >= sizeof(var.strVarName)) return false;
    std::strncpy(var.strVarName, pin_name_.c_str(), sizeof(var.strVarName) - 1);
    if (ioctl(process_image(), KB_FIND_VARIABLE, &var) < 0) return false;

    uint16_t expected = std::visit([](const auto& io) {
        return expected_bit_length<std::decay_t<decltype(io)>>();
    }, interface_);
    if (var.i16uLength != expected) return false;

    address_ = var.i16uAddress;
    bit_ = var.i8uBit;
    attached_ = true;
    return true;
}

bool electrical_interface::setHardwareState() {
    if (!attached_) return false;

    return std::visit([this](const auto& io) {
        using T = std::decay_t<decltype(io)>;
        if constexpr (!is_output_type<T>) {
            return false;
        } else if constexpr (is_bit_type<T>) {
            return write_bit(address_, bit_, io.value);
        } else if constexpr (is_analog_out_type<T>) {
            return write_bytes(address_, to_raw_analog(io.value));
        } else if constexpr (std::is_same_v<T, pwm_duty_out>) {
            float duty = std::clamp(std::round(io.value), 0.0f, 100.0f);
            return write_bytes(address_, static_cast<uint8_t>(duty));
        }
    }, interface_);
}

bool electrical_interface::getHardwareState() {
    if (!attached_) return false;

    return std::visit([this](auto& io) {
        using T = std::decay_t<decltype(io)>;
        if constexpr (is_bit_type<T>) {
            return read_bit(address_, bit_, io.value);
        } else if constexpr (is_analog_in_type<T> || is_analog_out_type<T>) {
            int16_t raw;
            if (!read_bytes(address_, raw)) return false;
            io.value = raw / MILLI_PER_UNIT;
            return true;
        } else if constexpr (std::is_same_v<T, pwm_duty_out>) {
            uint8_t raw;
            if (!read_bytes(address_, raw)) return false;
            io.value = raw;
            return true;
        } else if constexpr (std::is_same_v<T, pulse_in>) {
            return read_bytes(address_, io.value);
        }
    }, interface_);
}
