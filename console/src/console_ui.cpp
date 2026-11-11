#include "console_ui.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <thread>

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

#include "devices.h"
#include "types.h"

using namespace ftxui;

namespace {

constexpr int LABEL_WIDTH = 25;   // longest device name, so rows fit an 80-column terminal
constexpr int VALUE_WIDTH = 9;
constexpr auto REFRESH_PERIOD = std::chrono::milliseconds(500);

std::string formatFloat(float value, int decimals) {
    char text[32];
    std::snprintf(text, sizeof(text), "%.*f", decimals, static_cast<double>(value));
    return text;
}

bool parseFloat(const std::string& text, float& value) {
    if (text.empty()) return false;
    char* end = nullptr;
    value = std::strtof(text.c_str(), &end);
    return *end == '\0';
}

Element label(const std::string& text_value) {
    return text(text_value) | size(WIDTH, EQUAL, LABEL_WIDTH);
}

// A read value, or a red "---" if the read failed (owner down, stale, broken wire)
Element reading(bool ok, const std::string& value, Color good = Color::Default) {
    Element e = ok ? text(value) | color(good) : text("---") | color(Color::Red);
    return e | align_right | size(WIDTH, EQUAL, VALUE_WIDTH);
}

std::string clockText() {
    std::time_t now = std::time(nullptr);
    char text_value[16];
    std::strftime(text_value, sizeof(text_value), "%H:%M:%S", std::localtime(&now));
    return text_value;
}

class console_ui{
    public:
    console_ui(console_devices& farm, std::string ro_host, std::string upper_host)
    : farm_(farm), ro_host_(std::move(ro_host)), upper_host_(std::move(upper_host)){};

    int run();

    private:
    // Shows a Yes/No dialog; action runs on Yes and its result goes in the status line
    void ask(const std::string& question, std::function<bool()> action);

    Component sensorTab(std::vector<sensor_row>& rows, int decimals);
    Component floatTab(std::vector<float_row>& rows);
    Component pumpTab();
    Component valveTab();
    Component roTab();
    Component onOffButtons(const std::string& name, std::function<bool()> on, std::function<bool()> off,
                           const std::string& on_word = "On", const std::string& off_word = "Off");
    Component numberEntry(const std::string& what, const std::string& placeholder, std::function<bool(float)> send);
    Element header();

    console_devices& farm_;
    std::string ro_host_;
    std::string upper_host_;

    bool confirm_shown_ = false;
    std::string confirm_question_;
    std::function<bool()> confirm_action_;
    std::string status_ = "Ready";

