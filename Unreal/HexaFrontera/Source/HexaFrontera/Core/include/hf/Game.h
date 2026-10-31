// Estado y reglas de una partida de Hexa Frontera. No depende de Unreal:
// la capa visual solo envía acciones y lee el estado y los eventos.
#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "hf/Board.h"
#include "hf/Random.h"
#include "hf/Scenario.h"

namespace hf {

enum class Phase {
    Setup,      // cada jugador coloca 2 puestos y 2 rutas gratis (orden serpiente)
    AwaitRoll,  // inicio de turno: se puede reforzar una casilla y luego se tiran los dados
    Main,       // construir, comerciar y terminar el turno
    Finished,
};

enum class ActionType {
    BuildOutpost,  // target = vértice
    BuildRoute,    // target = arista
    UpgradeCenter, // target = vértice
    PublicWork,    // obra pública: prestigio sin ocupar lugar en el tablero
    Boost,         // target = casilla
    RollDice,      // dice = valores (1-6) si los dados se tiraron afuera, -1 para que los tire el núcleo
    MarketBuy,     // resource
    MarketSell,    // resource
    PlayerTrade,   // otherPlayer, give, get
    EndTurn,
};

struct Action {
    ActionType type = ActionType::EndTurn;
    int target = -1;
    int resource = -1;
    int otherPlayer = -1;
    int dice[2] = {-1, -1};
    std::vector<int> give;
    std::vector<int> get;

    static Action Outpost(int vertex) { Action a; a.type = ActionType::BuildOutpost; a.target = vertex; return a; }
    static Action Route(int edge) { Action a; a.type = ActionType::BuildRoute; a.target = edge; return a; }
    static Action Upgrade(int vertex) { Action a; a.type = ActionType::UpgradeCenter; a.target = vertex; return a; }
    static Action Work() { Action a; a.type = ActionType::PublicWork; return a; }
    static Action BoostTile(int tile) { Action a; a.type = ActionType::Boost; a.target = tile; return a; }
    static Action Roll() { Action a; a.type = ActionType::RollDice; return a; }
    // Usa el resultado de dados tirados afuera del núcleo (por ejemplo, con física en el motor).
    static Action RollWith(int first, int second) {
        Action a = Roll();
        a.dice[0] = first;
        a.dice[1] = second;
        return a;
    }
    static Action Buy(int resource) { Action a; a.type = ActionType::MarketBuy; a.resource = resource; return a; }
    static Action Sell(int resource) { Action a; a.type = ActionType::MarketSell; a.resource = resource; return a; }
    static Action End() { return Action(); }
    static Action Trade(int other, std::vector<int> giveAmounts, std::vector<int> getAmounts) {
        Action a;
        a.type = ActionType::PlayerTrade;
        a.otherPlayer = other;
        a.give = std::move(giveAmounts);
        a.get = std::move(getAmounts);
        return a;
    }
};

enum class EventType {
    TurnStarted,
    DiceRolled,      // player, amount = suma (ver LastDice)
    Produced,        // player, tile, resource, amount
    Boosted,         // player, tile
    OutpostBuilt,    // player, index = vértice
    RouteBuilt,      // player, index = arista
    CenterBuilt,     // player, index = vértice
    WorkBuilt,       // player
    RouteDestroyed,  // player, index = arista
    MarketTrade,     // player, resource, amount (+ compra / - venta)
    PlayerTraded,    // player, index = otro jugador
    Lava,            // tile
    DustStorm,       // tile
    Frost,           // tile
    PlatformSwap,    // tile, tile2
    ContractClaimed, // player, index = contrato
    Won,             // player
};

struct GameEvent {
    EventType type = EventType::TurnStarted;
    int player = -1;
    int tile = -1;
    int tile2 = -1;
    int resource = -1;
    int amount = 0;
    int index = -1;
};

// Suma de dados que dispara el evento del mundo en lugar de producir.
const int kWorldEventRoll = 7;
// Recursos que da cada nivel de construcción cuando sale el número de la casilla.
const int kDiceYield = 2;

struct Player {
    std::string name;
    std::vector<int> resources;
    int coins = 0;
    int influence = 0;
    int outposts = 0;
    int centers = 0;
    int routes = 0;
    int works = 0;
};

struct Building {
    int owner = -1;
    int level = 0; // 0 vacío, 1 puesto, 2 centro
};

struct ApplyResult {
    bool ok = false;
    std::string error;
    std::vector<GameEvent> events;
};

class Game {
public:
    // Prepara una partida nueva de 2 a 4 jugadores.
    bool Init(const Scenario& scenario, const std::vector<std::string>& playerNames, uint64_t seed, std::string& error);

