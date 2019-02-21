#pragma once

#include <functional>
#include <memory>

struct InputManagerImp;

class InputManager {
    InputManagerImp *imp_;
public:
    enum RawInputButton {
        RAW_INPUT_BUTTON_UP,
        RAW_INPUT_BUTTON_DOWN,
        RAW_INPUT_BUTTON_LEFT,
        RAW_INPUT_BUTTON_RIGHT,
        RAW_INPUT_BUTTON_EXIT,
        RAW_INPUT_BUTTON_RETURN,
        RAW_INPUT_BUTTON_BACKSPACE,
        RAW_INPUT_BUTTON_SHIFT,
        RAW_INPUT_BUTTON_DELETE,
        RAW_INPUT_BUTTON_TAB,
        RAW_INPUT_BUTTON_SPACE,
        RAW_INPUT_BUTTON_OTHER,
    };

    enum RawInputEvent {
        RAW_INPUT_NONE,
        RAW_INPUT_P1_DOWN,
        RAW_INPUT_P1_UP,
        RAW_INPUT_P1_MOVE,
        RAW_INPUT_P2_DOWN,
        RAW_INPUT_P2_UP,
        RAW_INPUT_P2_MOVE,
        RAW_INPUT_KEY_DOWN,
        RAW_INPUT_KEY_UP,
        RAW_INPUT_RESIZE,
        RAW_INPUT_MOUSE_WHEEL,
        NUM_EVENTS
    };

    struct Event {
        RawInputEvent type;
        RawInputButton key;
        int raw_key;
        struct {
            float x, y;
        } point;
        struct {
            float dx, dy;
        } move;
        unsigned int time_stamp;
    };

    InputManager();
    ~InputManager();
    InputManager(const InputManager &) = delete;
    InputManager &operator=(const InputManager &) = delete;

    void SetConverter(RawInputEvent evt_type, const std::function<void(Event &)> &conv);
    void AddRawInputEvent(Event &evt);
    bool PollEvent(unsigned int time, Event &evt);
    void ClearBuffer();
};

