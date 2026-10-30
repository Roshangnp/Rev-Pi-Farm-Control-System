#include "device_server.h"

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <sstream>
#include <type_traits>

#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

namespace {

constexpr size_t MAX_REQUEST_LENGTH = 1024;
constexpr int LISTEN_BACKLOG = 4;

std::string okBool(bool value) {
    return value ? "ok 1" : "ok 0";
}

std::string okFloat(bool success, float value) {
    if (!success) return "fail";
    char text[32];
    std::snprintf(text, sizeof(text), "ok %.6g", static_cast<double>(value));
    return text;
}

std::string okIf(bool success) {
    return success ? "ok" : "fail";
}

bool parseFloat(const std::string& text, float& value) {
    if (text.empty()) return false;
    char* end = nullptr;
    value = std::strtof(text.c_str(), &end);
    return *end == '\0';
}

}  // namespace

device_server::~device_server() {
    for (client& c : clients_) close(c.fd);
    if (listen_fd_ >= 0) close(listen_fd_);
}

bool device_server::addDevice(const std::string& name, pump& device) {return addDeviceRef(name, &device);}
bool device_server::addDevice(const std::string& name, float_switch& device) {return addDeviceRef(name, &device);}
bool device_server::addDevice(const std::string& name, valve& device) {return addDeviceRef(name, &device);}
bool device_server::addDevice(const std::string& name, pressure_transducer& device) {return addDeviceRef(name, &device);}
bool device_server::addDevice(const std::string& name, tds_sensor& device) {return addDeviceRef(name, &device);}
bool device_server::addDevice(const std::string& name, flow_sensor& device) {return addDeviceRef(name, &device);}

bool device_server::addDeviceRef(const std::string& name, device_ref device) {
    if (name.empty() || name.find_first_of(" \t\r\n") != std::string::npos || name == "ping") return false;
    return devices_.emplace(name, device).second;
}

