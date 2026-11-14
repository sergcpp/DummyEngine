
#include <cstdio>

#include "../Phy.h"

void test_bvhsplit();
void test_dyn_bvh();
void test_math();
void test_span();
void test_svol();

int main() {
    printf("Phy Version: %s\n", Phy::Version());
    puts(" ---------------");

    test_math();
    test_span();
    test_svol();
    test_bvhsplit();
    test_dyn_bvh();
}

