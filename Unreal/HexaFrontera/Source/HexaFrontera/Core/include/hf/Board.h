// Tablero: casillas, vértices (donde van los puestos) y aristas (donde van las rutas).
#pragma once

#include <cstdint>
#include <vector>

#include "hf/Hex.h"
#include "hf/Random.h"
#include "hf/Scenario.h"

namespace hf {

// Probabilidad de cada suma de dos dados, en 36avos (2 y 12: 1; 7: 6).
int DiceWays(int sum);

struct Tile {
    Axial coord;
    int terrain = -1;
    int number = 0;        // suma de dados que la hace producir (2-12, nunca 7); 0 si no produce
    int blockedRounds = 0; // tormenta: no produce
    int frostRounds = 0;   // helada: produce la mitad
    std::array<int, 6> vertices{};
    std::array<int, 6> edges{};
};

struct Vertex {
    VertexKey key;
    Point2 position;
    std::vector<int> tiles;     // 1 a 3 casillas que tocan este vértice
    std::vector<int> neighbors; // vértices a una arista de distancia
    std::vector<int> edges;
};

struct Edge {
    int a = -1;
    int b = -1;
    std::vector<int> tiles; // 1 o 2 casillas a los lados
};

class Board {
public:
    // Arma el tablero del escenario y reparte los terrenos y los números al azar con la semilla.
    void Build(const Scenario& scenario, uint64_t seed);

    const std::vector<Tile>& Tiles() const { return tiles; }
    const std::vector<Vertex>& Vertices() const { return vertices; }
    const std::vector<Edge>& Edges() const { return edges; }
    std::vector<Tile>& MutableTiles() { return tiles; }

    int TileAt(const Axial& coord) const; // -1 si no existe
    std::vector<int> TileNeighbors(int tile) const;
    int EdgeBetween(int vertexA, int vertexB) const; // -1 si no son vecinos

private:
    void AssignNumbers(const Scenario& scenario, Random& rng);

    std::vector<Tile> tiles;
    std::vector<Vertex> vertices;
    std::vector<Edge> edges;
};

} // namespace hf