bool device_server::start() {
    if (listen_fd_ >= 0) return true;

    int fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if (fd < 0) return false;
    int one = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(port_);
    if (bind(fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0 ||
        listen(fd, LISTEN_BACKLOG) < 0) {
        close(fd);
        return false;
    }
    listen_fd_ = fd;
    return true;
}

void device_server::poll(int timeout_ms) {
    if (listen_fd_ < 0) return;

    std::vector<pollfd> fds;
    fds.push_back({listen_fd_, POLLIN, 0});
    for (const client& c : clients_) fds.push_back({c.fd, POLLIN, 0});

    // Don't sleep past the next watchdog deadline
    auto now = std::chrono::steady_clock::now();
    for (const client& c : clients_) {
        auto left = std::chrono::duration_cast<std::chrono::milliseconds>(c.last_seen + watchdog_ - now).count();
        if (left < timeout_ms) timeout_ms = left > 0 ? static_cast<int>(left) : 0;
    }

    if (::poll(fds.data(), fds.size(), timeout_ms) > 0) {
        if (fds[0].revents & POLLIN) acceptClients();
        // Walk backwards so dropping a client doesn't shift the ones still to check
        for (size_t i = fds.size() - 1; i >= 1; --i) {
            if (fds[i].revents == 0) continue;
            if (!readClient(clients_[i - 1])) dropClient(i - 1);
        }
    }

    now = std::chrono::steady_clock::now();
    for (size_t i = clients_.size(); i-- > 0;) {
        if (now - clients_[i].last_seen > watchdog_) dropClient(i);
    }
}

void device_server::acceptClients() {
    while (true) {
        int fd = accept4(listen_fd_, nullptr, nullptr, SOCK_NONBLOCK | SOCK_CLOEXEC);
        if (fd < 0) return;
        int one = 1;
        setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
        clients_.push_back({fd, next_client_id_++, "", std::chrono::steady_clock::now()});
    }
}

bool device_server::readClient(client& c) {
    char buffer[256];
    ssize_t n = recv(c.fd, buffer, sizeof(buffer), 0);
    if (n == 0) return false;
    if (n < 0) return errno == EAGAIN || errno == EWOULDBLOCK;

    c.rx_buffer.append(buffer, static_cast<size_t>(n));
    c.last_seen = std::chrono::steady_clock::now();

    size_t newline;
    while ((newline = c.rx_buffer.find('\n')) != std::string::npos) {
        std::string line = c.rx_buffer.substr(0, newline);
        c.rx_buffer.erase(0, newline + 1);
        if (!line.empty() && line.back() == '\r') line.pop_back();

        std::string reply = handleRequest(c, line) + "\n";
        if (send(c.fd, reply.data(), reply.size(), MSG_NOSIGNAL) != static_cast<ssize_t>(reply.size())) return false;
    }
    return c.rx_buffer.size() <= MAX_REQUEST_LENGTH;
}

std::string device_server::handleRequest(client& c, const std::string& line) {
    std::istringstream words(line);
    std::string name, method, arg, extra;
    words >> name >> method >> arg >> extra;

    if (name == "ping" && method.empty()) return "ok";
    if (!extra.empty()) return "err too many arguments";

    auto found = devices_.find(name);
    if (found == devices_.end()) return "err unknown device";

    return std::visit([&](auto* device) -> std::string {
        using T = std::remove_pointer_t<decltype(device)>;
        bool is_command = false;
        std::string reply = "err unknown method";

        if constexpr (std::is_same_v<T, pump>) {
            if (method == "hasSpeedControl") reply = okBool(device->hasSpeedControl());
            else if (method == "hasAlarm") reply = okBool(device->hasAlarm());
            else if (method == "readAlarm") reply = okBool(device->readAlarm());
            else if (method == "setOn") {is_command = true; reply = okIf(device->setOn());}
            else if (method == "setOff") {is_command = true; reply = okIf(device->setOff());}
            else if (method == "setSpeed") {
                float speed;
                if (!parseFloat(arg, speed)) return "err bad argument";
                is_command = true;
                reply = okIf(device->setSpeed(speed));
            }
        } else if constexpr (std::is_same_v<T, valve>) {
            if (method == "setOpen") {is_command = true; reply = okIf(device->setOpen());}
            else if (method == "setClosed") {is_command = true; reply = okIf(device->setClosed());}
        } else if constexpr (std::is_same_v<T, float_switch>) {
            if (method == "readSubmerged") {
                bool submerged = false;
                bool success = device->readSubmerged(submerged);
                reply = success ? okBool(submerged) : "fail";
            }
        } else if constexpr (std::is_same_v<T, pressure_transducer>) {
            if (method == "readPressure") {
                float psi = 0.0f;
                bool success = device->readPressure(psi);
                reply = okFloat(success, psi);
            }
        } else if constexpr (std::is_same_v<T, tds_sensor>) {
            if (method == "readTds") {
                float ppm = 0.0f;
                bool success = device->readTds(ppm);
                reply = okFloat(success, ppm);
            }
        } else if constexpr (std::is_same_v<T, flow_sensor>) {
            if (method == "readFlow") {
                float gpm = 0.0f;
                bool success = device->readFlow(gpm);
                reply = okFloat(success, gpm);
            }
        }

        if (is_command) commanded_by_[name] = c.id;
        return reply;
    }, found->second);
}

void device_server::dropClient(size_t index) {
    int id = clients_[index].id;
    close(clients_[index].fd);
    clients_.erase(clients_.begin() + static_cast<std::ptrdiff_t>(index));
    applySafeState(id);
}

void device_server::applySafeState(int client_id) {
    for (auto it = commanded_by_.begin(); it != commanded_by_.end();) {
        if (it->second != client_id) {
            ++it;
            continue;
        }
        std::visit([](auto* device) {
            using T = std::remove_pointer_t<decltype(device)>;
            if constexpr (std::is_same_v<T, pump>) device->setOff();
            else if constexpr (std::is_same_v<T, valve>) device->setClosed();
        }, devices_.at(it->first));
        it = commanded_by_.erase(it);
    }
}
