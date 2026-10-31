#include "hf/Board.h"

#include <map>
#include <utility>

#include "hf/Random.h"

namespace hf {

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
