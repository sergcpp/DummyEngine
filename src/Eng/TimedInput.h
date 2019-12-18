#pragma once

#include <functional>
#include <memory>

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

enum RawInputButton {
    BtnUp,
    BtnDown,
    BtnLeft,
    BtnRight,
    BtnExit,
    BtnReturn,
    BtnBackspace,
    BtnShift,
    BtnDelete,
    BtnTab,
    BtnSpace,
    BtnOther,
};

class InputManager {
    InputManagerImp *imp_;
public:
    struct Event {
        RawInputEvent type = RawInputEvent::EvNone;
        RawInputButton key;
        int raw_key;
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
};