    // RO tab dropdowns
    std::vector<std::string> states_;
    std::vector<std::string> procedures_;
    int state_selected_ = 0;
    int procedure_selected_ = 0;
};

void console_ui::ask(const std::string& question, std::function<bool()> action) {
    confirm_question_ = question;
    confirm_action_ = std::move(action);
    confirm_shown_ = true;
}

Component console_ui::onOffButtons(const std::string& name, std::function<bool()> on, std::function<bool()> off,
                                   const std::string& on_word, const std::string& off_word) {
    auto on_button = Button(on_word, [this, name, on, on_word] {
        ask("Set " + name + " " + on_word + "?", on);
    }, ButtonOption::Ascii());
    auto off_button = Button(off_word, [this, name, off, off_word] {
        ask("Set " + name + " " + off_word + "?", off);
    }, ButtonOption::Ascii());
    return Container::Horizontal({on_button, off_button});
}

// Text box plus Send; the value is checked here, then confirmed
Component console_ui::numberEntry(const std::string& what, const std::string& placeholder,
                                  std::function<bool(float)> send) {
    auto entry = std::make_shared<std::string>();
    InputOption option;
    option.multiline = false;
    auto input = Input(entry.get(), placeholder, option);
    auto send_button = Button("Send", [this, entry, what, send] {
        float value;
        if (!parseFloat(*entry, value)) {
            status_ = "Not a number: \"" + *entry + "\"";
            return;
        }
        ask("Set " + what + " to " + *entry + "?", [send, value] { return send(value); });
    }, ButtonOption::Ascii());
    auto row = Container::Horizontal({input, send_button});
    return Renderer(row, [input, send_button, entry] {
        return hbox({text("["), input->Render() | size(WIDTH, EQUAL, 8), text("] "), send_button->Render()});
    });
}

Component console_ui::sensorTab(std::vector<sensor_row>& rows, int decimals) {
    return Renderer([&rows, decimals] {
        Elements lines;
        for (sensor_row& r : rows) {
            float value = 0.0f;
            bool ok = r.read(value);
            lines.push_back(hbox({label(r.label), reading(ok, formatFloat(value, decimals)), text(" " + r.unit)}));
        }
        return vbox(lines);
    });
}

Component console_ui::floatTab(std::vector<float_row>& rows) {
    return Renderer([&rows] {
        Elements lines;
        for (float_row& r : rows) {
            bool submerged = false;
            bool ok = r.read(submerged);
            lines.push_back(hbox({label(r.label),
                                  reading(ok, submerged ? "SUBMERGED" : "dry", submerged ? Color::Cyan : Color::Yellow)}));
        }
        return vbox(lines);
    });
}

Component console_ui::pumpTab() {
    Components rows;
    for (pump_row& p : farm_.pumps) {
        pump* device = p.device.get();
        // The irrigation pump has no run relay, only a pressure setpoint
        Component buttons = p.pressure_control
            ? Container::Horizontal({})
            : onOffButtons(p.label, [device] { return device->setOn(); }, [device] { return device->setOff(); });
        rows.push_back(Renderer(buttons, [&p, device, buttons] {
            Elements e = {label(p.label)};
            if (p.pressure_control) {
                float psi = 0.0f;
                bool ok = device->readPressure(psi);
                e.push_back(reading(ok, formatFloat(psi, 0) + " psi"));
            } else {
                bool on = false;
                bool ok = device->readOn(on);
                e.push_back(reading(ok, on ? "ON" : "off", on ? Color(Color::Green) : Color(Color::Default)));
                e.push_back(text("  "));
                e.push_back(buttons->Render());
            }
            if (device->hasAlarm() && device->readAlarm()) e.push_back(text("  ALARM") | color(Color::Red) | bold);
            return hbox(e);
        }));

        // Speed or pressure setpoint on its own line under the pump
        if (p.speed_control || p.pressure_control) {
            bool speed = p.speed_control;
            auto entry = speed
                ? numberEntry(p.label + " speed", "0-1", [device](float v) { return device->setSpeed(v); })
                : numberEntry(p.label + " pressure", "psi", [device](float v) { return device->setPressure(v); });
            rows.push_back(Renderer(entry, [device, entry, speed] {
                Elements e = {text(speed ? "  speed (0-1)" : "  setpoint (psi)") | size(WIDTH, EQUAL, LABEL_WIDTH)};
                if (speed) {
                    float value = 0.0f;
                    bool ok = device->readSpeed(value);
                    e.push_back(reading(ok, formatFloat(value, 2)));
                    e.push_back(text("  "));
                }
                e.push_back(entry->Render());
                return hbox(e);
            }));
        }
    }
    for (load_row& l : farm_.loads) {
        switched_load* device = l.device.get();
        auto buttons = onOffButtons(l.label, [device] { return device->setOn(); }, [device] { return device->setOff(); });
        rows.push_back(Renderer(buttons, [&l, device, buttons] {
            bool on = false;
            bool ok = device->readOn(on);
            return hbox({label(l.label), reading(ok, on ? "ON" : "off", on ? Color(Color::Green) : Color(Color::Default)),
                         text("  "), buttons->Render()});
        }));
    }
    return Container::Vertical(rows);
}

Component console_ui::valveTab() {
    Components rows;
    for (valve_row& v : farm_.valves) {
        valve* device = v.device.get();
        auto buttons = onOffButtons(v.label, [device] { return device->setOpen(); },
                                    [device] { return device->setClosed(); }, "Open", "Close");
        rows.push_back(Renderer(buttons, [&v, device, buttons] {
            bool open = false;
            bool ok = device->readOpen(open);
            return hbox({label(v.label), reading(ok, open ? "OPEN" : "closed", open ? Color(Color::Green) : Color(Color::Default)),
                         text("  "), buttons->Render()});
        }));
    }
    return Container::Vertical(rows);
}

Component console_ui::roTab() {
    for (RO_state s : ALL_RO_STATES) states_.push_back(toString(s));
    for (procedure_types p : ALL_PROCEDURE_TYPES) procedures_.push_back(toString(p));

    auto state_list = Dropdown(&states_, &state_selected_);
    auto set_state = Button("Set state", [this] {
        RO_state s = ALL_RO_STATES[state_selected_];
        ask("Set the RO to " + std::string(toString(s)) + "?", [this, s] { return farm_.ro.setState(s); });
    }, ButtonOption::Ascii());

    auto procedure_list = Dropdown(&procedures_, &procedure_selected_);
    auto run_procedure = Button("Run", [this] {
        procedure_types p = ALL_PROCEDURE_TYPES[procedure_selected_];
        ask("Run " + std::string(toString(p)) + "?", [this, p] { return farm_.ro.runProcedure(p); });
    }, ButtonOption::Ascii());
    auto stop_procedure = Button("Stop", [this] {
        ask("Stop the running procedure (the RO goes OFF)?", [this] { return farm_.ro.stopProcedure(); });
    }, ButtonOption::Ascii());

    auto speed = numberEntry("RO pump speed", "0-1", [this](float v) { return farm_.ro.setRoSpeed(v); });

    auto controls = Container::Vertical({
        Container::Horizontal({state_list, set_state}),
        Container::Horizontal({procedure_list, run_procedure, stop_procedure}),
        speed,
    });

    return Renderer(controls, [this, state_list, set_state, procedure_list, run_procedure, stop_procedure, speed] {
        RO_state state = RO_state::OFF;
        bool state_ok = farm_.ro.readState(state);
        bool settling = false;
        bool settling_ok = farm_.ro.isSettling(settling);

        Element state_text = state_ok ? text(toString(state)) | color(Color::Green) | bold : text("---") | color(Color::Red);
        Elements lines = {
            hbox({label("RO state"), state_text}),
            hbox({label("valves settling"), reading(settling_ok, settling ? "yes" : "no")}),
        };
        for (toggle_row& t : farm_.toggles) {
            bool on = false;
            bool ok = t.read(on);
            lines.push_back(hbox({label(t.label), reading(ok, on ? "ON" : "off")}));
        }
        lines.push_back(separator());
        // Short labels here: the dropdowns need the width
        auto short_label = [](const std::string& s) { return text(s) | size(WIDTH, EQUAL, 11); };
        lines.push_back(hbox({short_label("New state"), state_list->Render(), text(" "), set_state->Render() | vcenter}));
        lines.push_back(hbox({short_label("Procedure"), procedure_list->Render(), text(" "), run_procedure->Render() | vcenter,
                              text(" "), stop_procedure->Render() | vcenter}));
        lines.push_back(hbox({short_label("Speed 0-1"), speed->Render()}));
        return vbox(lines);
    });
}

Element console_ui::header() {
    auto link_status = [](const std::string& name, const std::string& host, const remote_link& link) {
        bool up = link.isConnected();
        return hbox({text(name + "@" + host + " "),
                     text(up ? "OK" : "DOWN") | color(up ? Color::Green : Color::Red) | bold});
    };
    return hbox({text(" Farm console") | bold, text("   "),
                 link_status("RO", ro_host_, farm_.ro_link), text("   "),
                 link_status("Upper tank", upper_host_, farm_.upper_link),
                 filler(), text(clockText() + " ")});
}

int console_ui::run() {
    std::vector<std::string> tab_names = {
        "RO-Pressure-Sensors",
        "RO-Flow-Sensors",
        "Tank-Water-Levels",
        "Chemical-Tank-Floats",
        "Water-Tank-Floats",
        "Manual-Pump-Control",
        "Manual-RO-Valve-Control",
        "RO-Control",
    };
    int tab_selected = 0;
    auto tab_menu = Menu(&tab_names, &tab_selected);
    auto tabs = Container::Tab({
        sensorTab(farm_.ro_pressures, 1),
        sensorTab(farm_.ro_flows, 2),
        sensorTab(farm_.tank_levels, 2),
        floatTab(farm_.chemical_floats),
        floatTab(farm_.water_floats),
        pumpTab(),
        valveTab(),
        roTab(),
    }, &tab_selected);

    auto main_container = Container::Horizontal({tab_menu, tabs});
    auto main_view = Renderer(main_container, [&] {
        return vbox({
            header(),
            separator(),
            hbox({tab_menu->Render() | size(WIDTH, EQUAL, 26), separator(), tabs->Render() | flex}) | flex,
            separator(),
            hbox({text(" " + status_) | flex, text(" Enter=select Esc=cancel ^C=quit ") | dim}),
        });
    });

    auto yes = Button("Yes", [this] {
        confirm_shown_ = false;
        bool ok = confirm_action_ && confirm_action_();
        std::string what = confirm_question_.substr(0, confirm_question_.size() - 1);   // without the '?'
        status_ = (ok ? "Done: " : "FAILED: ") + what;
    }, ButtonOption::Ascii());
    auto no = Button("No", [this] {
        confirm_shown_ = false;
        status_ = "Cancelled: " + confirm_question_;
    }, ButtonOption::Ascii());
    auto confirm_buttons = Container::Horizontal({no, yes});   // No first, so Enter alone cancels
    auto confirm_view = Renderer(confirm_buttons, [this, yes, no] {
        return vbox({text(confirm_question_) | bold, separator(),
                     hbox({filler(), no->Render(), text("   "), yes->Render(), filler()})})
               | border | size(WIDTH, GREATER_THAN, 40);
    });

    auto root = Modal(main_view, confirm_view, &confirm_shown_);
    root = CatchEvent(root, [this](Event event) {
        if (confirm_shown_ && event == Event::Escape) {
            confirm_shown_ = false;
            status_ = "Cancelled: " + confirm_question_;
            return true;
        }
        return false;
    });

    auto screen = ScreenInteractive::Fullscreen();

    // Redraw twice a second so values stay live without key presses
    std::atomic<bool> running{true};
    std::thread ticker([&] {
        while (running) {
            std::this_thread::sleep_for(REFRESH_PERIOD);
            screen.PostEvent(Event::Custom);
        }
    });
    screen.Loop(root);
    running = false;
    ticker.join();
    return 0;
}

}  // namespace

int runConsole(console_devices& farm, const std::string& ro_host, const std::string& upper_host) {
    console_ui ui(farm, ro_host, upper_host);
    return ui.run();
}
