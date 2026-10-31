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
    EXPECT(game.GetPhase() == Phase::AwaitRoll);
    EXPECT_EQ(game.CurrentPlayer(), 0);
    for (const Player& p : game.Players()) {
        EXPECT_EQ(p.outposts, 2);
        EXPECT_EQ(p.routes, 2);
    }
}

// ---------- Dados de terreno y producción ----------

// Cara (1-6) del dado que muestra lo pedido; -1 si no hay.
static int FindFace(const Scenario& scenario, int die, DieFaceKind kind, int terrain = -1) {
    for (int face = 0; face < kDieFaces; ++face) {
        const DieFace& f = scenario.dice[die][face];
        if (f.kind == kind && (kind != DieFaceKind::Terrain || terrain < 0 || f.terrain == terrain)) return face + 1;
    }
    return -1;
}

static int CountFaces(const Scenario& scenario, int die, DieFaceKind kind) {
    int total = 0;
    for (int face = 0; face < kDieFaces; ++face)
        if (scenario.dice[die][face].kind == kind) ++total;
    return total;
}

// Lo que deberían producir los terrenos A y B (iguales = dobles), a partir del estado antes de tirar.
static std::vector<std::vector<int>> ExpectedProduction(const Game& game, int terrainA, int terrainB, int boostedTile) {
    int intensity = 1;
    if (terrainA >= 0 && terrainA == terrainB) {
        intensity = 2;
        terrainB = -1;
    }
    std::vector<std::vector<int>> gain(game.Players().size(), std::vector<int>(game.GetScenario().resources.size(), 0));
    const auto& tiles = game.GetBoard().Tiles();
    for (size_t t = 0; t < tiles.size(); ++t) {
        const Tile& tile = tiles[t];
        const bool matched = tile.terrain == terrainA || tile.terrain == terrainB;
        const bool boosted = boostedTile == static_cast<int>(t);
        if (tile.blockedRounds > 0 || (!matched && !boosted)) continue;
        const int resource = game.GetScenario().terrains[static_cast<size_t>(tile.terrain)].resource;
        if (resource < 0) continue;
        for (int v : tile.vertices) {
            const Building& b = game.Buildings()[static_cast<size_t>(v)];
            if (b.owner < 0) continue;
            int amount = matched ? intensity * b.level : b.level;
            if (tile.frostRounds > 0) amount /= 2;
            if (matched && boosted) amount += 1;
            gain[static_cast<size_t>(b.owner)][static_cast<size_t>(resource)] += amount;
        }
    }
    return gain;
}

static void ExpectGain(const Game& before, const Game& after, const std::vector<std::vector<int>>& gain) {
    for (size_t p = 0; p < after.Players().size(); ++p) {
        for (size_t r = 0; r < gain[p].size(); ++r) {
            EXPECT_EQ(after.Players()[p].resources[r] - before.Players()[p].resources[r], gain[p][r]);
        }
    }
}

static Game ReadyGame(const std::string& id, int players, uint64_t seed) {
    Game game;
    std::string error;
    EXPECT(game.Init(LoadScenario(id), Names(players), seed, error));
    EXPECT(hftest::FinishSetup(game));
    return game;
}

TEST("Dados de terreno: caras por defecto (evento en el dado de terreno, comodines en el de región)") {
    for (const std::string& id : hftest::ScenarioIds()) {
        const Scenario scenario = LoadScenario(id);
        const std::vector<int> producing = scenario.ProducingTerrains();
        EXPECT_EQ(CountFaces(scenario, 0, DieFaceKind::Event), 1);
        EXPECT_EQ(CountFaces(scenario, 1, DieFaceKind::Event), 0);
        EXPECT(CountFaces(scenario, 1, DieFaceKind::Wild) >= 1);
        for (int terrain : producing) EXPECT(scenario.FacesShowingTerrain(terrain) >= 1);
        for (const auto& die : scenario.dice) {
            for (const DieFace& face : die) {
                if (face.kind != DieFaceKind::Terrain) continue;
                EXPECT(std::find(producing.begin(), producing.end(), face.terrain) != producing.end());
            }
        }
        if (producing.size() == 4) {
            EXPECT_EQ(CountFaces(scenario, 0, DieFaceKind::Wild), 1);
            EXPECT_EQ(CountFaces(scenario, 1, DieFaceKind::Wild), 2);
        }
        if (producing.size() == 5) {
            EXPECT_EQ(CountFaces(scenario, 0, DieFaceKind::Wild), 0);
            EXPECT_EQ(CountFaces(scenario, 1, DieFaceKind::Wild), 1);
        }
    }
}

