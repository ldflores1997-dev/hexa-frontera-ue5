// Parser JSON mínimo, sin excepciones (Unreal compila sin ellas).
#pragma once

#include <string>
#include <utility>
#include <vector>

namespace hf {

class JsonValue {
public:
    enum class Kind { Null, Bool, Number, String, Array, Object };

    Kind kind = Kind::Null;
    bool boolean = false;
    double number = 0.0;
    std::string str;
    std::vector<JsonValue> items;
    std::vector<std::pair<std::string, JsonValue>> members;

    bool IsNull() const { return kind == Kind::Null; }
    bool IsString() const { return kind == Kind::String; }
    bool IsNumber() const { return kind == Kind::Number; }
    bool IsArray() const { return kind == Kind::Array; }
    bool IsObject() const { return kind == Kind::Object; }

    // Devuelve nullptr si no es objeto o no tiene esa clave.
    const JsonValue* Find(const std::string& key) const;

    std::string GetString(const std::string& key, const std::string& fallback = "") const;
    double GetNumber(const std::string& key, double fallback = 0.0) const;
    int GetInt(const std::string& key, int fallback = 0) const;
    bool GetBool(const std::string& key, bool fallback = false) const;
};

// Devuelve false y llena `error` si el texto no es JSON válido.
bool ParseJson(const std::string& text, JsonValue& out, std::string& error);

} // namespace hf
