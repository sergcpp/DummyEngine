
#include <cstdio>

#include "../Gui.h"

void test_signal();
void test_span();
void test_unicode();
void test_widgets();

int main() {
    printf("Gui Version: %s\n", Gui::Version());
    puts(" ---------------");

    test_signal();
    test_span();
    test_unicode();
    test_widgets();
}

