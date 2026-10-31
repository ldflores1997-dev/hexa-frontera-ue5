#include <algorithm>
#include <map>
#include <set>

#include "TestUtil.h"
#include "hf/Json.h"

using namespace hf;
using hftest::LoadScenario;
using hftest::Names;

// ---------- JSON ----------

TEST("JSON: objetos, listas, números, textos con escapes y UTF-8") {
    JsonValue root;
    std::string error;
    EXPECT(ParseJson(R"({"a": [1, -2.5, true, null], "b": "Maíz \"x\"", "c": {"d": "ñ"}})", root, error));
    EXPECT(root.IsObject());
    const JsonValue* a = root.Find("a");
    EXPECT(a && a->IsArray() && a->items.size() == 4);
    EXPECT(a && a->items[1].number == -2.5);
    EXPECT(root.GetString("b") == "Maíz \"x\"");
    EXPECT(root.Find("c")->GetString("d") == "ñ");
}

TEST("JSON: errores no lanzan excepciones, devuelven mensaje") {
    JsonValue root;
    std::string error;
    EXPECT(!ParseJson("{\"a\": }", root, error));
    EXPECT(!error.empty());
    EXPECT(!ParseJson("[1, 2", root, error));
    EXPECT(!ParseJson("{} basura", root, error));
}

// ---------- Hexágonos y tablero ----------

TEST("Hex: espiral de radio 2 y 3 sin repetidos") {
    EXPECT_EQ(HexSpiral(2).size(), 19u);
    EXPECT_EQ(HexSpiral(3).size(), 37u);
    std::set<Axial> unique;
    for (const Axial& a : HexSpiral(3)) {
        EXPECT(HexDistance(a, {0, 0}) <= 3);
        unique.insert(a);
    }
    EXPECT_EQ(unique.size(), 37u);
}

TEST("Hex: casillas vecinas comparten exactamente 2 vértices") {
    for (const Axial& dir : HexDirections()) {
        const auto a = HexCorners({0, 0});
        const auto b = HexCorners(dir);
        int shared = 0;
        for (const VertexKey& ka : a)
            for (const VertexKey& kb : b)
                if (ka == kb) ++shared;
        EXPECT_EQ(shared, 2);
    }
}

TEST("Tablero: cantidad de vértices y aristas, posiciones a distancia 1 del centro") {
    Scenario islas = LoadScenario("islas_volcanicas");
    Board board;
    board.Build(islas, 7);
    EXPECT_EQ(board.Tiles().size(), 19u);
    EXPECT_EQ(board.Vertices().size(), 54u);
    EXPECT_EQ(board.Edges().size(), 72u);
    for (const Tile& tile : board.Tiles()) {
        const Point2 center = HexToPoint(tile.coord);
        for (int v : tile.vertices) {
            const Point2 p = board.Vertices()[static_cast<size_t>(v)].position;
            const double d2 = (p.x - center.x) * (p.x - center.x) + (p.y - center.y) * (p.y - center.y);
            EXPECT(d2 > 0.999 && d2 < 1.001);
        }
    }
    Scenario andes = LoadScenario("andes_valles");
    board.Build(andes, 7);
    EXPECT_EQ(board.Tiles().size(), 37u);
    EXPECT_EQ(board.Vertices().size(), 96u);
    EXPECT_EQ(board.Edges().size(), 132u);
}

TEST("Tablero: respeta la cantidad de cada terreno y la semilla es reproducible") {
    Scenario islas = LoadScenario("islas_volcanicas");
    Board a;
    Board b;
    a.Build(islas, 42);
    b.Build(islas, 42);
    std::map<int, int> counts;
    for (size_t i = 0; i < a.Tiles().size(); ++i) {
        ++counts[a.Tiles()[i].terrain];
        EXPECT_EQ(a.Tiles()[i].terrain, b.Tiles()[i].terrain);
    }
    for (size_t t = 0; t < islas.terrains.size(); ++t) EXPECT_EQ(counts[static_cast<int>(t)], islas.terrains[t].count);
}

// ---------- Escenarios ----------

