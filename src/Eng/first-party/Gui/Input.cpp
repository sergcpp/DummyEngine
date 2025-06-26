#include "Input.h"

char Gui::CharFromKeycode(const uint32_t key_code) {
    if (key_code >= eKey::A && key_code <= eKey::Z) {
        return 'a' + char(key_code - eKey::A);
    } else if (key_code >= eKey::_1 && key_code <= eKey::_9) {
        return '1' + char(key_code - eKey::_1);
    } else if (key_code == eKey::_0) {
        return '0';
    } else if (key_code == eKey::Minus) {
        return '-';
    } else if (key_code == eKey::Space) {
        return ' ';
    } else if (key_code == eKey::Period) {
        return '.';
    }
    return 0;
}
