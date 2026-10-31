#include "hf/Hex.h"

#include <cstdlib>

namespace hf {

namespace {
const double kSqrt3 = 1.7320508075688772;
}

const std::array<Axial, 6>& HexDirections() {
    static const std::array<Axial, 6> directions = {{
        {1, 0}, {1, -1}, {0, -1}, {-1, 0}, {-1, 1}, {0, 1},
    }};
    return directions;
}

int HexDistance(const Axial& a, const Axial& b) {
    const int dq = a.q - b.q;
    const int dr = a.r - b.r;
    return (std::abs(dq) + std::abs(dr) + std::abs(dq + dr)) / 2;
}

std::vector<Axial> HexSpiral(int radius) {
    std::vector<Axial> result;
    result.push_back({0, 0});
    const auto& dirs = HexDirections();
    for (int ring = 1; ring <= radius; ++ring) {
        // Arranca en la dirección 4 (sudoeste) multiplicada por el anillo y recorre los 6 lados.
        Axial cursor = {dirs[4].q * ring, dirs[4].r * ring};
        for (int side = 0; side < 6; ++side) {
            for (int step = 0; step < ring; ++step) {
                result.push_back(cursor);
                cursor = cursor + dirs[static_cast<size_t>(side)];
            }
        }
    }
    return result;
}

std::array<VertexKey, 6> HexCorners(const Axial& hex) {
    // Coordenada cúbica (x, z) = (q, r); la tercera queda implícita.
    const auto& dirs = HexDirections();
    std::array<VertexKey, 6> corners;
    for (size_t i = 0; i < 6; ++i) {
        const Axial& a = dirs[i];
        const Axial& b = dirs[(i + 1) % 6];
        corners[i] = {3 * hex.q + a.q + b.q, 3 * hex.r + a.r + b.r};
    }
    return corners;
}

Point2 HexToPoint(const Axial& hex) {
    return {kSqrt3 * (hex.q + hex.r / 2.0), 1.5 * hex.r};
}

Point2 VertexToPoint(const VertexKey& key) {
    const double q = key.x / 3.0;
    const double r = key.z / 3.0;
    return {kSqrt3 * (q + r / 2.0), 1.5 * r};
}

} // namespace hf
