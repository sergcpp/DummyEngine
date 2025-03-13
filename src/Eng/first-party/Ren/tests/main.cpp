
#include <cstdio>

#include "../Fwd.h"

void test_anim();
void test_buffer();
void test_freelist_alloc();
void test_hashmap();
void test_material();
void test_math();
void test_mesh();
void test_program();
void test_storage();
void test_small_vector();
void test_span();
void test_sparse_array();
void test_string();
void test_utils();

int main() {
    printf("Ren Version: %s\n", Ren::Version());
    puts(" ---------------");

    test_anim();
    test_buffer();
    test_freelist_alloc();
    test_hashmap();
    test_material();
    test_math();
    test_mesh();
    test_program();
    test_storage();
    test_small_vector();
    test_span();
    test_sparse_array();
    test_string();
    test_utils();
}