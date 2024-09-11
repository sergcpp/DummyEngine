#pragma once

#include "input/InputManager.h"

namespace Eng {
class ViewerState {
  public:
    virtual ~ViewerState() {}

    // Call every time we enter this state
    virtual void Enter() {}

    // Call every time we exit this state
    virtual void Exit() {}

    // Drawing
    virtual void Draw() {}

    // Called UPDATE_RATE times per second (usually 60)
    virtual void UpdateFixed(uint64_t /*dt_us*/) {}

    // Called once per frame with delta time from last frame
    virtual void UpdateAnim(const uint64_t dt_us) {}

    virtual bool HandleInput(const input_event_t &, const std::vector<bool> &) { return false; }
};
} // namespace Eng