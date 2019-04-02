
#include <cstdio>

void test_alloc();
void test_json();
void test_optional();
void test_pack();
void test_signal();

int main() {
    test_alloc();
    test_json();
    test_optional();
    //test_pack();
    test_signal();
    puts("OK");
}

