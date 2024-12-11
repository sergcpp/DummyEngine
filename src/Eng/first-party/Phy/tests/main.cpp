
#include <cstdio>

#include "../Phy.h"

void test_mat();
void test_span();
void test_svol();
void test_vec();

int main() {
    printf("Phy Version: %s\n", Phy::Version());
    puts(" ---------------");

    test_mat();
    test_vec();
    test_span();
    test_svol();
}

