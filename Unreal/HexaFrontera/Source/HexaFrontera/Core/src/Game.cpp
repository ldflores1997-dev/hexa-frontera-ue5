#include "hf/Game.h"

#include <algorithm>

namespace hf {

namespace {
const int kMaxInfluence = 6;
const int kMarketBasePrice = 8; // precio de compra = base - stock (mínimo 1)
} // namespace

const char* ActionTypeName(ActionType type) {
    switch (type) {
        case ActionType::BuildOutpost: return "BuildOutpost";
        case ActionType::BuildRoute: return "BuildRoute";
        case ActionType::UpgradeCenter: return "UpgradeCenter";
        case ActionType::PublicWork: return "PublicWork";
        case ActionType::Boost: return "Boost";
        case ActionType::RollDice: return "RollDice";
        case ActionType::ChooseWild: return "ChooseWild";
        case ActionType::MarketBuy: return "MarketBuy";
        case ActionType::MarketSell: return "MarketSell";
        case ActionType::PlayerTrade: return "PlayerTrade";
        case ActionType::EndTurn: return "EndTurn";
    }
    return "?";
}

bool Game::Init(const Scenario& scenarioDef, const std::vector<std::string>& playerNames, uint64_t seed,
                std::string& error) {
    if (playerNames.size() < 2 || playerNames.size() > 4) {
        error = "Se juega con 2 a 4 jugadores";
        return false;
    }
    scenario = scenarioDef;
    rng = Random(seed ^ 0xA5A5A5A5ull);
    board.Build(scenario, seed);

    const size_t resourceCount = scenario.resources.size();
    players.clear();
    for (const std::string& name : playerNames) {
        Player player;
        player.name = name;
        player.resources.assign(resourceCount, 0);
        // Los que juegan más tarde reciben una moneda extra por asiento para compensar.
        player.coins = scenario.startCoins + static_cast<int>(players.size());
        player.influence = scenario.startInfluence;
        players.push_back(player);
    }
    buildings.assign(board.Vertices().size(), Building());
    routeOwners.assign(board.Edges().size(), -1);
    contractOwners.assign(scenario.contracts.size(), -1);
    marketStock.assign(resourceCount, scenario.marketStartStock);

    hasRolled = false;
    lastDice[0] = lastDice[1] = 0;
    rolledTerrains[0] = rolledTerrains[1] = -1;
    pendingWilds = 0;
    phase = Phase::Setup;
    setupStep = 0;
    setupNeedsRoute = false;
    setupVertex = -1;
    currentPlayer = 0;
    round = 0;
    winner = -1;
    finalRound = false;
    boostedTile = -1;
    return true;
}

int Game::SetupPlayerForStep(int step) const {
    const int count = static_cast<int>(players.size());
    return step < count ? step : 2 * count - 1 - step;
}

ApplyResult Game::Fail(const std::string& message) const {
    ApplyResult result;
    result.ok = false;
    result.error = message;
    return result;
}

bool Game::VertexFreeByDistance(int vertex) const {
    if (buildings[static_cast<size_t>(vertex)].owner >= 0) return false;
    for (int neighbor : board.Vertices()[static_cast<size_t>(vertex)].neighbors) {
        if (buildings[static_cast<size_t>(neighbor)].owner >= 0) return false;
    }
    return true;
}

bool Game::RouteTouchesVertexFor(int player, int vertex, int exceptEdge) const {
    for (int edge : board.Vertices()[static_cast<size_t>(vertex)].edges) {
        if (edge != exceptEdge && routeOwners[static_cast<size_t>(edge)] == player) return true;
    }
    return false;
}

std::vector<int> Game::LegalOutpostVertices(int player) const {
    std::vector<int> result;
    const int vertexCount = static_cast<int>(board.Vertices().size());
    for (int v = 0; v < vertexCount; ++v) {
        if (!VertexFreeByDistance(v)) continue;
        if (phase == Phase::Setup || RouteTouchesVertexFor(player, v, -1)) result.push_back(v);
    }
    return result;
}

std::vector<int> Game::LegalRouteEdges(int player) const {
    std::vector<int> result;
    const int edgeCount = static_cast<int>(board.Edges().size());
    for (int e = 0; e < edgeCount; ++e) {
        if (routeOwners[static_cast<size_t>(e)] >= 0) continue;
        const Edge& edge = board.Edges()[static_cast<size_t>(e)];
        if (phase == Phase::Setup) {
            if (setupNeedsRoute && (edge.a == setupVertex || edge.b == setupVertex)) result.push_back(e);
            continue;
        }
        bool connected = false;
        for (int endpoint : {edge.a, edge.b}) {
            const Building& building = buildings[static_cast<size_t>(endpoint)];
            if (building.owner == player) connected = true;
            else if (building.owner < 0 && RouteTouchesVertexFor(player, endpoint, e)) connected = true;
        }
        if (connected) result.push_back(e);
    }
    return result;
}

std::vector<int> Game::LegalUpgradeVertices(int player) const {
    std::vector<int> result;
    for (size_t v = 0; v < buildings.size(); ++v) {
        if (buildings[v].owner == player && buildings[v].level == 1) result.push_back(static_cast<int>(v));
    }
    return result;
}

bool Game::CanAfford(int player, const std::vector<int>& cost) const {
    const Player& p = players[static_cast<size_t>(player)];
    for (size_t r = 0; r < cost.size() && r < p.resources.size(); ++r) {
        if (p.resources[r] < cost[r]) return false;
    }
    return true;
}

bool Game::CanBuild(int player, PieceKind kind) const {
    const Player& p = players[static_cast<size_t>(player)];
    const PieceDef& def = scenario.Piece(kind);
    int built = 0;
    switch (kind) {
        case PieceKind::Outpost: built = p.outposts; break;
        case PieceKind::Route: built = p.routes; break;
        case PieceKind::Center: built = p.centers; break;
        case PieceKind::Work: built = p.works; break;
    }
    return built < def.limit && CanAfford(player, def.cost);
}

void Game::Pay(int player, const std::vector<int>& cost) {
    Player& p = players[static_cast<size_t>(player)];
    for (size_t r = 0; r < cost.size() && r < p.resources.size(); ++r) {
        p.resources[r] -= cost[r];
    }
}

int Game::Prestige(int player) const {
    const Player& p = players[static_cast<size_t>(player)];
    int total = p.outposts * scenario.Piece(PieceKind::Outpost).prestige +
                p.centers * scenario.Piece(PieceKind::Center).prestige +
                p.routes * scenario.Piece(PieceKind::Route).prestige +
                p.works * scenario.Piece(PieceKind::Work).prestige;
    for (size_t c = 0; c < contractOwners.size(); ++c) {
        if (contractOwners[c] == player) total += scenario.contracts[c].prestige;
    }
    return total;
}

int Game::PrestigeTarget() const {
    return scenario.prestigeToWin - (static_cast<int>(players.size()) - 2);
}

int Game::MarketBuyPrice(int resource) const {
    return std::max(1, kMarketBasePrice - marketStock[static_cast<size_t>(resource)]);
}

int Game::MarketSellPrice(int resource) const {
    return std::max(1, kMarketBasePrice - 1 - marketStock[static_cast<size_t>(resource)]);
}

std::vector<int> Game::ReachableTilesOfTerrain(int player, int terrain) const {
    std::vector<int> result;
    for (size_t v = 0; v < buildings.size(); ++v) {
        if (buildings[v].owner != player) continue;
        for (int tile : board.Vertices()[v].tiles) {
            if (board.Tiles()[static_cast<size_t>(tile)].terrain == terrain &&
                std::find(result.begin(), result.end(), tile) == result.end()) {
                result.push_back(tile);
            }
        }
    }
    return result;
}

int Game::ResourceDiversity(int player) const {
    // Cuenta los recursos de los que el jugador toca al menos 2 casillas distintas.
    std::vector<int> tilesPerResource(scenario.resources.size(), 0);
    std::vector<bool> seenTile(board.Tiles().size(), false);
    for (size_t v = 0; v < buildings.size(); ++v) {
        if (buildings[v].owner != player) continue;
        for (int tile : board.Vertices()[v].tiles) {
            if (seenTile[static_cast<size_t>(tile)]) continue;
            seenTile[static_cast<size_t>(tile)] = true;
            const int terrain = board.Tiles()[static_cast<size_t>(tile)].terrain;
            if (terrain < 0) continue;
            const int resource = scenario.terrains[static_cast<size_t>(terrain)].resource;
            if (resource >= 0) ++tilesPerResource[static_cast<size_t>(resource)];
        }
    }
    return static_cast<int>(std::count_if(tilesPerResource.begin(), tilesPerResource.end(), [](int n) { return n >= 2; }));
}

bool Game::ContractMet(int player, const ContractDef& contract) const {
    const Player& p = players[static_cast<size_t>(player)];
    switch (contract.kind) {
        case ContractKind::Routes: return p.routes >= contract.value;
        case ContractKind::Centers: return p.centers >= contract.value;
        case ContractKind::TerrainReach:
            return static_cast<int>(ReachableTilesOfTerrain(player, contract.terrain).size()) >= contract.value;
        case ContractKind::Diversity: return ResourceDiversity(player) >= contract.value;
        case ContractKind::Coins: return p.coins >= contract.value;
    }
    return false;
}

void Game::CheckContractsAndVictory(int player, std::vector<GameEvent>& events) {
    for (size_t c = 0; c < scenario.contracts.size(); ++c) {
        if (contractOwners[c] >= 0 || !ContractMet(player, scenario.contracts[c])) continue;
        contractOwners[c] = player;
        GameEvent claimed;
        claimed.type = EventType::ContractClaimed;
        claimed.player = player;
        claimed.index = static_cast<int>(c);
        events.push_back(claimed);
    }
    if (phase != Phase::Setup && !finalRound && Prestige(player) >= PrestigeTarget()) {
        finalRound = true;
        GameEvent final;
        final.type = EventType::FinalRound;
        final.player = player;
        events.push_back(final);
    }
}

void Game::StartTurn(std::vector<GameEvent>& events) {
    Player& p = players[static_cast<size_t>(currentPlayer)];
    p.influence = std::min(kMaxInfluence, p.influence + p.centers);
    boostedTile = -1;
    phase = Phase::AwaitRoll;
    GameEvent started;
    started.type = EventType::TurnStarted;
    started.player = currentPlayer;
    started.index = round;
    events.push_back(started);
}

void Game::TickStatuses() {
    for (Tile& tile : board.MutableTiles()) {
        if (tile.blockedRounds > 0) --tile.blockedRounds;
        if (tile.frostRounds > 0) --tile.frostRounds;
    }
    // El mercado vuelve de a poco a su equilibrio.
    for (int& stock : marketStock) {
        if (stock < scenario.marketStartStock) ++stock;
        else if (stock > scenario.marketStartStock) --stock;
    }
}

DieFace Game::LastFace(int index) const {
    const int face = lastDice[index];
    if (face < 1 || face > kDieFaces) return DieFace{DieFaceKind::Terrain, -1};
    return scenario.dice[index][face - 1];
}

int Game::TerrainYieldFor(int player, int terrain) const {
    int total = 0;
    for (const Tile& tile : board.Tiles()) {
        if (tile.terrain != terrain || tile.blockedRounds > 0) continue;
        for (int vertex : tile.vertices) {
            const Building& building = buildings[static_cast<size_t>(vertex)];
            if (building.owner == player) total += tile.frostRounds > 0 ? building.level / 2 : building.level;
        }
    }
    return total;
}

void Game::Produce(int terrainA, int terrainB, std::vector<GameEvent>& events) {
    // Dobles: si los dos dados muestran el mismo terreno, produce el doble.
    int intensity = 1;
    if (terrainA >= 0 && terrainA == terrainB) {
        intensity = 2;
        terrainB = -1;
    }
    const std::vector<Tile>& tiles = board.Tiles();
    for (size_t t = 0; t < tiles.size(); ++t) {
        const Tile& tile = tiles[t];
        if (tile.terrain < 0 || tile.blockedRounds > 0) continue;
        // Produce si sale su terreno; la casilla reforzada produce igual aunque no salga.
        const bool matched = tile.terrain == terrainA || tile.terrain == terrainB;
        const bool boosted = boostedTile == static_cast<int>(t);
        if (!matched && !boosted) continue;
        const int resource = scenario.terrains[static_cast<size_t>(tile.terrain)].resource;
        if (resource < 0) continue;
        for (int vertex : tile.vertices) {
            const Building& building = buildings[static_cast<size_t>(vertex)];
            if (building.owner < 0) continue;
            int amount = matched ? intensity * building.level : building.level;
            if (tile.frostRounds > 0) amount /= 2;
            if (matched && boosted) amount += 1;
            if (amount <= 0) continue;
            players[static_cast<size_t>(building.owner)].resources[static_cast<size_t>(resource)] += amount;
            GameEvent produced;
            produced.type = EventType::Produced;
            produced.player = building.owner;
            produced.tile = static_cast<int>(t);
            produced.resource = resource;
            produced.amount = amount;
            events.push_back(produced);
        }
    }
}

void Game::FinishRoll(std::vector<GameEvent>& events) {
    Produce(rolledTerrains[0], rolledTerrains[1], events);
    boostedTile = -1;
    phase = Phase::Main;
}

void Game::ApplyWorldEvent(std::vector<GameEvent>& events) {
    std::vector<Tile>& tiles = board.MutableTiles();
    const WorldEventDef& def = scenario.worldEvent;
    const int tileCount = static_cast<int>(tiles.size());
    const int affected = std::max(1, def.tiles);

    switch (def.kind) {
        case WorldEventKind::Lava: {
            std::vector<int> candidates;
            for (int t = 0; t < tileCount; ++t) {
                if (tiles[static_cast<size_t>(t)].terrain != def.terrain) candidates.push_back(t);
            }
            rng.Shuffle(candidates);
            for (int i = 0; i < affected && i < static_cast<int>(candidates.size()); ++i) {
                const int t = candidates[static_cast<size_t>(i)];
                tiles[static_cast<size_t>(t)].terrain = def.terrain;
                GameEvent lava;
                lava.type = EventType::Lava;
                lava.tile = t;
                events.push_back(lava);
                for (int edge : tiles[static_cast<size_t>(t)].edges) {
                    const int owner = routeOwners[static_cast<size_t>(edge)];
                    if (owner < 0) continue;
                    routeOwners[static_cast<size_t>(edge)] = -1;
                    --players[static_cast<size_t>(owner)].routes;
                    GameEvent destroyed;
                    destroyed.type = EventType::RouteDestroyed;
                    destroyed.player = owner;
                    destroyed.index = edge;
                    destroyed.tile = t;
                    events.push_back(destroyed);
                }
            }
            break;
        }
        case WorldEventKind::DustStorm: {
            std::vector<int> candidates;
            for (int t = 0; t < tileCount; ++t) {
                if (tiles[static_cast<size_t>(t)].blockedRounds == 0) candidates.push_back(t);
            }
            rng.Shuffle(candidates);
            for (int i = 0; i < affected && i < static_cast<int>(candidates.size()); ++i) {
                const int t = candidates[static_cast<size_t>(i)];
                tiles[static_cast<size_t>(t)].blockedRounds = def.duration;
                GameEvent storm;
                storm.type = EventType::DustStorm;
                storm.tile = t;
                storm.amount = def.duration;
                events.push_back(storm);
            }
            break;
        }
        case WorldEventKind::Frost: {
            for (int i = 0; i < affected; ++i) {
                const int center = rng.NextInt(tileCount);
                std::vector<int> region = board.TileNeighbors(center);
                region.push_back(center);
                for (int t : region) {
                    tiles[static_cast<size_t>(t)].frostRounds = def.duration;
                    GameEvent frost;
                    frost.type = EventType::Frost;
                    frost.tile = t;
                    frost.tile2 = center;
                    frost.amount = def.duration;
                    events.push_back(frost);
                }
            }
            break;
        }
        case WorldEventKind::PlatformSwap: {
            for (int i = 0; i < affected; ++i) {
                const int first = rng.NextInt(tileCount);
                int second = rng.NextInt(tileCount - 1);
                if (second >= first) ++second;
                Tile& a = tiles[static_cast<size_t>(first)];
                Tile& b = tiles[static_cast<size_t>(second)];
                std::swap(a.terrain, b.terrain);
                std::swap(a.blockedRounds, b.blockedRounds);
                std::swap(a.frostRounds, b.frostRounds);
                GameEvent swap;
                swap.type = EventType::PlatformSwap;
                swap.tile = first;
                swap.tile2 = second;
                events.push_back(swap);
            }
            break;
        }
    }
}

ApplyResult Game::DoSetup(const Action& action) {
    ApplyResult result;
    const int player = currentPlayer;
    if (action.type == ActionType::BuildOutpost) {
        if (setupNeedsRoute) return Fail("Primero colocá la ruta de tu último puesto");
        if (action.target < 0 || action.target >= static_cast<int>(buildings.size())) return Fail("Vértice inválido");
        if (!VertexFreeByDistance(action.target)) return Fail("Muy cerca de otro puesto");
        buildings[static_cast<size_t>(action.target)] = {player, 1};
        ++players[static_cast<size_t>(player)].outposts;
        setupNeedsRoute = true;
        setupVertex = action.target;
        GameEvent built;
        built.type = EventType::OutpostBuilt;
        built.player = player;
        built.index = action.target;
        result.events.push_back(built);
        result.ok = true;
        return result;
    }
    if (action.type == ActionType::BuildRoute) {
        if (!setupNeedsRoute) return Fail("Primero colocá un puesto");
        if (action.target < 0 || action.target >= static_cast<int>(routeOwners.size())) return Fail("Arista inválida");
        const Edge& edge = board.Edges()[static_cast<size_t>(action.target)];
        if (routeOwners[static_cast<size_t>(action.target)] >= 0) return Fail("Ya hay una ruta ahí");
        if (edge.a != setupVertex && edge.b != setupVertex) return Fail("La ruta debe salir del puesto recién colocado");
        routeOwners[static_cast<size_t>(action.target)] = player;
        ++players[static_cast<size_t>(player)].routes;
        GameEvent built;
        built.type = EventType::RouteBuilt;
        built.player = player;
        built.index = action.target;
        result.events.push_back(built);

        // El segundo puesto da un recurso de cada casilla vecina.
        if (setupStep >= static_cast<int>(players.size())) {
            for (int tile : board.Vertices()[static_cast<size_t>(setupVertex)].tiles) {
                const int terrain = board.Tiles()[static_cast<size_t>(tile)].terrain;
                const int resource = terrain >= 0 ? scenario.terrains[static_cast<size_t>(terrain)].resource : -1;
                if (resource < 0) continue;
                ++players[static_cast<size_t>(player)].resources[static_cast<size_t>(resource)];
                GameEvent produced;
                produced.type = EventType::Produced;
                produced.player = player;
                produced.tile = tile;
                produced.resource = resource;
                produced.amount = 1;
                result.events.push_back(produced);
            }
        }

        setupNeedsRoute = false;
        setupVertex = -1;
        ++setupStep;
        if (setupStep >= 2 * static_cast<int>(players.size())) {
            currentPlayer = 0;
            round = 1;
            StartTurn(result.events);
        } else {
            currentPlayer = SetupPlayerForStep(setupStep);
        }
        result.ok = true;
        return result;
    }
    return Fail("Durante la preparación solo se colocan puestos y rutas");
}

ApplyResult Game::Apply(const Action& action) {
    if (phase == Phase::Finished) return Fail("La partida terminó");
    if (phase == Phase::Setup) return DoSetup(action);

    ApplyResult result;
    const int player = currentPlayer;
    Player& p = players[static_cast<size_t>(player)];
    const int resourceCount = static_cast<int>(scenario.resources.size());

    if (phase == Phase::AwaitRoll) {
        if (action.type == ActionType::Boost) {
            if (boostedTile >= 0) return Fail("Ya reforzaste una casilla este turno");
            if (p.influence < 1) return Fail("No tenés Influencia");
            if (action.target < 0 || action.target >= static_cast<int>(board.Tiles().size())) return Fail("Casilla inválida");
            bool adjacent = false;
            for (int vertex : board.Tiles()[static_cast<size_t>(action.target)].vertices) {
                if (buildings[static_cast<size_t>(vertex)].owner == player) adjacent = true;
            }
            if (!adjacent) return Fail("Solo podés reforzar casillas junto a tus construcciones");
            --p.influence;
            boostedTile = action.target;
            GameEvent boosted;
            boosted.type = EventType::Boosted;
            boosted.player = player;
            boosted.tile = action.target;
            result.events.push_back(boosted);
            result.ok = true;
            return result;
        }
        if (action.type != ActionType::RollDice) return Fail("Primero tirá los dados");
        const bool external = action.dice[0] >= 0 || action.dice[1] >= 0;
        if (external) {
            for (int die : action.dice) {
                if (die < 1 || die > 6) return Fail("Dados inválidos");
            }
            lastDice[0] = action.dice[0];
            lastDice[1] = action.dice[1];
        } else {
            lastDice[0] = rng.NextInt(6) + 1;
            lastDice[1] = rng.NextInt(6) + 1;
        }
        hasRolled = true;
        GameEvent rolled;
        rolled.type = EventType::DiceRolled;
        rolled.player = player;
        rolled.index = lastDice[0];
        rolled.amount = lastDice[1];
        result.events.push_back(rolled);

        pendingWilds = 0;
        for (int die = 0; die < kDiceCount; ++die) {
            const DieFace face = LastFace(die);
            rolledTerrains[die] = face.kind == DieFaceKind::Terrain ? face.terrain : -1;
            if (face.kind == DieFaceKind::Event) ApplyWorldEvent(result.events);
            if (face.kind == DieFaceKind::Wild) ++pendingWilds;
        }
        if (pendingWilds > 0) {
            phase = Phase::ChooseWild; // la producción espera a que el jugador elija
        } else {
            FinishRoll(result.events);
        }
        result.ok = true;
        return result;
    }

    if (phase == Phase::ChooseWild) {
        if (action.type != ActionType::ChooseWild) return Fail("Elegí qué terreno produce con el comodín");
        const std::vector<int> producing = scenario.ProducingTerrains();
        if (std::find(producing.begin(), producing.end(), action.target) == producing.end())
            return Fail("Elegí un terreno que produzca recursos");
        // Dos comodines: el terreno elegido produce como dobles.
        const int other = pendingWilds >= 2 ? action.target : (rolledTerrains[0] >= 0 ? rolledTerrains[0] : rolledTerrains[1]);
        rolledTerrains[0] = action.target;
        rolledTerrains[1] = other;
        pendingWilds = 0;
        GameEvent chosen;
        chosen.type = EventType::WildChosen;
        chosen.player = player;
        chosen.index = action.target;
        result.events.push_back(chosen);
        FinishRoll(result.events);
        result.ok = true;
        return result;
    }

    // Phase::Main
    switch (action.type) {
        case ActionType::BuildOutpost: {
            if (!CanBuild(player, PieceKind::Outpost)) return Fail("No te alcanza o no te quedan piezas");
            const std::vector<int> legal = LegalOutpostVertices(player);
            if (std::find(legal.begin(), legal.end(), action.target) == legal.end())
                return Fail("Ahí no se puede: necesita una ruta tuya y distancia de otros puestos");
            Pay(player, scenario.Piece(PieceKind::Outpost).cost);
            buildings[static_cast<size_t>(action.target)] = {player, 1};
            ++p.outposts;
            GameEvent built;
            built.type = EventType::OutpostBuilt;
            built.player = player;
            built.index = action.target;
            result.events.push_back(built);
            break;
        }
        case ActionType::BuildRoute: {
            if (!CanBuild(player, PieceKind::Route)) return Fail("No te alcanza o no te quedan rutas");
            const std::vector<int> legal = LegalRouteEdges(player);
            if (std::find(legal.begin(), legal.end(), action.target) == legal.end())
                return Fail("La ruta debe conectar con tus rutas o construcciones");
            Pay(player, scenario.Piece(PieceKind::Route).cost);
            routeOwners[static_cast<size_t>(action.target)] = player;
            ++p.routes;
            GameEvent built;
            built.type = EventType::RouteBuilt;
            built.player = player;
            built.index = action.target;
            result.events.push_back(built);
            break;
        }
        case ActionType::UpgradeCenter: {
            if (!CanBuild(player, PieceKind::Center)) return Fail("No te alcanza o no te quedan centros");
            if (action.target < 0 || action.target >= static_cast<int>(buildings.size())) return Fail("Vértice inválido");
            Building& building = buildings[static_cast<size_t>(action.target)];
            if (building.owner != player || building.level != 1) return Fail("Solo se mejoran tus propios puestos");
            Pay(player, scenario.Piece(PieceKind::Center).cost);
            building.level = 2;
            --p.outposts;
            ++p.centers;
            GameEvent built;
            built.type = EventType::CenterBuilt;
            built.player = player;
            built.index = action.target;
            result.events.push_back(built);
            break;
        }
        case ActionType::PublicWork: {
            if (!CanBuild(player, PieceKind::Work)) return Fail("No te alcanza o ya hiciste todas las obras");
            Pay(player, scenario.Piece(PieceKind::Work).cost);
            ++p.works;
            GameEvent built;
            built.type = EventType::WorkBuilt;
            built.player = player;
            result.events.push_back(built);
            break;
        }
        case ActionType::MarketBuy: {
            if (action.resource < 0 || action.resource >= resourceCount) return Fail("Recurso inválido");
            const size_t r = static_cast<size_t>(action.resource);
            if (marketStock[r] <= 0) return Fail("El mercado no tiene ese recurso");
            const int price = MarketBuyPrice(action.resource);
            if (p.coins < price) return Fail("No te alcanzan las monedas");
            p.coins -= price;
            --marketStock[r];
            ++p.resources[r];
            GameEvent trade;
            trade.type = EventType::MarketTrade;
            trade.player = player;
            trade.resource = action.resource;
            trade.amount = 1;
            result.events.push_back(trade);
            break;
        }
        case ActionType::MarketSell: {
            if (action.resource < 0 || action.resource >= resourceCount) return Fail("Recurso inválido");
            const size_t r = static_cast<size_t>(action.resource);
            if (p.resources[r] <= 0) return Fail("No tenés ese recurso");
            p.coins += MarketSellPrice(action.resource);
            ++marketStock[r];
            --p.resources[r];
            GameEvent trade;
            trade.type = EventType::MarketTrade;
            trade.player = player;
            trade.resource = action.resource;
            trade.amount = -1;
            result.events.push_back(trade);
            break;
        }
        case ActionType::PlayerTrade: {
            const int other = action.otherPlayer;
            if (other < 0 || other >= static_cast<int>(players.size()) || other == player) return Fail("Jugador inválido");
            if (static_cast<int>(action.give.size()) != resourceCount || static_cast<int>(action.get.size()) != resourceCount)
                return Fail("Oferta inválida");
            int giveTotal = 0;
            int getTotal = 0;
            Player& partner = players[static_cast<size_t>(other)];
            for (int r = 0; r < resourceCount; ++r) {
                const size_t i = static_cast<size_t>(r);
                if (action.give[i] < 0 || action.get[i] < 0) return Fail("Cantidades negativas");
                if (p.resources[i] < action.give[i]) return Fail("No tenés lo que ofrecés");
                if (partner.resources[i] < action.get[i]) return Fail("El otro jugador no tiene lo que pedís");
                giveTotal += action.give[i];
                getTotal += action.get[i];
            }
            if (giveTotal == 0 || getTotal == 0) return Fail("Un intercambio necesita dar y recibir algo");
            for (int r = 0; r < resourceCount; ++r) {
                const size_t i = static_cast<size_t>(r);
                p.resources[i] += action.get[i] - action.give[i];
                partner.resources[i] += action.give[i] - action.get[i];
            }
            GameEvent traded;
            traded.type = EventType::PlayerTraded;
            traded.player = player;
            traded.index = other;
            result.events.push_back(traded);
            CheckContractsAndVictory(other, result.events);
            break;
        }
        case ActionType::EndTurn: {
            const int next = (currentPlayer + 1) % static_cast<int>(players.size());
            if (finalRound && next == 0) {
                // Todos jugaron la misma cantidad de turnos: gana el de más prestigio. En un empate
                // gana el que jugó más tarde (tuvo que esperar más para cada turno).
                int best = 0;
                for (int i = 1; i < static_cast<int>(players.size()); ++i) {
                    if (Prestige(i) >= Prestige(best)) best = i;
                }
                phase = Phase::Finished;
                winner = best;
                GameEvent won;
                won.type = EventType::Won;
                won.player = best;
                result.events.push_back(won);
                result.ok = true;
                return result;
            }
            currentPlayer = next;
            if (currentPlayer == 0) {
                ++round;
                TickStatuses();
            }
            StartTurn(result.events);
            result.ok = true;
            return result;
        }
        case ActionType::Boost:
        case ActionType::RollDice:
        case ActionType::ChooseWild:
            return Fail("Ya tiraste los dados este turno");
    }

    CheckContractsAndVictory(player, result.events);
    result.ok = true;
    return result;
}

} // namespace hf
