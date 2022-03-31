
#include <cstdio>

void test_alloc();
void test_async_file();
void test_inplace_function();
void test_json();
void test_optional();
void test_pack();
void test_signal();
void test_vector();

int main() {
    test_alloc();
    test_async_file();
    test_inplace_function();
    test_json();
    test_optional();
    //test_pack();
    test_signal();
    test_vector();
    puts("OK");
}

