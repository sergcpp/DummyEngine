#pragma once

#include <memory>

enum class eGameState {
    GS_OCC_TEST, GS_DRAW_TEST, GS_PHY_TEST, GS_PLAY_TEST, GS_UI_TEST, GS_UI_TEST2, GS_UI_TEST3, GS_UI_TEST4, GS_VIDEO_TEST
};

class GameBase;
class GameState;

std::shared_ptr<GameState> GSCreate(eGameState state, GameBase *game);