TEST("Escenarios: los 4 JSON cargan y son válidos") {
    for (const std::string& id : hftest::ScenarioIds()) {
        Scenario scenario;
        std::string error;
        const bool ok = LoadScenarioFromJson(hftest::ReadFile(std::string(HF_SCENARIO_DIR) + "/" + id + ".json"),
                                             scenario, error);
        if (!ok) std::printf("  %s: %s\n", id.c_str(), error.c_str());
        EXPECT(ok);
        EXPECT(scenario.id == id);
        EXPECT(!scenario.contracts.empty());
        for (int k = 0; k < kPieceKindCount; ++k) EXPECT_EQ(scenario.pieces[k].cost.size(), scenario.resources.size());
    }
}

TEST("Escenarios: detecta errores comunes") {
    Scenario scenario;
    std::string error;
    const std::string base = hftest::ReadFile(std::string(HF_SCENARIO_DIR) + "/islas_volcanicas.json");

    std::string badCount = base;
    badCount.replace(badCount.find("\"count\": 4"), 10, "\"count\": 9");
    EXPECT(!LoadScenarioFromJson(badCount, scenario, error));
    EXPECT(error.find("casillas") != std::string::npos);

    std::string badResource = base;
    badResource.replace(badResource.find("\"palma\": 1, \"coral\": 1, \"azufre\""), 8, "\"oro\": 1");
    EXPECT(!LoadScenarioFromJson(badResource, scenario, error));

    std::string badEvent = base;
    badEvent.replace(badEvent.find("\"lava\""), 6, "\"meteoro\"");
    EXPECT(!LoadScenarioFromJson(badEvent, scenario, error));
}

// ---------- Preparación ----------

TEST("Preparación: orden serpiente, distancia entre puestos y ruta pegada al puesto") {
    Game game;
    std::string error;
    EXPECT(game.Init(LoadScenario("islas_volcanicas"), Names(3), 5, error));
    EXPECT(game.GetPhase() == Phase::Setup);

    const int vertex = game.LegalOutpostVertices(0).front();
    EXPECT(!game.Apply(Action::Route(0)).ok); // primero el puesto
    EXPECT(game.Apply(Action::Outpost(vertex)).ok);
    EXPECT(!game.Apply(Action::Outpost(vertex)).ok); // falta la ruta

    // Una ruta que no toca el puesto es ilegal.
    int farEdge = -1;
    for (size_t e = 0; e < game.GetBoard().Edges().size(); ++e) {
        const Edge& edge = game.GetBoard().Edges()[e];
        if (edge.a != vertex && edge.b != vertex) { farEdge = static_cast<int>(e); break; }
    }
    EXPECT(!game.Apply(Action::Route(farEdge)).ok);
    EXPECT(game.Apply(Action::Route(game.LegalRouteEdges(0).front())).ok);
    EXPECT_EQ(game.CurrentPlayer(), 1);

    // Un vecino del puesto ya no es legal para nadie.
    const int neighbor = game.GetBoard().Vertices()[static_cast<size_t>(vertex)].neighbors.front();
    const auto legal = game.LegalOutpostVertices(1);
    EXPECT(std::find(legal.begin(), legal.end(), neighbor) == legal.end());

    // Orden: 0, 1, 2, 2, 1, 0
    const int expected[] = {1, 2, 2, 1, 0};
    for (int step = 0; step < 5; ++step) {
        EXPECT_EQ(game.CurrentPlayer(), expected[step]);
        EXPECT(game.Apply(Action::Outpost(game.LegalOutpostVertices(game.CurrentPlayer()).front())).ok);
        EXPECT(game.Apply(Action::Route(game.LegalRouteEdges(game.CurrentPlayer()).front())).ok);
    }
    EXPECT(game.GetPhase() == Phase::AwaitPulse);
    EXPECT_EQ(game.CurrentPlayer(), 0);
    for (const Player& p : game.Players()) {
        EXPECT_EQ(p.outposts, 2);
        EXPECT_EQ(p.routes, 2);
    }
}

// ---------- Pulso y producción ----------

