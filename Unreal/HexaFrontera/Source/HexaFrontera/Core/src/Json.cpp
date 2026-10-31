#include "hf/Json.h"

#include <cstdlib>

namespace hf {

const JsonValue* JsonValue::Find(const std::string& key) const {
    if (kind != Kind::Object) return nullptr;
    for (const auto& member : members) {
        if (member.first == key) return &member.second;
    }
    return nullptr;
}

std::string JsonValue::GetString(const std::string& key, const std::string& fallback) const {
    const JsonValue* value = Find(key);
    return (value && value->IsString()) ? value->str : fallback;
}

double JsonValue::GetNumber(const std::string& key, double fallback) const {
    const JsonValue* value = Find(key);
    return (value && value->IsNumber()) ? value->number : fallback;
}

int JsonValue::GetInt(const std::string& key, int fallback) const {
    const JsonValue* value = Find(key);
    return (value && value->IsNumber()) ? static_cast<int>(value->number) : fallback;
}

bool JsonValue::GetBool(const std::string& key, bool fallback) const {
    const JsonValue* value = Find(key);
    return (value && value->kind == Kind::Bool) ? value->boolean : fallback;
}

namespace {

class JsonParser {
public:
    explicit JsonParser(const std::string& source) : text(source) {}

    bool ParseDocument(JsonValue& out, std::string& error) {
        SkipSpace();
        if (!ParseValue(out, 0)) {
            error = message + " (posición " + std::to_string(pos) + ")";
            return false;
        }
        SkipSpace();
        if (pos != text.size()) {
            error = "Texto sobrante al final del JSON (posición " + std::to_string(pos) + ")";
            return false;
        }
        return true;
    }

private:
    const std::string& text;
    size_t pos = 0;
    std::string message;

    bool Fail(const char* why) {
        message = why;
        return false;
    }

    void SkipSpace() {
        while (pos < text.size()) {
            const char c = text[pos];
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r') ++pos;
            else break;
        }
    }

    bool Match(const char* word) {
        size_t i = 0;
        while (word[i] != '\0') {
            if (pos + i >= text.size() || text[pos + i] != word[i]) return false;
            ++i;
        }
        pos += i;
        return true;
    }

    bool ParseValue(JsonValue& out, int depth) {
        if (depth > 64) return Fail("JSON demasiado anidado");
        if (pos >= text.size()) return Fail("Fin inesperado del JSON");
        const char c = text[pos];
        if (c == '{') return ParseObject(out, depth);
        if (c == '[') return ParseArray(out, depth);
        if (c == '"') {
            out.kind = JsonValue::Kind::String;
            return ParseString(out.str);
        }
        if (Match("true")) { out.kind = JsonValue::Kind::Bool; out.boolean = true; return true; }
        if (Match("false")) { out.kind = JsonValue::Kind::Bool; out.boolean = false; return true; }
        if (Match("null")) { out.kind = JsonValue::Kind::Null; return true; }
        if (c == '-' || (c >= '0' && c <= '9')) return ParseNumber(out);
        return Fail("Valor JSON inválido");
    }

    bool ParseNumber(JsonValue& out) {
        const size_t start = pos;
        if (text[pos] == '-') ++pos;
        while (pos < text.size()) {
            const char c = text[pos];
            if ((c >= '0' && c <= '9') || c == '.' || c == 'e' || c == 'E' || c == '+' || c == '-') ++pos;
            else break;
        }
        const std::string slice = text.substr(start, pos - start);
        char* end = nullptr;
        const double value = std::strtod(slice.c_str(), &end);
        if (end == slice.c_str() || *end != '\0') return Fail("Número inválido");
        out.kind = JsonValue::Kind::Number;
        out.number = value;
        return true;
    }

