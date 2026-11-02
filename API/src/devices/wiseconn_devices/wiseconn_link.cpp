#include "wiseconn_link.h"

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <ctime>

#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

#include "wiseconn_json.h"

extern char** environ;

namespace {

constexpr size_t MAX_REPLY = 4 * 1024 * 1024;
constexpr const char* REQUEST_TIMEOUT_S = "20";

// Value for a curl config file line: quoted, with \ and " escaped
std::string curlQuoted(const std::string& text) {
    std::string out = "\"";
    for (char c : text) {
        if (c == '"' || c == '\\') out += '\\';
        if (c == '\n' || c == '\r') continue;
        out += c;
    }
    return out + "\"";
}

std::string curlExitReason(int code) {
    switch (code) {
        case 6: return "can't resolve host (no internet?)";
        case 7: return "can't connect";
        case 28: return "timed out";
        case 127: return "curl not installed";
        default: return "curl exit " + std::to_string(code);
    }
}

// One HTTP request with the API key: GET, or POST of json_body if it isn't
// empty. The whole request (URL, key, body) goes to curl as a config file on
// stdin, so neither the key nor the body appears on a command line (visible
// to other users in ps). http_code is the server's status (0 if none came
// back); body is the reply. False only if no HTTP reply arrived (error says
// why); the caller judges the status code.
bool httpRequest(const std::string& url, const std::string& api_key, const std::string& json_body,
                 std::string& body, int& http_code, std::string& error, std::atomic<int>& pid_out) {
    http_code = 0;
    int in_pipe[2], out_pipe[2];
    if (pipe2(in_pipe, O_CLOEXEC) < 0) {
        error = "pipe failed";
        return false;
    }
    if (pipe2(out_pipe, O_CLOEXEC) < 0) {
        close(in_pipe[0]);
        close(in_pipe[1]);
        error = "pipe failed";
        return false;
    }

    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_adddup2(&actions, in_pipe[0], STDIN_FILENO);
    posix_spawn_file_actions_adddup2(&actions, out_pipe[1], STDOUT_FILENO);
    posix_spawn_file_actions_addopen(&actions, STDERR_FILENO, "/dev/null", O_WRONLY, 0);

    // The calling thread may block SIGPIPE; curl gets a normal signal mask
    posix_spawnattr_t attributes;
    posix_spawnattr_init(&attributes);
    sigset_t no_signals;
    sigemptyset(&no_signals);
    posix_spawnattr_setsigmask(&attributes, &no_signals);
    posix_spawnattr_setflags(&attributes, POSIX_SPAWN_SETSIGMASK);

    // No --fail: the status code is appended on its own last line instead, so
    // WiseConn's error message in the body can be reported
    std::vector<std::string> args = {"curl", "--silent", "--max-time", REQUEST_TIMEOUT_S,
                                     "--write-out", "\n%{http_code}", "--config", "-"};
    std::vector<char*> argv;
    for (std::string& a : args) argv.push_back(a.data());
    argv.push_back(nullptr);

    pid_t pid;
    int rc = posix_spawnp(&pid, "curl", &actions, &attributes, argv.data(), environ);
    posix_spawn_file_actions_destroy(&actions);
    posix_spawnattr_destroy(&attributes);
    close(in_pipe[0]);
    close(out_pipe[1]);
    if (rc != 0) {
        close(in_pipe[1]);
        close(out_pipe[0]);
        error = "can't run curl (is it installed?)";
        return false;
    }
    pid_out = pid;

    std::string config = "url = " + curlQuoted(url) + "\n"
                         "header = " + curlQuoted("api_key: " + api_key) + "\n"
                         "header = \"Accept: application/json\"\n";
    if (!json_body.empty()) {
        config += "header = \"Content-Type: application/json\"\n"
                  "data-raw = " + curlQuoted(json_body) + "\n";   // data makes it a POST
    }

    // If curl exits early, the write fails with EPIPE instead of killing the
    // program: SIGPIPE is blocked on this thread around the write, and a
    // pending one is cleared before restoring the mask. Under a KB, well under
    // the pipe buffer, so this can't block on curl.
    sigset_t pipe_signal, old_mask;
    sigemptyset(&pipe_signal);
    sigaddset(&pipe_signal, SIGPIPE);
    pthread_sigmask(SIG_BLOCK, &pipe_signal, &old_mask);
    size_t written = 0;
    while (written < config.size()) {
        ssize_t n = write(in_pipe[1], config.data() + written, config.size() - written);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) break;
        written += static_cast<size_t>(n);
    }
    close(in_pipe[1]);
    if (written != config.size()) {
        timespec no_wait{0, 0};
        sigtimedwait(&pipe_signal, nullptr, &no_wait);
    }
    pthread_sigmask(SIG_SETMASK, &old_mask, nullptr);

