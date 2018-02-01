#pragma once

#include <memory>

enum eGameState { GS_OCC_TEST };

class GameBase;
class GameState;

std::shared_ptr<GameState> GSCreate(eGameState state, GameBase *game);