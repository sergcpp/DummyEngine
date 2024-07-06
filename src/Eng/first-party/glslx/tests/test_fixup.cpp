#include "test_common.h"

#include <sstream>

#include "../Fixup.h"
#include "../WriterGLSL.h"

void test_fixup() {
    printf("Test fixup              | ");

    static const char source[] =
        "#extension GL_EXT_control_flow_attributes : require\n"
        "void test(const int a) {\n"
        "    for (int i = 0; i < 16; i += 8) {\n"
        "        for (int j = 0; j < 16; ++j) {\n"
        "        }\n"
        "    }\n"
        "    [[unroll, dependency_infinite]] for (int i = 0, j = 0, k = 0; i < 16; i += 8, ++j, --k) {\n"
        "    }\n"
        "    const int var1 = 42 + 3;\n"
        "    const int var2 = var1 + 2;\n"
        "    const int var3 = var1 + a;\n"
        "    [[flatten]] if (var1 < 10) {\n"
        "        return;\n"
        "    }\n"
        "}\n"
        "void main() {\n"
        "    gl_Position = vec4(0.0, 0.0, 1.0, 1.0);\n"
        "}\n";

    { // for loop counters rewrite
        static const char *expected = "#extension GL_EXT_control_flow_attributes : require\n"
                                      "void test(const int a) {\n"
                                      "    for (int i_0 = 0; (i_0 < 16); i_0 += 8) {\n"
                                      "        for (int j_1 = 0; (j_1 < 16); ++j_1) {\n"
                                      "        }\n"
                                      "    }\n"
                                      "    [[unroll, dependency_infinite]] for (int i_2 = 0;int j_3 = 0;int k_4 = 0; "
                                      "(i_2 < 16); ((i_2 += 8, ++j_3), --k_4)) {\n"
                                      "    }\n"
                                      "    const int var1 = (42 + 3);\n"
                                      "    const int var2 = (var1 + 2);\n"
                                      "    const int var3 = (var1 + a);\n"
                                      "    [[flatten]] if ((var1 < 10)) {\n"
                                      "        return;\n"
                                      "    }\n"
                                      "}\n"
                                      "void main() {\n"
                                      "    gl_Position = vec4(0.0, 0.0, 1.0, 1.0);\n"
                                      "}\n";

        glslx::Parser parser(source, "loop_counters.glsl");
        std::unique_ptr<glslx::TrUnit> tr_unit = parser.Parse(glslx::eTrUnitType::Vertex);
        require_fatal(tr_unit != nullptr);

        glslx::fixup_config_t config;
        config.randomize_loop_counters = true;
        glslx::Fixup(config).Apply(tr_unit.get());

        std::stringstream ss;
        glslx::WriterGLSL().Write(tr_unit.get(), ss);
        require(ss.str() == expected);
    }

    { // const removal
        static const char *expected = "#extension GL_EXT_control_flow_attributes : require\n"
                                      "void test(int a) {\n"
                                      "    for (int i = 0; (i < 16); i += 8) {\n"
                                      "        for (int j = 0; (j < 16); ++j) {\n"
                                      "        }\n"
                                      "    }\n"
                                      "    [[unroll, dependency_infinite]] for (int i = 0;int j = 0;int k = 0; (i < "
                                      "16); ((i += 8, ++j), --k)) {\n"
                                      "    }\n"
                                      "    const int var1 = (42 + 3);\n"
                                      "    const int var2 = (var1 + 2);\n"
                                      "    int var3 = (var1 + a);\n"
                                      "    [[flatten]] if ((var1 < 10)) {\n"
                                      "        return;\n"
                                      "    }\n"
                                      "}\n"
                                      "void main() {\n"
                                      "    gl_Position = vec4(0.0, 0.0, 1.0, 1.0);\n"
                                      "}\n";

        glslx::Parser parser(source, "loop_counters.glsl");
        std::unique_ptr<glslx::TrUnit> tr_unit = parser.Parse(glslx::eTrUnitType::Vertex);
        require_fatal(tr_unit != nullptr);

        glslx::fixup_config_t config;
        config.randomize_loop_counters = false;
        config.remove_const = true;
        glslx::Fixup(config).Apply(tr_unit.get());

        std::stringstream ss;
        glslx::WriterGLSL().Write(tr_unit.get(), ss);
        require(ss.str() == expected);
    }

    { // ctrl flow removal
        static const char *expected = "void test(const int a) {\n"
                                      "    for (int i = 0; (i < 16); i += 8) {\n"
                                      "        for (int j = 0; (j < 16); ++j) {\n"
                                      "        }\n"
                                      "    }\n"
                                      "    for (int i = 0;int j = 0;int k = 0; (i < 16); ((i += 8, ++j), --k)) {\n"
                                      "    }\n"
                                      "    const int var1 = (42 + 3);\n"
                                      "    const int var2 = (var1 + 2);\n"
                                      "    const int var3 = (var1 + a);\n"
                                      "    if ((var1 < 10)) {\n"
                                      "        return;\n"
                                      "    }\n"
                                      "}\n"
                                      "void main() {\n"
                                      "    gl_Position = vec4(0.0, 0.0, 1.0, 1.0);\n"
                                      "}\n";

        glslx::Parser parser(source, "loop_counters.glsl");
        std::unique_ptr<glslx::TrUnit> tr_unit = parser.Parse(glslx::eTrUnitType::Vertex);
        require_fatal(tr_unit != nullptr);

        glslx::fixup_config_t config;
        config.randomize_loop_counters = false;
        config.remove_ctrl_flow_attributes = true;
        glslx::Fixup(config).Apply(tr_unit.get());

        std::stringstream ss;
        glslx::WriterGLSL().Write(tr_unit.get(), ss);
        require(ss.str() == expected);
    }

    { // flip y
        static const char *expected = "#extension GL_EXT_control_flow_attributes : require\n"
                                      "void test(const int a) {\n"
                                      "    for (int i = 0; (i < 16); i += 8) {\n"
                                      "        for (int j = 0; (j < 16); ++j) {\n"
                                      "        }\n"
                                      "    }\n"
                                      "    [[unroll, dependency_infinite]] for (int i = 0;int j = 0;int k = 0; (i < "
                                      "16); ((i += 8, ++j), --k)) {\n"
                                      "    }\n"
                                      "    const int var1 = (42 + 3);\n"
                                      "    const int var2 = (var1 + 2);\n"
                                      "    const int var3 = (var1 + a);\n"
                                      "    [[flatten]] if ((var1 < 10)) {\n"
                                      "        return;\n"
                                      "    }\n"
                                      "}\n"
                                      "void main() {\n"
                                      "    gl_Position = vec4(0.0, 0.0, 1.0, 1.0);\n"
                                      "    gl_Position.y = -gl_Position.y;\n"
                                      "}\n";
        glslx::Parser parser(source, "flip_y.glsl");
        std::unique_ptr<glslx::TrUnit> tr_unit = parser.Parse(glslx::eTrUnitType::Vertex);
        require_fatal(tr_unit != nullptr);

        glslx::fixup_config_t config;
        config.randomize_loop_counters = false;
        config.flip_vertex_y = true;
        glslx::Fixup(config).Apply(tr_unit.get());

        std::stringstream ss;
        glslx::WriterGLSL().Write(tr_unit.get(), ss);
        require(ss.str() == expected);
    }

    printf("OK\n");
}