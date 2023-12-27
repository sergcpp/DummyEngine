#include "GameStateManager.h"

#include <stdexcept>

Eng::GameStateManager::~GameStateManager() {
    Clear();
}

std::shared_ptr<Eng::GameState> Eng::GameStateManager::Peek() {
    if (states_.empty()) {
        return {};
    } else {
        return states_.back();
    }
}

void Eng::GameStateManager::Push(const std::shared_ptr<GameState> &state) {
    if (!states_.empty()) {
        states_.back()->Exit();
    }
    states_.emplace_back(state);
    states_.back()->Enter();
}

std::shared_ptr<Eng::GameState> Eng::GameStateManager::Pop() {
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

void Eng::GameStateManager::PopLater() {
    pop_later_ = true;
}

std::shared_ptr<Eng::GameState> Eng::GameStateManager::Switch(const std::shared_ptr<GameState> &state) {
    std::shared_ptr<GameState> current_state = Peek();
    if (current_state) {
        Pop();
    }
    Push(state);
    return current_state;
}

void Eng::GameStateManager::Clear() {
    while (!states_.empty()) {
        Pop();
    }
}

void Eng::GameStateManager::UpdateFixed(uint64_t dt_us) {
    if (pop_later_) {
        Pop();
        pop_later_ = false;
    }

    std::shared_ptr<GameState> &st = states_.back();
    st->UpdateFixed(dt_us);
}

void Eng::GameStateManager::UpdateAnim(uint64_t dt_us) {
    std::shared_ptr<GameState> &st = states_.back();
    st->UpdateAnim(dt_us);
}

void Eng::GameStateManager::Draw() {
    std::shared_ptr<GameState> &st = states_.back();
    st->Draw();
}

void Eng::GameStateManager::HandleInput(Eng::InputManager::Event &evt) {
    std::shared_ptr<GameState> &st = states_.back();
    st->HandleInput(evt);
}