    body.clear();
    bool too_big = false;
    char buffer[16384];
    while (true) {
        ssize_t n = read(out_pipe[0], buffer, sizeof(buffer));
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) break;
        if (body.size() + static_cast<size_t>(n) > MAX_REPLY) too_big = true;
        else body.append(buffer, static_cast<size_t>(n));
    }
    close(out_pipe[0]);

    int status = 0;
    while (waitpid(pid, &status, 0) < 0 && errno == EINTR) {}
    pid_out = 0;

    if (WIFSIGNALED(status)) {
        error = "request stopped";
        return false;
    }
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        error = curlExitReason(WIFEXITED(status) ? WEXITSTATUS(status) : -1);
        return false;
    }
    if (too_big) {
        error = "reply too large";
        return false;
    }
    size_t last = body.rfind('\n');
    if (last == std::string::npos) {
        error = "no status code from curl";
        return false;
    }
    http_code = std::atoi(body.c_str() + last + 1);
    body.erase(last);
    return true;
}

// GET that only succeeds on a 2xx reply
bool httpGet(const std::string& url, const std::string& api_key, std::string& body, std::string& error,
             std::atomic<int>& pid_out) {
    int code;
    if (!httpRequest(url, api_key, "", body, code, error, pid_out)) return false;
    if (code < 200 || code > 299) {
        error = "HTTP " + std::to_string(code) + (code == 401 || code == 403 ? " (bad api_key?)" : "");
        return false;
    }
    return true;
}

// Farm local wall clock time as WiseConn wants it: no Z, no offset
std::string farmTimeText(const std::tm& t) {
    char text[64];
    std::snprintf(text, sizeof(text), "%04d-%02d-%02dT%02d:%02d:%02d", t.tm_year + 1900, t.tm_mon + 1,
                  t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec);
    return text;
}

// Shortened reply for log messages
std::string replySnippet(const std::string& body) {
    std::string out = body.substr(0, 300);
    for (char& c : out) {
        if (c == '\n' || c == '\r') c = ' ';
    }
    return out;
}

// This machine's time zone name: $TZ, else the /etc/localtime link target
std::string hostTimeZone() {
    const char* tz = std::getenv("TZ");
    if (tz && *tz) return tz[0] == ':' ? tz + 1 : tz;
    char target[256];
    ssize_t n = readlink("/etc/localtime", target, sizeof(target) - 1);
    if (n <= 0) return "";
    std::string path(target, static_cast<size_t>(n));
    size_t at = path.find("zoneinfo/");
    return at == std::string::npos ? path : path.substr(at + 9);
}

// "2026-11-01T16:30:00.000Z" -> time point. Despite the Z, WiseConn sends the
// farm's local time, so this reads it as this machine's local time (mktime,
// DST worked out). False if it doesn't parse.
bool parseFarmTime(const std::string& text, std::chrono::system_clock::time_point& out) {
    std::tm t{};
    if (std::sscanf(text.c_str(), "%d-%d-%dT%d:%d:%d", &t.tm_year, &t.tm_mon, &t.tm_mday,
                    &t.tm_hour, &t.tm_min, &t.tm_sec) != 6) {
        return false;
    }
    t.tm_year -= 1900;
    t.tm_mon -= 1;
    t.tm_isdst = -1;
    time_t seconds = std::mktime(&t);
    if (seconds == static_cast<time_t>(-1)) return false;
    out = std::chrono::system_clock::from_time_t(seconds);
    return true;
}

}  // namespace

wiseconn_link::wiseconn_link(const wiseconn_keys& keys)
    : wiseconn_link(keys.api_key, keys.base_url, std::chrono::seconds(keys.poll_seconds),
                    std::chrono::minutes(keys.max_age_minutes), keys.farm_id) {
    std::string host = hostTimeZone();
    if (enabled() && !keys.time_zone.empty() && host != keys.time_zone) {
        std::fprintf(stderr, "wiseconn: this machine's time zone is \"%s\" but the farm's is \"%s\": "
                     "WiseConn times are farm local, so soil readings will look too old or too new\n",
                     host.c_str(), keys.time_zone.c_str());
    }
}

