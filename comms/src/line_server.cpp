#include "line_server.h"

#include <algorithm>
#include <cerrno>
#include <sstream>

#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

namespace {

constexpr size_t MAX_REQUEST_LENGTH = 1024;
// A client that doesn't read its replies is dropped once this much is queued
constexpr size_t MAX_PENDING_REPLY_BYTES = 256 * 1024;
constexpr int LISTEN_BACKLOG = 4;

}  // namespace

void appendSnapshotEntry(std::string& out, const std::string& key, const std::string& reply) {
    if (!out.empty()) out += ';';
    out += key;
    if (reply.compare(0, 3, "ok ") == 0) {
        out += '=';
        out.append(reply, 3, std::string::npos);
    }
}

line_server::~line_server() {
    for (client& c : clients_) close(c.fd);
    if (listen_fd_ >= 0) close(listen_fd_);
}

bool line_server::addName(const std::string& name, line_handler& handler) {
    if (name.empty() || name.find_first_of(" \t\r\n.;=") != std::string::npos) return false;
    if (name == "ping" || name == "snapshot") return false;
    if (!names_.emplace(name, &handler).second) return false;
    if (std::find(handlers_.begin(), handlers_.end(), &handler) == handlers_.end()) handlers_.push_back(&handler);
    return true;
}

bool line_server::start() {
    if (listen_fd_ >= 0) return true;

    int fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if (fd < 0) return false;
    int one = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(loopback_only_ ? INADDR_LOOPBACK : INADDR_ANY);
    address.sin_port = htons(port_);
    if (bind(fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0 ||
        listen(fd, LISTEN_BACKLOG) < 0) {
        close(fd);
        return false;
    }
    listen_fd_ = fd;
    return true;
}

void line_server::poll(int timeout_ms) {
    if (listen_fd_ < 0) return;

    std::vector<pollfd> fds;
    fds.push_back({listen_fd_, POLLIN, 0});
    for (const client& c : clients_) {
        short events = POLLIN;
        if (!c.tx_buffer.empty()) events |= POLLOUT;
        fds.push_back({c.fd, events, 0});
    }

    // Don't sleep past the next idle deadline
    auto now = std::chrono::steady_clock::now();
    for (const client& c : clients_) {
        auto left = std::chrono::duration_cast<std::chrono::milliseconds>(c.last_seen + idle_timeout_ - now).count();
        if (left < timeout_ms) timeout_ms = left > 0 ? static_cast<int>(left) : 0;
    }

    if (::poll(fds.data(), fds.size(), timeout_ms) > 0) {
        // Walk backwards so dropping a client doesn't shift the ones still to check;
        // clients accepted below aren't in fds yet
        for (size_t i = fds.size() - 1; i >= 1; --i) {
            short revents = fds[i].revents;
            if (revents == 0) continue;
            client& c = clients_[i - 1];
            bool keep = true;
            if (revents & POLLOUT) keep = flushClient(c);
            if (keep && (revents & (POLLIN | POLLHUP | POLLERR))) keep = readClient(c);
            if (!keep) dropClient(i - 1);
        }
        if (fds[0].revents & POLLIN) acceptClients();
    }

    now = std::chrono::steady_clock::now();
    for (size_t i = clients_.size(); i-- > 0;) {
        if (now - clients_[i].last_seen > idle_timeout_) dropClient(i);
    }
}

void line_server::acceptClients() {
    while (true) {
        int fd = accept4(listen_fd_, nullptr, nullptr, SOCK_NONBLOCK | SOCK_CLOEXEC);
        if (fd < 0) return;
        int one = 1;
        setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
        clients_.push_back({fd, next_client_id_++, "", "", std::chrono::steady_clock::now()});
    }
}

bool line_server::readClient(client& c) {
    char buffer[4096];
    ssize_t n = recv(c.fd, buffer, sizeof(buffer), 0);
    if (n == 0) return false;
    if (n < 0) return errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR;

    c.rx_buffer.append(buffer, static_cast<size_t>(n));
    c.last_seen = std::chrono::steady_clock::now();

    size_t newline;
    while ((newline = c.rx_buffer.find('\n')) != std::string::npos) {
        std::string line = c.rx_buffer.substr(0, newline);
        c.rx_buffer.erase(0, newline + 1);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        c.tx_buffer += handleLine(c, line);
        c.tx_buffer += '\n';
    }
    if (c.rx_buffer.size() > MAX_REQUEST_LENGTH) return false;
    return flushClient(c);
}

bool line_server::flushClient(client& c) {
    while (!c.tx_buffer.empty()) {
        ssize_t n = send(c.fd, c.tx_buffer.data(), c.tx_buffer.size(), MSG_NOSIGNAL);
        if (n > 0) {
            c.tx_buffer.erase(0, static_cast<size_t>(n));
            continue;
        }
        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) break;  // rest goes on POLLOUT
        if (n < 0 && errno == EINTR) continue;
        return false;
    }
    return c.tx_buffer.size() <= MAX_PENDING_REPLY_BYTES;
}

std::string line_server::handleLine(const client& c, const std::string& line) {
    std::istringstream words(line);
    std::string name, method, arg, extra;
    words >> name >> method >> arg >> extra;

    if (name == "ping" && method.empty()) return "ok";
    if (name == "snapshot" && method.empty()) return snapshot();
    if (!extra.empty()) return "err too many arguments";

    auto found = names_.find(name);
    if (found == names_.end()) return "err unknown device";

    return found->second->handleRequest(c.id, name, method, arg);
}

std::string line_server::snapshot() {
    std::string entries;
    for (line_handler* h : handlers_) h->snapshot(entries);
    return "ok " + entries;
}

void line_server::dropClient(size_t index) {
    close(clients_[index].fd);
    clients_.erase(clients_.begin() + static_cast<std::ptrdiff_t>(index));
}
