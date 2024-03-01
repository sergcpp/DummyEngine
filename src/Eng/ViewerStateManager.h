#pragma once

#include <memory>
#include <vector>

#include "input/InputManager.h"
#include "ViewerState.h"

namespace Eng {
class ViewerStateManager {
    std::vector<std::unique_ptr<ViewerState>> states_;

    bool pop_later_ = false;

  public:
    virtual ~ViewerStateManager();

    ViewerState *Peek();

    void Push(std::unique_ptr<ViewerState> &&state);

    std::unique_ptr<ViewerState> Pop();

    void PopLater();

    ViewerState *Switch(std::unique_ptr<ViewerState> &&state);

    void Clear();

    void UpdateFixed(uint64_t dt_us);
    void UpdateAnim(uint64_t dt_us);

    void Draw();

    void HandleInput(Eng::InputManager::Event &);
};

} // namespace Eng