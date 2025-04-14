#include "test_common.h"

#include "../parser/Lexer.h"

namespace glslx {
extern const keyword_info_t g_keywords[];
extern const int g_keywords_count;

int lookup_keyword(const char *name);
}

void test_lexer() {
    using namespace glslx;

    printf("Test lexer              | ");

    { // keywords lookup
        for (int i = 0; i < g_keywords_count; ++i) {
            const int index = lookup_keyword(g_keywords[i].name);
            require(index == i);
        }
    }

    printf("OK\n");
}