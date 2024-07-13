
#include <cstdio>

#include "../Context.h"

void test_hashmap();

int main() {
    printf("Snd Version: %s\n", Snd::Version());
    puts(" ---------------");

    test_hashmap();
}