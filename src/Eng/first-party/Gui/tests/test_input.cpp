#include "test_common.h"

#include <string_view>

#include "../Input.h"

void test_input() {
    using namespace Gui;

    printf("Test input              | ");

    require(CharFromKeycode(eKey::A) == 'a');
    require(CharFromKeycode(eKey::Z) == 'z');
    require(CharFromKeycode(eKey::_1) == '1');
    require(CharFromKeycode(eKey::_9) == '9');
    require(CharFromKeycode(eKey::_0) == '0');
    require(CharFromKeycode(eKey::Space) == ' ');
    require(CharFromKeycode(eKey::Minus) == '-');
    require(CharFromKeycode(eKey::Equals) == '=');
    require(CharFromKeycode(eKey::LeftBracket) == '[');
    require(CharFromKeycode(eKey::RightBracket) == ']');
    require(CharFromKeycode(eKey::Backslash) == '\\');
    require(CharFromKeycode(eKey::KeyNonUSBackslash) == '\\');
    require(CharFromKeycode(eKey::Semicolon) == ';');
    require(CharFromKeycode(eKey::Quote) == '\'');
    require(CharFromKeycode(eKey::Comma) == ',');
    require(CharFromKeycode(eKey::Period) == '.');
    require(CharFromKeycode(eKey::Slash) == '/');

    { // File names are typed as key sequences
        const uint32_t key_seq[] = {eKey::E,   eKey::N, eKey::V, eKey::Slash, eKey::M, eKey::E,
                                    eKey::A,   eKey::D, eKey::O, eKey::W,     eKey::_2, eKey::K,
                                    eKey::Period, eKey::D, eKey::D, eKey::S};
        char typed[17] = {};
        for (int i = 0; i < int(sizeof(key_seq) / sizeof(key_seq[0])); ++i) {
            typed[i] = CharFromKeycode(key_seq[i]);
        }
        require(std::string_view(typed) == "env/meadow2k.dds");
    }

    { // Non printable keys produce nothing
        require(CharFromKeycode(eKey::None) == 0);
        require(CharFromKeycode(eKey::Grave) == 0);
        require(CharFromKeycode(eKey::Up) == 0);
        require(CharFromKeycode(eKey::LeftShift) == 0);
    }

    printf("OK\n");
}
