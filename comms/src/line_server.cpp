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
constexpr int LISTEN_BACKLOG = 4;

}  // namespace

line_server::~line_server() {
    for (client& c : clients_) close(c.fd);
    if (listen_fd_ >= 0) close(listen_fd_);
}

bool line_server::addName(const std::string& name, line_handler& handler) {
    if (name.empty() || name.find_first_of(" \t\r\n") != std::string::npos || name == "ping") return false;
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

void line_server::poll(int timeout_ms) {
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

void line_server::acceptClients() {
    while (true) {
        int fd = accept4(listen_fd_, nullptr, nullptr, SOCK_NONBLOCK | SOCK_CLOEXEC);
        if (fd < 0) return;
        int one = 1;
        setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
        clients_.push_back({fd, next_client_id_++, "", std::chrono::steady_clock::now()});
    }
}

bool line_server::readClient(client& c) {
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

        std::string reply = handleLine(c, line) + "\n";
        if (send(c.fd, reply.data(), reply.size(), MSG_NOSIGNAL) != static_cast<ssize_t>(reply.size())) return false;
    }
    return c.rx_buffer.size() <= MAX_REQUEST_LENGTH;
}

std::string line_server::handleLine(const client& c, const std::string& line) {
    std::istringstream words(line);
    std::string name, method, arg, extra;
    words >> name >> method >> arg >> extra;

    if (name == "ping" && method.empty()) return "ok";
    if (!extra.empty()) return "err too many arguments";

    auto found = names_.find(name);
    if (found == names_.end()) return "err unknown device";

    return found->second->handleRequest(c.id, name, method, arg);
}

void line_server::dropClient(size_t index) {
    int id = clients_[index].id;
    close(clients_[index].fd);
    clients_.erase(clients_.begin() + static_cast<std::ptrdiff_t>(index));
    for (line_handler* h : handlers_) h->clientGone(id);
}
