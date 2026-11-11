#include "remote_link.h"

#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <cstring>

#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <sys/eventfd.h>
#include <sys/socket.h>
#include <unistd.h>

using namespace link_timing;

namespace {

// Commands waiting for replies beyond this fail at once (owner stalled)
constexpr size_t MAX_PENDING_REQUESTS = 16;
// A reply line longer than this drops the link (protocol error)
constexpr size_t MAX_REPLY_LENGTH = 256 * 1024;
// The io thread wakes at least this often to check timers
constexpr auto IO_TICK = std::chrono::milliseconds(100);

int msUntil(std::chrono::steady_clock::time_point deadline) {
    auto left = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now()).count();
    return left > 0 ? static_cast<int>(left) : 0;
}

// Non-blocking connect with a timeout. Returns the (non-blocking) socket, or -1.
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
    : host_(std::move(host)), port_(port), created_(clock_type::now()),
      wake_fd_(eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC)) {
    io_ = std::thread(&remote_link::ioLoop, this);
}

remote_link::~remote_link() {
    running_ = false;
    wake();
    io_.join();
    std::lock_guard<std::mutex> lock(mutex_);
    disconnectLocked();
    if (wake_fd_ >= 0) close(wake_fd_);
}

bool remote_link::isConnected() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return connected_;
}

void remote_link::wake() {
    if (wake_fd_ < 0) return;
    uint64_t one = 1;
    ssize_t ignored = write(wake_fd_, &one, sizeof(one));
    (void)ignored;
}

// ---------------------------------------------------------------------------
// Callers
// ---------------------------------------------------------------------------

bool remote_link::command(const std::string& device, const std::string& method, const std::string& arg) {
    std::string line = device + " " + method;
    if (!arg.empty()) line += " " + arg;
    line += "\n";

    auto request = std::make_shared<pending>();
    std::unique_lock<std::mutex> lock(mutex_);
    if (!connected_ || pending_.size() >= MAX_PENDING_REQUESTS) return false;
    pending_.push_back(request);
    tx_buffer_ += line;
    lock.unlock();
    wake();

    lock.lock();
    if (!replies_.wait_until(lock, clock_type::now() + COMMAND_TIMEOUT, [&] { return request->done; })) {
        return false;  // reply discarded when it arrives
    }
    if (!request->replied || request->reply != "ok") return false;

    // Show the command's readback soon
    snapshot_soon_ = true;
    lock.unlock();
    wake();
    return true;
}

bool remote_link::query(const std::string& device, const std::string& method, std::string& value) {
    std::unique_lock<std::mutex> lock(mutex_);
    if (!have_snapshot_) {
        // Right after startup: give the first snapshot a moment to arrive
        replies_.wait_until(lock, created_ + FIRST_SNAPSHOT_GRACE, [&] { return have_snapshot_; });
    }
    auto found = cache_.find(device + "." + method);
    if (found == cache_.end()) return false;
    const cached& entry = found->second;
    if (!entry.ok || clock_type::now() - entry.time > STALE_TIMEOUT) return false;
    value = entry.value;
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
    if (!query(device, method, text) || text.empty()) return false;
    char* end = nullptr;
    float parsed = std::strtof(text.c_str(), &end);
    if (end == text.c_str() || *end != '\0') return false;
    value = parsed;
    return true;
}

// ---------------------------------------------------------------------------
// io thread
// ---------------------------------------------------------------------------

