#include "ntfy.h"

#include <cerrno>
#include <cstdio>
#include <vector>

#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

extern char** environ;

namespace {

constexpr size_t MAX_QUEUE = 50;
constexpr size_t MAX_MESSAGE_LENGTH = 3500;  // ntfy turns bodies over 4096 bytes into attachments
constexpr int ATTEMPTS = 3;
constexpr std::chrono::seconds RETRY_DELAYS[] = {std::chrono::seconds(5), std::chrono::seconds(30)};

std::string jsonString(const std::string& text) {
    std::string out = "\"";
    for (char ch : text) {
        unsigned char c = static_cast<unsigned char>(ch);
        if (ch == '"') out += "\\\"";
        else if (ch == '\\') out += "\\\\";
        else if (ch == '\n') out += "\\n";
        else if (ch == '\t') out += "\\t";
        else if (c < 0x20) continue;
        else out += ch;
    }
    return out + "\"";
}

// ntfy JSON publish body: posted to the server root, the topic goes in the JSON
// (keeps it out of curl's command line, which other users can see in ps)
std::string jsonBody(const std::string& topic, const std::string& title, const std::string& text,
                     int priority, const std::string& tags) {
    std::string body = "{\"topic\":" + jsonString(topic);
    if (!title.empty()) body += ",\"title\":" + jsonString(title);
    body += ",\"message\":" + jsonString(text.substr(0, MAX_MESSAGE_LENGTH));
    body += ",\"priority\":" + std::to_string(priority);
    if (!tags.empty()) {
        body += ",\"tags\":[";
        size_t start = 0;
        bool first = true;
        while (start <= tags.size()) {
            size_t comma = tags.find(',', start);
            if (comma == std::string::npos) comma = tags.size();
            std::string tag = tags.substr(start, comma - start);
            if (!tag.empty()) {
                body += (first ? "" : ",") + jsonString(tag);
                first = false;
            }
            start = comma + 1;
        }
        body += "]";
    }
    return body + "}";
}

// Runs curl with the body on stdin. True if the server answered 2xx.
bool runCurl(const std::string& url, const std::string& body) {
    int in_pipe[2];
    if (pipe2(in_pipe, O_CLOEXEC) < 0) return false;

    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_adddup2(&actions, in_pipe[0], STDIN_FILENO);
    posix_spawn_file_actions_addopen(&actions, STDOUT_FILENO, "/dev/null", O_WRONLY, 0);

    std::vector<std::string> args = {"curl", "--silent", "--show-error", "--fail", "--max-time", "10",
                                     "-H", "Content-Type: application/json", "--data-binary", "@-", url};
    std::vector<char*> argv;
    for (std::string& a : args) argv.push_back(a.data());
    argv.push_back(nullptr);

    // The worker thread blocks SIGPIPE; curl gets a normal signal mask
    posix_spawnattr_t attributes;
    posix_spawnattr_init(&attributes);
    sigset_t no_signals;
    sigemptyset(&no_signals);
    posix_spawnattr_setsigmask(&attributes, &no_signals);
    posix_spawnattr_setflags(&attributes, POSIX_SPAWN_SETSIGMASK);

    pid_t pid;
    int rc = posix_spawnp(&pid, "curl", &actions, &attributes, argv.data(), environ);
    posix_spawn_file_actions_destroy(&actions);
    posix_spawnattr_destroy(&attributes);
    close(in_pipe[0]);
    if (rc != 0) {
        close(in_pipe[1]);
        std::fprintf(stderr, "ntfy: can't run curl (is it installed?)\n");
        return false;
    }

    // Body is a few KB at most, well under the pipe buffer. If curl exits
    // early the write fails with EPIPE; the blocked SIGPIPE is cleared here
    // instead of killing the program.
    size_t written = 0;
    while (written < body.size()) {
        ssize_t n = write(in_pipe[1], body.data() + written, body.size() - written);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) break;
        written += static_cast<size_t>(n);
    }
    close(in_pipe[1]);
    if (written != body.size()) {
        sigset_t pipe_signal;
        sigemptyset(&pipe_signal);
        sigaddset(&pipe_signal, SIGPIPE);
        timespec no_wait{0, 0};
        sigtimedwait(&pipe_signal, nullptr, &no_wait);
    }

    int status = 0;
    while (waitpid(pid, &status, 0) < 0 && errno == EINTR) {}
    return written == body.size() && WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

}  // namespace

ntfy_notifier::ntfy_notifier(std::string topic, std::string server)
    : topic_(std::move(topic)), server_(std::move(server)) {
    while (!server_.empty() && server_.back() == '/') server_.pop_back();
    if (enabled()) worker_ = std::thread(&ntfy_notifier::workerLoop, this);
}

ntfy_notifier::~ntfy_notifier() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stopping_ = true;
    }
    wake_.notify_all();
    if (worker_.joinable()) worker_.join();
}

bool ntfy_notifier::enabled() const {
    return !topic_.empty() && !server_.empty();
}

bool ntfy_notifier::send(const std::string& title, const std::string& message, int priority,
                         const std::string& tags) {
    if (!enabled()) return false;
    if (priority < 1) priority = 1;
    if (priority > 5) priority = 5;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (stopping_) return false;
        if (queue_.size() >= MAX_QUEUE) {
            std::fprintf(stderr, "ntfy: queue full, dropped \"%s\"\n", title.c_str());
            ++failed_;
            return false;
        }
        queue_.push_back({title, message, priority, tags});
    }
    wake_.notify_all();
    return true;
}

bool ntfy_notifier::flush(std::chrono::milliseconds timeout) {
    std::unique_lock<std::mutex> lock(mutex_);
    return wake_.wait_for(lock, timeout, [this] { return queue_.empty() && !busy_; });
}

size_t ntfy_notifier::sentCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return sent_;
}

size_t ntfy_notifier::failedCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return failed_;
}

void ntfy_notifier::workerLoop() {
    sigset_t pipe_signal;
    sigemptyset(&pipe_signal);
    sigaddset(&pipe_signal, SIGPIPE);
    pthread_sigmask(SIG_BLOCK, &pipe_signal, nullptr);

    std::unique_lock<std::mutex> lock(mutex_);
    while (true) {
        wake_.wait(lock, [this] { return stopping_ || !queue_.empty(); });
        if (stopping_) return;
        message m = queue_.front();
        queue_.pop_front();
        busy_ = true;
        lock.unlock();

        bool ok = post(m);

        lock.lock();
        busy_ = false;
        ok ? ++sent_ : ++failed_;
        wake_.notify_all();
    }
}

bool ntfy_notifier::post(const message& m) {
    std::string body = jsonBody(topic_, m.title, m.text, m.priority, m.tags);
    for (int attempt = 0; attempt < ATTEMPTS; ++attempt) {
        if (attempt > 0) {
            // Retry wait, cut short by the destructor
            std::unique_lock<std::mutex> lock(mutex_);
            if (wake_.wait_for(lock, RETRY_DELAYS[attempt - 1], [this] { return stopping_; })) break;
        }
        if (runCurl(server_ + "/", body)) return true;
    }
    std::fprintf(stderr, "ntfy: gave up sending \"%s\"\n", m.title.c_str());
    return false;
}
