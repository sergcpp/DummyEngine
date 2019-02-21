#pragma once

#include "TimedInput.h"

class GameState {
public:
    virtual ~GameState() {}

    // Call every time we enter this state
    virtual void Enter() {};

    // Call every time we exit this state
    virtual void Exit() {};

    // Drawing
    virtual void Draw(float /*dt_s*/) {};

    // Called UPDATE_RATE times per second (usually 60)
    virtual void Update(int /*dt_ms*/) {};

    virtual void HandleInput(InputManager::Event) {};
};