    static void AppendUtf8(std::string& target, unsigned int codepoint) {
        if (codepoint < 0x80) {
            target += static_cast<char>(codepoint);
        } else if (codepoint < 0x800) {
            target += static_cast<char>(0xC0 | (codepoint >> 6));
            target += static_cast<char>(0x80 | (codepoint & 0x3F));
        } else if (codepoint < 0x10000) {
            target += static_cast<char>(0xE0 | (codepoint >> 12));
            target += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
            target += static_cast<char>(0x80 | (codepoint & 0x3F));
        } else {
            target += static_cast<char>(0xF0 | (codepoint >> 18));
            target += static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F));
            target += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
            target += static_cast<char>(0x80 | (codepoint & 0x3F));
        }
    }

    bool ParseHex4(unsigned int& value) {
        if (pos + 4 > text.size()) return Fail("Escape \\u incompleto");
        value = 0;
        for (int i = 0; i < 4; ++i) {
            const char c = text[pos++];
            value <<= 4;
            if (c >= '0' && c <= '9') value |= static_cast<unsigned int>(c - '0');
            else if (c >= 'a' && c <= 'f') value |= static_cast<unsigned int>(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') value |= static_cast<unsigned int>(c - 'A' + 10);
            else return Fail("Escape \\u inválido");
        }
        return true;
    }

    bool ParseString(std::string& out) {
        ++pos; // comilla inicial
        out.clear();
        while (pos < text.size()) {
            const char c = text[pos++];
            if (c == '"') return true;
            if (c != '\\') {
                out += c;
                continue;
            }
            if (pos >= text.size()) break;
            const char esc = text[pos++];
            switch (esc) {
                case '"': out += '"'; break;
                case '\\': out += '\\'; break;
                case '/': out += '/'; break;
                case 'b': out += '\b'; break;
                case 'f': out += '\f'; break;
                case 'n': out += '\n'; break;
                case 'r': out += '\r'; break;
                case 't': out += '\t'; break;
                case 'u': {
                    unsigned int codepoint = 0;
                    if (!ParseHex4(codepoint)) return false;
                    if (codepoint >= 0xD800 && codepoint <= 0xDBFF && Match("\\u")) {
                        unsigned int low = 0;
                        if (!ParseHex4(low)) return false;
                        codepoint = 0x10000 + ((codepoint - 0xD800) << 10) + (low - 0xDC00);
                    }
                    AppendUtf8(out, codepoint);
                    break;
                }
                default: return Fail("Escape de texto inválido");
            }
        }
        return Fail("Texto sin cerrar");
    }

    bool ParseArray(JsonValue& out, int depth) {
        ++pos;
        out.kind = JsonValue::Kind::Array;
        SkipSpace();
        if (pos < text.size() && text[pos] == ']') { ++pos; return true; }
        while (true) {
            SkipSpace();
            JsonValue item;
            if (!ParseValue(item, depth + 1)) return false;
            out.items.push_back(std::move(item));
            SkipSpace();
            if (pos >= text.size()) return Fail("Lista sin cerrar");
            if (text[pos] == ',') { ++pos; continue; }
            if (text[pos] == ']') { ++pos; return true; }
            return Fail("Se esperaba ',' o ']'");
        }
    }

    bool ParseObject(JsonValue& out, int depth) {
        ++pos;
        out.kind = JsonValue::Kind::Object;
        SkipSpace();
        if (pos < text.size() && text[pos] == '}') { ++pos; return true; }
        while (true) {
            SkipSpace();
            if (pos >= text.size() || text[pos] != '"') return Fail("Se esperaba una clave entre comillas");
            std::string key;
            if (!ParseString(key)) return false;
            SkipSpace();
            if (pos >= text.size() || text[pos] != ':') return Fail("Se esperaba ':'");
            ++pos;
            SkipSpace();
            JsonValue value;
            if (!ParseValue(value, depth + 1)) return false;
            out.members.emplace_back(std::move(key), std::move(value));
            SkipSpace();
            if (pos >= text.size()) return Fail("Objeto sin cerrar");
            if (text[pos] == ',') { ++pos; continue; }
            if (text[pos] == '}') { ++pos; return true; }
            return Fail("Se esperaba ',' o '}'");
        }
    }
};

} // namespace

bool ParseJson(const std::string& text, JsonValue& out, std::string& error) {
    out = JsonValue();
    JsonParser parser(text);
    return parser.ParseDocument(out, error);
}

} // namespace hf