// Calcula lo que debería producir la carta, a partir del estado antes de revelarla.
static std::vector<std::vector<int>> ExpectedProduction(const Game& game, const PulseCard& card, int boostedTile) {
    std::vector<std::vector<int>> gain(game.Players().size(), std::vector<int>(game.GetScenario().resources.size(), 0));
    const auto& tiles = game.GetBoard().Tiles();
    for (size_t t = 0; t < tiles.size(); ++t) {
        const Tile& tile = tiles[t];
        if (tile.blockedRounds > 0 || (tile.terrain != card.terrainA && tile.terrain != card.terrainB)) continue;
        const int resource = game.GetScenario().terrains[static_cast<size_t>(tile.terrain)].resource;
        if (resource < 0) continue;
        for (int v : tile.vertices) {
            const Building& b = game.Buildings()[static_cast<size_t>(v)];
            if (b.owner < 0) continue;
            int amount = card.intensity * b.level;
            if (tile.frostRounds > 0) amount /= 2;
            if (boostedTile == static_cast<int>(t)) amount += 1;
            gain[static_cast<size_t>(b.owner)][static_cast<size_t>(resource)] += amount;
        }
    }
    return gain;
}

TEST("Pulso: la producción coincide con la carta en los 4 escenarios") {
    for (const std::string& id : hftest::ScenarioIds()) {
        Game game;
        std::string error;
        EXPECT(game.Init(LoadScenario(id), Names(4), 11, error));
        EXPECT(hftest::FinishSetup(game));
        int checked = 0;
        for (int turn = 0; turn < 60; ++turn) {
            EXPECT(game.GetPhase() == Phase::AwaitPulse);
            // Probamos hacer un intercambio antes del Pulso: no se permite.
            EXPECT(!game.Apply(Action::Sell(0)).ok);
            Game before = game;
            ApplyResult result = game.Apply(Action::Reveal());
            EXPECT(result.ok);
            if (!game.LastPulse().isEvent) {
                const auto gain = ExpectedProduction(before, game.LastPulse(), -1);
                for (size_t p = 0; p < game.Players().size(); ++p) {
                    for (size_t r = 0; r < gain[p].size(); ++r) {
                        EXPECT_EQ(game.Players()[p].resources[r] - before.Players()[p].resources[r], gain[p][r]);
                    }
                }
                ++checked;
            }
            EXPECT(game.Apply(Action::End()).ok);
        }
        EXPECT(checked > 30);
    }
}

TEST("Refuerzo: gasta Influencia, solo junto a construcciones propias y suma 1 por construcción") {
    Game game;
    std::string error;
    EXPECT(game.Init(LoadScenario("islas_volcanicas"), Names(2), 3, error));
    EXPECT(hftest::FinishSetup(game));
    const int player = game.CurrentPlayer();
    int ownTile = -1;
    int foreignTile = -1;
    for (size_t t = 0; t < game.GetBoard().Tiles().size(); ++t) {
        bool mine = false;
        for (int v : game.GetBoard().Tiles()[t].vertices)
            if (game.Buildings()[static_cast<size_t>(v)].owner == player) mine = true;
        if (mine && ownTile < 0) ownTile = static_cast<int>(t);
        if (!mine && foreignTile < 0) foreignTile = static_cast<int>(t);
    }
    EXPECT(!game.Apply(Action::BoostTile(foreignTile)).ok);
    const int influence = game.Players()[static_cast<size_t>(player)].influence;
    EXPECT(game.Apply(Action::BoostTile(ownTile)).ok);
    EXPECT_EQ(game.Players()[static_cast<size_t>(player)].influence, influence - 1);
    EXPECT(!game.Apply(Action::BoostTile(ownTile)).ok); // una vez por turno
    Game before = game;
    EXPECT(game.Apply(Action::Reveal()).ok);
    if (!game.LastPulse().isEvent) {
        const auto gain = ExpectedProduction(before, game.LastPulse(), ownTile);
        for (size_t p = 0; p < game.Players().size(); ++p)
            for (size_t r = 0; r < gain[p].size(); ++r)
                EXPECT_EQ(game.Players()[p].resources[r] - before.Players()[p].resources[r], gain[p][r]);
    }
}

// ---------- Mercado ----------

