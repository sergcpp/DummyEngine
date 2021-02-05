#include "GSCreate.h"

#include <stdexcept>

#include "GSDrawTest.h"
#include "GSPhyTest.h"
#include "GSPlayTest.h"
#include "GSUITest.h"
#include "GSUITest2.h"
#include "GSUITest3.h"
#include "GSUITest4.h"
#include "GSVideoTest.h"

std::shared_ptr<GameState> GSCreate(eGameState state, GameBase *game) {
    if (state == eGameState::GS_DRAW_TEST) {
        return std::make_shared<GSDrawTest>(game);
    } else if (state == eGameState::GS_PHY_TEST) {
        return std::make_shared<GSPhyTest>(game);
    } else if (state == eGameState::GS_PLAY_TEST) {
        return std::make_shared<GSPlayTest>(game);
    } else if (state == eGameState::GS_UI_TEST) {
        return std::make_shared<GSUITest>(game);
    } else if (state == eGameState::GS_UI_TEST2) {
        return std::make_shared<GSUITest2>(game);
    } else if (state == eGameState::GS_UI_TEST3) {
        return std::make_shared<GSUITest3>(game);
    } else if (state == eGameState::GS_UI_TEST4) {
        return std::make_shared<GSUITest4>(game);
    } else if (state == eGameState::GS_VIDEO_TEST) {
        return std::make_shared<GSVideoTest>(game);
    }
    throw std::invalid_argument("Unknown game state!");
}