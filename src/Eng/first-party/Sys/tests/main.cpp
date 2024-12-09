
#include <cstdio>

#include "../Sys.h"

void test_alloc();
void test_async_file();
void test_inplace_function();
void test_json();
void test_scope_exit();
void test_small_vector();
void test_thread_pool();

int main() {
    printf("Sys Version: %s\n", Sys::Version());
    puts(" ---------------");

    test_alloc();
    test_async_file();
    test_inplace_function();
    test_json();
    test_scope_exit();
    test_small_vector();
    test_thread_pool();
}

