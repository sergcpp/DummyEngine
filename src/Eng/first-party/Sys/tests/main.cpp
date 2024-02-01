
#include <cstdio>

void test_alloc();
void test_async_file();
void test_inplace_function();
void test_json();
void test_pack();
void test_scope_exit();
void test_signal();
void test_thread_pool();
void test_vector();

int main() {
    test_alloc();
    test_async_file();
    test_inplace_function();
    test_json();
    //test_pack();
    test_scope_exit();
    test_signal();
    test_thread_pool();
    test_vector();
    puts("OK");
}

