#include "ViewerStateManager.h"

#include "ViewerState.h"

#include <stdexcept>

Eng::ViewerStateManager::~ViewerStateManager() {
    Clear();
}

std::shared_ptr<Eng::ViewerState> Eng::ViewerStateManager::Peek() {
    if (states_.empty()) {
        return {};
    } else {
        return states_.back();
    }
}

void Eng::ViewerStateManager::Push(const std::shared_ptr<ViewerState> &state) {
    if (!states_.empty()) {
        states_.back()->Exit();
    }
    states_.emplace_back(state);
    states_.back()->Enter();
}

std::shared_ptr<Eng::ViewerState> Eng::ViewerStateManager::Pop() {
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

void Eng::ViewerStateManager::PopLater() {
    pop_later_ = true;
}

std::shared_ptr<Eng::ViewerState> Eng::ViewerStateManager::Switch(const std::shared_ptr<ViewerState> &state) {
    std::shared_ptr<ViewerState> current_state = Peek();
    if (current_state) {
        Pop();
    }
    Push(state);
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

    std::shared_ptr<ViewerState> &st = states_.back();
    st->UpdateFixed(dt_us);
}

void Eng::ViewerStateManager::UpdateAnim(uint64_t dt_us) {
    std::shared_ptr<ViewerState> &st = states_.back();
    st->UpdateAnim(dt_us);
}

void Eng::ViewerStateManager::Draw() {
    std::shared_ptr<ViewerState> &st = states_.back();
    st->Draw();
}

void Eng::ViewerStateManager::HandleInput(Eng::InputManager::Event &evt) {
    std::shared_ptr<ViewerState> &st = states_.back();
    st->HandleInput(evt);
}
