#include "console_ui.h"
#include "config.h"
#include "procedure_config.h"
#include "ro_config.h"

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

// The procedures run on ro_procedure (the RO tile and the RO-Dashboard list);
// other procedures are in the Procedures tile
constexpr procedure_types RO_PROCEDURES[] = {
    procedure_types::LOW_PH_CIP_CYCLE, procedure_types::HIGH_PH_CIP_CYCLE,
    procedure_types::MEDIA_BACKWASH_CYCLE, procedure_types::EMPTY_FEED_TANK};

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

// A read value (green), or a red "---" if the read failed (owner down, stale, broken wire)
Element reading(bool ok, const std::string& value, Color good = Color::Green, int width = VALUE_WIDTH) {
    Element e = ok ? text(value) | color(good) : text("---") | color(Color::Red);
    return e | align_right | size(WIDTH, EQUAL, width);
}

// An on/off reading: green when on, yellow when off, red "---" if the read failed
Element onOffReading(bool ok, bool on, const std::string& on_text, const std::string& off_text,
                     int width = VALUE_WIDTH) {
    return reading(ok, on ? on_text : off_text, on ? Color::Green : Color::Yellow, width);
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
                              onOffReading(ok, submerged, "SUBMERGED", "dry")}));
    }
    return vbox(lines);
}

// WiseConn soil probes: per probe a moisture, EC and temperature line, one
// column per depth. A read is all depths or none (red "---" in every column)
Element soilTable(std::vector<soil_row>& rows) {
    constexpr int PROBE_WIDTH = 11;
    constexpr int QUANTITY_WIDTH = 11;
    constexpr int DEPTH_WIDTH = 7;
    size_t depth_count = 0;
    for (const soil_row& r : rows) depth_count = std::max(depth_count, r.depths.size());
    const std::vector<std::string>* headings = nullptr;
    for (const soil_row& r : rows) {
        if (r.depths.size() == depth_count) headings = &r.depths;
    }

    Elements lines;
    Elements head = {label("", PROBE_WIDTH + QUANTITY_WIDTH)};
    for (size_t i = 0; i < depth_count; ++i) {
        head.push_back(text(headings ? (*headings)[i] : "") | dim | align_right | size(WIDTH, EQUAL, DEPTH_WIDTH));
    }
    lines.push_back(hbox(head));

    struct quantity {
        const char* name;
        int decimals;
        bool (soil_sensor::*read)(std::vector<float>&);
    };
    const quantity quantities[] = {
        {"moisture %", 1, &soil_sensor::readMoisture},
        {"EC (VIC)", 0, &soil_sensor::readETC},
        {"temp F", 1, &soil_sensor::readTemperature},
    };
    for (soil_row& r : rows) {
        bool first = true;
        for (const quantity& q : quantities) {
            std::vector<float> values;
            bool ok = ((*r.device).*q.read)(values);
            Elements line = {label(first ? r.label : "", PROBE_WIDTH), label(q.name, QUANTITY_WIDTH)};
            for (size_t i = 0; i < depth_count; ++i) {
                bool have = ok && i < values.size();
                line.push_back(reading(have, have ? formatFloat(values[i], q.decimals) : "", Color::Green, DEPTH_WIDTH));
            }
            lines.push_back(hbox(line));
            first = false;
        }
    }
    return vbox(lines);
}

// WiseConn irrigation zones, ON/off, in two columns so the tile fits at 80x24
Element irrigationList(std::vector<irrigation_row>& rows) {
    int width = labelWidth(rows);
    auto column = [&](size_t from, size_t to) {
        Elements lines;
        for (size_t i = from; i < to; ++i) {
            bool on = false;
            bool ok = rows[i].device->readOn(on);
            lines.push_back(hbox({label(rows[i].label, width), onOffReading(ok, on, "ON", "off", 3)}));
        }
        return vbox(lines);
    };
    size_t half = (rows.size() + 1) / 2;
    return hbox({column(0, half), text("   "), column(half, rows.size())});
}

// ---- Arrow keys by screen position ----
// The arrow keys move to the nearest control on screen in that direction, as
// if the tiles weren't there: Down from a valve's Close goes to the Close
// under it, Right goes to whatever is to the right (the next tile across),
// Down from a tile's last row goes to the tile below it.