TEST("Mercado: comprar sube el precio, vender lo baja, y vuelve al equilibrio con las rondas") {
    Game game;
    std::string error;
    EXPECT(game.Init(LoadScenario("colonia_marte"), Names(2), 9, error));
    EXPECT(hftest::FinishSetup(game));
    EXPECT(game.Apply(Action::Reveal()).ok);

    const int start = game.MarketBuyPrice(0);
    const int coins = game.Players()[0].coins;
    EXPECT(game.Apply(Action::Buy(0)).ok);
    EXPECT_EQ(game.Players()[0].coins, coins - start);
    EXPECT_EQ(game.MarketBuyPrice(0), start + 1);

    const int sellPrice = game.MarketSellPrice(0);
    EXPECT(game.Apply(Action::Sell(0)).ok);
    EXPECT_EQ(game.Players()[0].coins, coins - start + sellPrice);
    EXPECT_EQ(game.MarketBuyPrice(0), start);

    // Sin monedas no se compra.
    while (game.Players()[0].coins >= game.MarketBuyPrice(1)) EXPECT(game.Apply(Action::Buy(1)).ok);
    EXPECT(!game.Apply(Action::Buy(1)).ok);
    const int depleted = game.MarketStock(1);
    EXPECT(depleted < game.GetScenario().marketStartStock);
    EXPECT(game.Apply(Action::End()).ok); // jugador 1
    EXPECT(game.Apply(Action::Reveal()).ok);
    EXPECT(game.Apply(Action::End()).ok); // nueva ronda
    EXPECT_EQ(game.MarketStock(1), depleted + 1);
}

// ---------- Obra pública ----------

TEST("Obra pública: cuesta 1 de cada recurso, da prestigio y tiene límite") {
    Game game;
    std::string error;
    EXPECT(game.Init(LoadScenario("islas_volcanicas"), Names(2), 13, error));
    EXPECT(hftest::FinishSetup(game));
    EXPECT(game.Apply(Action::Reveal()).ok);
    const PieceDef& work = game.GetScenario().Piece(PieceKind::Work);
    EXPECT_EQ(work.limit, 3);
    for (int amount : work.cost) EXPECT_EQ(amount, 1);
    // Pasan turnos sin construir nada hasta que el jugador 0 junta 1 de cada recurso.
    int built = 0;
    for (int attempt = 0; attempt < 400 && built < work.limit + 1; ++attempt) {
        if (game.GetPhase() == Phase::Finished) break;
        if (game.GetPhase() == Phase::AwaitPulse) { EXPECT(game.Apply(Action::Reveal()).ok); continue; }
        if (game.CurrentPlayer() != 0) { EXPECT(game.Apply(Action::End()).ok); continue; }
        const int prestige = game.Prestige(0);
        if (game.CanBuild(0, PieceKind::Work)) {
            EXPECT(game.Apply(Action::Work()).ok);
            if (game.GetPhase() == Phase::Finished) break;
            EXPECT_EQ(game.Prestige(0), prestige + work.prestige);
            ++built;
        } else {
            EXPECT(game.Apply(Action::End()).ok);
        }
    }
    EXPECT(built >= 1);
    EXPECT(game.Players()[0].works <= work.limit);
    if (game.Players()[0].works == work.limit && game.GetPhase() == Phase::Main && game.CurrentPlayer() == 0)
        EXPECT(!game.Apply(Action::Work()).ok);
}

// ---------- Intercambio entre jugadores ----------

TEST("Intercambio: valida que ambos tengan lo ofrecido") {
    Game game;
    std::string error;
    EXPECT(game.Init(LoadScenario("islas_volcanicas"), Names(2), 21, error));
    EXPECT(hftest::FinishSetup(game));
    EXPECT(game.Apply(Action::Reveal()).ok);
    const size_t n = game.GetScenario().resources.size();
    int mine = -1;
    int theirs = -1;
    for (size_t r = 0; r < n; ++r) {
        if (game.Players()[0].resources[r] > 0 && mine < 0) mine = static_cast<int>(r);
        if (game.Players()[1].resources[r] > 0 && theirs < 0) theirs = static_cast<int>(r);
    }
    EXPECT(mine >= 0 && theirs >= 0);
    std::vector<int> give(n, 0), get(n, 0);
    EXPECT(!game.Apply(Action::Trade(1, give, get)).ok); // vacío
    give[static_cast<size_t>(mine)] = 1;
    get[static_cast<size_t>(theirs)] = 99;
    EXPECT(!game.Apply(Action::Trade(1, give, get)).ok); // pide de más
    get[static_cast<size_t>(theirs)] = 1;
    EXPECT(!game.Apply(Action::Trade(0, give, get)).ok); // consigo mismo
    const auto a = game.Players()[0].resources;
    const auto b = game.Players()[1].resources;
    EXPECT(game.Apply(Action::Trade(1, give, get)).ok);
    EXPECT_EQ(game.Players()[0].resources[static_cast<size_t>(mine)] + game.Players()[1].resources[static_cast<size_t>(mine)],
              a[static_cast<size_t>(mine)] + b[static_cast<size_t>(mine)]);
    EXPECT_EQ(game.Players()[0].resources[static_cast<size_t>(theirs)],
              a[static_cast<size_t>(theirs)] + 1 - (mine == theirs ? 1 : 0));
}

