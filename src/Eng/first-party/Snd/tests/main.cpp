
#include <cstdio>

#include "../Context.h"

void test_hashmap();
void test_hashset();
void test_span();

int main() {
    printf("Snd Version: %s\n", Snd::Version());
    puts(" ---------------");

    test_hashset();
    test_hashmap();
    test_span();
}