    // Aplica una acción del jugador actual. Si no es legal, no cambia nada.
    ApplyResult Apply(const Action& action);

    // Consultas para la interfaz y los bots
    std::vector<int> LegalOutpostVertices(int player) const;
    std::vector<int> LegalRouteEdges(int player) const;
    std::vector<int> LegalUpgradeVertices(int player) const;
    bool CanAfford(int player, const std::vector<int>& cost) const;
    bool CanBuild(int player, PieceKind kind) const; // costo y límite de piezas
    int Prestige(int player) const;
    int PrestigeTarget() const; // baja 1 por cada jugador después del segundo
    int MarketBuyPrice(int resource) const;
    int MarketSellPrice(int resource) const;
    int MarketStock(int resource) const { return marketStock[static_cast<size_t>(resource)]; }
    std::vector<int> ReachableTilesOfTerrain(int player, int terrain) const;
    int ResourceDiversity(int player) const; // recursos con 2+ casillas al alcance
    bool ContractMet(int player, const ContractDef& contract) const;

    const Scenario& GetScenario() const { return scenario; }
    const Board& GetBoard() const { return board; }
    const std::vector<Player>& Players() const { return players; }
    const std::vector<Building>& Buildings() const { return buildings; }
    const std::vector<int>& RouteOwners() const { return routeOwners; }
    const std::vector<int>& ContractOwners() const { return contractOwners; }
    Phase GetPhase() const { return phase; }
    int CurrentPlayer() const { return currentPlayer; }
    int Round() const { return round; }
    int Winner() const { return winner; }
    int BoostedTile() const { return boostedTile; }
    // Últimos dados tirados (0 antes de la primera tirada).
    int LastDie(int index) const { return lastDice[index]; }
    int LastRoll() const { return lastDice[0] + lastDice[1]; }
    bool HasRolled() const { return hasRolled; }

    // En la preparación: true si el jugador actual debe poner la ruta de su último puesto.
    bool SetupNeedsRoute() const { return setupNeedsRoute; }
    int SetupVertex() const { return setupVertex; }

private:
    Scenario scenario;
    Board board;
    Random rng;
    std::vector<Player> players;
    std::vector<Building> buildings;   // por vértice
    std::vector<int> routeOwners;      // por arista, -1 libre
    std::vector<int> contractOwners;   // por contrato, -1 libre
    std::vector<int> marketStock;
    int lastDice[2] = {0, 0};
    bool hasRolled = false;
    Phase phase = Phase::Setup;
    int currentPlayer = 0;
    int round = 0;
    int winner = -1;
    int boostedTile = -1;
    int setupStep = 0;
    bool setupNeedsRoute = false;
    int setupVertex = -1;

    bool VertexFreeByDistance(int vertex) const;
    bool RouteTouchesVertexFor(int player, int vertex, int exceptEdge) const;
    void Pay(int player, const std::vector<int>& cost);
    void StartTurn(std::vector<GameEvent>& events);
    void Produce(int roll, std::vector<GameEvent>& events);
    void ApplyWorldEvent(std::vector<GameEvent>& events);
    void TickStatuses();
    void CheckContractsAndVictory(int player, std::vector<GameEvent>& events);
    int SetupPlayerForStep(int step) const;

    ApplyResult DoSetup(const Action& action);
    ApplyResult Fail(const std::string& message) const;
};

const char* ActionTypeName(ActionType type);

} // namespace hf
