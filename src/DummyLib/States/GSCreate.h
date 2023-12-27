#pragma once

#include <memory>

enum class eGameState {
    GS_OCC_TEST,
    GS_DRAW_TEST,
    GS_PHY_TEST,
    GS_PLAY_TEST,
    GS_UI_TEST,
    GS_UI_TEST2,
    GS_UI_TEST3,
    GS_UI_TEST4,
    GS_VIDEO_TEST
};

namespace Eng {
class GameBase;
class GameState;
} // namespace Eng

std::shared_ptr<Eng::GameState> GSCreate(eGameState state, Eng::GameBase *game);