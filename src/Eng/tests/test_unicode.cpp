#include "test_common.h"

#include "../gui/Utils.h"

void test_unicode() {
    printf("Test unicode            | ");

    const char test_str1[] = u8"z\u6c34\U0001d10b";

    { // utf8 -> unicode
        int pos = 0;
        uint32_t unicode[4];

        pos += Gui::ConvChar_UTF8_to_Unicode(&test_str1[pos], unicode[0]);
        pos += Gui::ConvChar_UTF8_to_Unicode(&test_str1[pos], unicode[1]);
        pos += Gui::ConvChar_UTF8_to_Unicode(&test_str1[pos], unicode[2]);
        pos += Gui::ConvChar_UTF8_to_Unicode(&test_str1[pos], unicode[3]);

        require(pos == 9);
        require(unicode[0] == 0x7a);
        require(unicode[1] == 0x6c34);
        require(unicode[2] == 0x0001d10b);
        require(unicode[3] == 0);
    }

    { // utf8 -> utf16
        int pos = 0;
        uint16_t utf16[8];

        pos += Gui::ConvChar_UTF8_to_UTF16(&test_str1[pos], &utf16[0]);
        pos += Gui::ConvChar_UTF8_to_UTF16(&test_str1[pos], &utf16[2]);
        pos += Gui::ConvChar_UTF8_to_UTF16(&test_str1[pos], &utf16[4]);
        pos += Gui::ConvChar_UTF8_to_UTF16(&test_str1[pos], &utf16[6]);

        require(pos == 9);
        require(utf16[0] == 0x7a);
        require(utf16[1] == 0);
        require(utf16[2] == 0x6c34);
        require(utf16[3] == 0);
        require(utf16[4] == 0xd834);
        require(utf16[5] == 0xdd0b);
        require(utf16[6] == 0);
        require(utf16[7] == 0);
    }

    printf("OK\n");
}