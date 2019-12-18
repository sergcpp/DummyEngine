#include "TimedInput.h"

#include <queue>

struct InputManagerImp {
    std::function<void(InputManager::Event &)> input_converters[(int)RawInputEvent::EvCount];
    std::queue<InputManager::Event> input_buffer;
};

InputManager::InputManager() {
    imp_ = new InputManagerImp();
}

InputManager::~InputManager() {
    delete imp_;
}

void InputManager::SetConverter(RawInputEvent evt_type, const std::function<void(Event &)> &conv) {
    imp_->input_converters[(int)evt_type] = conv;
}

void InputManager::AddRawInputEvent(Event &evt) {
    if (imp_->input_buffer.size() > 100) {
        return;
    }
    auto conv = imp_->input_converters[(int)evt.type];
    if (conv) {
        conv(evt);
    }
    imp_->input_buffer.push(evt);
}

bool InputManager::PollEvent(uint64_t time_us, Event &evt) {
    if (imp_->input_buffer.empty()) {
        return false;
    } else {
        evt = imp_->input_buffer.front();
        if (evt.time_stamp <= time_us) {
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