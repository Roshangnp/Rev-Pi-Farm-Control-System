#include "wiseconn_json.h"

#include <cstdlib>

namespace {

constexpr int MAX_DEPTH = 64;

class parser{
    public:
    explicit parser(const std::string& text) : text_(text){};

    bool parseDocument(json_value& out, std::string& error) {
        bool ok = parseValue(out, 0);
        skipSpace();
        if (ok && pos_ != text_.size()) ok = fail("trailing characters");
        if (!ok) error = error_ + " at offset " + std::to_string(pos_);
        return ok;
    }

    private:
    bool fail(const char* message) {
        if (error_.empty()) error_ = message;
        return false;
    }

    void skipSpace() {
        while (pos_ < text_.size() && (text_[pos_] == ' ' || text_[pos_] == '\t' ||
                                       text_[pos_] == '\n' || text_[pos_] == '\r')) {
            ++pos_;
        }
    }

    bool literal(const char* word) {
        size_t n = std::char_traits<char>::length(word);
        if (text_.compare(pos_, n, word) != 0) return fail("bad literal");
        pos_ += n;
        return true;
    }

    bool parseValue(json_value& out, int depth) {
        if (depth > MAX_DEPTH) return fail("nested too deep");
        skipSpace();
        if (pos_ >= text_.size()) return fail("unexpected end");
        char c = text_[pos_];
        if (c == '{') return parseObject(out, depth);
        if (c == '[') return parseArray(out, depth);
        if (c == '"') {
            out.type = json_value::kind::string;
            return parseString(out.text);
        }
        if (c == 't') {
            out.type = json_value::kind::boolean;
            out.boolean = true;
            return literal("true");
        }
        if (c == 'f') {
            out.type = json_value::kind::boolean;
            out.boolean = false;
            return literal("false");
        }
        if (c == 'n') {
            out.type = json_value::kind::null_value;
            return literal("null");
        }
        return parseNumber(out);
    }

    bool parseNumber(json_value& out) {
        const char* start = text_.c_str() + pos_;
        char* end = nullptr;
        double value = std::strtod(start, &end);
        if (end == start) return fail("bad value");
        pos_ += static_cast<size_t>(end - start);
        out.type = json_value::kind::number;
        out.number = value;
        return true;
    }

    static void appendUtf8(std::string& out, unsigned long code) {
        if (code < 0x80) {
            out += static_cast<char>(code);
        } else if (code < 0x800) {
            out += static_cast<char>(0xC0 | (code >> 6));
            out += static_cast<char>(0x80 | (code & 0x3F));
        } else if (code < 0x10000) {
            out += static_cast<char>(0xE0 | (code >> 12));
            out += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (code & 0x3F));
        } else {
            out += static_cast<char>(0xF0 | (code >> 18));
            out += static_cast<char>(0x80 | ((code >> 12) & 0x3F));
            out += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (code & 0x3F));
        }
    }

    bool parseHex4(unsigned long& code) {
        if (pos_ + 4 > text_.size()) return fail("bad \\u escape");
        std::string hex = text_.substr(pos_, 4);
        char* end = nullptr;
        code = std::strtoul(hex.c_str(), &end, 16);
        if (end != hex.c_str() + 4) return fail("bad \\u escape");
        pos_ += 4;
        return true;
    }

    bool parseString(std::string& out) {
        ++pos_;  // opening quote
        out.clear();
        while (pos_ < text_.size()) {
            char c = text_[pos_++];
            if (c == '"') return true;
            if (c != '\\') {
                out += c;
                continue;
            }
            if (pos_ >= text_.size()) break;
            char e = text_[pos_++];
            switch (e) {
                case '"': out += '"'; break;
                case '\\': out += '\\'; break;
                case '/': out += '/'; break;
                case 'b': out += '\b'; break;
                case 'f': out += '\f'; break;
                case 'n': out += '\n'; break;
                case 'r': out += '\r'; break;
                case 't': out += '\t'; break;
                case 'u': {
                    unsigned long code;
                    if (!parseHex4(code)) return false;
                    // Surrogate pair
                    if (code >= 0xD800 && code < 0xDC00 && text_.compare(pos_, 2, "\\u") == 0) {
                        pos_ += 2;
                        unsigned long low;
                        if (!parseHex4(low)) return false;
                        code = 0x10000 + ((code - 0xD800) << 10) + (low - 0xDC00);
                    }
                    appendUtf8(out, code);
                    break;
                }
                default: return fail("bad escape");
            }
        }
        return fail("unterminated string");
    }

    bool parseArray(json_value& out, int depth) {
        ++pos_;  // [
        out.type = json_value::kind::array;
        skipSpace();
        if (pos_ < text_.size() && text_[pos_] == ']') {
            ++pos_;
            return true;
        }
        while (true) {
            out.items.emplace_back();
            if (!parseValue(out.items.back(), depth + 1)) return false;
            skipSpace();
            if (pos_ >= text_.size()) return fail("unterminated array");
            char c = text_[pos_++];
            if (c == ']') return true;
            if (c != ',') return fail("expected , or ]");
        }
    }

    bool parseObject(json_value& out, int depth) {
        ++pos_;  // {
        out.type = json_value::kind::object;
        skipSpace();
        if (pos_ < text_.size() && text_[pos_] == '}') {
            ++pos_;
            return true;
        }
        while (true) {
            skipSpace();
            if (pos_ >= text_.size() || text_[pos_] != '"') return fail("expected key");
            out.keys.emplace_back();
            if (!parseString(out.keys.back())) return false;
            skipSpace();
            if (pos_ >= text_.size() || text_[pos_++] != ':') return fail("expected :");
            out.items.emplace_back();
            if (!parseValue(out.items.back(), depth + 1)) return false;
            skipSpace();
            if (pos_ >= text_.size()) return fail("unterminated object");
            char c = text_[pos_++];
            if (c == '}') return true;
            if (c != ',') return fail("expected , or }");
        }
    }

    const std::string& text_;
    size_t pos_ = 0;
    std::string error_;
};

}  // namespace

const json_value* json_value::find(const std::string& key) const {
    if (type != kind::object) return nullptr;
    for (size_t i = 0; i < keys.size(); ++i) {
        if (keys[i] == key) return &items[i];
    }
    return nullptr;
}

bool parseJson(const std::string& text, json_value& out, std::string& error) {
    out = json_value{};
    return parser(text).parseDocument(out, error);
}
