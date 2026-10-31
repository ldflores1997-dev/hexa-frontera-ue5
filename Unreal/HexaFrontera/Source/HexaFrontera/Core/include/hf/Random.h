// Generador pseudoaleatorio propio: la misma semilla da la misma partida
// en cualquier compilador (las distribuciones de <random> no lo garantizan).
#pragma once

#include <cstdint>
#include <utility>
#include <vector>

namespace hf {

class Random {
public:
    explicit Random(uint64_t seed = 1) : state(seed) {}

    uint64_t NextU64() {
        // SplitMix64
        uint64_t z = (state += 0x9E3779B97F4A7C15ull);
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
        return z ^ (z >> 31);
    }

    // Entero en [0, bound). bound debe ser > 0.
    int NextInt(int bound) {
        return static_cast<int>(NextU64() % static_cast<uint64_t>(bound));
    }

    template <typename T>
    void Shuffle(std::vector<T>& values) {
        for (int i = static_cast<int>(values.size()) - 1; i > 0; --i) {
            const int j = NextInt(i + 1);
            std::swap(values[static_cast<size_t>(i)], values[static_cast<size_t>(j)]);
        }
    }

    uint64_t GetState() const { return state; }

private:
    uint64_t state;
};

} // namespace hf