// Records the box an element was laid out in. Unlike reflect(), not clipped
// to the visible area, so controls scrolled out of view can still be found
class box_tracker : public Node {
    public:
    box_tracker(Element child, Box* out) : Node({std::move(child)}), out_(out) {}

    void ComputeRequirement() override {
        Node::ComputeRequirement();
        requirement_ = children_[0]->requirement();
    }

    void SetBox(Box box) override {
        *out_ = box;
        Node::SetBox(box);
        children_[0]->SetBox(box);
    }

    private:
    Box* out_;
};

// One stop for the arrow keys: a button, text box, dropdown or read-only tile
class nav_item : public ComponentBase {
    public:
    // grabs: while it returns true (an open dropdown) the item gets every arrow key
    nav_item(Component child, std::function<bool()> grabs) : grabs_(std::move(grabs)) {
        Add(std::move(child));
    }

    Element OnRender() override {
        return std::make_shared<box_tracker>(ChildAt(0)->Render(), &box_);
    }

    Box box_;
    std::function<bool()> grabs_;
};

// The stops and tiles of one dashboard
class nav_map {
    public:
    Component item(Component child, std::function<bool()> grabs = nullptr) {
        auto i = std::make_shared<nav_item>(std::move(child), std::move(grabs));
        items_.push_back(i);
        return i;
    }

    // Box a tile records itself in (used to keep Up/Down within a column of tiles)
    Box* newTile() {
        tiles_.push_back(std::make_unique<Box>());
        return tiles_.back().get();
    }

    // Handles every arrow key (so the containers never move the focus
    // themselves); left_edge runs on Left with nothing to the left
    bool onArrow(const Event& event, const std::function<void()>& left_edge) {
        int dx = 0;
        int dy = 0;
        if (event == Event::ArrowUp) dy = -1;
        else if (event == Event::ArrowDown) dy = 1;
        else if (event == Event::ArrowLeft) dx = -1;
        else if (event == Event::ArrowRight) dx = 1;
        else return false;

        nav_item* current = nullptr;
        for (auto& i : items_) {
            if (i->Focused()) current = i.get();
        }
        if (!current) return false;

        if (current->grabs_ && current->grabs_()) {
            current->OnEvent(event);
            return true;
        }
        // Left/Right move the cursor in a text box first
        if (dx != 0 && current->OnEvent(event)) return true;

        nav_item* target = dy != 0 ? findVertical(current, dy) : findHorizontal(current, dx);
        if (target) target->TakeFocus();
        else if (dx < 0 && left_edge) left_edge();
        return true;
    }

    private:
    static int centerX(const Box& b) { return (b.x_min + b.x_max) / 2; }
    static bool overlapY(const Box& a, const Box& b) { return a.y_min <= b.y_max && b.y_min <= a.y_max; }
    static bool inside(const Box& inner, const Box& outer) {
        return inner.x_min >= outer.x_min && inner.x_max <= outer.x_max &&
               inner.y_min >= outer.y_min && inner.y_max <= outer.y_max;
    }
    // Columns between x and the box (0 if x is over it)
    static int distanceX(const Box& b, int x) { return std::max({0, b.x_min - x, x - b.x_max}); }

    // The tile a stop sits in (a read-only tile is its own)
    Box tileOf(const Box& b) const {
        for (const auto& t : tiles_) {
            if (inside(b, *t)) return *t;
        }
        return b;
    }

    // Best stop by a "less than" ordering, or nullptr if none passes the filter
    template <typename filter, typename less>
    nav_item* best(nav_item* current, filter allowed, less better) {
        nav_item* found = nullptr;
        for (auto& i : items_) {
            if (i.get() == current || !allowed(i->box_)) continue;
            if (!found || better(i->box_, found->box_)) found = i.get();
        }
        return found;
    }

