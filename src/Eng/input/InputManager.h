#pragma once

#include <functional>
#include <memory>

#include <Gui/Input.h>

namespace Eng {
struct InputManagerImp;

using Gui::eInputEvent;
using Gui::eKey;
using Gui::input_event_t;

class InputManager {
    std::unique_ptr<InputManagerImp> imp_;

  public:
    InputManager();
    ~InputManager();
    InputManager(const InputManager &) = delete;
    InputManager &operator=(const InputManager &) = delete;

    const std::vector<bool> &keys_state() const;

    void SetConverter(eInputEvent evt_type, const std::function<input_event_t(const input_event_t &)> &conv);
    void AddRawInputEvent(input_event_t evt);
    bool PollEvent(uint64_t time_us, input_event_t &evt);
    void ClearBuffer();

    static char CharFromKeycode(uint32_t key_code);
};
} // namespace Eng