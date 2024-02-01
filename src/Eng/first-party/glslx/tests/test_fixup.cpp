#include "test_common.h"

#include <sstream>

#include "../Fixup.h"
#include "../WriterGLSL.h"

void test_fixup() {
    printf("Test fixup              | ");

    { // for loop counters rewrite
        static const char source[] = "void test() {\n"
                                     "    for (int i = 0; i < 16; i += 8) {\n"
                                     "        for (int j = 0; j < 16; ++j) {\n"
                                     "        }\n"
                                     "    }\n"
                                     "    for (int i = 0, j = 0, k = 0; i < 16; i += 8, ++j, --k) {\n"
                                     "    }\n"
                                     "}\n";
        static const char *expected = "void test() {\n"
                                      "    for (int i_0 = 0; (i_0 < 16); i_0 += 8) {\n"
                                      "        for (int j_1 = 0; (j_1 < 16); ++j_1) {\n"
                                      "        }\n"
                                      "    }\n"
                                      "    for (int i_2 = 0;int j_3 = 0;int k_4 = 0; (i_2 < 16); ((i_2 += 8, ++j_3), --k_4)) {\n"
                                      "    }\n"
                                      "}\n";

        glslx::Parser parser(source, "loop_counters.glsl");
        std::unique_ptr<glslx::TrUnit> tr_unit = parser.Parse(glslx::eTrUnitType::Compute);
        require_fatal(tr_unit != nullptr);

        glslx::Fixup().Apply(tr_unit.get());

        std::stringstream ss;
        glslx::WriterGLSL().Write(tr_unit.get(), ss);
        require(ss.str() == expected);
    }

    printf("OK\n");
}