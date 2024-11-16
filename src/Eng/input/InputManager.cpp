#include "InputManager.h"

namespace Eng {
struct InputManagerImp {
    std::function<input_event_t(const input_event_t &)> input_converters[int(eInputEvent::_Count)];
    std::deque<input_event_t> input_buffer;
    std::vector<bool> keys_state;
};
} // namespace Eng

Eng::InputManager::InputManager() {
    imp_ = std::make_unique<InputManagerImp>();
    imp_->keys_state.resize(256, false);
}

Eng::InputManager::~InputManager() = default;

const std::vector<bool> &Eng::InputManager::keys_state() const { return imp_->keys_state; }

const std::deque<Eng::input_event_t> &Eng::InputManager::peek_events() const { return imp_->input_buffer; }

void Eng::InputManager::SetConverter(const eInputEvent evt_type,
                                     const std::function<input_event_t(const input_event_t &)> &conv) {
    imp_->input_converters[int(evt_type)] = conv;
}

void Eng::InputManager::AddRawInputEvent(input_event_t evt) {
    if (imp_->input_buffer.size() > 100) {
        return;
    }
    auto conv = imp_->input_converters[int(evt.type)];
    if (conv) {
        evt = conv(evt);
    }
    imp_->input_buffer.push_back(evt);
}

bool Eng::InputManager::PollEvent(const uint64_t time_us, input_event_t &evt) {
    if (imp_->input_buffer.empty()) {
        return false;
    } else {
        evt = imp_->input_buffer.front();
        if (evt.time_stamp <= time_us) {
            imp_->input_buffer.pop_front();
            if (evt.type == eInputEvent::KeyDown) {
                if (evt.key_code < imp_->keys_state.size()) {
                    imp_->keys_state[evt.key_code] = true;
                }
            } else if (evt.type == eInputEvent::KeyUp) {
                if (evt.key_code < imp_->keys_state.size()) {
                    imp_->keys_state[evt.key_code] = false;
                }
            }
            return true;
        } else {
            return false;
        }
    }
}

void Eng::InputManager::ClearBuffer() { imp_->input_buffer.clear(); }

char Eng::InputManager::CharFromKeycode(const uint32_t key_code) {
    if (key_code >= Eng::eKey::A && key_code <= Eng::eKey::Z) {
        return 'a' + char(key_code - Eng::eKey::A);
    } else if (key_code >= Eng::eKey::_1 && key_code <= Eng::eKey::_9) {
        return '1' + char(key_code - Eng::eKey::_1);
    } else if (key_code == Eng::eKey::_0) {
        return '0';
    } else if (key_code == Eng::eKey::Minus) {
        return '-';
    } else if (key_code == Eng::eKey::Space) {
        return ' ';
    } else if (key_code == Eng::eKey::Period) {
        return '.';
    }
    return 0;
}