#include "ViewerStateManager.h"

#include <stdexcept>

Eng::ViewerStateManager::~ViewerStateManager() { Clear(); }

Eng::ViewerState *Eng::ViewerStateManager::Peek() {
    if (states_.empty()) {
        return {};
    } else {
        return states_.back().get();
    }
}

void Eng::ViewerStateManager::Push(std::unique_ptr<ViewerState> &&state) {
    if (!states_.empty()) {
        states_.back()->Exit();
    }
    states_.emplace_back(std::move(state))->Enter();
}

std::unique_ptr<Eng::ViewerState> Eng::ViewerStateManager::Pop() {
    if (states_.empty()) {
        throw std::runtime_error("Attempted to pop from an empty game state stack");
    }

    auto popped = std::move(states_.back());
    popped->Exit();
    states_.pop_back();
    if (!states_.empty()) {
        states_.back()->Enter();
    }

    return popped;
}

void Eng::ViewerStateManager::PopLater() { pop_later_ = true; }

Eng::ViewerState *Eng::ViewerStateManager::Switch(std::unique_ptr<ViewerState> &&state) {
    ViewerState *current_state = Peek();
    if (current_state) {
        Pop();
    }
    Push(std::move(state));
    return current_state;
}

void Eng::ViewerStateManager::Clear() {
    while (!states_.empty()) {
        Pop();
    }
}

void Eng::ViewerStateManager::UpdateFixed(uint64_t dt_us) {
    if (pop_later_) {
        Pop();
        pop_later_ = false;
    }
    states_.back()->UpdateFixed(dt_us);
}

void Eng::ViewerStateManager::UpdateAnim(uint64_t dt_us) { states_.back()->UpdateAnim(dt_us); }

void Eng::ViewerStateManager::Draw() { states_.back()->Draw(); }

void Eng::ViewerStateManager::HandleInput(const input_event_t &evt, const std::vector<bool> &keys_state) {
    states_.back()->HandleInput(evt, keys_state);
}