wiseconn_link::wiseconn_link(std::string api_key, std::string base_url, std::chrono::seconds poll_interval,
                             std::chrono::minutes max_age, std::string farm_id)
    : api_key_(std::move(api_key)), base_url_(std::move(base_url)), poll_interval_(poll_interval),
      max_age_(max_age), farm_id_(std::move(farm_id)) {
    while (!base_url_.empty() && base_url_.back() == '/') base_url_.pop_back();
    if (enabled()) worker_ = std::thread(&wiseconn_link::pollLoop, this);
}

wiseconn_link::~wiseconn_link() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stopping_ = true;
    }
    wake_.notify_all();
    int pid = curl_pid_;
    if (pid > 0) kill(pid, SIGTERM);
    if (worker_.joinable()) worker_.join();
}

bool wiseconn_link::enabled() const {
    return !api_key_.empty() && !base_url_.empty();
}

void wiseconn_link::addZone(const std::string& zone_id) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (std::find(zones_.begin(), zones_.end(), zone_id) != zones_.end()) return;
        zones_.push_back(zone_id);
        poll_now_ = true;
    }
    wake_.notify_all();
}

bool wiseconn_link::readMeasure(const std::string& measure_id, float& value) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto found = samples_.find(measure_id);
    if (found == samples_.end()) return false;
    if (std::chrono::system_clock::now() - found->second.measured > max_age_) return false;
    value = found->second.value;
    return true;
}

void wiseconn_link::watchHydraulics() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (hydraulics_wanted_) return;
        if (farm_id_.empty()) {
            std::fprintf(stderr, "wiseconn: no farm_id in wiseconn_keys.txt; valves can't be read\n");
            return;
        }
        hydraulics_wanted_ = true;
        poll_now_ = true;
    }
    wake_.notify_all();
}

std::chrono::steady_clock::duration wiseconn_link::hydraulicsMaxAge() const {
    return std::max<std::chrono::steady_clock::duration>(3 * poll_interval_, std::chrono::minutes(3));
}

bool wiseconn_link::readZoneOn(const std::string& zone_id, bool& on) const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (hydraulics_ok_ == -1 || std::chrono::steady_clock::now() - hydraulics_fetched_ > hydraulicsMaxAge()) return false;
    bool found = false, any_on = false;
    for (const hydraulic& h : hydraulics_) {
        if (h.type != "Valve" || std::find(h.zone_ids.begin(), h.zone_ids.end(), zone_id) == h.zone_ids.end()) continue;
        if (h.status == "On") any_on = true;
        else if (h.status != "Off") return false;   // unknown status: don't guess
        found = true;
    }
    if (!found) return false;
    on = any_on;
    return true;
}

bool wiseconn_link::createIrrigation(const std::string& zone_id, const std::tm& start, const std::tm& end,
                                     std::string& error) {
    return postIrrigation("{\"zoneId\":" + zone_id + ",\"initTime\":\"" + farmTimeText(start) +
                          "\",\"endTime\":\"" + farmTimeText(end) + "\",\"directSchedule\":true}", error);
}

bool wiseconn_link::createIrrigationNow(const std::string& zone_id, int duration_minutes, std::string& error) {
    return postIrrigation("{\"zoneId\":" + zone_id + ",\"duration\":" + std::to_string(duration_minutes) + "}",
                          error);
}

bool wiseconn_link::postIrrigation(const std::string& json_body, std::string& error) {
    if (!enabled()) {
        error = "no api_key";
        return false;
    }
    std::string body;
    int code;
    std::atomic<int> pid{0};
    if (!httpRequest(base_url_ + "/irrigations", api_key_, json_body, body, code, error, pid)) {
        if (error == "timed out") error += ": WiseConn may or may not have created it, check the app";
        return false;
    }
    if (code < 200 || code > 299) {
        error = "WiseConn refused it: HTTP " + std::to_string(code) + " " + replySnippet(body);
        return false;
    }
    std::fprintf(stderr, "wiseconn: irrigation created: %s -> %s\n", json_body.c_str(), replySnippet(body).c_str());
    {
        // Refresh the zone states soon so readOn() catches the change
        std::lock_guard<std::mutex> lock(mutex_);
        poll_now_ = true;
    }
    wake_.notify_all();
    return true;
}