    // Up/Down: the next line of stops in the same tile; past the tile's edge,
    // the next tile above/below (the one under the column we started from,
    // kept over repeated Up/Down so passing a shorter row doesn't lose it)
    // and its first line from that side. Tiles go before lines, so a tile
    // with few controls isn't skipped
    nav_item* findVertical(nav_item* current, int dy) {
        const Box c = current->box_;
        const Box tile = tileOf(c);
        const int x = current == sticky_item_ ? sticky_x_ : centerX(c);
        auto beyond = [&](const Box& b, const Box& from) { return dy > 0 ? b.y_min > from.y_max : b.y_max < from.y_min; };
        auto nearer = [&](const Box& a, const Box& b) { return dy > 0 ? a.y_min < b.y_min : a.y_max > b.y_max; };
        auto closerToX = [&](const Box& a, const Box& b) {
            if (distanceX(a, x) != distanceX(b, x)) return distanceX(a, x) < distanceX(b, x);
            return std::abs(centerX(a) - x) < std::abs(centerX(b) - x);
        };
        // Among the stops allowed, the nearest line, then the closest to x on it
        auto onNearestLine = [&](auto allowed) -> nav_item* {
            nav_item* line = best(current, allowed, nearer);
            if (!line) return nullptr;
            const Box l = line->box_;
            return best(current, [&](const Box& b) { return allowed(b) && overlapY(b, l); }, closerToX);
        };

        nav_item* target = onNearestLine([&](const Box& b) { return inside(b, tile) && beyond(b, c); });
        if (!target) {
            // Nearest tile beyond this one, then of the tiles level with it the one closest to x
            nav_item* first = best(current, [&](const Box& b) { return beyond(tileOf(b), tile); },
                                   [&](const Box& a, const Box& b) { return nearer(tileOf(a), tileOf(b)); });
            if (first) {
                const Box level = tileOf(first->box_);
                nav_item* in_tile = best(current,
                    [&](const Box& b) { return beyond(tileOf(b), tile) && overlapY(tileOf(b), level); },
                    [&](const Box& a, const Box& b) { return closerToX(tileOf(a), tileOf(b)); });
                const Box chosen = tileOf(in_tile->box_);
                target = onNearestLine([&](const Box& b) { return inside(b, chosen); });
            }
        }
        if (target) {
            sticky_item_ = target;
            sticky_x_ = x;
        }
        return target;
    }

    // Left/Right: the next stop on the same line in the same tile; past the
    // tile's edge, the nearest tile across and its stop closest to this line
    // (nearest the edge we came from)
    nav_item* findHorizontal(nav_item* current, int dx) {
        sticky_item_ = nullptr;
        const Box c = current->box_;
        const Box tile = tileOf(c);
        auto gapX = [&](const Box& b, const Box& from) { return dx > 0 ? b.x_min - from.x_max : from.x_min - b.x_max; };
        auto gapY = [&](const Box& b) { return std::max({0, b.y_min - c.y_max, c.y_min - b.y_max}); };
        auto closest = [&](const Box& a, const Box& b) {
            if (gapY(a) != gapY(b)) return gapY(a) < gapY(b);
            if (gapX(a, c) != gapX(b, c)) return gapX(a, c) < gapX(b, c);
            return a.y_min < b.y_min;   // leaving a tall read-only tile: the top
        };

        nav_item* target = best(current, [&](const Box& b) { return inside(b, tile) && gapX(b, c) > 0 && overlapY(b, c); },
                                [&](const Box& a, const Box& b) { return gapX(a, c) < gapX(b, c); });
        if (target) return target;

        // Nearest tile across that shares a line with this stop, else any tile on that side
        auto across = [&](const Box& b) { return gapX(tileOf(b), tile) > 0; };
        nav_item* first = best(current, [&](const Box& b) { return across(b) && overlapY(tileOf(b), c); },
                               [&](const Box& a, const Box& b) { return gapX(tileOf(a), tile) < gapX(tileOf(b), tile); });
        if (!first) {
            first = best(current, across, closest);
        }
        if (!first) return nullptr;
        const Box chosen = tileOf(first->box_);
        return best(current, [&](const Box& b) { return inside(b, chosen); }, closest);
    }

    std::vector<std::shared_ptr<nav_item>> items_;
    std::vector<std::unique_ptr<Box>> tiles_;
    nav_item* sticky_item_ = nullptr;
    int sticky_x_ = 0;
};

// A dashboard tile: a titled box, heavy border while it holds the focus
Element tile(const std::string& title, Element content, bool focused) {
    return window(text(" " + title + " ") | bold, content, focused ? HEAVY : ROUNDED);
}

