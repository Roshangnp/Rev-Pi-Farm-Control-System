#pragma once
#include <string>
#include "devices.h"
#include "remote_link.h"


// A push button owned by another RevPi. link must outlive the button.
// Reads the owner's raw counter, so readPushButtonCount() here counts presses
// on its own and doesn't take them from other readers.
class remote_push_button : public push_button{
    public:
    remote_push_button(remote_link& link, std::string device_name)
    : link_(&link), device_name_(std::move(device_name)){};

    bool readPressTotal(uint32_t& total) override;

    private:
    remote_link* link_;
    std::string device_name_;
};
