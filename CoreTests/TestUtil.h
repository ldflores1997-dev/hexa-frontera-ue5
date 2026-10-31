// Mini framework de tests y utilidades compartidas por tests.cpp y simulate.cpp.
#pragma once

#include <cstdio>
#include <fstream>
#include <functional>
#include <sstream>
#include <string>
#include <vector>

#include "hf/Bot.h"
#include "hf/Game.h"
#include "hf/Scenario.h"

namespace hftest {

struct TestCase {
    const char* name;
    std::function<void()> body;
};

inline std::vector<TestCase>& Registry() {
    static std::vector<TestCase> tests;
    return tests;
}

inline int& Failures() {
    static int failures = 0;
    return failures;
}

struct Registrar {
    Registrar(const char* name, std::function<void()> body) { Registry().push_back({name, std::move(body)}); }
};

#define HF_CONCAT2(a, b) a##b
#define HF_CONCAT(a, b) HF_CONCAT2(a, b)
#define TEST(name)                                                                  \
    static void HF_CONCAT(test_fn_, __LINE__)();                                    \
    static hftest::Registrar HF_CONCAT(test_reg_, __LINE__)(name, HF_CONCAT(test_fn_, __LINE__)); \
    static void HF_CONCAT(test_fn_, __LINE__)()

#define EXPECT(cond)                                                                    \
    do {                                                                                \
        if (!(cond)) {                                                                  \
            std::printf("  FALLÓ %s:%d: %s\n", __FILE__, __LINE__, #cond);              \
            ++hftest::Failures();                                                       \
        }                                                                               \
    } while (0)

#define EXPECT_EQ(a, b)                                                                              \
    do {                                                                                             \
        const auto va = (a);                                                                         \
        const auto vb = (b);                                                                         \
        if (!(va == vb)) {                                                                           \
            std::printf("  FALLÓ %s:%d: %s == %s (%lld vs %lld)\n", __FILE__, __LINE__, #a, #b,      \
                        static_cast<long long>(va), static_cast<long long>(vb));                     \
            ++hftest::Failures();                                                                    \
        }                                                                                            \
    } while (0)

inline int RunAll() {
    for (const TestCase& test : Registry()) {
        const int before = Failures();
        test.body();
        std::printf("%s %s\n", Failures() == before ? "[ok]  " : "[FALLA]", test.name);
    }
    std::printf("\n%zu tests, %d fallas\n", Registry().size(), Failures());
    return Failures() == 0 ? 0 : 1;
}

inline std::string ReadFile(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

inline const std::vector<std::string>& ScenarioIds() {
    static const std::vector<std::string> ids = {"islas_volcanicas", "colonia_marte", "andes_valles", "ciudad_flotante"};
    return ids;
}

inline hf::Scenario LoadScenario(const std::string& id) {
    hf::Scenario scenario;
    std::string error;
    const std::string text = ReadFile(std::string(HF_SCENARIO_DIR) + "/" + id + ".json");
    if (!hf::LoadScenarioFromJson(text, scenario, error)) {
        std::printf("No se pudo cargar %s: %s\n", id.c_str(), error.c_str());
    }
    return scenario;
}

inline std::vector<std::string> Names(int count) {
    std::vector<std::string> names;
    for (int i = 0; i < count; ++i) names.push_back("Jugador " + std::to_string(i + 1));
    return names;
}

// Aplica la acción del bot; devuelve false si el bot propuso algo ilegal.
inline bool BotStep(hf::Game& game, int& actionsThisTurn, hf::ApplyResult* out = nullptr) {
    const int before = game.CurrentPlayer();
    const hf::Action action = hf::Bot::ChooseAction(game, actionsThisTurn);
    hf::ApplyResult result = game.Apply(action);
    if (!result.ok) {
        std::printf("  acción ilegal del bot (%s): %s\n", hf::ActionTypeName(action.type), result.error.c_str());
        return false;
    }
    actionsThisTurn = game.CurrentPlayer() == before ? actionsThisTurn + 1 : 0;
    if (out) *out = std::move(result);
    return true;
}

// Termina la preparación con bots.
inline bool FinishSetup(hf::Game& game) {
    int actions = 0;
    while (game.GetPhase() == hf::Phase::Setup) {
        if (!BotStep(game, actions)) return false;
    }
    return true;
}

} // namespace hftest
