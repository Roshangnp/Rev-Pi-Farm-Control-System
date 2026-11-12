#include "console_ui.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <thread>

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/terminal.hpp>

#include "devices.h"
#include "procedure.h"   // procedure_types
#include "types.h"

using namespace ftxui;

namespace {

constexpr int LABEL_WIDTH = 25;
constexpr int VALUE_WIDTH = 9;
constexpr int MENU_WIDTH = 17;    // tab list on the left
constexpr int PUMP_LABEL_WIDTH = 17;
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

Element label(const std::string& text_value, int width = LABEL_WIDTH) {
    return text(text_value) | size(WIDTH, EQUAL, width);
}

// Short labels in front of the dropdowns, which need the width
Element shortLabel(const std::string& text_value) {
    return label(text_value, 11);
}

// A read value, or a red "---" if the read failed (owner down, stale, broken wire)
Element reading(bool ok, const std::string& value, Color good = Color::Default) {
    Element e = ok ? text(value) | color(good) : text("---") | color(Color::Red);
    return e | align_right | size(WIDTH, EQUAL, VALUE_WIDTH);
}

// Label width for a tile: the longest label plus a space
template <typename row>
int labelWidth(const std::vector<row>& rows) {
    size_t widest = 0;
    for (const row& r : rows) widest = std::max(widest, r.label.size());
    return static_cast<int>(widest) + 1;
}

Element sensorList(std::vector<sensor_row>& rows, int decimals, int width) {
    Elements lines;
    for (sensor_row& r : rows) {
        float value = 0.0f;
        bool ok = r.read(value);
        lines.push_back(hbox({label(r.label, width), reading(ok, formatFloat(value, decimals)), text(" " + r.unit)}));
    }
    return vbox(lines);
}

Element floatList(std::vector<float_row>& rows, int width) {
    Elements lines;
    for (float_row& r : rows) {
        bool submerged = false;
        bool ok = r.read(submerged);
        lines.push_back(hbox({label(r.label, width),
                              reading(ok, submerged ? "SUBMERGED" : "dry", submerged ? Color::Cyan : Color::Yellow)}));
    }
    return vbox(lines);
}

// A dashboard tile: a titled box, heavy border while it holds the focus
Element tile(const std::string& title, Element content, bool focused) {
    return window(text(" " + title + " ") | bold, content, focused ? HEAVY : ROUNDED);
}

// Read-only tile. Focusable, so Up/Down walk through every tile and the
// dashboard scrolls to it on a small terminal
Component readTile(const std::string& title, std::function<Element()> content) {
    return Renderer([title, content](bool focused) {
        Element e = tile(title, content(), focused);
        return focused ? e | focus : e;
    });
}

// Tile around controls; top (optional) is read-only content shown above them
Component controlTile(const std::string& title, Component inner, std::function<Element()> top = nullptr) {
    return Renderer(inner, [title, inner, top] {
        Element content = top ? vbox({top(), inner->Render()}) : inner->Render();
        return tile(title, content, inner->Focused());
    });
}

// Tiles laid out in rows as wide as the screen allows (one column on an
// 80-column terminal), scrolling to the focused tile when they don't fit.
// Up/Down move through the tiles in reading order
Component dashboard(Components tiles) {
    auto container = Container::Vertical(tiles);
    return Renderer(container, [container, tiles] {
        int width = Terminal::Size().dimx - MENU_WIDTH - 1;
        Elements rows;
        Elements row;
        int used = 0;
        for (const Component& t : tiles) {
            Element e = t->Render();
            e->ComputeRequirement();
            int w = e->requirement().min_x;
            if (!row.empty() && used + w > width) {
                rows.push_back(hbox(row));
                row.clear();
                used = 0;
            }
            row.push_back(e | xflex);
            used += w;
        }
        if (!row.empty()) rows.push_back(hbox(row));
        // Not focused (tab list selected): show the top, not wherever the
        // tiles' first buttons are
        if (!container->Focused() && !rows.empty()) rows[0] = rows[0] | focus;
        return vbox(rows) | vscroll_indicator | yframe | flex;
    });
}

std::string clockText() {
    std::time_t now = std::time(nullptr);
    char text_value[16];
    std::strftime(text_value, sizeof(text_value), "%H:%M:%S", std::localtime(&now));
    return text_value;
}

class console_ui{
    public:
    console_ui(console_devices& farm, std::string ro_host, std::string upper_host);

    int run();

    private:
    // Shows a Yes/No dialog; action runs on Yes and its result goes in the status line
    void ask(const std::string& question, std::function<bool()> action);

    Component mainDashboard();
    Component roDashboard();
    Components valveRows(int width);

    Components pumpRows(std::vector<pump_row>& pumps, int width);
    Components loadRows(int width);
    // RO pieces, shared by both dashboards and the RO-Control tab
    Element roStatus(bool with_settling);
    Component procedureControls();
    Component procedureButtons();
    Component stateControls(const std::string& row_label);
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

