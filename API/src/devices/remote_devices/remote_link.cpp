#include "remote_link.h"

#include <cerrno>
#include <cstdlib>
#include <cstring>

#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

namespace {

using clock_type = std::chrono::steady_clock;

constexpr auto CONNECT_TIMEOUT = std::chrono::milliseconds(300);
constexpr auto REPLY_TIMEOUT = std::chrono::milliseconds(300);
constexpr auto RECONNECT_BACKOFF = std::chrono::milliseconds(1000);
constexpr auto HEARTBEAT_INTERVAL = std::chrono::milliseconds(500);
constexpr auto HEARTBEAT_TICK = std::chrono::milliseconds(50);
constexpr size_t MAX_REPLY_LENGTH = 1024;

int remainingMs(clock_type::time_point deadline) {
    auto left = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - clock_type::now()).count();
    return left > 0 ? static_cast<int>(left) : 0;
}

// Non-blocking connect with a timeout. Returns the socket, or -1.
int connectWithTimeout(const std::string& host, uint16_t port) {
    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    addrinfo* result = nullptr;
    if (getaddrinfo(host.c_str(), std::to_string(port).c_str(), &hints, &result) != 0) return -1;

    int fd = socket(result->ai_family, result->ai_socktype | SOCK_CLOEXEC, result->ai_protocol);
    if (fd < 0) {
        freeaddrinfo(result);
        return -1;
    }
    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);

    int rc = connect(fd, result->ai_addr, result->ai_addrlen);
    freeaddrinfo(result);
    if (rc < 0 && errno != EINPROGRESS) {
        close(fd);
        return -1;
    }
    if (rc < 0) {
        pollfd p{fd, POLLOUT, 0};
        int error = 0;
        socklen_t len = sizeof(error);
        if (::poll(&p, 1, static_cast<int>(CONNECT_TIMEOUT.count())) != 1 ||
            getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &len) < 0 || error != 0) {
            close(fd);
            return -1;
        }
    }

    int one = 1;
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
    return fd;
}

}  // namespace

remote_link::remote_link(std::string host, uint16_t port)
    : host_(std::move(host)), port_(port), heartbeat_(&remote_link::heartbeatLoop, this) {}

remote_link::~remote_link() {
    running_ = false;
    heartbeat_.join();
    std::lock_guard<std::mutex> lock(mutex_);
    disconnectLocked();
}

bool remote_link::isConnected() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return fd_ >= 0;
}

bool remote_link::command(const std::string& device, const std::string& method, const std::string& arg) {
    std::string reply;
    std::string request = device + " " + method;
    if (!arg.empty()) request += " " + arg;
    return call(request, reply) && reply == "ok";
}

bool remote_link::query(const std::string& device, const std::string& method, std::string& value) {
    std::string reply;
    if (!call(device + " " + method, reply)) return false;
    if (reply.compare(0, 3, "ok ") != 0) return false;
    value = reply.substr(3);
    return true;
}

bool remote_link::queryBool(const std::string& device, const std::string& method, bool& value) {
    std::string text;
    if (!query(device, method, text) || (text != "0" && text != "1")) return false;
    value = text == "1";
    return true;
}

bool remote_link::queryFloat(const std::string& device, const std::string& method, float& value) {
    std::string text;
    if (!query(device, method, text)) return false;
    char* end = nullptr;
    float parsed = std::strtof(text.c_str(), &end);
    if (end == text.c_str() || *end != '\0') return false;
    value = parsed;
    return true;
}

bool remote_link::connectLocked() {
    auto now = clock_type::now();
    if (now - last_connect_attempt_ < RECONNECT_BACKOFF) return false;
    last_connect_attempt_ = now;

    fd_ = connectWithTimeout(host_, port_);
    rx_buffer_.clear();
    return fd_ >= 0;
}

void remote_link::disconnectLocked() {
    if (fd_ >= 0) close(fd_);
    fd_ = -1;
    rx_buffer_.clear();
}

bool remote_link::call(const std::string& request, std::string& reply) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (fd_ < 0 && !connectLocked()) return false;

    std::string line = request + "\n";
    size_t sent = 0;
    auto deadline = clock_type::now() + REPLY_TIMEOUT;
    while (sent < line.size()) {
        ssize_t n = send(fd_, line.data() + sent, line.size() - sent, MSG_NOSIGNAL);
        if (n > 0) {
            sent += static_cast<size_t>(n);
            continue;
        }
        pollfd p{fd_, POLLOUT, 0};
        if (n < 0 && errno == EAGAIN && ::poll(&p, 1, remainingMs(deadline)) == 1) continue;
        disconnectLocked();
        return false;
    }

    // A late or missing reply would leave the stream out of step, so any
    // timeout or error drops the connection.
    size_t newline;
    while ((newline = rx_buffer_.find('\n')) == std::string::npos) {
        pollfd p{fd_, POLLIN, 0};
        char buffer[256];
        ssize_t n = -1;
        if (::poll(&p, 1, remainingMs(deadline)) == 1) n = recv(fd_, buffer, sizeof(buffer), 0);
        if (n <= 0 || rx_buffer_.size() > MAX_REPLY_LENGTH) {
            disconnectLocked();
            return false;
        }
        rx_buffer_.append(buffer, static_cast<size_t>(n));
    }

    reply = rx_buffer_.substr(0, newline);
    rx_buffer_.erase(0, newline + 1);
    last_activity_ = clock_type::now();
    return true;
}

void remote_link::heartbeatLoop() {
    while (running_) {
        std::this_thread::sleep_for(HEARTBEAT_TICK);
        bool due;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            due = clock_type::now() - last_activity_ >= HEARTBEAT_INTERVAL;
        }
        std::string reply;
        if (due) call("ping", reply);
    }
}