void remote_link::ioLoop() {
    while (running_) {
        if (fd_ < 0) {
            auto retry_at = last_connect_attempt_ + RECONNECT_BACKOFF;
            if (clock_type::now() >= retry_at) {
                last_connect_attempt_ = clock_type::now();
                if (connectNow()) continue;
            }
            pollfd p{wake_fd_, POLLIN, 0};
            ::poll(&p, 1, std::min(msUntil(retry_at), static_cast<int>(IO_TICK.count())));
            uint64_t drained;
            ssize_t ignored = read(wake_fd_, &drained, sizeof(drained));
            (void)ignored;
            continue;
        }

        bool want_write;
        int timeout_ms;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto now = clock_type::now();
            if (!snapshot_outstanding_ && (snapshot_soon_ || now >= next_snapshot_)) {
                auto request = std::make_shared<pending>();
                request->snapshot = true;
                pending_.push_back(request);
                tx_buffer_ += "snapshot\n";
                snapshot_outstanding_ = true;
                snapshot_soon_ = false;
                next_snapshot_ = now + SNAPSHOT_PERIOD;
            }
            if (!pending_.empty() && now - last_receive_ > LINK_DEAD_TIMEOUT) {
                disconnectLocked();
                continue;
            }
            if (!sendLocked()) {
                disconnectLocked();
                continue;
            }
            want_write = !tx_buffer_.empty();
            timeout_ms = std::min(msUntil(next_snapshot_), static_cast<int>(IO_TICK.count()));
            if (snapshot_outstanding_) timeout_ms = static_cast<int>(IO_TICK.count());
        }

        pollfd fds[2] = {{fd_, static_cast<short>(POLLIN | (want_write ? POLLOUT : 0)), 0},
                         {wake_fd_, POLLIN, 0}};
        if (::poll(fds, 2, timeout_ms) < 0 && errno != EINTR) continue;
        if (fds[1].revents & POLLIN) {
            uint64_t drained;
            ssize_t ignored = read(wake_fd_, &drained, sizeof(drained));
            (void)ignored;
        }
        if (fds[0].revents & (POLLIN | POLLHUP | POLLERR)) {
            if (!receive()) {
                std::lock_guard<std::mutex> lock(mutex_);
                disconnectLocked();
            }
        }
        // POLLOUT: the next pass sends
    }
}

bool remote_link::connectNow() {
    int fd = connectWithTimeout(host_, port_);
    if (fd < 0) return false;
    std::lock_guard<std::mutex> lock(mutex_);
    fd_ = fd;
    connected_ = true;
    tx_buffer_.clear();
    rx_buffer_.clear();
    pending_.clear();
    snapshot_outstanding_ = false;
    snapshot_soon_ = true;
    last_receive_ = clock_type::now();
    return true;
}

void remote_link::disconnectLocked() {
    if (fd_ >= 0) close(fd_);
    fd_ = -1;
    connected_ = false;
    tx_buffer_.clear();
    rx_buffer_.clear();
    for (auto& request : pending_) request->done = true;  // replied stays false
    pending_.clear();
    snapshot_outstanding_ = false;
    // The owner closed the link (usually a restart) or it was dead for
    // LINK_DEAD_TIMEOUT: its old values can't be trusted, so reads fail at once
    cache_.clear();
    replies_.notify_all();
}

bool remote_link::sendLocked() {
    while (!tx_buffer_.empty()) {
        ssize_t n = send(fd_, tx_buffer_.data(), tx_buffer_.size(), MSG_NOSIGNAL);
        if (n > 0) {
            tx_buffer_.erase(0, static_cast<size_t>(n));
            continue;
        }
        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) return true;
        if (n < 0 && errno == EINTR) continue;
        return false;
    }
    return true;
}

bool remote_link::receive() {
    char buffer[16384];
    while (true) {
        ssize_t n = recv(fd_, buffer, sizeof(buffer), 0);
        if (n == 0) return false;
        if (n < 0) return errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR;

        std::lock_guard<std::mutex> lock(mutex_);
        rx_buffer_.append(buffer, static_cast<size_t>(n));
        last_receive_ = clock_type::now();
        size_t newline;
        while ((newline = rx_buffer_.find('\n')) != std::string::npos) {
            std::string line = rx_buffer_.substr(0, newline);
            rx_buffer_.erase(0, newline + 1);
            handleReplyLocked(line);
        }
        if (rx_buffer_.size() > MAX_REPLY_LENGTH) return false;
    }
}

void remote_link::handleReplyLocked(const std::string& line) {
    if (pending_.empty()) return;  // nothing asked: ignore
    std::shared_ptr<pending> request = pending_.front();
    pending_.pop_front();

    if (request->snapshot) {
        snapshot_outstanding_ = false;
        storeSnapshotLocked(line, clock_type::now());
    }
    request->reply = line;
    request->replied = true;
    request->done = true;
    replies_.notify_all();
}

void remote_link::storeSnapshotLocked(const std::string& reply, clock_type::time_point now) {
    if (reply.compare(0, 2, "ok") != 0) return;
    size_t pos = reply.size() > 3 ? 3 : reply.size();
    while (pos < reply.size()) {
        size_t end = reply.find(';', pos);
        if (end == std::string::npos) end = reply.size();
        size_t equals = reply.find('=', pos);
        if (equals != std::string::npos && equals < end) {
            cache_[reply.substr(pos, equals - pos)] = {true, reply.substr(equals + 1, end - equals - 1), now};
        } else if (end > pos) {
            cache_[reply.substr(pos, end - pos)] = {false, "", now};
        }
        pos = end + 1;
    }
    have_snapshot_ = true;
}
