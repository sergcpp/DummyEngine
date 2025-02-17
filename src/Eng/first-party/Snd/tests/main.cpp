
#include <cstdio>

#include "../Context.h"

void test_hashmap();
void test_span();

int main() {
    printf("Snd Version: %s\n", Snd::Version());
    puts(" ---------------");

    test_hashmap();
    test_span();
}