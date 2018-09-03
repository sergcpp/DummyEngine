#include "GSCreate.h"

#include <stdexcept>

#include "GSBicubicTest.h"
#include "GSDefTest.h"
#include "GSDrawTest.h"
#include "GSIKTest.h"
#include "GSOccTest.h"

std::shared_ptr<GameState> GSCreate(eGameState state, GameBase *game) {
    if (state == GS_OCC_TEST) {
        return std::make_shared<GSOccTest>(game);
    } else if (state == GS_BICUBIC_TEST) {
        return std::make_shared<GSBicubicTest>(game);
    } else if (state == GS_DEF_TEST) {
        return std::make_shared<GSDefTest>(game);
    } else if (state == GS_IK_TEST) {
        return std::make_shared<GSIKTest>(game);
    } else if (state == GS_DRAW_TEST) {
        return std::make_shared<GSDrawTest>(game);
    }
    throw std::invalid_argument("Unknown game state!");
}