TEST("Dados de terreno: se pueden definir en el JSON y se validan") {
    const std::string base = hftest::ReadFile(std::string(HF_SCENARIO_DIR) + "/islas_volcanicas.json");
    auto withDice = [&base](const std::string& dice) {
        std::string text = base;
        text.replace(text.find("\"contracts\""), 0, "\"dice\": " + dice + ",\n  ");
        return text;
    };
    Scenario scenario;
    std::string error;
    EXPECT(LoadScenarioFromJson(withDice(R"({"terrain": ["palmar", "palmar", "arrecife", "fumarola", "campo_obsidiana", "event"],
                                             "region": ["palmar", "arrecife", "fumarola", "campo_obsidiana", "wild", "event"]})"),
                                scenario, error));
    EXPECT_EQ(scenario.FacesShowingTerrain(scenario.FindTerrain("palmar")), 3);
    EXPECT(scenario.dice[1][5].kind == DieFaceKind::Event);
    EXPECT(!LoadScenarioFromJson(withDice(R"({"terrain": ["palmar"], "region": ["palmar"]})"), scenario, error));
    EXPECT(!LoadScenarioFromJson(withDice(R"({"terrain": ["crater", "palmar", "arrecife", "fumarola", "campo_obsidiana", "event"],
                                              "region": ["palmar", "arrecife", "fumarola", "campo_obsidiana", "wild", "wild"]})"),
                                 scenario, error));
    EXPECT(!LoadScenarioFromJson(withDice(R"({"terrain": ["palmar", "palmar", "palmar", "fumarola", "event", "event"],
                                              "region": ["palmar", "fumarola", "wild", "wild", "wild", "wild"]})"),
                                 scenario, error));
    EXPECT(error.find("ningún dado") != std::string::npos);
}

TEST("Dados de terreno: producen los terrenos que salen, en los 4 escenarios") {
    for (const std::string& id : hftest::ScenarioIds()) {
        Game game = ReadyGame(id, 4, 11);
        int checked = 0;
        int events = 0;
        int wilds = 0;
        for (int turn = 0; turn < 150; ++turn) {
            EXPECT(game.GetPhase() == Phase::AwaitRoll);
            EXPECT(!game.Apply(Action::Sell(0)).ok); // antes de tirar no se comercia
            Game before = game;
            EXPECT(game.Apply(Action::Roll()).ok);
            EXPECT(game.LastDie(0) >= 1 && game.LastDie(0) <= 6);
            EXPECT(game.LastDie(1) >= 1 && game.LastDie(1) <= 6);
            const bool event = game.LastFace(0).kind == DieFaceKind::Event;
            if (event) ++events;
            if (game.GetPhase() == Phase::ChooseWild) {
                ++wilds;
                EXPECT(!game.Apply(Action::End()).ok); // primero se elige el terreno
                EXPECT(game.Apply(Action::Wild(Bot::BestWildTerrain(game, game.CurrentPlayer()))).ok);
            }
            EXPECT(game.GetPhase() == Phase::Main);
            // Con evento el tablero pudo cambiar antes de producir: ahí no se compara.
            if (!event) {
                ExpectGain(before, game, ExpectedProduction(before, game.RolledTerrain(0), game.RolledTerrain(1), -1));
                ++checked;
            }
            EXPECT(!game.Apply(Action::Roll()).ok); // una tirada por turno
            EXPECT(game.Apply(Action::End()).ok);
        }
        EXPECT(checked > 100);
        EXPECT(events > 10 && events < 45); // 1 de cada 6 tiradas
        EXPECT(wilds > 10);
    }
}

TEST("Dados de terreno: una tirada hecha afuera (física) usa esas caras y se valida") {
    Game game = ReadyGame("andes_valles", 3, 17);
    EXPECT(!game.Apply(Action::RollWith(0, 3)).ok);
    EXPECT(!game.Apply(Action::RollWith(4, 7)).ok);
    EXPECT(!game.Apply(Action::RollWith(-1, 5)).ok);
    EXPECT(game.GetPhase() == Phase::AwaitRoll);
    const Scenario& scenario = game.GetScenario();
    const int a = scenario.dice[0][1].terrain;
    const int b = scenario.dice[1][2].terrain;
    Game before = game;
    ApplyResult result = game.Apply(Action::RollWith(2, 3));
    EXPECT(result.ok);
    EXPECT(result.events.front().type == EventType::DiceRolled);
    EXPECT_EQ(result.events.front().index, 2);
    EXPECT_EQ(result.events.front().amount, 3);
    EXPECT_EQ(game.RolledTerrain(0), a);
    EXPECT_EQ(game.RolledTerrain(1), b);
    ExpectGain(before, game, ExpectedProduction(before, a, b, -1));
}