// Read-only tile. A stop for the arrow keys, so the dashboard scrolls to it
// on a small terminal
Component readTile(nav_map& nav, const std::string& title, std::function<Element()> content) {
    Box* box = nav.newTile();
    return nav.item(Renderer([title, content, box](bool focused) {
        Element e = std::make_shared<box_tracker>(tile(title, content(), focused), box);
        return focused ? e | focus : e;
    }));
}

// Tile around controls; top (optional) is read-only content shown above them
Component controlTile(nav_map& nav, const std::string& title, Component inner,
                      std::function<Element()> top = nullptr) {
    Box* box = nav.newTile();
    return Renderer(inner, [title, inner, top, box] {
        Element content = top ? vbox({top(), inner->Render()}) : inner->Render();
        return std::make_shared<box_tracker>(tile(title, content, inner->Focused()), box);
    });
}

// Tiles laid out in rows as wide as the screen allows (one column on an
// 80-column terminal), scrolling to the focused tile when they don't fit.
// The arrow keys go by screen position (nav_map); Left with nothing to the
// left runs left_edge (back to the tab list)
Component dashboard(Components tiles, std::shared_ptr<nav_map> nav, std::function<void()> left_edge) {
    auto container = Container::Vertical(tiles);
    auto view = Renderer(container, [container, tiles] {
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
    return CatchEvent(view, [nav, left_edge](Event event) { return nav->onArrow(event, left_edge); });
}

std::string clockText() {
    std::time_t now = std::time(nullptr);
    char text_value[16];
    std::strftime(text_value, sizeof(text_value), "%H:%M:%S", std::localtime(&now));
    return text_value;
}

class console_ui{
    public:
    explicit console_ui(console_devices& farm);

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

    // Buttons, text boxes and dropdowns on a dashboard are stops for the arrow keys
    Component button(const std::string& text_value, std::function<void()> on_press);
    Component dropdown(std::vector<std::string>* entries, int* selected);
    Component dashboardOf(Components tiles);

    console_devices& farm_;

    bool confirm_shown_ = false;
    std::string confirm_question_;
    std::function<bool()> confirm_action_;
    std::string status_ = "Ready";

    // Dropdown lists; the selections are shared by every copy of the controls
    std::vector<std::string> states_;
    std::vector<std::string> procedures_;
    int state_selected_ = 0;
    int procedure_selected_ = 0;

    // Tab list on the left, made before the dashboards so Left can return to it
    std::vector<std::string> tab_names_ = {"Main-Dashboard", "RO-Dashboard"};
    int tab_selected_ = 0;
    Component tab_menu_;
    // Stops of the dashboard being built
    std::shared_ptr<nav_map> nav_;
};

console_ui::console_ui(console_devices& farm)
    : farm_(farm) {
    for (RO_state s : ALL_RO_STATES) states_.push_back(toString(s));
    for (procedure_types p : RO_PROCEDURES) procedures_.push_back(toString(p));
}

Component console_ui::button(const std::string& text_value, std::function<void()> on_press) {
    return nav_->item(Button(text_value, std::move(on_press), ButtonOption::Ascii()));
}

// While open, the dropdown keeps the arrow keys for its list
Component console_ui::dropdown(std::vector<std::string>* entries, int* selected) {
    auto open = std::make_shared<bool>(false);
    DropdownOption option;
    option.radiobox.entries = entries;
    option.radiobox.selected = selected;
    option.checkbox.checked = open.get();
    return nav_->item(Dropdown(option), [open] { return *open; });
}

Component console_ui::dashboardOf(Components tiles) {
    Component menu = tab_menu_;
    auto view = dashboard(std::move(tiles), nav_, [menu] { menu->TakeFocus(); });
    nav_ = nullptr;
    return view;
}

void console_ui::ask(const std::string& question, std::function<bool()> action) {
    confirm_question_ = question;
    confirm_action_ = std::move(action);
    confirm_shown_ = true;
}

Component console_ui::onOffButtons(const std::string& name, std::function<bool()> on, std::function<bool()> off,
                                   const std::string& on_word, const std::string& off_word) {
    auto on_button = button(on_word, [this, name, on, on_word] {
        ask("Set " + name + " " + on_word + "?", on);
    });
    auto off_button = button(off_word, [this, name, off, off_word] {
        ask("Set " + name + " " + off_word + "?", off);
    });
    return Container::Horizontal({on_button, off_button});
}

// Text box plus Send; the value is checked here, then confirmed
Component console_ui::numberEntry(const std::string& what, const std::string& placeholder,
                                  std::function<bool(float)> send) {
    auto entry = std::make_shared<std::string>();
    InputOption option;
    option.multiline = false;
    auto input = nav_->item(Input(entry.get(), placeholder, option));
    auto send_button = button("Send", [this, entry, what, send] {
        float value;
        if (!parseFloat(*entry, value)) {
            status_ = "Not a number: \"" + *entry + "\"";
            return;
        }
        ask("Set " + what + " to " + *entry + "?", [send, value] { return send(value); });
    });
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
                e.push_back(onOffReading(ok, on, "ON", "off"));
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
            return hbox({label(l.label, width), onOffReading(ok, on, "ON", "off"),
                         text("  "), buttons->Render()});
        }));
    }
    return rows;
}