    // Dropdown lists; the selections are shared by every copy of the controls
    std::vector<std::string> states_;
    std::vector<std::string> procedures_;
    int state_selected_ = 0;
    int procedure_selected_ = 0;
};

console_ui::console_ui(console_devices& farm, std::string ro_host, std::string upper_host)
    : farm_(farm), ro_host_(std::move(ro_host)), upper_host_(std::move(upper_host)) {
    for (RO_state s : ALL_RO_STATES) states_.push_back(toString(s));
    for (procedure_types p : ALL_PROCEDURE_TYPES) procedures_.push_back(toString(p));
}

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

Components console_ui::pumpRows(std::vector<pump_row>& pumps, int width) {
    Components rows;
    for (pump_row& p : pumps) {
        pump* device = p.device.get();
        // The irrigation pump has no run relay, only a pressure setpoint
        Component buttons = p.pressure_control
            ? Container::Horizontal({})
            : onOffButtons(p.label, [device] { return device->setOn(); }, [device] { return device->setOff(); });
        rows.push_back(Renderer(buttons, [&p, device, buttons, width] {
            Elements e = {label(p.label, width)};
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
            rows.push_back(Renderer(entry, [device, entry, speed, width] {
                Elements e = {label(speed ? "  speed (0-1)" : "  setpoint (psi)", width)};
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
    return rows;
}

Components console_ui::loadRows(int width) {
    Components rows;
    for (load_row& l : farm_.loads) {
        switched_load* device = l.device.get();
        auto buttons = onOffButtons(l.label, [device] { return device->setOn(); }, [device] { return device->setOff(); });
        rows.push_back(Renderer(buttons, [&l, device, buttons, width] {
            bool on = false;
            bool ok = device->readOn(on);
            return hbox({label(l.label, width), reading(ok, on ? "ON" : "off", on ? Color(Color::Green) : Color(Color::Default)),
                         text("  "), buttons->Render()});
        }));
    }
    return rows;
}

// RO state and the running procedure
Element console_ui::roStatus(bool with_settling) {
    RO_state state = RO_state::OFF;
    bool state_ok = farm_.ro.readState(state);
    Element state_text = state_ok ? text(toString(state)) | color(Color::Green) | bold : text("---") | color(Color::Red);

    bool running = false;
    procedure_types p = procedure_types::LOW_PH_CIP_CYCLE;
    bool procedure_ok = farm_.ro_procedures.readProcedure(running, p);
    Element procedure_text = !procedure_ok ? text("---") | color(Color::Red)
                           : running ? text(toString(p)) | color(Color::Green) | bold
                           : text("none");

    Elements lines = {
        hbox({shortLabel("RO state"), state_text}),
        hbox({shortLabel("Running"), procedure_text}),
    };
    if (with_settling) {
        bool settling = false;
        bool ok = farm_.ro.isSettling(settling);
        Element settling_text = ok ? text(settling ? "valves settling" : "no") : text("---") | color(Color::Red);
        lines.push_back(hbox({shortLabel("Settling"), settling_text}));
    }
    return vbox(lines);
}

Component console_ui::procedureControls() {
    auto list = Dropdown(&procedures_, &procedure_selected_);
    auto run_button = Button("Run", [this] {
        procedure_types p = ALL_PROCEDURE_TYPES[procedure_selected_];
        ask("Run " + std::string(toString(p)) + "?", [this, p] { return farm_.ro_procedures.run(p); });
    }, ButtonOption::Ascii());
    auto stop_button = Button("Stop", [this] {
        ask("Stop the running procedure (the RO goes OFF)?", [this] { return farm_.ro_procedures.stop(); });
    }, ButtonOption::Ascii());
    auto row = Container::Horizontal({list, run_button, stop_button});
    return Renderer(row, [list, run_button, stop_button] {
        return hbox({shortLabel("Procedure"), list->Render(), text(" "), run_button->Render() | vcenter,
                     text(" "), stop_button->Render() | vcenter});
    });
}

// Sets the RO state directly (an override: a running procedure keeps its
// remaining steps, which can change the state again)
Component console_ui::stateControls(const std::string& row_label) {
    auto list = Dropdown(&states_, &state_selected_);
    auto set_button = Button("Set state", [this] {
        RO_state s = ALL_RO_STATES[state_selected_];
        ask("Set the RO to " + std::string(toString(s)) + "?", [this, s] { return farm_.ro.setState(s); });
    }, ButtonOption::Ascii());
    auto row = Container::Horizontal({list, set_button});
    return Renderer(row, [row_label, list, set_button] {
        return hbox({shortLabel(row_label), list->Render(), text(" "), set_button->Render() | vcenter});
    });
}

// One Start button per procedure plus one Stop (Main-Dashboard): no dropdown,
// each press asks Yes/No. The running procedure is marked
Component console_ui::procedureButtons() {
    Components buttons;
    for (procedure_types p : ALL_PROCEDURE_TYPES) {
        buttons.push_back(Button("Start " + std::string(toString(p)), [this, p] {
            ask("Start " + std::string(toString(p)) + "?", [this, p] { return farm_.ro_procedures.run(p); });
        }, ButtonOption::Ascii()));
    }
    auto stop_button = Button("Stop", [this] {
        ask("Stop the running procedure (the RO goes OFF)?", [this] { return farm_.ro_procedures.stop(); });
    }, ButtonOption::Ascii());
    Components all = buttons;
    all.push_back(stop_button);
    auto list = Container::Vertical(all);
    return Renderer(list, [this, buttons, stop_button] {
        bool running = false;
        procedure_types current = procedure_types::LOW_PH_CIP_CYCLE;
        bool ok = farm_.ro_procedures.readProcedure(running, current);
        Elements lines;
        for (size_t i = 0; i < buttons.size(); ++i) {
            bool this_one = ok && running && current == ALL_PROCEDURE_TYPES[i];
            lines.push_back(hbox({buttons[i]->Render(), this_one ? text("  running") | color(Color::Green) | bold : text("")}));
        }
        lines.push_back(stop_button->Render());
        return vbox(lines);
    });
}

// RO state and procedure, water tank floats and levels, the upper tank pumps.
// TODO: WiseConn irrigation tiles
Component console_ui::mainDashboard() {
    auto water_width = labelWidth(farm_.water_floats);
    auto level_width = labelWidth(farm_.tank_levels);
    return dashboard({
        controlTile("RO", procedureButtons(), [this] { return roStatus(false); }),
        readTile("Water tank floats", [this, water_width] { return floatList(farm_.water_floats, water_width); }),
        readTile("Water levels", [this, level_width] { return sensorList(farm_.tank_levels, 2, level_width); }),
        controlTile("Upper tank pumps", Container::Vertical(pumpRows(farm_.upper_pumps, PUMP_LABEL_WIDTH))),
    });
}

// RO control with the state override and panel switches, chemical floats, pressures, flows,
// conductivity, the RO pumps, CIP heater, valves
Component console_ui::roDashboard() {
    int chemical_width = labelWidth(farm_.chemical_floats);
    int pressure_width = labelWidth(farm_.ro_pressures);
    int flow_width = labelWidth(farm_.ro_flows);
    int conductivity_width = labelWidth(farm_.conductivities);
    constexpr int HEATER_WIDTH = 12;   // "cip heater", "cip temp"

    auto ro_controls = Container::Vertical({procedureControls(), stateControls("Override")});
    return dashboard({
        controlTile("RO control", ro_controls, [this] {
            Elements lines = {roStatus(true)};
            for (toggle_row& t : farm_.toggles) {   // panel switches
                bool on = false;
                bool ok = t.read(on);
                lines.push_back(hbox({label(t.label, labelWidth(farm_.toggles)), reading(ok, on ? "ON" : "off")}));
            }
            return vbox(lines);
        }),
        readTile("Chemical tank floats", [this, chemical_width] { return floatList(farm_.chemical_floats, chemical_width); }),
        readTile("Pressure sensors", [this, pressure_width] { return sensorList(farm_.ro_pressures, 1, pressure_width); }),
        readTile("Flow sensors", [this, flow_width] { return sensorList(farm_.ro_flows, 2, flow_width); }),
        readTile("Conductivity", [this, conductivity_width] { return sensorList(farm_.conductivities, 1, conductivity_width); }),
        controlTile("Pumps", Container::Vertical(pumpRows(farm_.ro_pumps, PUMP_LABEL_WIDTH))),
        controlTile("CIP heater", Container::Vertical(loadRows(HEATER_WIDTH)),
                    [this] { return sensorList(farm_.cip_temps, 1, HEATER_WIDTH); }),
        controlTile("Valves", Container::Vertical(valveRows(labelWidth(farm_.valves)))),
    });
}

Components console_ui::valveRows(int width) {
    Components rows;
    for (valve_row& v : farm_.valves) {
        valve* device = v.device.get();
        auto buttons = onOffButtons(v.label, [device] { return device->setOpen(); },
                                    [device] { return device->setClosed(); }, "Open", "Close");
        rows.push_back(Renderer(buttons, [&v, device, buttons, width] {
            bool open = false;
            bool ok = device->readOpen(open);
            return hbox({label(v.label, width), reading(ok, open ? "OPEN" : "closed", open ? Color(Color::Green) : Color(Color::Default)),
                         text("  "), buttons->Render()});
        }));
    }
    return rows;
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
    std::vector<std::string> tab_names = {"Main-Dashboard", "RO-Dashboard"};
    int tab_selected = 0;
    auto tab_menu = Menu(&tab_names, &tab_selected);
    auto tabs = Container::Tab({mainDashboard(), roDashboard()}, &tab_selected);

    auto main_container = Container::Horizontal({tab_menu, tabs});
    auto main_view = Renderer(main_container, [&] {
        return vbox({
            header(),
            separator(),
            hbox({tab_menu->Render() | size(WIDTH, EQUAL, MENU_WIDTH), separator(), tabs->Render() | flex}) | flex,
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
