// Coordenadas hexagonales axiales (hexágonos con punta arriba).
#pragma once

#include <array>
#include <vector>

namespace hf {

struct Axial {
    int q = 0;
    int r = 0;

    bool operator==(const Axial& other) const { return q == other.q && r == other.r; }
    bool operator!=(const Axial& other) const { return !(*this == other); }
    bool operator<(const Axial& other) const { return q != other.q ? q < other.q : r < other.r; }
    Axial operator+(const Axial& other) const { return {q + other.q, r + other.r}; }
};

// Las 6 direcciones vecinas, en sentido antihorario empezando por el este.
const std::array<Axial, 6>& HexDirections();

int HexDistance(const Axial& a, const Axial& b);

// Todas las casillas a distancia <= radius del centro, en orden de espiral.
std::vector<Axial> HexSpiral(int radius);

// Clave entera de un vértice: 3 * (coordenada cúbica del centro) + suma de dos
// direcciones vecinas. Tres hexágonos que comparten un vértice dan la misma clave.
struct VertexKey {
    int x = 0;
    int z = 0;
    bool operator==(const VertexKey& other) const { return x == other.x && z == other.z; }
    bool operator<(const VertexKey& other) const { return x != other.x ? x < other.x : z < other.z; }
};

// Los 6 vértices de un hexágono, en orden; el vértice i y el i+1 forman la arista i.
std::array<VertexKey, 6> HexCorners(const Axial& hex);

// Posición en el plano para un hexágono de radio 1 (centro a vértice).
struct Point2 {
    double x = 0.0;
    double y = 0.0;
};

Point2 HexToPoint(const Axial& hex);
Point2 VertexToPoint(const VertexKey& key);

} // namespace hf