// RO state and the running procedure
Element console_ui::roStatus(bool with_settling) {
    RO_state state = RO_state::OFF;
    bool state_ok = readROState(state);
    Element state_text = state_ok ? text(toString(state)) | color(state == RO_state::OFF ? Color::Yellow : Color::Green) | bold : text("---") | color(Color::Red);

    bool running = false;
    procedure_types p = procedure_types::LOW_PH_CIP_CYCLE;
    bool procedure_ok = ro_procedure.readProcedure(running, p);
    Element procedure_text = !procedure_ok ? text("---") | color(Color::Red)
                           : running ? text(toString(p)) | color(Color::Green) | bold
                           : text("none") | color(Color::Yellow);

    Elements lines = {
        hbox({shortLabel("RO state"), state_text}),
        hbox({shortLabel("Running"), procedure_text}),
    };
    if (with_settling) {
        bool settling = false;
        bool ok = readROSettling(settling);
        Element settling_text = ok ? text(settling ? "valves settling" : "no") | color(settling ? Color::Green : Color::Yellow) : text("---") | color(Color::Red);
        lines.push_back(hbox({shortLabel("Settling"), settling_text}));
    }
    return vbox(lines);
}

Component console_ui::procedureControls() {
    auto list = dropdown(&procedures_, &procedure_selected_);
    auto run_button = button("Run", [this] {
        procedure_types p = RO_PROCEDURES[procedure_selected_];
        ask("Run " + std::string(toString(p)) + "?", [this, p] { return ro_procedure.run(p); });
    });
    auto stop_button = button("Stop", [this] {
        ask("Stop the running procedure (the RO goes OFF)?", [this] { return ro_procedure.stop(); });
    });
    auto row = Container::Horizontal({list, run_button, stop_button});
    return Renderer(row, [list, run_button, stop_button] {
        return hbox({shortLabel("Procedure"), list->Render(), text(" "), run_button->Render() | vcenter,
                     text(" "), stop_button->Render() | vcenter});
    });
}

// Sets the RO state directly (an override: a running procedure keeps its
// remaining steps, which can change the state again)
Component console_ui::stateControls(const std::string& row_label) {
    auto list = dropdown(&states_, &state_selected_);
    auto set_button = button("Set state", [this] {
        RO_state s = ALL_RO_STATES[state_selected_];
        ask("Set the RO to " + std::string(toString(s)) + "?", [this, s] { return setROState(s); });
    });
    auto row = Container::Horizontal({list, set_button});
    return Renderer(row, [row_label, list, set_button] {
        return hbox({shortLabel(row_label), list->Render(), text(" "), set_button->Render() | vcenter});
    });
}

