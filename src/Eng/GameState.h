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
    virtual void Draw(uint64_t /*dt_us*/) {};

    // Called UPDATE_RATE times per second (usually 60)
    virtual void Update(uint64_t /*dt_us*/) {};

    virtual void HandleInput(const InputManager::Event &) {};
};
