#include "GSCreate.h"

#include <stdexcept>

#include "GSDrawTest.h"
#include "GSUITest.h"

std::shared_ptr<GameState> GSCreate(eGameState state, GameBase *game) {
    if (state == GS_DRAW_TEST) {
        return std::make_shared<GSDrawTest>(game);
    } else if (state == GS_UI_TEST) {
        return std::make_shared<GSUITest>(game);
    }
    throw std::invalid_argument("Unknown game state!");
}