TEST("Dados de terreno: dobles producen el doble") {
    Game game = ReadyGame("islas_volcanicas", 2, 23);
    const Scenario& scenario = game.GetScenario();
    const int terrain = scenario.ProducingTerrains().front();
    const int first = FindFace(scenario, 0, DieFaceKind::Terrain, terrain);
    const int second = FindFace(scenario, 1, DieFaceKind::Terrain, terrain);
    EXPECT(first > 0 && second > 0);
    Game before = game;
    EXPECT(game.Apply(Action::RollWith(first, second)).ok);
    const auto gain = ExpectedProduction(before, terrain, terrain, -1);
    ExpectGain(before, game, gain);
    // Comparado con una sola cara del mismo terreno, da exactamente el doble.
    const auto single = ExpectedProduction(before, terrain, -1, -1);
    for (size_t p = 0; p < gain.size(); ++p)
        for (size_t r = 0; r < gain[p].size(); ++r) EXPECT_EQ(gain[p][r], 2 * single[p][r]);
}

TEST("Dados de terreno: la cara de evento dispara el evento y el otro dado produce igual") {
    Game game = ReadyGame("colonia_marte", 2, 5);
    const Scenario& scenario = game.GetScenario();
    const int terrain = scenario.dice[1][0].terrain;
    EXPECT(terrain >= 0);
    ApplyResult result = game.Apply(Action::RollWith(FindFace(scenario, 0, DieFaceKind::Event), 1));
    EXPECT(result.ok);
    EXPECT(game.GetPhase() == Phase::Main);
    int storms = 0;
    for (const GameEvent& e : result.events) {
        if (e.type == EventType::DustStorm) ++storms;
        if (e.type == EventType::Produced) {
            const Tile& tile = game.GetBoard().Tiles()[static_cast<size_t>(e.tile)];
            EXPECT_EQ(tile.terrain, terrain);
            EXPECT_EQ(tile.blockedRounds, 0);
        }
    }
    EXPECT_EQ(storms, 3);
    EXPECT_EQ(game.RolledTerrain(0), -1);
    EXPECT_EQ(game.RolledTerrain(1), terrain);
}

TEST("Dados de terreno: con el comodín el jugador elige qué terreno produce") {
    Game game = ReadyGame("islas_volcanicas", 2, 31);
    const Scenario& scenario = game.GetScenario();
    const int other = scenario.dice[0][0].terrain;
    const int wild = FindFace(scenario, 1, DieFaceKind::Wild);
    Game before = game;
    ApplyResult rolled = game.Apply(Action::RollWith(1, wild));
    EXPECT(rolled.ok);
    EXPECT(game.GetPhase() == Phase::ChooseWild);
    EXPECT_EQ(game.PendingWilds(), 1);
    for (const GameEvent& e : rolled.events) EXPECT(e.type != EventType::Produced); // todavía no produce
    EXPECT(!game.Apply(Action::End()).ok);
    EXPECT(!game.Apply(Action::Buy(0)).ok);
    EXPECT(!game.Apply(Action::Wild(scenario.FindTerrain("crater"))).ok); // no produce recursos
    EXPECT(!game.Apply(Action::Wild(99)).ok);
    const int chosen = scenario.FindTerrain("fumarola");
    ApplyResult result = game.Apply(Action::Wild(chosen));
    EXPECT(result.ok);
    EXPECT(game.GetPhase() == Phase::Main);
    EXPECT(result.events.front().type == EventType::WildChosen);
    EXPECT_EQ(result.events.front().index, chosen);
    ExpectGain(before, game, ExpectedProduction(before, chosen, other, -1));

    // Dos comodines: el terreno elegido produce como dobles.
    Game twice = ReadyGame("islas_volcanicas", 2, 31);
    Game start = twice;
    EXPECT(twice.Apply(Action::RollWith(FindFace(scenario, 0, DieFaceKind::Wild), wild)).ok);
    EXPECT_EQ(twice.PendingWilds(), 2);
    EXPECT(twice.Apply(Action::Wild(chosen)).ok);
    ExpectGain(start, twice, ExpectedProduction(start, chosen, chosen, -1));
}

