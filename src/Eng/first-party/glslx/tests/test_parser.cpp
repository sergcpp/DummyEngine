#include "test_common.h"

#include <iostream>
#include <sstream>

#include "../WriterGLSL.h"

void test_parser() {
    printf("Test parser             | ");

    { // directives
        static const char source[] = "#version 330 core\n"
                                     "#extension GL_EXT_shader_explicit_arithmetic_types_float16 : require\n";
        static const char *expected = source;

        glslx::Parser parser(source, "directives.glsl");
        std::unique_ptr<glslx::TrUnit> tr_unit = parser.Parse(glslx::eTrUnitType::Compute);
        require_fatal(tr_unit != nullptr);

        std::stringstream ss;
        glslx::WriterGLSL().Write(tr_unit.get(), ss);
        require(ss.str() == expected);
    }
    { // uniforms
        static const char source[] = "uniform float float1;\n"
                                     "uniform float float2 = 10.0f;\n";
        static const char expected[] = "uniform float float1;\n"
                                       "uniform float float2 = 10.0;\n";

        glslx::Parser parser(source, "uniforms.glsl");
        std::unique_ptr<glslx::TrUnit> tr_unit = parser.Parse(glslx::eTrUnitType::Compute);
        require_fatal(tr_unit != nullptr);

        std::stringstream ss;
        glslx::WriterGLSL().Write(tr_unit.get(), ss);
        require(ss.str() == expected);
    }
    { // interface blocks
        static const char source[] = "uniform uniform_block { float x; };\n"
                                     "in input_block { float y; };\n"
                                     "out output_block { float z; };\n"
                                     "buffer buffer_block { float w; };\n"
                                     "\n"
                                     "// same thing with instance names\n"
                                     "uniform uniform_block { float x; } uniform_data;\n"
                                     "in input_block { float y; } input_data;\n"
                                     "out output_block { float z; } output_data;\n"
                                     "buffer buffer_block { float w; } buffer_data;\n";
        static const char *expected = "uniform uniform_block {\n"
                                      "    float x;\n"
                                      "};\n"
                                      "in input_block {\n"
                                      "    float y;\n"
                                      "};\n"
                                      "out output_block {\n"
                                      "    float z;\n"
                                      "};\n"
                                      "buffer buffer_block {\n"
                                      "    float w;\n"
                                      "};\n"
                                      "uniform uniform_block {\n"
                                      "    float x;\n"
                                      "} uniform_data;\n"
                                      "in input_block {\n"
                                      "    float y;\n"
                                      "} input_data;\n"
                                      "out output_block {\n"
                                      "    float z;\n"
                                      "} output_data;\n"
                                      "buffer buffer_block {\n"
                                      "    float w;\n"
                                      "} buffer_data;\n";

        glslx::Parser parser(source, "interface_blocks.glsl");
        std::unique_ptr<glslx::TrUnit> tr_unit = parser.Parse(glslx::eTrUnitType::Compute);
        require_fatal(tr_unit != nullptr);

        std::stringstream ss;
        glslx::WriterGLSL().Write(tr_unit.get(), ss);
        require(ss.str() == expected);
    }
    { // constants
        static const char source[] = "const float test = 1;\n"
                                     "const float test_neg = -1;\n"
                                     "const float test_pos_two_sub = test - test_neg;\n"
                                     "const float test_neg_two_sub = test_neg - test;\n"
                                     "const float test_zero_add = test_neg + test;\n"
                                     "const uint test_uint = (1u << 31);\n"
                                     "const uint test_bitneg = ~test_uint;\n";
        static const char *expected = "const float test = 1;\n"
                                      "const float test_neg = -1;\n"
                                      "const float test_pos_two_sub = 2;\n"
                                      "const float test_neg_two_sub = -2;\n"
                                      "const float test_zero_add = 0;\n"
                                      "const uint test_uint = 2147483648u;\n"
                                      "const uint test_bitneg = 2147483647u;\n";

        glslx::Parser parser(source, "constants.glsl");
        std::unique_ptr<glslx::TrUnit> tr_unit = parser.Parse(glslx::eTrUnitType::Compute);
        require_fatal(tr_unit != nullptr);

        std::stringstream ss;
        glslx::WriterGLSL().Write(tr_unit.get(), ss);
        require(ss.str() == expected);
    }
    { // continuations (comma-separated declarations)
        static const char source[] = "mat4 model, view, projection;\n"
                                     "struct foo { float x; } a, b, c, d;\n";
        static const char *expected = "mat4 model;\n"
                                      "mat4 view;\n"
                                      "mat4 projection;\n"
                                      "struct foo {\n"
                                      "    float x;\n"
                                      "};\n"
                                      "foo a;\n"
                                      "foo b;\n"
                                      "foo c;\n"
                                      "foo d;\n";

        glslx::Parser parser(source, "continuations.glsl");
        std::unique_ptr<glslx::TrUnit> tr_unit = parser.Parse(glslx::eTrUnitType::Compute);
        require_fatal(tr_unit != nullptr);

        std::stringstream ss;
        glslx::WriterGLSL().Write(tr_unit.get(), ss);
        require(ss.str() == expected);
    }
    { // sequence
        static const char source[] = "void test() {\n"
                                     "    1, 2, 3, 4;\n"
                                     "    1, 2, (3, 4);\n"
                                     "    1, (2, 3, 4);\n"
                                     "    (1, 2), 3, 4;\n"
                                     "    (1, 2, 3), 4;\n"
                                     "    (1, 2, (3, 4));\n"
                                     "    (1, (2, 3, 4));\n"
                                     "    ((1, 2), 3, 4);\n"
                                     "    ((1, 2, 3), 4);\n"
                                     "}\n";
        static const char expected[] = "void test() {\n"
                                       "    (((1, 2), 3), 4);\n"
                                       "    ((1, 2), (3, 4));\n"
                                       "    (1, ((2, 3), 4));\n"
                                       "    (((1, 2), 3), 4);\n"
                                       "    (((1, 2), 3), 4);\n"
                                       "    ((1, 2), (3, 4));\n"
                                       "    (1, ((2, 3), 4));\n"
                                       "    (((1, 2), 3), 4);\n"
                                       "    (((1, 2), 3), 4);\n"
                                       "}\n";

        glslx::Parser parser(source, "sequence.glsl");
        std::unique_ptr<glslx::TrUnit> tr_unit = parser.Parse(glslx::eTrUnitType::Compute);
        require_fatal(tr_unit != nullptr);

        std::stringstream ss;
        glslx::WriterGLSL().Write(tr_unit.get(), ss);
        require(ss.str() == expected);
    }
    { // builtin types
        static const char source[] = "bool test_bool;\n"
                                     "int test_int;\n"
                                     "uint test_uint;\n"
                                     "float test_float;\n"
                                     "double test_double;\n"
                                     "vec2 test_vec2;\n"
                                     "vec3 test_vec3;\n"
                                     "vec4 test_vec4;\n"
                                     "dvec2 test_dvec2;\n"
                                     "dvec3 test_dvec3;\n"
                                     "dvec4 test_dvec4;\n"
                                     "bvec2 test_bvec2;\n"
                                     "bvec3 test_bvec3;\n"
                                     "bvec4 test_bvec4;\n"
                                     "ivec2 test_ivec2;\n"
                                     "ivec3 test_ivec3;\n"
                                     "ivec4 test_ivec4;\n"
                                     "uvec2 test_uvec2;\n"
                                     "uvec3 test_uvec3;\n"
                                     "uvec4 test_uvec4;\n"
                                     "mat2 test_mat2;\n"
                                     "mat3 test_mat3;\n"
                                     "mat4 test_mat4;\n"
                                     "mat2x2 test_mat2x2;\n"
                                     "mat2x3 test_mat2x3;\n"
                                     "mat2x4 test_mat2x4;\n"
                                     "mat3x2 test_mat3x2;\n"
                                     "mat3x3 test_mat3x3;\n"
                                     "mat3x4 test_mat3x4;\n"
                                     "mat4x2 test_mat4x2;\n"
                                     "mat4x3 test_mat4x3;\n"
                                     "mat4x4 test_mat4x4;\n"
                                     "dmat2 test_dmat2;\n"
                                     "dmat3 test_dmat3;\n"
                                     "dmat4 test_dmat4;\n"
                                     "dmat2x2 test_dmat2x2;\n"
                                     "dmat2x3 test_dmat2x3;\n"
                                     "dmat2x4 test_dmat2x4;\n"
                                     "dmat3x2 test_dmat3x2;\n"
                                     "dmat3x3 test_dmat3x3;\n"
                                     "dmat3x4 test_dmat3x4;\n"
                                     "dmat4x2 test_dmat4x2;\n"
                                     "dmat4x3 test_dmat4x3;\n"
                                     "dmat4x4 test_dmat4x4;\n"
                                     "sampler1D test_sampler1D;\n"
                                     "image1D test_image1D;\n"
                                     "sampler2D test_sampler2D;\n"
                                     "image2D test_image2D;\n"
                                     "sampler3D test_sampler3D;\n"
                                     "image3D test_image3D;\n"
                                     "samplerCube test_samplerCube;\n"
                                     "imageCube test_imageCube;\n"
                                     "sampler2DRect test_sampler2DRect;\n"
                                     "image2DRect test_image2DRect;\n"
                                     "sampler1DArray test_sampler1DArray;\n"
                                     "image1DArray test_image1DArray;\n"
                                     "sampler2DArray test_sampler2DArray;\n"
                                     "image2DArray test_image2DArray;\n"
                                     "samplerBuffer test_samplerBuffer;\n"
                                     "imageBuffer test_imageBuffer;\n"
                                     "sampler2DMS test_sampler2DMS;\n"
                                     "image2DMS test_image2DMS;\n"
                                     "sampler2DMSArray test_sampler2DMSArray;\n"
                                     "image2DMSArray test_image2DMSArray;\n"
                                     "samplerCubeArray test_samplerCubeArray;\n"
                                     "imageCubeArray test_imageCubeArray;\n"
                                     "sampler1DShadow test_sampler1DShadow;\n"
                                     "sampler2DShadow test_sampler2DShadow;\n"
                                     "sampler2DRectShadow test_sampler2DRectShadow;\n"
                                     "sampler1DArrayShadow test_sampler1DArrayShadow;\n"
                                     "sampler2DArrayShadow test_sampler2DArrayShadow;\n"
                                     "samplerCubeShadow test_samplerCubeShadow;\n"
                                     "samplerCubeArrayShadow test_samplerCubeArrayShadow;\n"
                                     "isampler1D test_isampler1D;\n"
                                     "iimage1D test_iimage1D;\n"
                                     "isampler2D test_isampler2D;\n"
                                     "iimage2D test_iimage2D;\n"
                                     "isampler3D test_isampler3D;\n"
                                     "iimage3D test_iimage3D;\n"
                                     "isamplerCube test_isamplerCube;\n"
                                     "iimageCube test_iimageCube;\n"
                                     "isampler2DRect test_isampler2DRect;\n"
                                     "iimage2DRect test_iimage2DRect;\n"
                                     "isampler1DArray test_isampler1DArray;\n"
                                     "iimage1DArray test_iimage1DArray;\n"
                                     "isampler2DArray test_isampler2DArray;\n"
                                     "iimage2DArray test_iimage2DArray;\n"
                                     "isamplerBuffer test_isamplerBuffer;\n"
                                     "iimageBuffer test_iimageBuffer;\n"
                                     "isampler2DMS test_isampler2DMS;\n"
                                     "iimage2DMS test_iimage2DMS;\n"
                                     "isampler2DMSArray test_isampler2DMSArray;\n"
                                     "iimage2DMSArray test_iimage2DMSArray;\n"
                                     "isamplerCubeArray test_isamplerCubeArray;\n"
                                     "iimageCubeArray test_iimageCubeArray;\n"
                                     "atomic_uint test_atomic_uint;\n"
                                     "usampler1D test_usampler1D;\n"
                                     "uimage1D test_uimage1D;\n"
                                     "usampler2D test_usampler2D;\n"
                                     "uimage2D test_uimage2D;\n"
                                     "usampler3D test_usampler3D;\n"
                                     "uimage3D test_uimage3D;\n"
                                     "usamplerCube test_usamplerCube;\n"
                                     "uimageCube test_uimageCube;\n"
                                     "usampler2DRect test_usampler2DRect;\n"
                                     "uimage2DRect test_uimage2DRect;\n"
                                     "usampler1DArray test_usampler1DArray;\n"
                                     "uimage1DArray test_uimage1DArray;\n"
                                     "usampler2DArray test_usampler2DArray;\n"
                                     "uimage2DArray test_uimage2DArray;\n"
                                     "usamplerBuffer test_usamplerBuffer;\n"
                                     "uimageBuffer test_uimageBuffer;\n"
                                     "usampler2DMS test_usampler2DMS;\n"
                                     "uimage2DMS test_uimage2DMS;\n"
                                     "usampler2DMSArray test_usampler2DMSArray;\n"
                                     "uimage2DMSArray test_uimage2DMSArray;\n"
                                     "usamplerCubeArray test_usamplerCubeArray;\n"
                                     "uimageCubeArray test_uimageCubeArray;\n";
        const char *expected = source;

        glslx::Parser parser(source, "builtins.glsl");
        std::unique_ptr<glslx::TrUnit> tr_unit = parser.Parse(glslx::eTrUnitType::Compute);
        require_fatal(tr_unit != nullptr);

        std::stringstream ss;
        glslx::WriterGLSL().Write(tr_unit.get(), ss);
        require(ss.str() == expected);
    }
    { // comments
        static const char source[] = "// Single line line-comment\n"
                                     "\n"
                                     "/* Single line block-comment */\n"
                                     "\n"
                                     "/* Multiline\n"
                                     " * block-comment\n"
                                     " */\n"
                                     "\n"
                                     "int a;\n";
        static const char expected[] = "int a;\n";

        glslx::Parser parser(source, "comments.glsl");
        std::unique_ptr<glslx::TrUnit> tr_unit = parser.Parse(glslx::eTrUnitType::Compute);
        require_fatal(tr_unit != nullptr);

        std::stringstream ss;
        glslx::WriterGLSL().Write(tr_unit.get(), ss);
        require(ss.str() == expected);
    }
    { // booleans
        static const char source[] = "void test() {\n"
                                     "    bool test_uninitialized;\n"
                                     "    bool test_true_initialized = true;\n"
                                     "    bool test_false_initialized = false;\n"
                                     "    bool test_assign;\n"
                                     "    test_assign = test_true_initialized;\n"
                                     "    test_assign = test_false_initialized;\n"
                                     "    test_assign = true;\n"
                                     "    test_assign = false;\n"
                                     "}\n";
        static const char *expected = source;

        glslx::Parser parser(source, "booleans.glsl");
        std::unique_ptr<glslx::TrUnit> tr_unit = parser.Parse(glslx::eTrUnitType::Compute);
        require_fatal(tr_unit != nullptr);

        std::stringstream ss;
        glslx::WriterGLSL().Write(tr_unit.get(), ss);
        require(ss.str() == expected);
    }
    { // int literals
        static const char source[] = "void test(int a, int b) {\n"
                                     "    int test_uninitialized_int;\n"
                                     "    int test_initialized_int = 42;\n"
                                     "    uint test_uninitialized_uint;\n"
                                     "    uint test_initialized_uint_no_suffix = 42;\n"
                                     "    uint test_initialized_uint_suffix = 42u;\n"
                                     "    uint test_hex_no_suffix_upper = 0xFF;\n"
                                     "    uint test_hex_suffix_upper = 0xFFu;\n"
                                     "    uint test_hex_no_suffix_lower = 0xff;\n"
                                     "    uint test_hex_suffix_lower = 0xffu;\n"
                                     "    uint test_hex_no_suffix_mixed = 0xFf;\n"
                                     "    uint test_hex_suffix_mixed = 0xFfU;\n"
                                     "    int test_negative = -1;\n"
                                     "    uint test_octal = 0777;\n"
                                     "}\n";
        static const char expected[] = "void test(int a, int b) {\n"
                                       "    int test_uninitialized_int;\n"
                                       "    int test_initialized_int = 42;\n"
                                       "    uint test_uninitialized_uint;\n"
                                       "    uint test_initialized_uint_no_suffix = 42;\n"
                                       "    uint test_initialized_uint_suffix = 42u;\n"
                                       "    uint test_hex_no_suffix_upper = 255;\n"
                                       "    uint test_hex_suffix_upper = 255u;\n"
                                       "    uint test_hex_no_suffix_lower = 255;\n"
                                       "    uint test_hex_suffix_lower = 255u;\n"
                                       "    uint test_hex_no_suffix_mixed = 255;\n"
                                       "    uint test_hex_suffix_mixed = 255u;\n"
                                       "    int test_negative = -1;\n"
                                       "    uint test_octal = 511;\n"
                                       "}\n";

        glslx::Parser parser(source, "int_literals.glsl");
        std::unique_ptr<glslx::TrUnit> tr_unit = parser.Parse(glslx::eTrUnitType::Compute);
        require_fatal(tr_unit != nullptr);

        std::stringstream ss;
        glslx::WriterGLSL().Write(tr_unit.get(), ss);
        require(ss.str() == expected);
    }
    { // float literals
        static const char source[] = "void test() {\n"
                                     "    float test_float_uninitialized;\n"
                                     "    float test_float_initialized = 1.5;\n"
                                     "    float test_float_f_lower = 1.5f;\n"
                                     "    float test_float_f_upper = 1.5F;\n"
                                     "    double test_double_uininitialized;\n"
                                     "    double test_double_initialized = 1.5;\n"
                                     "    double test_double_lf_lower = 1.5lf;\n"
                                     "    double test_double_lf_upper = 1.5LF;\n"
                                     "    float test_float_f_zero = 1.0f;\n"
                                     "}\n";
        static const char expected[] = "void test() {\n"
                                       "    float test_float_uninitialized;\n"
                                       "    float test_float_initialized = 1.5;\n"
                                       "    float test_float_f_lower = 1.5;\n"
                                       "    float test_float_f_upper = 1.5;\n"
                                       "    double test_double_uininitialized;\n"
                                       "    double test_double_initialized = 1.5;\n"
                                       "    double test_double_lf_lower = 1.5;\n"
                                       "    double test_double_lf_upper = 1.5;\n"
                                       "    float test_float_f_zero = 1.0;\n"
                                       "}\n";

        glslx::Parser parser(source, "float_literals.glsl");
        std::unique_ptr<glslx::TrUnit> tr_unit = parser.Parse(glslx::eTrUnitType::Compute);
        require_fatal(tr_unit != nullptr);

        std::stringstream ss;
        glslx::WriterGLSL().Write(tr_unit.get(), ss);
        require(ss.str() == expected);
    }
    { // structures
        static const char source[] = "struct foo\n"
                                     "{\n"
                                     "    vec3 a;\n"
                                     "    vec2 b;\n"
                                     "    float c[100];\n"
                                     "    float d, e, f;\n"
                                     "};\n"
                                     "\n"
                                     "struct bar\n"
                                     "{\n"
                                     "    foo a, b, c;\n"
                                     "} a, b, c;\n"
                                     "\n"
                                     "struct ray_hash_t {\n"
                                     "    uint hash, index;\n"
                                     "};\n"
                                     "\n"
                                     "void func(bar arg) {\n"
                                     "    ray_hash_t h = ray_hash_t(0xffffffff, 0xffffffff);\n"
                                     "}\n"
                                     "\n"
                                     "void main( )\n"
                                     "{\n"
                                     "    bar a, b, c;\n"
                                     "    float d = c.b.a.x;\n"
                                     "    float e = c.b.a[0];\n"
                                     "    e = d = 1.0;\n"
                                     "}\n";
        static const char expected[] = "struct foo {\n"
                                       "    vec3 a;\n"
                                       "    vec2 b;\n"
                                       "    float c[100];\n"
                                       "    float d;\n"
                                       "    float e;\n"
                                       "    float f;\n"
                                       "};\n"
                                       "struct bar {\n"
                                       "    foo a;\n"
                                       "    foo b;\n"
                                       "    foo c;\n"
                                       "};\n"
                                       "struct ray_hash_t {\n"
                                       "    uint hash;\n"
                                       "    uint index;\n"
                                       "};\n"
                                       "bar a;\n"
                                       "bar b;\n"
                                       "bar c;\n"
                                       "void func(bar arg) {\n"
                                       "    ray_hash_t h = ray_hash_t(-1, -1);\n"
                                       "}\n"
                                       "void main() {\n"
                                       "    bar a;\n"
                                       "    bar b;\n"
                                       "    bar c;\n"
                                       "    float d = c.b.a.x;\n"
                                       "    float e = c.b.a[0];\n"
                                       "    e = d = 1.0;\n"
                                       "}\n";

        glslx::Parser parser(source, "structures.glsl");
        std::unique_ptr<glslx::TrUnit> tr_unit = parser.Parse(glslx::eTrUnitType::Compute);
        require_fatal(tr_unit != nullptr);

        std::stringstream ss;
        glslx::WriterGLSL().Write(tr_unit.get(), ss);
        require(ss.str() == expected);
    }
    { // arrays
        static const char source[] = "float a[1];\n"
                                     "const float b[1][2] = { { 0.227027029, 0.194594592 } };\n"
                                     "float c[1][2][3];\n"
                                     "float d[1][2][3][4];\n"
                                     "float[2] e[3];\n" // same as float[2][3]
                                     "float[3][2] f;\n" // same as float[2][3]
                                     "struct foo1 { float a; };\n"
                                     "foo1 bar1[1][2];\n"
                                     "foo1[1] bar2[2];\n"
                                     "foo1[1] aa[1], bb;\n"
                                     "void main() {\n"
                                     "    float a[1];\n"
                                     "    float b[1][2] = { { 0.227027029, 0.194594592 } };\n"
                                     "    float c[1][2][3];\n"
                                     "    float d[1][2][3][4];\n"
                                     "    float[2] e[3];\n" // same as float[2][3]
                                     "    float[3][2] f;\n" // same as float[2][3]
                                     "    foo1 bar1[1][2];\n"
                                     "    foo1[1] bar2[2];\n"
                                     "    foo1[1] aa[1], bb;\n"
                                     "}\n";
        static const char expected[] = "float a[1];\n"
                                       "const float b[1][2] = { { 0.227027029, 0.194594592 } };\n"
                                       "float c[1][2][3];\n"
                                       "float d[1][2][3][4];\n"
                                       "float e[2][3];\n"
                                       "float f[2][3];\n"
                                       "struct foo1 {\n"
                                       "    float a;\n"
                                       "};\n"
                                       "foo1 bar1[1][2];\n"
                                       "foo1 bar2[1][2];\n"
                                       "foo1 aa[1][1];\n"
                                       "foo1 bb[1];\n"
                                       "void main() {\n"
                                       "    float a[1];\n"
                                       "    float b[1][2] = { { 0.227027029, 0.194594592 } };\n"
                                       "    float c[1][2][3];\n"
                                       "    float d[1][2][3][4];\n"
                                       "    float e[2][3];\n"
                                       "    float f[2][3];\n"
                                       "    foo1 bar1[1][2];\n"
                                       "    foo1 bar2[1][2];\n"
                                       "    foo1 aa[1][1];\n"
                                       "    foo1 bb[1];\n"
                                       "}\n";

        glslx::Parser parser(source, "arrays.glsl");
        std::unique_ptr<glslx::TrUnit> tr_unit = parser.Parse(glslx::eTrUnitType::Compute);
        require_fatal(tr_unit != nullptr);

        std::stringstream ss;
        glslx::WriterGLSL().Write(tr_unit.get(), ss);
        require(ss.str() == expected);
    }
    { // if statement
        static const char source[] = "void test() {\n"
                                     "    [[flatten]] if (true) {}\n"
                                     "    [[branch]] if (false) {}\n"
                                     "}\n";
        static const char expected[] = "void test() {\n"
                                       "    [[flatten]] if (true) {\n"
                                       "    }\n"
                                       "    [[dont_flatten]] if (false) {\n"
                                       "    }\n"
                                       "}\n";

        glslx::Parser parser(source, "if_statement.glsl");
        std::unique_ptr<glslx::TrUnit> tr_unit = parser.Parse(glslx::eTrUnitType::Compute);
        require_fatal(tr_unit != nullptr);

        std::stringstream ss;
        glslx::WriterGLSL().Write(tr_unit.get(), ss);
        require(ss.str() == expected);
    }
    { // do statement
        static const char source[] = "void test() {\n"
                                     "    [[unroll, dependency_infinite]] do a(); while (true);\n"
                                     "    [[dont_unroll, dependency_length(4)]] do { } while (true);\n"
                                     "}\n";
        static const char expected[] = "void test() {\n"
                                       "    [[unroll, dependency_infinite]] do a();\n"
                                       "    while (true);\n"
                                       "    [[dont_unroll, dependency_length(4)]] do {\n"
                                       "    }\n"
                                       "    while (true);\n"
                                       "}\n";

        glslx::Parser parser(source, "do_statement.glsl");
        std::unique_ptr<glslx::TrUnit> tr_unit = parser.Parse(glslx::eTrUnitType::Compute);
        require_fatal(tr_unit != nullptr);

        std::stringstream ss;
        glslx::WriterGLSL().Write(tr_unit.get(), ss);
        require(ss.str() == expected);
    }
    { // while statement
        static const char source[] = "void test() {\n"
                                     "    int i = 0;\n"
                                     "    [[unroll, dependency_infinite]] while (true) { }\n"
                                     "    [[dont_unroll, dependency_length(4)]] while (i < 10) { }\n"
                                     "}\n";
        static const char expected[] = "void test() {\n"
                                       "    int i = 0;\n"
                                       "    [[unroll, dependency_infinite]] while (true) {\n"
                                       "    }\n"
                                       "    [[dont_unroll, dependency_length(4)]] while ((i < 10)) {\n"
                                       "    }\n"
                                       "}\n";

        glslx::Parser parser(source, "while_statement.glsl");
        std::unique_ptr<glslx::TrUnit> tr_unit = parser.Parse(glslx::eTrUnitType::Compute);
        require_fatal(tr_unit != nullptr);

        std::stringstream ss;
        glslx::WriterGLSL().Write(tr_unit.get(), ss);
        require(ss.str() == expected);
    }
    { // for statement
        static const char source[] = "void main() {\n"
                                     "    int i = 0;\n"
                                     "    for(;;) { }\n"
                                     "    for(i = 0;; ) { }\n"
                                     "    [[unroll, dependency_infinite]] for(int i = 0; i < 10;) { }\n"
                                     "    [[dont_unroll, dependency_length(4)]] for(int i = 0; i < 10; i++) { }\n"
                                     "}\n";
        static const char expected[] = "void main() {\n"
                                       "    int i = 0;\n"
                                       "    for (;;) {\n"
                                       "    }\n"
                                       "    for (i = 0;;) {\n"
                                       "    }\n"
                                       "    [[unroll, dependency_infinite]] for (int i = 0; (i < 10);) {\n"
                                       "    }\n"
                                       "    [[dont_unroll, dependency_length(4)]] for (int i = 0; (i < 10); i++) {\n"
                                       "    }\n"
                                       "}\n";

        glslx::Parser parser(source, "for_statement.glsl");
        std::unique_ptr<glslx::TrUnit> tr_unit = parser.Parse(glslx::eTrUnitType::Compute);
        require_fatal(tr_unit != nullptr);

        std::stringstream ss;
        glslx::WriterGLSL().Write(tr_unit.get(), ss);
        require(ss.str() == expected);
    }
    { // ternary
        static const char source[] = "void main() {\n"
                                     "    float a = 0, b = 0, c = 0, d = 0;\n"
                                     "    float y = (a < b / 2 ? a - 1 : b - 2);\n"
                                     "    float w = (a ? a,b : b,c);\n"
                                     "    float z = a ? b ? c : d : w;\n"
                                     "}\n";
        static const char expected[] = "void main() {\n"
                                       "    float a = 0;\n"
                                       "    float b = 0;\n"
                                       "    float c = 0;\n"
                                       "    float d = 0;\n"
                                       "    float y = ((a < (b / 2)) ? (a - 1) : (b - 2));\n"
                                       "    float w = (a ? (a, b) : (b, c));\n"
                                       "    float z = (a ? (b ? c : d) : w);\n"
                                       "}\n";

        glslx::Parser parser(source, "ternary.glsl");
        std::unique_ptr<glslx::TrUnit> tr_unit = parser.Parse(glslx::eTrUnitType::Compute);
        require_fatal(tr_unit != nullptr);

        std::stringstream ss;
        glslx::WriterGLSL().Write(tr_unit.get(), ss);
        require(ss.str() == expected);
    }
    { // switch
        static const char source[] = "void test() {\n"
                                     "    // simple test\n"
                                     "    int i = 10;\n"
                                     "    [[flatten]] switch (i) {\n"
                                     "    case 0: break;\n"
                                     "    default: break;\n"
                                     "    }\n"
                                     "    // nested\n"
                                     "    [[branch]] switch (i) {\n"
                                     "    case 0:\n"
                                     "        switch (1) {\n"
                                     "        case 1:\n"
                                     "            break;\n"
                                     "        default:\n"
                                     "            break;\n"
                                     "        }\n"
                                     "        break;\n"
                                     "    default:\n"
                                     "        break;\n"
                                     "    }\n"
                                     "}\n";
        static const char expected[] = "void test() {\n"
                                       "    int i = 10;\n"
                                       "    [[flatten]] switch (i) {\n"
                                       "        case 0:\n"
                                       "        break;\n"
                                       "        default:\n"
                                       "        break;\n"
                                       "    }\n"
                                       "    [[dont_flatten]] switch (i) {\n"
                                       "        case 0:\n"
                                       "        switch (1) {\n"
                                       "            case 1:\n"
                                       "            break;\n"
                                       "            default:\n"
                                       "            break;\n"
                                       "        }\n"
                                       "        break;\n"
                                       "        default:\n"
                                       "        break;\n"
                                       "    }\n"
                                       "}\n";

        glslx::Parser parser(source, "switch.glsl");
        std::unique_ptr<glslx::TrUnit> tr_unit = parser.Parse(glslx::eTrUnitType::Compute);
        require_fatal(tr_unit != nullptr);

        std::stringstream ss;
        glslx::WriterGLSL().Write(tr_unit.get(), ss);
        require(ss.str() == expected);
    }
    { // vector stuff
        static const char source[] = "const vec4 g = {1.0, 2.0, 3.0, 4.0};\n"
                                     "const highp vec2 p[2] = vec2[2](vec2(-0.5, 0.0),\n"
                                     "                                vec2(0.0, 0.5));\n"
                                     "const highp vec2 pp[2] = vec2[](vec2(-0.5, 0.0),\n"
                                     "                                vec2(0.0, 0.5));\n"
                                     "vec4 f() { return g; }\n"
                                     "float func(const vec3 color, float x, float y) {\n"
                                     "    vec2 s = {x, y};\n"
                                     "    highp float t = f()[2];\n"
                                     "    vec3 a = (vec3(1.0) - x) / (vec3(1.0) + x);\n"
                                     "    return 0.212671 * color[0] + 0.715160 * color.y + 0.072169 * color.z;\n"
                                     "}\n";
        static const char expected[] =
            "const vec4 g = { 1.0, 2.0, 3.0, 4.0 };\n"
            "const highp vec2 p[2] = { vec2(-0.5, 0.0), vec2(0.0, 0.5) };\n"
            "const highp vec2 pp[2] = { vec2(-0.5, 0.0), vec2(0.0, 0.5) };\n"
            "vec4 f() {\n"
            "    return g;\n"
            "}\n"
            "float func(const vec3 color, float x, float y) {\n"
            "    vec2 s = { x, y };\n"
            "    highp float t = f()[2];\n"
            "    vec3 a = ((vec3(1.0) - x) / (vec3(1.0) + x));\n"
            "    return (((0.212670997 * color[0]) + (0.715160012 * color.y)) + (0.0721689984 * color.z));\n"
            "}\n";

        glslx::Parser parser(source, "vector.glsl");
        std::unique_ptr<glslx::TrUnit> tr_unit = parser.Parse(glslx::eTrUnitType::Compute);
        require_fatal(tr_unit != nullptr);

        std::stringstream ss;
        glslx::WriterGLSL().Write(tr_unit.get(), ss);
        require(ss.str() == expected);
    }
    { // assignment inside of expression
        static const char source[] = "vec3 normalize_len(const vec3 v, out float len) {\n"
                                     "    return (v / (len = length(v)));\n"
                                     "}\n";
        static const char *expected = source;

        glslx::Parser parser(source, "assign.glsl");
        std::unique_ptr<glslx::TrUnit> tr_unit = parser.Parse(glslx::eTrUnitType::Compute);
        require_fatal(tr_unit != nullptr);

        std::stringstream ss;
        glslx::WriterGLSL().Write(tr_unit.get(), ss);
        require(ss.str() == expected);
    }
    { // GL_EXT_control_flow_attributes2
        static const char source[] =
            "void test() {\n"
            "    [[unroll, min_iterations(2)]] for (int i = 0; (i < 8); ++i) {\n"
            "    }\n"
            "    [[unroll, min_iterations(2), max_iterations(4)]] for (int i = 0; (i < 8); ++i) {\n"
            "    }\n"
            "    [[unroll, iteration_multiple(4)]] for (int i = 0; (i < 16); ++i) {\n"
            "    }\n"
            "    [[unroll, peel_count(3)]] for (int i = 0; (i < 11); ++i) {\n"
            "    }\n"
            "    [[unroll, partial_count(2)]] for (int i = 0; (i < 8); ++i) {\n"
            "    }\n"
            "}\n";
        static const char *expected = source;

        glslx::Parser parser(source, "control_flow2.glsl");
        std::unique_ptr<glslx::TrUnit> tr_unit = parser.Parse(glslx::eTrUnitType::Compute);
        require_fatal(tr_unit != nullptr);

        std::stringstream ss;
        glslx::WriterGLSL().Write(tr_unit.get(), ss);
        require(ss.str() == expected);
    }
    { // default precision qualifiers
        static const char source[] = "precision highp int;\n"
                                     "precision mediump float;\n";
        static const char *expected = source;

        glslx::Parser parser(source, "default_precision.glsl");
        std::unique_ptr<glslx::TrUnit> tr_unit = parser.Parse(glslx::eTrUnitType::Compute);
        require_fatal(tr_unit != nullptr);

        std::stringstream ss;
        glslx::WriterGLSL().Write(tr_unit.get(), ss);
        require(ss.str() == expected);
    }
    { // invariant
        static const char source[] = "invariant gl_Position;\n"
                                     "out vec3 Color;\n"
                                     "invariant Color;\n";
        static const char *expected = "invariant gl_Position;\n"
                                      "out invariant vec3 Color;\n";

        glslx::Parser parser(source, "default_precision.glsl");
        std::unique_ptr<glslx::TrUnit> tr_unit = parser.Parse(glslx::eTrUnitType::Vertex);
        require_fatal(tr_unit != nullptr);

        std::stringstream ss;
        glslx::WriterGLSL().Write(tr_unit.get(), ss);
        require(ss.str() == expected);
    }
    { // line directive
        static const char source[] = "\n"
                                     "\n"
                                     "\n"
                                     "#line 42\n"
                                     "#error 1111\n";

        glslx::Parser parser(source, "line_directive.glsl");
        std::unique_ptr<glslx::TrUnit> tr_unit = parser.Parse(glslx::eTrUnitType::Compute);
        require(tr_unit == nullptr);
        require(strcmp(parser.error(), "line_directive.glsl:43:12: error: 1111") == 0);
    }
    { // first character invalid
        const char source[] = "`\n";

        glslx::Parser parser(source, "first_character_invalid.glsl");
        std::unique_ptr<glslx::TrUnit> tr_unit = parser.Parse(glslx::eTrUnitType::Compute);
        require(tr_unit == nullptr);
        require(strcmp(parser.error(), "first_character_invalid.glsl:1:1: error: Invalid character encountered") == 0);
    }
    { // arrayness of redeclared block
        static const char source[] = "in gl_PerVertex {\n"
                                     "    vec4 gl_Position;\n"
                                     "    float gl_PointSize;\n"
                                     "    float gl_ClipDistance[];\n"
                                     "    float gl_CullDistance[];\n"
                                     "} gl_in[];\n";
        static const char *expected = "in gl_PerVertex {\n"
                                     "    vec4 gl_Position;\n"
                                     "    float gl_PointSize;\n"
                                     "    float gl_ClipDistance[];\n"
                                     "    float gl_CullDistance[];\n"
                                      "} gl_in[];\n";

        glslx::Parser parser(source, "arrayness.glsl");
        std::unique_ptr<glslx::TrUnit> tr_unit = parser.Parse(glslx::eTrUnitType::Geometry);
        require_fatal(tr_unit != nullptr);

        std::stringstream ss;
        glslx::WriterGLSL().Write(tr_unit.get(), ss);
        require(ss.str() == expected);
    }
    printf("OK\n");
}
