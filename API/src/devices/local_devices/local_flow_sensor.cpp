#include "local_flow_sensor.h"

namespace {

// update() calls closer together than this don't store a sample (the average
// is still recomputed), so a fast loop can't grow the history without bound
constexpr auto MIN_SAMPLE_SPACING = std::chrono::milliseconds(10);
// readFlow() fails if update() hasn't run for this many windows
constexpr int STALE_WINDOWS = 3;

}  // namespace

void local_flow_sensor::update() {
    auto now = clock_type::now();
    last_update_ = now;
    valid_ = false;

    if (pulse_input_ == nullptr) return;
    if (pulse_input_->getType() != electrical_interface_type::PULSE_IN) return;
    if (pulses_per_gallon_ <= 0.0f) return;
    if (!pulse_input_->getHardwareState()) return;

    uint32_t count = pulse_input_->get<pulse_in>().value;

    // Counter went backwards (reset, e.g. PiCtory reload): start over
    if (!samples_.empty() && count < samples_.back().count) samples_.clear();

    if (samples_.empty() || now - samples_.back().time >= MIN_SAMPLE_SPACING) samples_.push_back({now, count});

    // Keep the newest sample that is at least window old as the front;
    // anything older than it is no longer needed
    while (samples_.size() >= 2 && now - samples_[1].time >= window_) samples_.pop_front();

    const sample& start = samples_.front();
    if (now - start.time < window_) return;  // no full window yet

    float minutes = std::chrono::duration<float, std::ratio<60>>(now - start.time).count();
    gpm_ = static_cast<float>(count - start.count) / pulses_per_gallon_ / minutes;
    valid_ = true;
}

bool local_flow_sensor::readFlow(float& gpm) {
    if (!valid_) return false;
    if (clock_type::now() - last_update_ > STALE_WINDOWS * window_) return false;
    gpm = gpm_;
    return true;
}
