
#include <atomic>
#include <cstdio>

#include "../glslx.h"

void test_fixup();
void test_hashset();
void test_hashmap();
void test_hlsl_writer();
void test_scope_exit();
void test_small_vector();
void test_span();
void test_lexer();
void test_parser();
void test_pool_alloc();
void test_preprocessor();

bool g_stop_on_fail;
bool g_tests_success{true};

int main() {
    printf("glslx version: %s\n", glslx::Version());
    puts(" ---------------");

    test_scope_exit();
    test_small_vector();
    test_hashset();
    test_hashmap();
    test_span();
    test_pool_alloc();
    test_preprocessor();
    test_lexer();
    test_parser();
    test_fixup();
    test_hlsl_writer();

    return g_tests_success ? 0 : -1;
}