// ---------- Eventos del mundo ----------

// Avanza con bots hasta que se revele una carta de evento; devuelve el estado previo y los eventos.
static bool RunUntilWorldEvent(Game& game, Game& before, std::vector<GameEvent>& events) {
    int actions = 0;
    for (int guard = 0; guard < 20000 && game.GetPhase() != Phase::Finished; ++guard) {
        if (game.GetPhase() == Phase::AwaitPulse) {
            before = game;
            ApplyResult result = game.Apply(Action::Reveal());
            if (!result.ok) return false;
            actions = 0;
            if (game.LastPulse().isEvent) {
                events = result.events;
                return true;
            }
            continue;
        }
        if (!hftest::BotStep(game, actions)) return false;
    }
    return false;
}

static int CountEvents(const std::vector<GameEvent>& events, EventType type) {
    return static_cast<int>(std::count_if(events.begin(), events.end(), [type](const GameEvent& e) { return e.type == type; }));
}

TEST("Evento lava: convierte la casilla y quema las rutas que la tocan") {
    Game game, before;
    std::vector<GameEvent> events;
    std::string error;
    EXPECT(game.Init(LoadScenario("islas_volcanicas"), Names(4), 2, error));
    EXPECT(RunUntilWorldEvent(game, before, events));
    EXPECT_EQ(CountEvents(events, EventType::Lava), 1);
    for (const GameEvent& e : events) {
        if (e.type != EventType::Lava) continue;
        const Tile& tile = game.GetBoard().Tiles()[static_cast<size_t>(e.tile)];
        EXPECT_EQ(tile.terrain, game.GetScenario().worldEvent.terrain);
        int burned = 0;
        for (int edge : tile.edges) {
            EXPECT_EQ(game.RouteOwners()[static_cast<size_t>(edge)], -1);
            if (before.RouteOwners()[static_cast<size_t>(edge)] >= 0) ++burned;
        }
        EXPECT_EQ(CountEvents(events, EventType::RouteDestroyed), burned);
    }
    int routesBefore = 0, routesAfter = 0;
    for (size_t p = 0; p < game.Players().size(); ++p) {
        routesBefore += before.Players()[p].routes;
        routesAfter += game.Players()[p].routes;
    }
    EXPECT_EQ(routesBefore - routesAfter, CountEvents(events, EventType::RouteDestroyed));
}

TEST("Evento tormenta: bloquea 3 casillas 2 rondas y no producen") {
    Game game, before;
    std::vector<GameEvent> events;
    std::string error;
    EXPECT(game.Init(LoadScenario("colonia_marte"), Names(3), 4, error));
    EXPECT(RunUntilWorldEvent(game, before, events));
    EXPECT_EQ(CountEvents(events, EventType::DustStorm), 3);
    std::vector<int> stormed;
    for (const GameEvent& e : events) {
        if (e.type != EventType::DustStorm) continue;
        EXPECT_EQ(game.GetBoard().Tiles()[static_cast<size_t>(e.tile)].blockedRounds, 2);
        stormed.push_back(e.tile);
    }
    // Durante el resto de la ronda, esas casillas no producen nada.
    EXPECT(game.Apply(Action::End()).ok);
    while (game.CurrentPlayer() != 0 && game.GetPhase() == Phase::AwaitPulse) {
        ApplyResult result = game.Apply(Action::Reveal());
        for (const GameEvent& e : result.events) {
            if (e.type == EventType::Produced)
                EXPECT(std::find(stormed.begin(), stormed.end(), e.tile) == stormed.end());
        }
        EXPECT(game.Apply(Action::End()).ok);
    }
}

