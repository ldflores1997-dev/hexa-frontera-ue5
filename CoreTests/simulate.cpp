// Simula muchas partidas entre bots para revisar el equilibrio de cada escenario.
// Uso: hf_simulate [partidas_por_configuración]
#include <cstdlib>

#include "TestUtil.h"

int main(int argc, char** argv) {
    const int matches = argc > 1 ? std::atoi(argv[1]) : 200;
    for (const std::string& id : hftest::ScenarioIds()) {
        const hf::Scenario scenario = hftest::LoadScenario(id);
        for (int players = 2; players <= 4; ++players) {
            int finished = 0;
            long totalRounds = 0;
            std::vector<int> wins(static_cast<size_t>(players), 0);
            std::vector<int> contractClaims(scenario.contracts.size(), 0);
            for (int seed = 1; seed <= matches; ++seed) {
                hf::Game game;
                std::string error;
                game.Init(scenario, hftest::Names(players), static_cast<uint64_t>(seed), error);
                int actions = 0;
                while (game.GetPhase() != hf::Phase::Finished && game.Round() < 200) {
                    if (!hftest::BotStep(game, actions)) break;
                }
                if (game.GetPhase() != hf::Phase::Finished) continue;
                ++finished;
                totalRounds += game.Round();
                ++wins[static_cast<size_t>(game.Winner())];
                for (size_t c = 0; c < contractClaims.size(); ++c)
                    if (game.ContractOwners()[c] >= 0) ++contractClaims[c];
            }
            std::printf("%-18s %d jugadores: %d/%d terminadas, %.1f rondas promedio, victorias por asiento:", id.c_str(),
                        players, finished, matches, finished ? static_cast<double>(totalRounds) / finished : 0.0);
            for (int w : wins) std::printf(" %d", w);
            std::printf(" | contratos cumplidos:");
            for (int c : contractClaims) std::printf(" %d", c);
            std::printf("\n");
        }
    }
    return 0;
}
