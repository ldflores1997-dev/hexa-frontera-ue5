#include "hf/Board.h"

#include <iterator>
#include <map>
#include <utility>

namespace hf {

int DiceWays(int sum) {
    if (sum < 2 || sum > 12) return 0;
    return 6 - (sum > 7 ? sum - 7 : 7 - sum);
}

void Board::AssignNumbers(const Scenario& scenario, Random& rng) {
    // Cada tanda tiene dos fichas de cada número del 3 al 11 y una de 2 y de 12 (el 7 no se usa:
    // con 7 ocurre el evento del mundo). Si hay más casillas que fichas se agrega otra tanda.
    static const int kBatch[] = {2, 3, 3, 4, 4, 5, 5, 6, 6, 8, 8, 9, 9, 10, 10, 11, 11, 12};
    std::vector<int> producing;
    for (size_t t = 0; t < tiles.size(); ++t) {
        const int terrain = tiles[t].terrain;
        if (terrain >= 0 && scenario.terrains[static_cast<size_t>(terrain)].resource >= 0) producing.push_back(static_cast<int>(t));
    }
    std::vector<int> bag;
    while (bag.size() < producing.size()) bag.insert(bag.end(), std::begin(kBatch), std::end(kBatch));
    rng.Shuffle(bag);
    bag.resize(producing.size());

    // Los números más probables (6 y 8) se ubican primero, nunca dos juntos; después el resto.
    std::vector<int> hot;
    std::vector<int> cold;
    for (int number : bag) (number == 6 || number == 8 ? hot : cold).push_back(number);
    const int kAttempts = 100;
    for (int attempt = 0; attempt < kAttempts; ++attempt) {
        for (int t : producing) tiles[static_cast<size_t>(t)].number = 0;
        std::vector<int> order = producing;
        rng.Shuffle(order);
        std::vector<int> free;
        size_t placed = 0;
        for (int t : order) {
            bool nearHot = false;
            for (int neighbor : TileNeighbors(t)) {
                const int other = tiles[static_cast<size_t>(neighbor)].number;
                if (other == 6 || other == 8) nearHot = true;
            }
            // En el último intento se ubican igual, para que ninguna casilla quede sin número.
            if (placed < hot.size() && (!nearHot || attempt == kAttempts - 1)) tiles[static_cast<size_t>(t)].number = hot[placed++];
            else free.push_back(t);
        }
        for (size_t i = 0; i < free.size() && i < cold.size(); ++i) tiles[static_cast<size_t>(free[i])].number = cold[i];
        if (placed == hot.size()) return;
    }
}

void Board::Build(const Scenario& scenario, uint64_t seed) {
    tiles.clear();
    vertices.clear();
    edges.clear();

    std::vector<int> terrainBag;
    for (size_t t = 0; t < scenario.terrains.size(); ++t) {
        for (int i = 0; i < scenario.terrains[t].count; ++i) terrainBag.push_back(static_cast<int>(t));
    }
    Random rng(seed);
    rng.Shuffle(terrainBag);

    const std::vector<Axial> coords = HexSpiral(scenario.radius);
    std::map<VertexKey, int> vertexIds;
    std::map<std::pair<int, int>, int> edgeIds;

    for (size_t i = 0; i < coords.size(); ++i) {
        Tile tile;
        tile.coord = coords[i];
        tile.terrain = i < terrainBag.size() ? terrainBag[i] : -1;
        const int tileIndex = static_cast<int>(tiles.size());

        const std::array<VertexKey, 6> corners = HexCorners(tile.coord);
        for (size_t c = 0; c < 6; ++c) {
            auto found = vertexIds.find(corners[c]);
            int vertexIndex;
            if (found == vertexIds.end()) {
                vertexIndex = static_cast<int>(vertices.size());
                Vertex vertex;
                vertex.key = corners[c];
                vertex.position = VertexToPoint(corners[c]);
                vertices.push_back(vertex);
                vertexIds.emplace(corners[c], vertexIndex);
            } else {
                vertexIndex = found->second;
            }
            vertices[static_cast<size_t>(vertexIndex)].tiles.push_back(tileIndex);
            tile.vertices[c] = vertexIndex;
        }

        for (size_t c = 0; c < 6; ++c) {
            int a = tile.vertices[c];
            int b = tile.vertices[(c + 1) % 6];
            if (a > b) std::swap(a, b);
            auto found = edgeIds.find({a, b});
            int edgeIndex;
            if (found == edgeIds.end()) {
                edgeIndex = static_cast<int>(edges.size());
                Edge edge;
                edge.a = a;
                edge.b = b;
                edges.push_back(edge);
                edgeIds.emplace(std::make_pair(a, b), edgeIndex);
                vertices[static_cast<size_t>(a)].neighbors.push_back(b);
                vertices[static_cast<size_t>(b)].neighbors.push_back(a);
                vertices[static_cast<size_t>(a)].edges.push_back(edgeIndex);
                vertices[static_cast<size_t>(b)].edges.push_back(edgeIndex);
            } else {
                edgeIndex = found->second;
            }
            edges[static_cast<size_t>(edgeIndex)].tiles.push_back(tileIndex);
            tile.edges[c] = edgeIndex;
        }
        tiles.push_back(tile);
    }
    AssignNumbers(scenario, rng);
}

int Board::TileAt(const Axial& coord) const {
    for (size_t i = 0; i < tiles.size(); ++i) {
        if (tiles[i].coord == coord) return static_cast<int>(i);
    }
    return -1;
}

std::vector<int> Board::TileNeighbors(int tile) const {
    std::vector<int> result;
    const Axial center = tiles[static_cast<size_t>(tile)].coord;
    for (const Axial& dir : HexDirections()) {
        const int neighbor = TileAt(center + dir);
        if (neighbor >= 0) result.push_back(neighbor);
    }
    return result;
}

int Board::EdgeBetween(int vertexA, int vertexB) const {
    for (int edgeIndex : vertices[static_cast<size_t>(vertexA)].edges) {
        const Edge& edge = edges[static_cast<size_t>(edgeIndex)];
        if ((edge.a == vertexA && edge.b == vertexB) || (edge.a == vertexB && edge.b == vertexA)) return edgeIndex;
    }
    return -1;
}

} // namespace hf