TEST("Bot: elige para el comodín el terreno que más le rinde") {
    Game game = ReadyGame("andes_valles", 3, 41);
    const int player = game.CurrentPlayer();
    const int best = Bot::BestWildTerrain(game, player);
    for (int terrain : game.GetScenario().ProducingTerrains())
        EXPECT(game.TerrainYieldFor(player, best) >= game.TerrainYieldFor(player, terrain));
    EXPECT(game.TerrainYieldFor(player, best) > 0);
}

TEST("Refuerzo: gasta Influencia, solo junto a construcciones propias; produce aunque no salga su terreno") {
    Game game = ReadyGame("islas_volcanicas", 2, 3);
    const Scenario& scenario = game.GetScenario();
    const int player = game.CurrentPlayer();
    int ownTile = -1;
    int foreignTile = -1;
    for (size_t t = 0; t < game.GetBoard().Tiles().size(); ++t) {
        const Tile& tile = game.GetBoard().Tiles()[t];
        bool mine = false;
        for (int v : tile.vertices)
            if (game.Buildings()[static_cast<size_t>(v)].owner == player) mine = true;
        if (mine && ownTile < 0 && scenario.terrains[static_cast<size_t>(tile.terrain)].resource >= 0) ownTile = static_cast<int>(t);
        if (!mine && foreignTile < 0) foreignTile = static_cast<int>(t);
    }
    EXPECT(ownTile >= 0);
    EXPECT(!game.Apply(Action::BoostTile(foreignTile)).ok);
    const int influence = game.Players()[static_cast<size_t>(player)].influence;
    EXPECT(game.Apply(Action::BoostTile(ownTile)).ok);
    EXPECT_EQ(game.Players()[static_cast<size_t>(player)].influence, influence - 1);
    EXPECT(!game.Apply(Action::BoostTile(ownTile)).ok); // una vez por turno

    const int terrain = game.GetBoard().Tiles()[static_cast<size_t>(ownTile)].terrain;
    // Si sale su terreno: 1 por nivel más 1 por construcción.
    Game matched = game;
    const int face = FindFace(scenario, 0, DieFaceKind::Terrain, terrain);
    int otherFace = -1;
    for (int f = 1; f <= kDieFaces; ++f) {
        const DieFace& d = scenario.dice[1][f - 1];
        if (d.kind == DieFaceKind::Terrain && d.terrain != terrain) { otherFace = f; break; }
    }
    EXPECT(matched.Apply(Action::RollWith(face, otherFace)).ok);
    ExpectGain(game, matched, ExpectedProduction(game, terrain, scenario.dice[1][otherFace - 1].terrain, ownTile));

    // Si no sale: produce 1 por nivel igual.
    int missFace = -1;
    for (int f = 1; f <= kDieFaces; ++f) {
        const DieFace& d = scenario.dice[0][f - 1];
        if (d.kind == DieFaceKind::Terrain && d.terrain != terrain && d.terrain != scenario.dice[1][otherFace - 1].terrain) {
            missFace = f;
            break;
        }
    }
    Game missed = game;
    EXPECT(missed.Apply(Action::RollWith(missFace, otherFace)).ok);
    ExpectGain(game, missed, ExpectedProduction(game, scenario.dice[0][missFace - 1].terrain, scenario.dice[1][otherFace - 1].terrain, ownTile));
    int gained = 0;
    for (size_t r = 0; r < scenario.resources.size(); ++r)
        gained += missed.Players()[static_cast<size_t>(player)].resources[r] - game.Players()[static_cast<size_t>(player)].resources[r];
    EXPECT(gained > 0);
}

// ---------- Mercado ----------

TEST("Mercado: comprar sube el precio, vender lo baja, y vuelve al equilibrio con las rondas") {
    Game game;
    std::string error;
    EXPECT(game.Init(LoadScenario("colonia_marte"), Names(2), 9, error));
    EXPECT(hftest::FinishSetup(game));
    EXPECT(hftest::RollAndResolve(game));

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
    EXPECT(hftest::RollAndResolve(game));
    EXPECT(game.Apply(Action::End()).ok); // nueva ronda
    EXPECT_EQ(game.MarketStock(1), depleted + 1);
}

// ---------- Obra pública ----------

