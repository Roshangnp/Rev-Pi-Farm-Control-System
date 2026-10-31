#include "web_ui.h"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <sstream>
#include <type_traits>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

namespace {

constexpr size_t MAX_REQUEST_LENGTH = 16384;
constexpr size_t MAX_CLIENTS = 16;
constexpr int LISTEN_BACKLOG = 8;
constexpr auto CLIENT_TIMEOUT = std::chrono::seconds(5);

// The page polls /api/devices and builds one row per device signal. Inputs are
// created once and only their values updated, so typing isn't interrupted.
const char* const PAGE = R"html(<!doctype html>
<html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Devices</title>
<style>
body{font-family:system-ui,sans-serif;margin:16px;background:#fafafa;color:#222}
table{border-collapse:collapse;width:100%;max-width:720px;background:#fff}
th,td{padding:6px 10px;border-bottom:1px solid #ddd;text-align:left}
th{background:#eee}
td.value{font-variant-numeric:tabular-nums}
.error{color:#b00020;font-weight:600}
input[type=number]{width:6em}
#status{color:#666;font-size:0.9em;min-height:1.2em}
</style></head><body>
<h1>Devices</h1>
<p id="status"></p>
<table><thead><tr><th>Device</th><th>Signal</th><th>Value</th><th>Control</th></tr></thead>
<tbody id="rows"></tbody></table>
<script>
const rows = document.getElementById('rows');
const statusLine = document.getElementById('status');
const cells = {};

function showStatus(text, error) {
  statusLine.textContent = text;
  statusLine.className = error ? 'error' : '';
}

function format(field) {
  const v = field.value;
  if (v === null) return 'read failed';
  if (typeof v === 'boolean') {
    if (field.kind === 'switch') return v ? 'on' : 'off';
    return v ? 'yes' : 'no';
  }
  return String(+v.toPrecision(4)) + (field.unit ? ' ' + field.unit : '');
}

async function command(line) {
  try {
    const reply = await (await fetch('api/command', {method: 'POST', body: line})).text();
    if (reply !== 'ok') showStatus(line + ': ' + reply, true);
  } catch (e) {
    showStatus(line + ': ' + e, true);
  }
  refresh();
}

function makeRow(device, field, first) {
  const tr = document.createElement('tr');
  const name = tr.insertCell(), label = tr.insertCell(), value = tr.insertCell(), control = tr.insertCell();
  if (first) name.textContent = device.name;
  label.textContent = field.label;
  value.className = 'value';
  const cell = {value: value};
  if (field.kind === 'switch') {
    const box = document.createElement('input');
    box.type = 'checkbox';
    box.onchange = () => command(device.name + ' ' + (box.checked ? field.on : field.off));
    control.appendChild(box);
    cell.box = box;
  } else if (field.kind === 'setpoint') {
    const input = document.createElement('input');
    input.type = 'number';
    input.step = 'any';
    const send = document.createElement('button');
    send.textContent = 'Send';
    send.onclick = () => { if (input.value !== '') command(device.name + ' ' + field.method + ' ' + input.value); };
    input.onkeydown = (e) => { if (e.key === 'Enter') send.onclick(); };
    control.append(input, ' ', send);
  }
  rows.appendChild(tr);
  return cell;
}

let timer = null;
async function refresh() {
  clearTimeout(timer);
  try {
    const devices = await (await fetch('api/devices')).json();
    for (const device of devices) {
      device.fields.forEach((field, i) => {
        const key = device.name + '/' + field.label;
        const cell = cells[key] || (cells[key] = makeRow(device, field, i === 0));
        cell.value.textContent = format(field);
        cell.value.classList.toggle('error', field.value === null || (field.label === 'alarm' && field.value === true));
        if (cell.box && field.value !== null) cell.box.checked = field.value;
      });
    }
    if (!statusLine.classList.contains('error')) showStatus('Updated ' + new Date().toLocaleTimeString(), false);
  } catch (e) {
    showStatus('Connection lost: ' + e, true);
  }
  timer = setTimeout(refresh, 1000);
}
statusLine.onclick = () => showStatus('', false);
refresh();
</script></body></html>
)html";

std::string jsonString(const std::string& text) {
    std::string out = "\"";
    for (char ch : text) {
        if (ch == '"' || ch == '\\') out += '\\';
        if (static_cast<unsigned char>(ch) < 0x20) continue;
        out += ch;
    }
    return out + "\"";
}

std::string jsonBool(bool success, bool value) {
    if (!success) return "null";
    return value ? "true" : "false";
}

std::string jsonFloat(bool success, float value) {
    if (!success || !std::isfinite(value)) return "null";
    char text[32];
    std::snprintf(text, sizeof(text), "%.6g", static_cast<double>(value));
    return text;
}

// Field kinds understood by the page: "bool" / "value" are read only,
// "switch" has on/off command methods, "setpoint" has one method taking a number.
std::string readOnlyField(const std::string& label, const std::string& kind, const std::string& value,
                          const std::string& unit = "") {
    return "{\"label\":" + jsonString(label) + ",\"kind\":" + jsonString(kind) +
           ",\"value\":" + value + ",\"unit\":" + jsonString(unit) + "}";
}

std::string switchField(const std::string& label, const std::string& value,
                        const std::string& on_method, const std::string& off_method) {
    return "{\"label\":" + jsonString(label) + ",\"kind\":\"switch\",\"value\":" + value +
           ",\"on\":" + jsonString(on_method) + ",\"off\":" + jsonString(off_method) + "}";
}

std::string setpointField(const std::string& label, const std::string& value,
                          const std::string& method, const std::string& unit) {
    return "{\"label\":" + jsonString(label) + ",\"kind\":\"setpoint\",\"value\":" + value +
           ",\"method\":" + jsonString(method) + ",\"unit\":" + jsonString(unit) + "}";
}

std::string httpResponse(const std::string& status, const std::string& content_type, const std::string& body) {
    return "HTTP/1.1 " + status + "\r\n"
           "Content-Type: " + content_type + "\r\n"
           "Content-Length: " + std::to_string(body.size()) + "\r\n"
           "Cache-Control: no-store\r\n"
           "Connection: close\r\n\r\n" + body;
}

bool parseFloat(const std::string& text, float& value) {
    if (text.empty()) return false;
    char* end = nullptr;
    value = std::strtof(text.c_str(), &end);
    return *end == '\0';
}

// Content-Length of a request, 0 if absent. Header names are case insensitive.
size_t contentLength(const std::string& headers) {
    std::string lower = headers;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char ch) {return std::tolower(ch);});
    size_t at = lower.find("\r\ncontent-length:");
    if (at == std::string::npos) return 0;
    return std::strtoul(lower.c_str() + at + 17, nullptr, 10);
}

}  // namespace

web_ui::~web_ui() {
    for (client& c : clients_) close(c.fd);
    if (listen_fd_ >= 0) close(listen_fd_);
}

bool web_ui::addDevice(const std::string& name, pump& device) {return addDeviceRef(name, &device);}
bool web_ui::addDevice(const std::string& name, float_switch& device) {return addDeviceRef(name, &device);}
bool web_ui::addDevice(const std::string& name, valve& device) {return addDeviceRef(name, &device);}
bool web_ui::addDevice(const std::string& name, pressure_transducer& device) {return addDeviceRef(name, &device);}
bool web_ui::addDevice(const std::string& name, tds_sensor& device) {return addDeviceRef(name, &device);}
bool web_ui::addDevice(const std::string& name, flow_sensor& device) {return addDeviceRef(name, &device);}
bool web_ui::addDevice(const std::string& name, level_sensor& device) {return addDeviceRef(name, &device);}

bool web_ui::addDeviceRef(const std::string& name, device_ref device) {
    if (name.empty() || name.find_first_of(" \t\r\n") != std::string::npos) return false;
    for (const auto& entry : devices_) {
        if (entry.first == name) return false;
    }
    devices_.emplace_back(name, device);
    return true;
}

bool web_ui::start() {
    if (listen_fd_ >= 0) return true;

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(port_);
    if (inet_pton(AF_INET, bind_address_.c_str(), &address.sin_addr) != 1) return false;

    int fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if (fd < 0) return false;
    int one = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

    if (bind(fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0 ||
        listen(fd, LISTEN_BACKLOG) < 0) {
        close(fd);
        return false;
    }
    listen_fd_ = fd;
    return true;
}

void web_ui::poll(int timeout_ms) {
    if (listen_fd_ < 0) return;

    std::vector<pollfd> fds;
    fds.push_back({listen_fd_, POLLIN, 0});
    for (const client& c : clients_) {
        fds.push_back({c.fd, static_cast<short>(c.tx_buffer.empty() ? POLLIN : POLLOUT), 0});
    }

    if (::poll(fds.data(), fds.size(), std::max(timeout_ms, 0)) > 0) {
        // Walk backwards so dropping a client doesn't shift the ones still to check
        for (size_t i = fds.size() - 1; i >= 1; --i) {
            if (fds[i].revents == 0) continue;
            client& c = clients_[i - 1];
            bool keep = c.tx_buffer.empty() ? readClient(c) : writeClient(c);
            if (!keep) {
                close(c.fd);
                clients_.erase(clients_.begin() + static_cast<std::ptrdiff_t>(i - 1));
            }
        }
        if (fds[0].revents & POLLIN) acceptClients();
    }

    auto now = std::chrono::steady_clock::now();
    for (size_t i = clients_.size(); i-- > 0;) {
        if (now - clients_[i].last_seen > CLIENT_TIMEOUT) {
            close(clients_[i].fd);
            clients_.erase(clients_.begin() + static_cast<std::ptrdiff_t>(i));
        }
    }
}

void web_ui::acceptClients() {
    while (true) {
        int fd = accept4(listen_fd_, nullptr, nullptr, SOCK_NONBLOCK | SOCK_CLOEXEC);
        if (fd < 0) return;
        if (clients_.size() >= MAX_CLIENTS) {
            close(fd);
            continue;
        }
        clients_.push_back({fd, "", "", std::chrono::steady_clock::now()});
    }
}

bool web_ui::readClient(client& c) {
    char buffer[2048];
    ssize_t n = recv(c.fd, buffer, sizeof(buffer), 0);
    if (n == 0) return false;
    if (n < 0) return errno == EAGAIN || errno == EWOULDBLOCK;

    c.rx_buffer.append(buffer, static_cast<size_t>(n));
    c.last_seen = std::chrono::steady_clock::now();
    if (c.rx_buffer.size() > MAX_REQUEST_LENGTH) return false;

    size_t header_end = c.rx_buffer.find("\r\n\r\n");
    if (header_end == std::string::npos) return true;
    size_t body_length = contentLength(c.rx_buffer.substr(0, header_end + 2));
    if (header_end + 4 + body_length > MAX_REQUEST_LENGTH) return false;
    if (c.rx_buffer.size() < header_end + 4 + body_length) return true;

    std::istringstream request_line(c.rx_buffer.substr(0, c.rx_buffer.find("\r\n")));
    std::string method, target;
    request_line >> method >> target;
    std::string path = target.substr(0, target.find('?'));
    std::string body = c.rx_buffer.substr(header_end + 4, body_length);

    c.tx_buffer = handleRequest(method, path, body);
    c.rx_buffer.clear();
    return writeClient(c);
}

bool web_ui::writeClient(client& c) {
    ssize_t n = send(c.fd, c.tx_buffer.data(), c.tx_buffer.size(), MSG_NOSIGNAL);
    if (n < 0) return errno == EAGAIN || errno == EWOULDBLOCK;
    c.tx_buffer.erase(0, static_cast<size_t>(n));
    c.last_seen = std::chrono::steady_clock::now();
    return !c.tx_buffer.empty();
}

std::string web_ui::handleRequest(const std::string& method, const std::string& path, const std::string& body) {
    if (path == "/" || path == "/index.html") {
        if (method != "GET") return httpResponse("405 Method Not Allowed", "text/plain", "GET only\n");
        return httpResponse("200 OK", "text/html; charset=utf-8", PAGE);
    }
    if (path == "/api/devices") {
        if (method != "GET") return httpResponse("405 Method Not Allowed", "text/plain", "GET only\n");
        return httpResponse("200 OK", "application/json", devicesJson());
    }
    if (path == "/api/command") {
        if (method != "POST") return httpResponse("405 Method Not Allowed", "text/plain", "POST only\n");
        return httpResponse("200 OK", "text/plain", runCommand(body));
    }
    return httpResponse("404 Not Found", "text/plain", "not found\n");
}

std::string web_ui::devicesJson() {
    std::string json = "[";
    for (const auto& [name, ref] : devices_) {
        if (json.size() > 1) json += ",";

        std::string fields = std::visit([](auto* device) -> std::string {
            using T = std::remove_pointer_t<decltype(device)>;
            if constexpr (std::is_same_v<T, pump>) {
                bool on = false;
                bool on_ok = device->readOn(on);
                std::string fields = switchField("run", jsonBool(on_ok, on), "setOn", "setOff");
                if (device->hasSpeedControl()) {
                    float speed = 0.0f;
                    bool speed_ok = device->readSpeed(speed);
                    fields += "," + setpointField("speed", jsonFloat(speed_ok, speed), "setSpeed", "(0-1)");
                }
                if (device->hasAlarm()) {
                    fields += "," + readOnlyField("alarm", "bool", jsonBool(true, device->readAlarm()));
                }
                return fields;
            } else if constexpr (std::is_same_v<T, valve>) {
                bool open = false;
                bool success = device->readOpen(open);
                return switchField("open", jsonBool(success, open), "setOpen", "setClosed");
            } else if constexpr (std::is_same_v<T, float_switch>) {
                bool submerged = false;
                bool success = device->readSubmerged(submerged);
                return readOnlyField("submerged", "bool", jsonBool(success, submerged));
            } else if constexpr (std::is_same_v<T, pressure_transducer>) {
                float psi = 0.0f;
                bool success = device->readPressure(psi);
                return readOnlyField("pressure", "value", jsonFloat(success, psi), "psi");
            } else if constexpr (std::is_same_v<T, tds_sensor>) {
                float ppm = 0.0f;
                bool success = device->readTds(ppm);
                return readOnlyField("tds", "value", jsonFloat(success, ppm), "ppm");
            } else if constexpr (std::is_same_v<T, flow_sensor>) {
                float gpm = 0.0f;
                bool success = device->readFlow(gpm);
                return readOnlyField("flow", "value", jsonFloat(success, gpm), "gpm");
            } else if constexpr (std::is_same_v<T, level_sensor>) {
                float level = 0.0f;
                bool success = device->readLevel(level);
                return readOnlyField("level", "value", jsonFloat(success, level));
            }
        }, ref);

        json += "{\"name\":" + jsonString(name) + ",\"fields\":[" + fields + "]}";
    }
    return json + "]";
}

std::string web_ui::runCommand(const std::string& line) {
    std::istringstream words(line);
    std::string name, method, arg, extra;
    words >> name >> method >> arg >> extra;
    if (!extra.empty()) return "err too many arguments";

    auto found = std::find_if(devices_.begin(), devices_.end(),
                              [&](const auto& entry) {return entry.first == name;});
    if (found == devices_.end()) return "err unknown device";

    // Only commands; reads go through /api/devices
    return std::visit([&](auto* device) -> std::string {
        using T = std::remove_pointer_t<decltype(device)>;
        if constexpr (std::is_same_v<T, pump>) {
            if (method == "setOn") return device->setOn() ? "ok" : "fail";
            if (method == "setOff") return device->setOff() ? "ok" : "fail";
            if (method == "setSpeed") {
                float speed;
                if (!parseFloat(arg, speed)) return "err bad argument";
                return device->setSpeed(speed) ? "ok" : "fail";
            }
        } else if constexpr (std::is_same_v<T, valve>) {
            if (method == "setOpen") return device->setOpen() ? "ok" : "fail";
            if (method == "setClosed") return device->setClosed() ? "ok" : "fail";
        }
        return "err unknown method";
    }, found->second);
}
