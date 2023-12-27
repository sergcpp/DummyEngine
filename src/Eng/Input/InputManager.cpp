#include "InputManager.h"

#include <queue>

namespace Eng {
struct InputManagerImp {
    std::function<void(InputManager::Event &)> input_converters[(int)RawInputEv::Count];
    std::queue<InputManager::Event> input_buffer;
};
} // namespace Eng

Eng::InputManager::InputManager() { imp_.reset(new InputManagerImp()); }

Eng::InputManager::~InputManager() = default;

void Eng::InputManager::SetConverter(RawInputEv evt_type, const std::function<void(Event &)> &conv) {
    imp_->input_converters[int(evt_type)] = conv;
}

void Eng::InputManager::AddRawInputEvent(Event &evt) {
    if (imp_->input_buffer.size() > 100) {
        return;
    }
    auto conv = imp_->input_converters[(int)evt.type];
    if (conv) {
        conv(evt);
    }
    imp_->input_buffer.push(evt);
}

bool Eng::InputManager::PollEvent(uint64_t time_us, Event &evt) {
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

void Eng::InputManager::ClearBuffer() {
    while (imp_->input_buffer.size()) {
        imp_->input_buffer.pop();
    }
}

char Eng::InputManager::CharFromKeycode(uint32_t key_code) {
    if (key_code >= KeyA && key_code <= KeyZ) {
        return 'a' + char(key_code - KeyA);
    } else if (key_code >= Key1 && key_code <= Key9) {
        return '1' + char(key_code - Key1);
    } else if (key_code == Key0) {
        return '0';
    } else if (key_code == KeyMinus) {
        return '-';
    } else if (key_code == KeySpace) {
        return ' ';
    } else if (key_code == KeyPeriod) {
        return '.';
    }
    return 0;
}