TEST("Obra pública: cuesta 1 de cada recurso, da prestigio y tiene límite") {
    Game game;
    std::string error;
    EXPECT(game.Init(LoadScenario("islas_volcanicas"), Names(2), 13, error));
    EXPECT(hftest::FinishSetup(game));
    EXPECT(hftest::RollAndResolve(game));
    const PieceDef& work = game.GetScenario().Piece(PieceKind::Work);
    EXPECT_EQ(work.limit, 5);
    for (int amount : work.cost) EXPECT_EQ(amount, 1);
    // Pasan turnos sin construir nada hasta que el jugador 0 junta 1 de cada recurso.
    // Lo que no produce lo compra en el mercado, vendiendo lo que le sobra.
    int built = 0;
    for (int attempt = 0; attempt < 600 && built < work.limit + 1; ++attempt) {
        if (game.GetPhase() == Phase::Finished) break;
        if (game.GetPhase() == Phase::AwaitRoll) { EXPECT(hftest::RollAndResolve(game)); continue; }
        if (game.CurrentPlayer() != 0) { EXPECT(game.Apply(Action::End()).ok); continue; }
        const int prestige = game.Prestige(0);
        if (game.CanBuild(0, PieceKind::Work)) {
            EXPECT(game.Apply(Action::Work()).ok);
            if (game.GetPhase() == Phase::Finished) break;
            EXPECT_EQ(game.Prestige(0), prestige + work.prestige);
            ++built;
            continue;
        }
        const Player& me = game.Players()[0];
        Action trade = Action::End();
        for (int r = 0; r < game.GetScenario().ResourceCount(); ++r) {
            const size_t i = static_cast<size_t>(r);
            if (me.resources[i] == 0 && game.MarketStock(r) > 0 && me.coins >= game.MarketBuyPrice(r)) trade = Action::Buy(r);
            else if (me.resources[i] > 2 && trade.type == ActionType::EndTurn) trade = Action::Sell(r);
        }
        EXPECT(game.Apply(trade).ok);
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
    EXPECT(hftest::RollAndResolve(game));
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

// Avanza con bots hasta que salga la cara de evento; devuelve el estado previo y los eventos.
static bool RunUntilWorldEvent(Game& game, Game& before, std::vector<GameEvent>& events) {
    int actions = 0;
    for (int guard = 0; guard < 20000 && game.GetPhase() != Phase::Finished; ++guard) {
        if (game.GetPhase() == Phase::AwaitRoll) {
            before = game;
            ApplyResult result = game.Apply(Action::Roll());
            if (!result.ok) return false;
            actions = 0;
            if (game.GetPhase() == Phase::ChooseWild) {
                ApplyResult chosen = game.Apply(Action::Wild(Bot::BestWildTerrain(game, game.CurrentPlayer())));
                if (!chosen.ok) return false;
                result.events.insert(result.events.end(), chosen.events.begin(), chosen.events.end());
            }
            if (game.LastFace(0).kind == DieFaceKind::Event) {
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
    while (game.CurrentPlayer() != 0 && game.GetPhase() == Phase::AwaitRoll) {
        ApplyResult result = game.Apply(Action::Roll());
        if (game.GetPhase() == Phase::ChooseWild) {
            ApplyResult chosen = game.Apply(Action::Wild(Bot::BestWildTerrain(game, game.CurrentPlayer())));
            result.events.insert(result.events.end(), chosen.events.begin(), chosen.events.end());
        }
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

TEST("Ronda final: al llegar a la meta se completa la ronda y gana el de más prestigio") {
    for (int players = 2; players <= 4; ++players) {
        for (uint64_t seed = 1; seed <= 15; ++seed) {
            Game game;
            std::string error;
            EXPECT(game.Init(LoadScenario("islas_volcanicas"), Names(players), seed, error));
            int actions = 0;
            int finalRoundBy = -1;
            int finalRoundNumber = -1;
            while (game.GetPhase() != Phase::Finished && game.Round() < 200) {
                hf::ApplyResult result;
                EXPECT(hftest::BotStep(game, actions, &result));
                for (const GameEvent& e : result.events) {
                    if (e.type == EventType::FinalRound) {
                        EXPECT_EQ(finalRoundBy, -1); // se anuncia una sola vez
                        finalRoundBy = e.player;
                        finalRoundNumber = game.Round();
                        EXPECT(game.Prestige(e.player) >= game.PrestigeTarget());
                    }
                }
                if (finalRoundBy >= 0 && game.GetPhase() != Phase::Finished) EXPECT(game.IsFinalRound());
            }
            EXPECT(game.GetPhase() == Phase::Finished);
            EXPECT(finalRoundBy >= 0);
            // Termina al cerrar esa misma ronda, con el último jugador como último en jugar.
            EXPECT_EQ(game.Round(), finalRoundNumber);
            EXPECT_EQ(game.CurrentPlayer(), players - 1);
            for (int p = 0; p < players; ++p) EXPECT(game.Prestige(game.Winner()) >= game.Prestige(p));
        }
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