// One Start button per procedure plus one Stop (Main-Dashboard): no dropdown,
// each press asks Yes/No. The running procedure is marked
Component console_ui::procedureButtons() {
    Components buttons;
    for (procedure_types p : RO_PROCEDURES) {
        buttons.push_back(button("Start " + std::string(toString(p)), [this, p] {
            ask("Start " + std::string(toString(p)) + "?", [this, p] { return ro_procedure.run(p); });
        }));
    }
    auto stop_button = button("Stop", [this] {
        ask("Stop the running procedure (the RO goes OFF)?", [this] { return ro_procedure.stop(); });
    });
    Components all = buttons;
    all.push_back(stop_button);
    auto list = Container::Vertical(all);
    return Renderer(list, [this, buttons, stop_button] {
        bool running = false;
        procedure_types current = procedure_types::LOW_PH_CIP_CYCLE;
        bool ok = ro_procedure.readProcedure(running, current);
        Elements lines;
        for (size_t i = 0; i < buttons.size(); ++i) {
            bool this_one = ok && running && current == RO_PROCEDURES[i];
            lines.push_back(hbox({buttons[i]->Render(), this_one ? text("  running") | color(Color::Green) | bold : text("")}));
        }
        lines.push_back(stop_button->Render());
        return vbox(lines);
    });
}

