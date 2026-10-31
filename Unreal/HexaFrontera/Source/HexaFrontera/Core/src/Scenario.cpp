#include "hf/Scenario.h"

#include "hf/Json.h"

namespace hf {

int Scenario::FindResource(const std::string& resourceId) const {
    for (size_t i = 0; i < resources.size(); ++i) {
        if (resources[i].id == resourceId) return static_cast<int>(i);
    }
    return -1;
}

int Scenario::FindTerrain(const std::string& terrainId) const {
    for (size_t i = 0; i < terrains.size(); ++i) {
        if (terrains[i].id == terrainId) return static_cast<int>(i);
    }
    return -1;
}

namespace {

int HexDigit(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

// "#rrggbb" -> Color. Devuelve false si el formato no es válido.
bool ParseColor(const std::string& text, Color& out) {
    if (text.size() != 7 || text[0] != '#') return false;
    int channels[3];
    for (int i = 0; i < 3; ++i) {
        const int hi = HexDigit(text[static_cast<size_t>(1 + i * 2)]);
        const int lo = HexDigit(text[static_cast<size_t>(2 + i * 2)]);
        if (hi < 0 || lo < 0) return false;
        channels[i] = hi * 16 + lo;
    }
    out.r = static_cast<float>(channels[0]) / 255.0f;
    out.g = static_cast<float>(channels[1]) / 255.0f;
    out.b = static_cast<float>(channels[2]) / 255.0f;
    return true;
}

bool ReadColor(const JsonValue& object, const char* key, Color& out, std::string& error, const std::string& where) {
    const JsonValue* value = object.Find(key);
    if (!value) return true; // opcional: queda el valor por defecto
    if (!value->IsString() || !ParseColor(value->str, out)) {
        error = where + ": el color '" + key + "' debe tener el formato #rrggbb";
        return false;
    }
    return true;
}

bool ReadCost(const Scenario& scenario, const JsonValue* costJson, std::vector<int>& cost, std::string& error,
              const std::string& where) {
    cost.assign(scenario.resources.size(), 0);
    if (!costJson) return true;
    if (!costJson->IsObject()) {
        error = where + ": 'cost' debe ser un objeto {recurso: cantidad}";
        return false;
    }
    for (const auto& member : costJson->members) {
        const int index = scenario.FindResource(member.first);
        if (index < 0) {
            error = where + ": recurso desconocido en el costo: " + member.first;
            return false;
        }
        if (!member.second.IsNumber() || member.second.number < 0) {
            error = where + ": la cantidad de " + member.first + " debe ser un número >= 0";
            return false;
        }
        cost[static_cast<size_t>(index)] = static_cast<int>(member.second.number);
    }
    return true;
}

bool ParseEventKind(const std::string& text, WorldEventKind& out) {
    if (text == "lava") { out = WorldEventKind::Lava; return true; }
    if (text == "dust_storm") { out = WorldEventKind::DustStorm; return true; }
    if (text == "frost") { out = WorldEventKind::Frost; return true; }
    if (text == "platform_swap") { out = WorldEventKind::PlatformSwap; return true; }
    return false;
}

bool ParseContractKind(const std::string& text, ContractKind& out) {
    if (text == "routes") { out = ContractKind::Routes; return true; }
    if (text == "centers") { out = ContractKind::Centers; return true; }
    if (text == "terrain_reach") { out = ContractKind::TerrainReach; return true; }
    if (text == "diversity") { out = ContractKind::Diversity; return true; }
    if (text == "coins") { out = ContractKind::Coins; return true; }
    return false;
}

} // namespace

bool LoadScenarioFromJson(const std::string& text, Scenario& out, std::string& error) {
    out = Scenario();
    JsonValue root;
    if (!ParseJson(text, root, error)) return false;
    if (!root.IsObject()) {
        error = "El escenario debe ser un objeto JSON";
        return false;
    }

    out.id = root.GetString("id");
    out.name = root.GetString("name", out.id);
    out.description = root.GetString("description");
    out.radius = root.GetInt("radius", 2);
    out.prestigeToWin = root.GetInt("prestige_to_win", 12);
    out.startCoins = root.GetInt("start_coins", 3);
    out.startInfluence = root.GetInt("start_influence", 2);
    out.marketStartStock = root.GetInt("market_start_stock", 6);
    if (out.id.empty()) {
        error = "Falta 'id'";
        return false;
    }
    if (out.radius < 2 || out.radius > 4) {
        error = "'radius' debe estar entre 2 y 4";
        return false;
    }

    // Recursos
    const JsonValue* resources = root.Find("resources");
    if (!resources || !resources->IsArray() || resources->items.size() < 3 || resources->items.size() > 6) {
        error = "'resources' debe ser una lista de 3 a 6 recursos";
        return false;
    }
    for (const JsonValue& item : resources->items) {
        ResourceDef resource;
        resource.id = item.GetString("id");
        resource.name = item.GetString("name", resource.id);
        if (resource.id.empty() || out.FindResource(resource.id) >= 0) {
            error = "Recurso sin 'id' o repetido: '" + resource.id + "'";
            return false;
        }
        if (!ReadColor(item, "color", resource.color, error, "Recurso " + resource.id)) return false;
        out.resources.push_back(resource);
    }

    // Terrenos
    const JsonValue* terrains = root.Find("terrains");
    if (!terrains || !terrains->IsArray() || terrains->items.empty()) {
        error = "'terrains' debe ser una lista";
        return false;
    }
    int totalTiles = 0;
    for (const JsonValue& item : terrains->items) {
        TerrainDef terrain;
        terrain.id = item.GetString("id");
        terrain.name = item.GetString("name", terrain.id);
        terrain.count = item.GetInt("count", 0);
        terrain.height = static_cast<float>(item.GetNumber("height", 0.2));
        terrain.decoration = item.GetString("decoration");
        if (terrain.id.empty() || out.FindTerrain(terrain.id) >= 0) {
            error = "Terreno sin 'id' o repetido: '" + terrain.id + "'";
            return false;
        }
        const JsonValue* resource = item.Find("resource");
        if (resource && resource->IsString()) {
            terrain.resource = out.FindResource(resource->str);
            if (terrain.resource < 0) {
                error = "Terreno " + terrain.id + ": recurso desconocido " + resource->str;
                return false;
            }
        }
        if (terrain.count < 0) {
            error = "Terreno " + terrain.id + ": 'count' no puede ser negativo";
            return false;
        }
        if (!ReadColor(item, "color", terrain.color, error, "Terreno " + terrain.id)) return false;
        totalTiles += terrain.count;
        out.terrains.push_back(terrain);
    }
    if (totalTiles != out.TileCount()) {
        error = "La suma de 'count' de los terrenos es " + std::to_string(totalTiles) + " pero un tablero de radio " +
                std::to_string(out.radius) + " tiene " + std::to_string(out.TileCount()) + " casillas";
        return false;
    }
    for (size_t r = 0; r < out.resources.size(); ++r) {
        bool produced = false;
        for (const TerrainDef& terrain : out.terrains) {
            if (terrain.resource == static_cast<int>(r) && terrain.count > 0) produced = true;
        }
        if (!produced) {
            error = "Ningún terreno del tablero produce " + out.resources[r].id;
            return false;
        }
    }

    // Piezas
    const JsonValue* pieces = root.Find("pieces");
    if (!pieces || !pieces->IsObject()) {
        error = "Falta 'pieces' con outpost, route y center";
        return false;
    }
    const char* pieceKeys[kPieceKindCount] = {"outpost", "route", "center", "work"};
    const int defaultLimits[kPieceKindCount] = {5, 15, 4, 3};
    const int defaultPrestige[kPieceKindCount] = {1, 0, 2, 1};
    for (int i = 0; i < kPieceKindCount; ++i) {
        const JsonValue* piece = pieces->Find(pieceKeys[i]);
        if (i == static_cast<int>(PieceKind::Work) && !piece) {
            // La obra pública es opcional: por defecto cuesta 1 de cada recurso.
            PieceDef& work = out.pieces[i];
            work.name = "Obra pública";
            work.limit = defaultLimits[i];
            work.prestige = defaultPrestige[i];
            work.cost.assign(out.resources.size(), 1);
            continue;
        }
        if (!piece || !piece->IsObject()) {
            error = std::string("Falta la pieza '") + pieceKeys[i] + "'";
            return false;
        }
        PieceDef& def = out.pieces[i];
        def.name = piece->GetString("name", pieceKeys[i]);
        def.limit = piece->GetInt("limit", defaultLimits[i]);
        def.prestige = piece->GetInt("prestige", defaultPrestige[i]);
        if (!ReadCost(out, piece->Find("cost"), def.cost, error, std::string("Pieza ") + pieceKeys[i])) return false;
    }

    // Evento del mundo
    const JsonValue* worldEvent = root.Find("event");
    if (!worldEvent || !worldEvent->IsObject()) {
        error = "Falta 'event'";
        return false;
    }
    if (!ParseEventKind(worldEvent->GetString("type"), out.worldEvent.kind)) {
        error = "Tipo de evento desconocido: '" + worldEvent->GetString("type") +
                "' (usar lava, dust_storm, frost o platform_swap)";
        return false;
    }
    out.worldEvent.name = worldEvent->GetString("name", worldEvent->GetString("type"));
    out.worldEvent.description = worldEvent->GetString("description");
    out.worldEvent.cards = worldEvent->GetInt("cards", 3);
    out.worldEvent.tiles = worldEvent->GetInt("tiles", 1);
    out.worldEvent.duration = worldEvent->GetInt("duration", 2);
    if (out.worldEvent.kind == WorldEventKind::Lava) {
        out.worldEvent.terrain = out.FindTerrain(worldEvent->GetString("terrain"));
        if (out.worldEvent.terrain < 0) {
            error = "El evento de lava necesita 'terrain' con un terreno existente";
            return false;
        }
    }

    // Contratos
    const JsonValue* contracts = root.Find("contracts");
    if (contracts && contracts->IsArray()) {
        for (const JsonValue& item : contracts->items) {
            ContractDef contract;
            contract.id = item.GetString("id");
            contract.name = item.GetString("name", contract.id);
            contract.value = item.GetInt("value", 1);
            contract.prestige = item.GetInt("prestige", 2);
            if (!ParseContractKind(item.GetString("type"), contract.kind)) {
                error = "Contrato " + contract.id + ": tipo desconocido '" + item.GetString("type") + "'";
                return false;
            }
            if (contract.kind == ContractKind::TerrainReach) {
                contract.terrain = out.FindTerrain(item.GetString("terrain"));
                if (contract.terrain < 0) {
                    error = "Contrato " + contract.id + ": terreno desconocido '" + item.GetString("terrain") + "'";
                    return false;
                }
            }
            out.contracts.push_back(contract);
        }
    }

    // Ambiente (solo visual)
    const JsonValue* environment = root.Find("environment");
    if (environment && environment->IsObject()) {
        EnvironmentDef& env = out.environment;
        env.sunElevation = static_cast<float>(environment->GetNumber("sun_elevation", env.sunElevation));
        env.sunIntensity = static_cast<float>(environment->GetNumber("sun_intensity", env.sunIntensity));
        env.fogDensity = static_cast<float>(environment->GetNumber("fog_density", env.fogDensity));
        env.ground = environment->GetString("ground", env.ground);
        env.clouds = environment->GetBool("clouds", env.clouds);
        if (!ReadColor(*environment, "sun_color", env.sunColor, error, "environment")) return false;
        if (!ReadColor(*environment, "sky_tint", env.skyTint, error, "environment")) return false;
        if (!ReadColor(*environment, "fog_color", env.fogColor, error, "environment")) return false;
        if (!ReadColor(*environment, "ground_color", env.groundColor, error, "environment")) return false;
    }
    return true;
}

} // namespace hf
