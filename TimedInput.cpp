#include "TimedInput.h"

#include <queue>

struct InputManagerImp {
    std::function<void(InputManager::Event &)> input_converters[InputManager::NUM_EVENTS];
    std::queue<InputManager::Event> input_buffer;
};

InputManager::InputManager() {
    imp_ = new InputManagerImp();
}

InputManager::~InputManager() {
    delete imp_;
}

void InputManager::SetConverter(RawInputEvent evt_type, const std::function<void(Event &)> &conv) {
    imp_->input_converters[evt_type] = conv;
}

void InputManager::AddRawInputEvent(Event &evt) {
    if (imp_->input_buffer.size() > 10) {
        return;
    }
    auto conv = imp_->input_converters[evt.type];
    if (conv) {
        conv(evt);
    }
    imp_->input_buffer.push(evt);
}

bool InputManager::PollEvent(unsigned int time, Event &evt) {
    if (imp_->input_buffer.empty()) {
        return false;
    } else {
        evt = imp_->input_buffer.front();
        if (evt.time_stamp <= time) {
            imp_->input_buffer.pop();
            return true;
        } else {
            return false;
        }
    }
}

void InputManager::ClearBuffer() {
    while (imp_->input_buffer.size()) {
        imp_->input_buffer.pop();
    }
}