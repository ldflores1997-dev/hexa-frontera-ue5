// Definición de un escenario (mundo). Se carga desde JSON para poder agregar
// mundos nuevos sin tocar código.
#pragma once

#include <string>
#include <vector>

namespace hf {

struct Color {
    float r = 1.0f;
    float g = 1.0f;
    float b = 1.0f;
};

struct ResourceDef {
    std::string id;
    std::string name;
    Color color;
};

struct TerrainDef {
    std::string id;
    std::string name;
    int resource = -1;   // índice en Scenario::resources, -1 si no produce
    int count = 0;       // cuántas casillas de este terreno hay en el tablero
    Color color;
    float height = 0.2f; // altura relativa de la casilla (para el 3D)
    std::string decoration; // pista visual: "volcano", "trees", "rocks", "crystals"...
};

enum class PieceKind { Outpost = 0, Route = 1, Center = 2, Work = 3 };
const int kPieceKindCount = 4;

struct PieceDef {
    std::string name;
    std::vector<int> cost; // cantidad por recurso
    int limit = 0;
    int prestige = 0;
};

enum class WorldEventKind { Lava, DustStorm, Frost, PlatformSwap };

struct WorldEventDef {
    WorldEventKind kind = WorldEventKind::DustStorm;
    std::string name;
    std::string description;
    int tiles = 1;        // casillas afectadas (el evento ocurre cuando los dados suman 7)
    int duration = 2;     // rondas que dura (tormenta, helada)
    int terrain = -1;     // terreno resultante (lava)
};

// Cara de un dado de terreno: un terreno que produce, el evento del mundo o un comodín
// (el jugador que tiró elige qué terreno produce).
enum class DieFaceKind { Terrain, Event, Wild };

struct DieFace {
    DieFaceKind kind = DieFaceKind::Terrain;
    int terrain = -1; // solo para DieFaceKind::Terrain
};

const int kDieFaces = 6;
const int kDiceCount = 2;

enum class ContractKind { Routes, Centers, TerrainReach, Diversity, Coins };

struct ContractDef {
    std::string id;
    std::string name;
    ContractKind kind = ContractKind::Routes;
    int value = 0;
    int terrain = -1; // para TerrainReach
    int prestige = 2;
};

struct EnvironmentDef {
    Color sunColor;
    float sunElevation = 40.0f; // grados
    float sunIntensity = 8.0f;  // lux
    Color skyTint;
    Color fogColor;
    float fogDensity = 0.02f;
    Color groundColor;
    std::string ground = "water"; // "water", "sand", "grass", "clouds"
    bool clouds = true;
};

struct Scenario {
    std::string id;
    std::string name;
    std::string description;
    int radius = 2;
    int prestigeToWin = 12;
    int startCoins = 3;
    int startInfluence = 2;
    int marketStartStock = 6;
    std::vector<ResourceDef> resources;
    std::vector<TerrainDef> terrains;
    PieceDef pieces[kPieceKindCount];
    WorldEventDef worldEvent;
    // Dado 0: «dado de terreno» (incluye la cara de evento). Dado 1: «dado de región» (incluye comodines).
    DieFace dice[kDiceCount][kDieFaces];
    std::vector<ContractDef> contracts;
    EnvironmentDef environment;

    int ResourceCount() const { return static_cast<int>(resources.size()); }
    int TileCount() const { return 3 * radius * (radius + 1) + 1; }
    const PieceDef& Piece(PieceKind kind) const { return pieces[static_cast<int>(kind)]; }
    int FindResource(const std::string& resourceId) const;
    int FindTerrain(const std::string& terrainId) const;
    // Terrenos que producen recursos y aparecen en el tablero, en el orden del escenario.
    std::vector<int> ProducingTerrains() const;
    // Cuántas caras (de las 12) muestran este terreno.
    int FacesShowingTerrain(int terrain) const;
};

// Carga y valida el escenario. Devuelve false con un mensaje en `error`.
bool LoadScenarioFromJson(const std::string& text, Scenario& out, std::string& error);

} // namespace hf
