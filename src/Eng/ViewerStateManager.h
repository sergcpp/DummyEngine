#pragma once

#include <memory>
#include <vector>

#include "input/InputManager.h"

namespace Eng {
class ViewerState;
class ViewerStateManager {
    std::vector<std::shared_ptr<ViewerState>> states_;

    bool pop_later_ = false;

  public:
    virtual ~ViewerStateManager();

    std::shared_ptr<ViewerState> Peek();

    void Push(const std::shared_ptr<ViewerState> &state);

    std::shared_ptr<ViewerState> Pop();

    void PopLater();

    std::shared_ptr<ViewerState> Switch(const std::shared_ptr<ViewerState> &state);

    void Clear();

    void UpdateFixed(uint64_t dt_us);
    void UpdateAnim(uint64_t dt_us);

    void Draw();

    void HandleInput(Eng::InputManager::Event &);
};

} // namespace Eng