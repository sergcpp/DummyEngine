#pragma once

#include <functional>
#include <memory>
#include <deque>

#include <Gui/Input.h>
#include <Ren/Span.h>

namespace Eng {
struct InputManagerImp;

using Gui::eInputEvent;
using Gui::eKey;
using Gui::input_event_t;

using Gui::CharFromKeycode;

class InputManager {
    std::unique_ptr<InputManagerImp> imp_;

  public:
    InputManager();
    ~InputManager();
    InputManager(const InputManager &) = delete;
    InputManager &operator=(const InputManager &) = delete;

    const std::vector<bool> &keys_state() const;
    const std::deque<input_event_t> &peek_events() const;

    void SetConverter(eInputEvent evt_type, const std::function<input_event_t(const input_event_t &)> &conv);
    void AddRawInputEvent(input_event_t evt);
    bool PollEvent(uint64_t time_us, input_event_t &evt);
    void ClearBuffer();
};
} // namespace Eng