// RO state and procedure, water tank floats and levels, the upper tank pumps,
// the WiseConn irrigation zones and soil probes (read only)
Component console_ui::mainDashboard() {
    auto water_width = labelWidth(farm_.water_floats);
    auto level_width = labelWidth(farm_.tank_levels);
    auto flow_width = labelWidth(farm_.wiseconn_flows);
    nav_ = std::make_shared<nav_map>();

    // Procedures tile: one row per procedure (name, idle/running/stopping, Run, Stop).
    // Add a line to procedures to show another one
    struct procedure_entry {
        std::string name;
        procedure_control* instance;
        procedure_types type;
        std::string warning;   // added to the Run question
        // The irrigation patterns: Stop also runs this cancel procedure (the pattern's
        // own stop only removes its tasks), shown as "stopping" while it runs
        procedure_control* cancel_instance = nullptr;
        procedure_types cancel_type = procedure_types::CANCEL_OVERHEAD_PATTERN_1;
    };
    const std::vector<procedure_entry> procedures = {
        {"overhead pattern 1", &overhead_pattern_procedure, procedure_types::OVERHEAD_PATTERN_1,
         "books 18 real irrigations, today 14:00-16:15",
         &overhead_cancel_procedure, procedure_types::CANCEL_OVERHEAD_PATTERN_1},
        {"irrigation pattern 1", &irrigation_pattern_procedure, procedure_types::IRRIGATION_PATTERN_1,
         "books 17 real 1 h irrigations back to back, starting 1 h from now, today only",
         &irrigation_cancel_procedure, procedure_types::CANCEL_IRRIGATION_PATTERN_1},
        {"repressure", &repressure_procedure, procedure_types::REPRESSURE_CONTROL, ""},
    };
    Components procedure_rows;
    for (const procedure_entry& entry : procedures) {
        auto run_button = button("Run", [this, entry] {
            std::string question = "Run " + entry.name + (entry.warning.empty() ? "" : " (" + entry.warning + ")") + "?";
            ask(question, [entry] { return entry.instance->run(entry.type); });
        });
        auto stop_button = button("Stop", [this, entry] {
            if (!entry.cancel_instance) {
                ask("Stop " + entry.name + "?", [entry] { return entry.instance->stop(); });
                return;
            }
            // Removes the pattern's remaining tasks (fails if it already finished,
            // which doesn't matter), then cancels its bookings, one zone every 2 s
            ask("Stop " + entry.name + " and cancel its bookings?", [entry] {
                entry.instance->stop();
                return entry.cancel_instance->run(entry.cancel_type);
            });
        });
        auto buttons = Container::Horizontal({run_button, stop_button});
        procedure_rows.push_back(Renderer(buttons, [entry, buttons] {
            bool running = false;
            procedure_types current;
            bool ok = entry.instance->readProcedure(running, current);
            // Its cancel procedure (the Stop button's) is running
            bool cancel_running = false;
            procedure_types cancel_current;
            bool cancelling = entry.cancel_instance
                && entry.cancel_instance->readProcedure(cancel_running, cancel_current)
                && cancel_running && cancel_current == entry.cancel_type;
            Element state = !ok ? text("---") | color(Color::Red)
                          : cancelling ? text("stopping") | color(Color::Magenta) | bold
                          : running && current == entry.type ? text("running") | color(Color::Green) | bold
                          : running ? text("busy") | color(Color::Yellow)
                          : text("idle") | color(Color::Yellow);
            return hbox({label(entry.name, 22), state | size(WIDTH, EQUAL, 8), text("  "), buttons->Render()});
        }));
    }
    return dashboardOf({
        controlTile(*nav_, "RO", procedureButtons(), [this] { return roStatus(false); }),
        readTile(*nav_, "Water tank floats", [this, water_width] { return floatList(farm_.water_floats, water_width); }),
        readTile(*nav_, "Water levels", [this, level_width] { return sensorList(farm_.tank_levels, 2, level_width); }),
        controlTile(*nav_, "Upper tank pumps", Container::Vertical(pumpRows(farm_.upper_pumps, PUMP_LABEL_WIDTH))),
        readTile(*nav_, "Irrigation zones (WiseConn)", [this] { return irrigationList(farm_.irrigations); }),
        readTile(*nav_, "Soil probes (WiseConn)", [this] { return soilTable(farm_.soil_probes); }),
        readTile(*nav_, "Flow meters (WiseConn)", [this, flow_width] { return sensorList(farm_.wiseconn_flows, 1, flow_width); }),
        controlTile(*nav_, "Procedures", Container::Vertical(procedure_rows)),
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
    nav_ = std::make_shared<nav_map>();

    auto ro_controls = Container::Vertical({procedureControls(), stateControls("Override")});
    return dashboardOf({
        controlTile(*nav_, "RO control", ro_controls, [this] {
            Elements lines = {roStatus(true)};
            for (toggle_row& t : farm_.toggles) {   // panel switches
                bool on = false;
                bool ok = t.read(on);
                lines.push_back(hbox({label(t.label, labelWidth(farm_.toggles)), onOffReading(ok, on, "ON", "off")}));
            }
            return vbox(lines);
        }),
        readTile(*nav_, "Chemical tank floats", [this, chemical_width] { return floatList(farm_.chemical_floats, chemical_width); }),
        readTile(*nav_, "Pressure sensors", [this, pressure_width] { return sensorList(farm_.ro_pressures, 1, pressure_width); }),
        readTile(*nav_, "Flow sensors", [this, flow_width] { return sensorList(farm_.ro_flows, 2, flow_width); }),
        readTile(*nav_, "Conductivity", [this, conductivity_width] { return sensorList(farm_.conductivities, 1, conductivity_width); }),
        controlTile(*nav_, "Pumps", Container::Vertical(pumpRows(farm_.ro_pumps, PUMP_LABEL_WIDTH))),
        controlTile(*nav_, "CIP heater", Container::Vertical(loadRows(HEATER_WIDTH)),
                    [this] { return sensorList(farm_.cip_temps, 1, HEATER_WIDTH); }),
        controlTile(*nav_, "Valves", Container::Vertical(valveRows(labelWidth(farm_.valves)))),
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
            return hbox({label(v.label, width), onOffReading(ok, open, "OPEN", "closed"),
                         text("  "), buttons->Render()});
        }));
    }
    return rows;
}

Element console_ui::header() {
    // Up = one of its values reads (RO state, upper tank high float), so DOWN
    // means that machine's main or the link to it is down. Hosts are the client
    // config's (FARM_*_HOST, set by API/api.cmake)
    auto machine_status = [](const std::string& name, const std::string& host, bool up) {
        return hbox({text(name + "@" + host + " "),
                     text(up ? "OK" : "DOWN") | color(up ? Color::Green : Color::Red) | bold});
    };
    return hbox({text(" Farm console") | bold, text("   "),
                 machine_status("RO", FARM_RO_HOST, farm_.roUp()), text("   "),
                 machine_status("Upper tank", FARM_UPPER_HOST, farm_.upperTankUp()),
                 filler(), text(clockText() + " ")});
}

int console_ui::run() {
    tab_menu_ = Menu(&tab_names_, &tab_selected_);
    auto tab_menu = tab_menu_;
    auto tabs = Container::Tab({mainDashboard(), roDashboard()}, &tab_selected_);

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

int runConsole(console_devices& farm) {
    console_ui ui(farm);
    return ui.run();
}
