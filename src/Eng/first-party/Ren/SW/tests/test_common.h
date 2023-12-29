#ifndef TEST_COMMON_H
#define TEST_COMMON_H

#include <stdio.h>
#include <stdlib.h>

static void handle_assert(int passed, const char* assert, const char* file, long line) {
    if (!passed) {
        printf("Assertion failed %s in %s at line %i\n", assert, file, (int)line);
        exit(-1);
    }
}

#define require(x) handle_assert((x) != 0, #x , __FILE__, __LINE__ )


#endif
