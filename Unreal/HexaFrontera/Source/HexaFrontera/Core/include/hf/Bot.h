// Bot simple por reglas: elige la próxima acción del jugador actual.
#pragma once

#include "hf/Game.h"

namespace hf {

class Bot {
public:
    // Devuelve una acción legal para el jugador actual. `actionsThisTurn` evita
    // que el bot comercie sin fin: pasado cierto número termina el turno.
    static Action ChooseAction(const Game& game, int actionsThisTurn);

    // Valor estimado de un vértice para colocar un puesto.
    static double ScoreVertex(const Game& game, int player, int vertex);
};

} // namespace hf
