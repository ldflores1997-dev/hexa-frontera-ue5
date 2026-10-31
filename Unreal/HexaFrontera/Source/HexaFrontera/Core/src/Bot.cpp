#include "hf/Bot.h"

#include <algorithm>
#include <vector>

namespace hf {

namespace {

const int kMaxBotActionsPerTurn = 25;

bool BotVertexFree(const Game& game, int vertex) {
    const auto& buildings = game.Buildings();
    if (buildings[static_cast<size_t>(vertex)].owner >= 0) return false;
    for (int neighbor : game.GetBoard().Vertices()[static_cast<size_t>(vertex)].neighbors) {
        if (buildings[static_cast<size_t>(neighbor)].owner >= 0) return false;
    }
    return true;
}

int BestVertex(const Game& game, int player, const std::vector<int>& candidates) {
    int best = -1;
    double bestScore = -1e9;
    for (int vertex : candidates) {
        const double score = Bot::ScoreVertex(game, player, vertex);
        if (score > bestScore) {
            bestScore = score;
            best = vertex;
        }
    }
    return best;
}

// Qué tan prometedor es el extremo de una ruta para expandirse.
double ScoreEdgeTowards(const Game& game, int player, int edgeIndex, int fromVertex) {
    const Edge& edge = game.GetBoard().Edges()[static_cast<size_t>(edgeIndex)];
    double best = 0.0;
    for (int endpoint : {edge.a, edge.b}) {
        if (endpoint == fromVertex) continue;
        if (BotVertexFree(game, endpoint)) {
            best = std::max(best, Bot::ScoreVertex(game, player, endpoint));
        }
        for (int next : game.GetBoard().Vertices()[static_cast<size_t>(endpoint)].neighbors) {
            if (BotVertexFree(game, next)) best = std::max(best, 0.6 * Bot::ScoreVertex(game, player, next));
        }
    }
    return best - edgeIndex * 1e-7;
}

int BestRoute(const Game& game, int player, const std::vector<int>& candidates, int fromVertex) {
    int best = -1;
    double bestScore = -1e9;
    for (int edge : candidates) {
        const double score = ScoreEdgeTowards(game, player, edge, fromVertex);
        if (score > bestScore) {
            bestScore = score;
            best = edge;
        }
    }
    return best;
}

} // namespace

int Bot::BestWildTerrain(const Game& game, int player) {
    int best = -1;
    int bestYield = -1;
    for (int terrain : game.GetScenario().ProducingTerrains()) {
        const int yield = game.TerrainYieldFor(player, terrain);
        if (yield > bestYield) {
            bestYield = yield;
            best = terrain;
        }
    }
    return best;
}

double Bot::ScoreVertex(const Game& game, int player, int vertex) {
    const Scenario& scenario = game.GetScenario();
    const Board& board = game.GetBoard();
    std::vector<bool> owned(scenario.resources.size(), false);
    for (size_t v = 0; v < game.Buildings().size(); ++v) {
        if (game.Buildings()[v].owner != player) continue;
        for (int tile : board.Vertices()[v].tiles) {
            const int terrain = board.Tiles()[static_cast<size_t>(tile)].terrain;
            if (terrain >= 0 && scenario.terrains[static_cast<size_t>(terrain)].resource >= 0)
                owned[static_cast<size_t>(scenario.terrains[static_cast<size_t>(terrain)].resource)] = true;
        }
    }
    double score = 0.0;
    std::vector<bool> counted(scenario.resources.size(), false);
    for (int tile : board.Vertices()[static_cast<size_t>(vertex)].tiles) {
        const Tile& t = board.Tiles()[static_cast<size_t>(tile)];
        if (t.terrain < 0) continue;
        const int resource = scenario.terrains[static_cast<size_t>(t.terrain)].resource;
        if (resource < 0) continue;
        // Vale más un terreno que aparece en más caras de los dados (lo normal: 2 de 12).
        double value = scenario.FacesShowingTerrain(t.terrain) / 2.0;
        if (t.blockedRounds > 0) value -= 0.4;
        if (t.frostRounds > 0) value -= 0.2;
        if (!owned[static_cast<size_t>(resource)] && !counted[static_cast<size_t>(resource)]) value += 0.6;
        counted[static_cast<size_t>(resource)] = true;
        score += value;
    }
    return score - vertex * 1e-7;
}

Action Bot::ChooseAction(const Game& game, int actionsThisTurn) {
    const int player = game.CurrentPlayer();
    const Scenario& scenario = game.GetScenario();
    const Player& me = game.Players()[static_cast<size_t>(player)];

    if (game.GetPhase() == Phase::Setup) {
        if (!game.SetupNeedsRoute()) return Action::Outpost(BestVertex(game, player, game.LegalOutpostVertices(player)));
        return Action::Route(BestRoute(game, player, game.LegalRouteEdges(player), game.SetupVertex()));
    }

    if (game.GetPhase() == Phase::AwaitRoll) {
        if (me.influence >= 3 && game.BoostedTile() < 0) {
            // Refuerza donde tiene más construcciones; a igualdad, el terreno que menos sale en los dados.
            int bestTile = -1;
            int bestScore = 0;
            const auto& tiles = game.GetBoard().Tiles();
            for (size_t t = 0; t < tiles.size(); ++t) {
                if (tiles[t].terrain < 0 || tiles[t].blockedRounds > 0 ||
                    scenario.terrains[static_cast<size_t>(tiles[t].terrain)].resource < 0)
                    continue;
                int level = 0;
                for (int vertex : tiles[t].vertices) {
                    if (game.Buildings()[static_cast<size_t>(vertex)].owner == player)
                        level += game.Buildings()[static_cast<size_t>(vertex)].level;
                }
                const int score = level * 10 - scenario.FacesShowingTerrain(tiles[t].terrain);
                if (level > 0 && score > bestScore) {
                    bestScore = score;
                    bestTile = static_cast<int>(t);
                }
            }
            if (bestTile >= 0) return Action::BoostTile(bestTile);
        }
        return Action::Roll();
    }

    if (game.GetPhase() == Phase::ChooseWild) return Action::Wild(BestWildTerrain(game, player));

    if (game.GetPhase() != Phase::Main || actionsThisTurn >= kMaxBotActionsPerTurn) return Action::End();

    const std::vector<int> upgrades = game.LegalUpgradeVertices(player);
    const std::vector<int> outpostSpots = game.LegalOutpostVertices(player);
    const std::vector<int> routeSpots = game.LegalRouteEdges(player);
    const bool outpostsLeft = me.outposts < scenario.Piece(PieceKind::Outpost).limit;

    if (!upgrades.empty() && game.CanBuild(player, PieceKind::Center))
        return Action::Upgrade(BestVertex(game, player, upgrades));
    if (!outpostSpots.empty() && game.CanBuild(player, PieceKind::Outpost))
        return Action::Outpost(BestVertex(game, player, outpostSpots));
    const bool routesLeft = me.routes < scenario.Piece(PieceKind::Route).limit;
    const bool canExpand = (!outpostSpots.empty() && outpostsLeft) || (!routeSpots.empty() && routesLeft);
    if (!routeSpots.empty() && game.CanBuild(player, PieceKind::Route) && (outpostSpots.empty() || !outpostsLeft))
        return Action::Route(BestRoute(game, player, routeSpots, -1));
    // Sin lugar para crecer (o ya cerca de ganar), las obras públicas dan prestigio.
    const bool nearWin = game.Prestige(player) + scenario.Piece(PieceKind::Work).prestige >= game.PrestigeTarget();
    if (game.CanBuild(player, PieceKind::Work) && (!canExpand || nearWin)) return Action::Work();

    // Qué pieza conviene juntar.
    PieceKind goal = PieceKind::Route;
    if (!upgrades.empty() && me.centers < scenario.Piece(PieceKind::Center).limit) goal = PieceKind::Center;
    else if (!outpostSpots.empty() && outpostsLeft) goal = PieceKind::Outpost;
    else if (!routeSpots.empty() && routesLeft) goal = PieceKind::Route;
    else if (me.works < scenario.Piece(PieceKind::Work).limit) goal = PieceKind::Work;
    else return Action::End();

    const std::vector<int>& cost = scenario.Piece(goal).cost;
    const int resourceCount = scenario.ResourceCount();
    for (int r = 0; r < resourceCount; ++r) {
        const size_t i = static_cast<size_t>(r);
        if (me.resources[i] < cost[i] && game.MarketStock(r) > 0 && me.coins >= game.MarketBuyPrice(r))
            return Action::Buy(r);
    }
    // Vende el excedente que no necesita para la meta.
    int sellResource = -1;
    int bestSurplus = 0;
    for (int r = 0; r < resourceCount; ++r) {
        const size_t i = static_cast<size_t>(r);
        const int surplus = me.resources[i] - cost[i];
        if (surplus > bestSurplus) {
            bestSurplus = surplus;
            sellResource = r;
        }
    }
    bool missingSomething = false;
    for (int r = 0; r < resourceCount; ++r) {
        if (me.resources[static_cast<size_t>(r)] < cost[static_cast<size_t>(r)]) missingSomething = true;
    }
    if (missingSomething && sellResource >= 0) return Action::Sell(sellResource);
    return Action::End();
}

} // namespace hf