void wiseconn_link::pollLoop() {
    sigset_t pipe_signal;
    sigemptyset(&pipe_signal);
    sigaddset(&pipe_signal, SIGPIPE);
    pthread_sigmask(SIG_BLOCK, &pipe_signal, nullptr);

    std::unique_lock<std::mutex> lock(mutex_);
    while (true) {
        wake_.wait_for(lock, poll_interval_, [this] { return stopping_ || poll_now_; });
        if (stopping_) return;
        poll_now_ = false;
        std::vector<std::string> zones = zones_;
        bool hydraulics = hydraulics_wanted_;
        lock.unlock();

        // Valves first: a status change matters sooner than 15-minute soil data
        if (hydraulics) pollHydraulics();
        for (const std::string& zone : zones) {
            {
                std::lock_guard<std::mutex> check(mutex_);
                if (stopping_) return;
            }
            pollZone(zone);
        }

        lock.lock();
    }
}

void wiseconn_link::pollZone(const std::string& zone_id) {
    std::string body, error;
    bool ok = httpGet(base_url_ + "/zones/" + zone_id + "/measures", api_key_, body, error, curl_pid_);

    std::map<std::string, sample> fresh;
    if (ok) {
        json_value reply;
        if (!parseJson(body, reply, error)) {
            ok = false;
            error = "bad JSON: " + error;
        } else if (reply.type != json_value::kind::array) {
            ok = false;
            error = "unexpected reply (not a list)";
        } else {
            for (const json_value& m : reply.items) {
                const json_value* id = m.find("id");
                const json_value* value = m.find("lastData");
                const json_value* date = m.find("lastDataDate");
                std::chrono::system_clock::time_point measured;
                if (!id || id->type != json_value::kind::string) continue;
                if (!value || value->type != json_value::kind::number) continue;  // null: no data
                if (!date || date->type != json_value::kind::string || !parseFarmTime(date->text, measured)) continue;
                fresh[id->text] = {static_cast<float>(value->number), measured};
            }
        }
    }

    std::lock_guard<std::mutex> lock(mutex_);
    if (stopping_) return;   // request was cut short by the destructor, not a real failure
    auto now = std::chrono::system_clock::now();
    for (auto& entry : fresh) {
        if (!warned_future_ && entry.second.measured > now + std::chrono::minutes(10)) {
            std::fprintf(stderr, "wiseconn: measure %s is timestamped in the future: check this machine's "
                         "time zone matches the farm's\n", entry.first.c_str());
            warned_future_ = true;
        }
        samples_[entry.first] = entry.second;
    }
    auto previous = zone_ok_.find(zone_id);
    if (previous == zone_ok_.end() || previous->second != ok) {
        if (ok) std::fprintf(stderr, "wiseconn: zone %s OK (%zu measures)\n", zone_id.c_str(), fresh.size());
        else std::fprintf(stderr, "wiseconn: zone %s poll failed: %s\n", zone_id.c_str(), error.c_str());
    }
    zone_ok_[zone_id] = ok;
}

void wiseconn_link::pollHydraulics() {
    std::string body, error;
    bool ok = httpGet(base_url_ + "/farms/" + farm_id_ + "/hydraulics", api_key_, body, error, curl_pid_);

    std::vector<hydraulic> fresh;
    if (ok) {
        json_value reply;
        if (!parseJson(body, reply, error)) {
            ok = false;
            error = "bad JSON: " + error;
        } else if (reply.type != json_value::kind::array) {
            ok = false;
            error = "unexpected reply (not a list)";
        } else {
            for (const json_value& h : reply.items) {
                const json_value* type = h.find("type");
                const json_value* status = h.find("status");
                const json_value* zones = h.find("zoneIds");
                if (!type || type->type != json_value::kind::string) continue;
                if (!status || status->type != json_value::kind::string) continue;
                hydraulic entry{type->text, status->text, {}};
                if (zones && zones->type == json_value::kind::array) {
                    for (const json_value& z : zones->items) {
                        if (z.type == json_value::kind::number) {
                            entry.zone_ids.push_back(std::to_string(static_cast<long long>(z.number)));
                        }
                    }
                }
                fresh.push_back(std::move(entry));
            }
        }
    }

    std::lock_guard<std::mutex> lock(mutex_);
    if (stopping_) return;   // request was cut short by the destructor, not a real failure
    if (ok) {
        hydraulics_ = std::move(fresh);
        hydraulics_fetched_ = std::chrono::steady_clock::now();
    }
    if (hydraulics_ok_ != (ok ? 1 : 0)) {
        if (ok) std::fprintf(stderr, "wiseconn: hydraulics OK (%zu valves/pumps)\n", hydraulics_.size());
        else std::fprintf(stderr, "wiseconn: hydraulics poll failed: %s\n", error.c_str());
    }
    hydraulics_ok_ = ok ? 1 : 0;
}
