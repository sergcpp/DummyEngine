#pragma once

#include <memory>
#include <vector>

#include "GameState.h"
#include "TimedInput.h"

class GameState;

class GameStateManager {
    std::vector<std::shared_ptr<GameState>> states_;

    bool pop_later_ = false;
public:
    virtual ~GameStateManager();

    std::shared_ptr<GameState> Peek();

    void Push(const std::shared_ptr<GameState> &state);

    std::shared_ptr<GameState> Pop();

    void PopLater();

    std::shared_ptr<GameState> Switch(const std::shared_ptr<GameState> &state);

    void Clear();

    void Update(int dt_ms);

    void Draw(float dt_s);

    void HandleInput(InputManager::Event &);
};

