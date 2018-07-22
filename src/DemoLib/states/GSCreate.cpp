#include "GSCreate.h"

#include <stdexcept>

#include "GSBicubicTest.h"
#include "GSDefTest.h"
#include "GSOccTest.h"

std::shared_ptr<GameState> GSCreate(eGameState state, GameBase *game) {
    if (state == GS_OCC_TEST) {
        return std::make_shared<GSOccTest>(game);
    } else if (state == GS_BICUBIC_TEST) {
        return std::make_shared<GSBicubicTest>(game);
    } else if (state == GS_DEF_TEST) {
        return std::make_shared<GSDefTest>(game);
    }
    throw std::invalid_argument("Unknown game state!");
}