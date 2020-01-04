#pragma once

#include <functional>
#include <memory>

#include "Keycode.h"

struct InputManagerImp;

enum RawInputEvent {
    EvNone,
    EvP1Down, EvP1Up, EvP1Move,
    EvP2Down, EvP2Up, EvP2Move,
    EvKeyDown, EvKeyUp,
    EvResize,
    EvMouseWheel,
    EvCount
};

class InputManager {
    InputManagerImp *imp_;
public:
    struct Event {
        RawInputEvent type = RawInputEvent::EvNone;
        uint32_t key_code;
        struct {
            float x, y;
        } point;
        struct {
            float dx, dy;
        } move;
        uint64_t time_stamp;
    };

    InputManager();
    ~InputManager();
    InputManager(const InputManager &) = delete;
    InputManager &operator=(const InputManager &) = delete;

    void SetConverter(RawInputEvent evt_type, const std::function<void(Event &)> &conv);
    void AddRawInputEvent(Event &evt);
    bool PollEvent(uint64_t time_us, Event &evt);
    void ClearBuffer();

    static char CharFromKeycode(uint32_t key_code);
};

