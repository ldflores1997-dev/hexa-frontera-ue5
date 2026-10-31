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
    AwaitPulse, // inicio de turno: se puede reforzar una casilla y luego se revela el Pulso
    Main,       // construir, comerciar y terminar el turno
    Finished,
};

enum class ActionType {
    BuildOutpost,  // target = vértice
    BuildRoute,    // target = arista
    UpgradeCenter, // target = vértice
    PublicWork,    // obra pública: prestigio sin ocupar lugar en el tablero
    Boost,         // target = casilla
    RevealPulse,
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
    std::vector<int> give;
    std::vector<int> get;

    static Action Outpost(int vertex) { Action a; a.type = ActionType::BuildOutpost; a.target = vertex; return a; }
    static Action Route(int edge) { Action a; a.type = ActionType::BuildRoute; a.target = edge; return a; }
    static Action Upgrade(int vertex) { Action a; a.type = ActionType::UpgradeCenter; a.target = vertex; return a; }
    static Action Work() { Action a; a.type = ActionType::PublicWork; return a; }
    static Action BoostTile(int tile) { Action a; a.type = ActionType::Boost; a.target = tile; return a; }
    static Action Reveal() { Action a; a.type = ActionType::RevealPulse; return a; }
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
    PulseRevealed,   // index = carta (ver LastPulse)
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

struct PulseCard {
    bool isEvent = false;
    int terrainA = -1;
    int terrainB = -1; // -1 si la carta nombra un solo terreno
    int intensity = 1;
};

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
    const PulseCard& LastPulse() const { return lastPulse; }
    bool HasPulse() const { return hasPulse; }
    int PulseCardsLeft() const { return static_cast<int>(deck.size() - deckPos); }

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
    std::vector<PulseCard> deck;
    size_t deckPos = 0;
    PulseCard lastPulse;
    bool hasPulse = false;
    Phase phase = Phase::Setup;
    int currentPlayer = 0;
    int round = 0;
    int winner = -1;
    int boostedTile = -1;
    int setupStep = 0;
    bool setupNeedsRoute = false;
    int setupVertex = -1;

    void BuildDeck();
    PulseCard DrawCard();
    bool VertexFreeByDistance(int vertex) const;
    bool RouteTouchesVertexFor(int player, int vertex, int exceptEdge) const;
    void Pay(int player, const std::vector<int>& cost);
    void StartTurn(std::vector<GameEvent>& events);
    void Produce(const PulseCard& card, std::vector<GameEvent>& events);
    void ApplyWorldEvent(std::vector<GameEvent>& events);
    void TickStatuses();
    void CheckContractsAndVictory(int player, std::vector<GameEvent>& events);
    int SetupPlayerForStep(int step) const;

    ApplyResult DoSetup(const Action& action);
    ApplyResult Fail(const std::string& message) const;
};

const char* ActionTypeName(ActionType type);

} // namespace hf
