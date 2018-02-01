#include "GSCreate.h"

#include <stdexcept>

#include "GSOccTest.h"

std::shared_ptr<GameState> GSCreate(eGameState state, GameBase *game) {
    if (state == GS_OCC_TEST) {
        return std::make_shared<GSOccTest>(game);
    }
    throw std::invalid_argument("Unknown game state!");
}