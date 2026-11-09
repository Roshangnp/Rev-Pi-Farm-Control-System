#include "remote_push_button.h"

#include <cerrno>
#include <cstdlib>

bool remote_push_button::readPushButtonCount(uint32_t& presses) {
    std::string text;
    if (!link_->query(device_name_, "readPushButtonCount", text)) return false;
    if (text.empty() || text[0] < '0' || text[0] > '9') return false;

    char* end = nullptr;
    errno = 0;
    unsigned long long parsed = std::strtoull(text.c_str(), &end, 10);
    if (*end != '\0' || errno == ERANGE || parsed > UINT32_MAX) return false;

    presses = static_cast<uint32_t>(parsed);
    return true;
}
