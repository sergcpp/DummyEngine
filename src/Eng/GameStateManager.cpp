#include "GameStateManager.h"

#include <Sys/Log.h>

GameStateManager::~GameStateManager() {
    Clear();
}

std::shared_ptr<GameState> GameStateManager::Peek() {
    if (states_.empty()) {
        return std::shared_ptr<GameState>();
    } else {
        return states_.back();
    }
}

void GameStateManager::Push(const std::shared_ptr<GameState> &state) {
    if (!states_.empty()) {
        states_.back()->Exit();
    }
    states_.emplace_back(state);
    states_.back()->Enter();
}

std::shared_ptr<GameState> GameStateManager::Pop() {
    if (states_.empty()) {
        throw std::runtime_error("Attempted to pop from an empty game state stack");
    }

    auto popped = states_.back();
    popped->Exit();
    states_.pop_back();
    if (!states_.empty()) {
        states_.back()->Enter();
    }

    return popped;
}

void GameStateManager::PopLater() {
    pop_later_ = true;
}

std::shared_ptr<GameState> GameStateManager::Switch(const std::shared_ptr<GameState> &state) {
    std::shared_ptr<GameState> current_state = Peek();
    if (current_state) {
        Pop();
    }
    Push(state);
    return current_state;
}

void GameStateManager::Clear() {
    while (!states_.empty()) {
        Pop();
    }
}

void GameStateManager::Update(int dt_ms) {
    if (pop_later_) {
        Pop();
        pop_later_ = false;
    }

    auto &st = states_.back();
    st->Update(dt_ms);
}

void GameStateManager::Draw(float dt_s) {
    auto &st = states_.back();
    st->Draw(dt_s);
}

void GameStateManager::HandleInput(InputManager::Event &evt) {
    auto &st = states_.back();
    st->HandleInput(evt);
}