TEST("Evento helada: una casilla y sus vecinas producen la mitad") {
    Game game, before;
    std::vector<GameEvent> events;
    std::string error;
    EXPECT(game.Init(LoadScenario("andes_valles"), Names(4), 6, error));
    EXPECT(RunUntilWorldEvent(game, before, events));
    const int frozen = CountEvents(events, EventType::Frost);
    EXPECT(frozen >= 4 && frozen <= 7); // centro + 3 a 6 vecinas
    for (const GameEvent& e : events) {
        if (e.type == EventType::Frost) EXPECT_EQ(game.GetBoard().Tiles()[static_cast<size_t>(e.tile)].frostRounds, 1);
    }
}

TEST("Evento desacople: dos plataformas intercambian terreno") {
    Game game, before;
    std::vector<GameEvent> events;
    std::string error;
    EXPECT(game.Init(LoadScenario("ciudad_flotante"), Names(2), 8, error));
    EXPECT(RunUntilWorldEvent(game, before, events));
    EXPECT_EQ(CountEvents(events, EventType::PlatformSwap), 1);
    for (const GameEvent& e : events) {
        if (e.type != EventType::PlatformSwap) continue;
        EXPECT(e.tile != e.tile2);
        EXPECT_EQ(game.GetBoard().Tiles()[static_cast<size_t>(e.tile)].terrain,
                  before.GetBoard().Tiles()[static_cast<size_t>(e.tile2)].terrain);
        EXPECT_EQ(game.GetBoard().Tiles()[static_cast<size_t>(e.tile2)].terrain,
                  before.GetBoard().Tiles()[static_cast<size_t>(e.tile)].terrain);
    }
}

// ---------- Partidas completas ----------

struct MatchSummary {
    bool finished = false;
    int winner = -1;
    int rounds = 0;
    bool legal = true;
};

static MatchSummary PlayBotMatch(const Scenario& scenario, int players, uint64_t seed) {
    MatchSummary summary;
    Game game;
    std::string error;
    if (!game.Init(scenario, Names(players), seed, error)) return summary;
    int actions = 0;
    while (game.GetPhase() != Phase::Finished && game.Round() < 200) {
        if (!hftest::BotStep(game, actions)) {
            summary.legal = false;
            break;
        }
        // Invariantes: nunca recursos ni monedas negativos, piezas dentro del límite.
        for (const Player& p : game.Players()) {
            for (int amount : p.resources) {
                if (amount < 0) summary.legal = false;
            }
            if (p.coins < 0 || p.outposts > scenario.Piece(PieceKind::Outpost).limit ||
                p.routes > scenario.Piece(PieceKind::Route).limit || p.centers > scenario.Piece(PieceKind::Center).limit)
                summary.legal = false;
        }
    }
    summary.finished = game.GetPhase() == Phase::Finished;
    summary.winner = game.Winner();
    summary.rounds = game.Round();
    if (summary.finished && game.Prestige(summary.winner) < game.PrestigeTarget()) summary.legal = false;
    return summary;
}

TEST("Partidas de bots: terminan con ganador y sin acciones ilegales (4 escenarios x 2-4 jugadores)") {
    for (const std::string& id : hftest::ScenarioIds()) {
        const Scenario scenario = LoadScenario(id);
        int finished = 0;
        int total = 0;
        for (int players = 2; players <= 4; ++players) {
            for (uint64_t seed = 1; seed <= 10; ++seed) {
                const MatchSummary summary = PlayBotMatch(scenario, players, seed);
                EXPECT(summary.legal);
                if (summary.finished) ++finished;
                ++total;
            }
        }
        if (finished != total) std::printf("  %s: %d de %d partidas terminadas\n", id.c_str(), finished, total);
        EXPECT_EQ(finished, total);
    }
}

TEST("Partidas de bots: la misma semilla da exactamente la misma partida") {
    const Scenario scenario = LoadScenario("islas_volcanicas");
    const MatchSummary a = PlayBotMatch(scenario, 3, 1234);
    const MatchSummary b = PlayBotMatch(scenario, 3, 1234);
    EXPECT_EQ(a.winner, b.winner);
    EXPECT_EQ(a.rounds, b.rounds);
}

int main() { return hftest::RunAll(); }
