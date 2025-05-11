#include "test_common.h"

#include <iostream>
#include <sstream>

#include "../WriterHLSL.h"

void test_hlsl_writer() {
    using namespace glslx;

    printf("Test hlsl_writer        | ");

    { // constants
        static const char source[] = "const float test = 1;\n"
                                     "const float test_neg = -1;\n"
                                     "const float test_pos_two_sub = test - test_neg;\n"
                                     "const float test_neg_two_sub = test_neg - test;\n"
                                     "const float test_zero_add = test_neg + test;\n"
                                     "const uint test_uint = (1u << 31);\n"
                                     "const uint test_bitneg = ~test_uint;\n";
        static const char *expected = "static const float test = 1;\n"
                                      "static const float test_neg = -1;\n"
                                      "static const float test_pos_two_sub = 2;\n"
                                      "static const float test_neg_two_sub = -2;\n"
                                      "static const float test_zero_add = 0;\n"
                                      "static const uint test_uint = 2147483648u;\n"
                                      "static const uint test_bitneg = 2147483647u;\n";

        Parser parser(source, "constants.glsl");
        std::unique_ptr<TrUnit> tr_unit = parser.Parse(eTrUnitType::Compute);
        require_fatal(tr_unit != nullptr);

        std::stringstream ss;
        WriterHLSL().Write(tr_unit.get(), ss);
        require(ss.str() == expected);
    }
    { // continuations (comma-separated declarations)
        static const char source[] = "mat4 model, view, projection;\n"
                                     "struct foo { float x; } a, b, c, d;\n";
        static const char *expected = "row_major float4x4 model;\n"
                                      "row_major float4x4 view;\n"
                                      "row_major float4x4 projection;\n"
                                      "struct foo {\n"
                                      "    float x;\n"
                                      "};\n"
                                      "foo a;\n"
                                      "foo b;\n"
                                      "foo c;\n"
                                      "foo d;\n";

        Parser parser(source, "continuations.glsl");
        std::unique_ptr<TrUnit> tr_unit = parser.Parse(eTrUnitType::Compute);
        require_fatal(tr_unit != nullptr);

        std::stringstream ss;
        WriterHLSL().Write(tr_unit.get(), ss);
        require(ss.str() == expected);
    }
    { // builtin types
        static const char source[] =
            "bool test_bool;\n"
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
            "layout(binding = 0) uniform sampler1D test_sampler1D;\n"
            "layout(binding = 1, rgba32f) uniform image1D test_image1D;\n"
            "layout(binding = 2) uniform sampler2D test_sampler2D;\n"
            "layout(binding = 3, rgba16f) uniform image2D test_image2D;\n"
            "layout(binding = 4) uniform sampler3D test_sampler3D;\n"
            "layout(binding = 5, rg32f) uniform image3D test_image3D;\n"
            "layout(binding = 6) uniform samplerCube test_samplerCube;\n"
            "layout(binding = 7) uniform sampler1DArray test_sampler1DArray;\n"
            "layout(binding = 8, rg16f) uniform image1DArray test_image1DArray;\n"
            "layout(binding = 9) uniform sampler2DArray test_sampler2DArray;\n"
            "layout(binding = 10, r32f) uniform image2DArray test_image2DArray;\n"
            "layout(binding = 11) uniform samplerBuffer test_samplerBuffer;\n"
            "layout(binding = 12, r16f) uniform imageBuffer test_imageBuffer;\n"
            "layout(binding = 13) uniform sampler2DMS test_sampler2DMS;\n"
            "layout(binding = 14, rgba16) uniform image2DMS test_image2DMS;\n"
            "layout(binding = 15) uniform sampler2DMSArray test_sampler2DMSArray;\n"
            "layout(binding = 16, rgb10_a2) uniform image2DMSArray test_image2DMSArray;\n"
            "layout(binding = 17) uniform samplerCubeArray test_samplerCubeArray;\n"
            "layout(binding = 18) uniform sampler1DShadow test_sampler1DShadow;\n"
            "layout(binding = 19) uniform sampler2DShadow test_sampler2DShadow;\n"
            "layout(binding = 20) uniform sampler1DArrayShadow test_sampler1DArrayShadow;\n"
            "layout(binding = 21) uniform sampler2DArrayShadow test_sampler2DArrayShadow;\n"
            "layout(binding = 22) uniform samplerCubeShadow test_samplerCubeShadow;\n"
            "layout(binding = 23) uniform samplerCubeArrayShadow test_samplerCubeArrayShadow;\n"
            "layout(binding = 24) uniform isampler1D test_isampler1D;\n"
            "layout(binding = 25, rgba32i) uniform iimage1D test_iimage1D;\n"
            "layout(binding = 26) uniform isampler2D test_isampler2D;\n"
            "layout(binding = 27) uniform iimage2D test_iimage2D;\n"
            "layout(binding = 28) uniform isampler3D test_isampler3D;\n"
            "layout(binding = 29, rgba8i) uniform iimage3D test_iimage3D;\n"
            "layout(binding = 30) uniform isamplerCube test_isamplerCube;\n"
            "layout(binding = 31) uniform isampler1DArray test_isampler1DArray;\n"
            "layout(binding = 32, rg32i) uniform iimage1DArray test_iimage1DArray;\n"
            "layout(binding = 33) uniform isampler2DArray test_isampler2DArray;\n"
            "layout(binding = 34, rg16i) uniform iimage2DArray test_iimage2DArray;\n"
            "layout(binding = 35) uniform isamplerBuffer test_isamplerBuffer;\n"
            "layout(binding = 36, rg8i) uniform iimageBuffer test_iimageBuffer;\n"
            "layout(binding = 37) uniform isampler2DMS test_isampler2DMS;\n"
            "layout(binding = 38, r32i) uniform iimage2DMS test_iimage2DMS;\n"
            "layout(binding = 39) uniform isampler2DMSArray test_isampler2DMSArray;\n"
            "layout(binding = 40, r16i) uniform iimage2DMSArray test_iimage2DMSArray;\n"
            "layout(binding = 41) uniform isamplerCubeArray test_isamplerCubeArray;\n"
            "layout(binding = 42) uniform usampler1D test_usampler1D;\n"
            "layout(binding = 43, rgba32ui) uniform uimage1D test_uimage1D;\n"
            "layout(binding = 44) uniform usampler2D test_usampler2D;\n"
            "layout(binding = 45, rgba16ui) uniform uimage2D test_uimage2D;\n"
            "layout(binding = 46) uniform usampler3D test_usampler3D;\n"
            "layout(binding = 47, rgb10_a2ui) uniform uimage3D test_uimage3D;\n"
            "layout(binding = 48) uniform usamplerCube test_usamplerCube;\n"
            "layout(binding = 49) uniform usampler1DArray test_usampler1DArray;\n"
            "layout(binding = 50, rg32ui) uniform uimage1DArray test_uimage1DArray;\n"
            "layout(binding = 51) uniform usampler2DArray test_usampler2DArray;\n"
            "layout(binding = 52, rg16ui) uniform uimage2DArray test_uimage2DArray;\n"
            "layout(binding = 53) uniform usamplerBuffer test_usamplerBuffer;\n"
            "layout(binding = 54, rg8ui) uniform uimageBuffer test_uimageBuffer;\n"
            "layout(binding = 55) uniform usampler2DMS test_usampler2DMS;\n"
            "layout(binding = 56, r32ui) uimage2DMS test_uimage2DMS;\n"
            "layout(binding = 57) uniform usampler2DMSArray test_usampler2DMSArray;\n"
            "layout(binding = 58, r16ui) uimage2DMSArray test_uimage2DMSArray;\n"
            "layout(binding = 59) uniform usamplerCubeArray test_usamplerCubeArray;\n";
        static const char expected[] =
            "bool test_bool;\n"
            "int test_int;\n"
            "uint test_uint;\n"
            "float test_float;\n"
            "double test_double;\n"
            "float2 test_vec2;\n"
            "float3 test_vec3;\n"
            "float4 test_vec4;\n"
            "double2 test_dvec2;\n"
            "double3 test_dvec3;\n"
            "double4 test_dvec4;\n"
            "bool2 test_bvec2;\n"
            "bool3 test_bvec3;\n"
            "bool4 test_bvec4;\n"
            "int2 test_ivec2;\n"
            "int3 test_ivec3;\n"
            "int4 test_ivec4;\n"
            "uint2 test_uvec2;\n"
            "uint3 test_uvec3;\n"
            "uint4 test_uvec4;\n"
            "row_major float2x2 test_mat2;\n"
            "row_major float3x3 test_mat3;\n"
            "row_major float4x4 test_mat4;\n"
            "row_major float2x2 test_mat2x2;\n"
            "row_major float2x3 test_mat2x3;\n"
            "row_major float2x4 test_mat2x4;\n"
            "row_major float3x2 test_mat3x2;\n"
            "row_major float3x3 test_mat3x3;\n"
            "row_major float3x4 test_mat3x4;\n"
            "row_major float4x2 test_mat4x2;\n"
            "row_major float4x3 test_mat4x3;\n"
            "row_major float4x4 test_mat4x4;\n"
            "row_major double2x2 test_dmat2;\n"
            "row_major double3x3 test_dmat3;\n"
            "row_major double4x4 test_dmat4;\n"
            "row_major double2x2 test_dmat2x2;\n"
            "row_major double2x3 test_dmat2x3;\n"
            "row_major double2x4 test_dmat2x4;\n"
            "row_major double3x2 test_dmat3x2;\n"
            "row_major double3x3 test_dmat3x3;\n"
            "row_major double3x4 test_dmat3x4;\n"
            "row_major double4x2 test_dmat4x2;\n"
            "row_major double4x3 test_dmat4x3;\n"
            "row_major double4x4 test_dmat4x4;\n"
            "Texture1D<float4> test_sampler1D : register(t0, space0);\n"
            "SamplerState test_sampler1D_sampler : register(s0, space0);\n"
            "RWTexture1D<float4> test_image1D : register(u1, space0);\n"
            "Texture2D<float4> test_sampler2D : register(t2, space0);\n"
            "SamplerState test_sampler2D_sampler : register(s2, space0);\n"
            "RWTexture2D<float4> test_image2D : register(u3, space0);\n"
            "Texture3D<float4> test_sampler3D : register(t4, space0);\n"
            "SamplerState test_sampler3D_sampler : register(s4, space0);\n"
            "RWTexture3D<float2> test_image3D : register(u5, space0);\n"
            "TextureCube<float4> test_samplerCube : register(t6, space0);\n"
            "SamplerState test_samplerCube_sampler : register(s6, space0);\n"
            "Texture1DArray<float4> test_sampler1DArray : register(t7, space0);\n"
            "SamplerState test_sampler1DArray_sampler : register(s7, space0);\n"
            "RWTexture1DArray<float2> test_image1DArray : register(u8, space0);\n"
            "Texture2DArray<float4> test_sampler2DArray : register(t9, space0);\n"
            "SamplerState test_sampler2DArray_sampler : register(s9, space0);\n"
            "RWTexture2DArray<float> test_image2DArray : register(u10, space0);\n"
            "Buffer<float4> test_samplerBuffer : register(t11, space0);\n"
            "RWBuffer<float> test_imageBuffer : register(u12, space0);\n"
            "Texture2DMS<float4> test_sampler2DMS : register(t13, space0);\n"
            "SamplerState test_sampler2DMS_sampler : register(s13, space0);\n"
            "RWTexture2DMS<unorm float4> test_image2DMS : register(u14, space0);\n"
            "Texture2DMSArray<float4> test_sampler2DMSArray : register(t15, space0);\n"
            "SamplerState test_sampler2DMSArray_sampler : register(s15, space0);\n"
            "RWTexture2DMSArray<unorm float4> test_image2DMSArray : register(u16, space0);\n"
            "TextureCubeArray<float4> test_samplerCubeArray : register(t17, space0);\n"
            "SamplerState test_samplerCubeArray_sampler : register(s17, space0);\n"
            "Texture1D<float4> test_sampler1DShadow : register(t18, space0);\n"
            "SamplerComparisonState test_sampler1DShadow_sampler : register(s18, space0);\n"
            "Texture2D<float4> test_sampler2DShadow : register(t19, space0);\n"
            "SamplerComparisonState test_sampler2DShadow_sampler : register(s19, space0);\n"
            "Texture1DArray<float4> test_sampler1DArrayShadow : register(t20, space0);\n"
            "SamplerComparisonState test_sampler1DArrayShadow_sampler : register(s20, space0);\n"
            "Texture2DArray<float4> test_sampler2DArrayShadow : register(t21, space0);\n"
            "SamplerComparisonState test_sampler2DArrayShadow_sampler : register(s21, space0);\n"
            "TextureCube<float4> test_samplerCubeShadow : register(t22, space0);\n"
            "SamplerComparisonState test_samplerCubeShadow_sampler : register(s22, space0);\n"
            "TextureCubeArray<float4> test_samplerCubeArrayShadow : register(t23, space0);\n"
            "SamplerComparisonState test_samplerCubeArrayShadow_sampler : register(s23, space0);\n"
            "Texture1D<int4> test_isampler1D : register(t24, space0);\n"
            "SamplerState test_isampler1D_sampler : register(s24, space0);\n"
            "RWTexture1D<int4> test_iimage1D : register(u25, space0);\n"
            "Texture2D<int4> test_isampler2D : register(t26, space0);\n"
            "SamplerState test_isampler2D_sampler : register(s26, space0);\n"
            "RWTexture2D<int4> test_iimage2D : register(u27, space0);\n"
            "Texture3D<int4> test_isampler3D : register(t28, space0);\n"
            "SamplerState test_isampler3D_sampler : register(s28, space0);\n"
            "RWTexture3D<int4> test_iimage3D : register(u29, space0);\n"
            "TextureCube<int4> test_isamplerCube : register(t30, space0);\n"
            "SamplerState test_isamplerCube_sampler : register(s30, space0);\n"
            "Texture1DArray<int4> test_isampler1DArray : register(t31, space0);\n"
            "SamplerState test_isampler1DArray_sampler : register(s31, space0);\n"
            "RWTexture1DArray<int2> test_iimage1DArray : register(u32, space0);\n"
            "Texture2DArray<int4> test_isampler2DArray : register(t33, space0);\n"
            "SamplerState test_isampler2DArray_sampler : register(s33, space0);\n"
            "RWTexture2DArray<int2> test_iimage2DArray : register(u34, space0);\n"
            "Buffer<int4> test_isamplerBuffer : register(t35, space0);\n"
            "RWBuffer<int2> test_iimageBuffer : register(u36, space0);\n"
            "Texture2DMS<int4> test_isampler2DMS : register(t37, space0);\n"
            "SamplerState test_isampler2DMS_sampler : register(s37, space0);\n"
            "RWTexture2DMS<int> test_iimage2DMS : register(u38, space0);\n"
            "Texture2DMSArray<int4> test_isampler2DMSArray : register(t39, space0);\n"
            "SamplerState test_isampler2DMSArray_sampler : register(s39, space0);\n"
            "RWTexture2DMSArray<int> test_iimage2DMSArray : register(u40, space0);\n"
            "TextureCubeArray<int4> test_isamplerCubeArray : register(t41, space0);\n"
            "SamplerState test_isamplerCubeArray_sampler : register(s41, space0);\n"
            "Texture1D<uint4> test_usampler1D : register(t42, space0);\n"
            "SamplerState test_usampler1D_sampler : register(s42, space0);\n"
            "RWTexture1D<uint4> test_uimage1D : register(u43, space0);\n"
            "Texture2D<uint4> test_usampler2D : register(t44, space0);\n"
            "SamplerState test_usampler2D_sampler : register(s44, space0);\n"
            "RWTexture2D<uint4> test_uimage2D : register(u45, space0);\n"
            "Texture3D<uint4> test_usampler3D : register(t46, space0);\n"
            "SamplerState test_usampler3D_sampler : register(s46, space0);\n"
            "RWTexture3D<uint4> test_uimage3D : register(u47, space0);\n"
            "TextureCube<uint4> test_usamplerCube : register(t48, space0);\n"
            "SamplerState test_usamplerCube_sampler : register(s48, space0);\n"
            "Texture1DArray<uint4> test_usampler1DArray : register(t49, space0);\n"
            "SamplerState test_usampler1DArray_sampler : register(s49, space0);\n"
            "RWTexture1DArray<uint2> test_uimage1DArray : register(u50, space0);\n"
            "Texture2DArray<uint4> test_usampler2DArray : register(t51, space0);\n"
            "SamplerState test_usampler2DArray_sampler : register(s51, space0);\n"
            "RWTexture2DArray<uint2> test_uimage2DArray : register(u52, space0);\n"
            "Buffer<uint4> test_usamplerBuffer : register(t53, space0);\n"
            "RWBuffer<uint2> test_uimageBuffer : register(u54, space0);\n"
            "Texture2DMS<uint4> test_usampler2DMS : register(t55, space0);\n"
            "SamplerState test_usampler2DMS_sampler : register(s55, space0);\n"
            "RWTexture2DMS<uint> test_uimage2DMS : register(u56, space0);\n"
            "Texture2DMSArray<uint4> test_usampler2DMSArray : register(t57, space0);\n"
            "SamplerState test_usampler2DMSArray_sampler : register(s57, space0);\n"
            "RWTexture2DMSArray<uint> test_uimage2DMSArray : register(u58, space0);\n"
            "TextureCubeArray<uint4> test_usamplerCubeArray : register(t59, space0);\n"
            "SamplerState test_usamplerCubeArray_sampler : register(s59, space0);\n";

        Parser parser(source, "builtins.glsl");
        std::unique_ptr<TrUnit> tr_unit = parser.Parse(eTrUnitType::Compute);
        require_fatal(tr_unit != nullptr);

        std::stringstream ss;
        WriterHLSL().Write(tr_unit.get(), ss);
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

        Parser parser(source, "booleans.glsl");
        std::unique_ptr<TrUnit> tr_unit = parser.Parse(eTrUnitType::Compute);
        require_fatal(tr_unit != nullptr);

        std::stringstream ss;
        WriterHLSL().Write(tr_unit.get(), ss);
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
                                     "    int16_t test_int16 = 42s;\n"
                                     "    uint16_t test_uint16_lower = 42us;\n"
                                     "    uint16_t test_uint16_upper = 42US;\n"
                                     "    int32_t test_int32 = 42;\n"
                                     "    uint32_t test_uint32_lower = 42u;\n"
                                     "    uint32_t test_uint32_upper = 42U;\n"
                                     "    int64_t test_int64_lower = 42l;\n"
                                     "    int64_t test_int64_upper = 42L;\n"
                                     "    uint64_t test_uint64_lower = 42ul;\n"
                                     "    uint64_t test_uint64_upper = 42UL;\n"
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
                                       "    int test_negative = (-1);\n"
                                       "    uint test_octal = 511;\n"
                                       "    short test_int16 = 42s;\n"
                                       "    ushort test_uint16_lower = 42us;\n"
                                       "    ushort test_uint16_upper = 42us;\n"
                                       "    int test_int32 = 42;\n"
                                       "    uint test_uint32_lower = 42u;\n"
                                       "    uint test_uint32_upper = 42u;\n"
                                       "    int64_t test_int64_lower = 42l;\n"
                                       "    int64_t test_int64_upper = 42l;\n"
                                       "    uint64_t test_uint64_lower = 42ul;\n"
                                       "    uint64_t test_uint64_upper = 42ul;\n"
                                       "}\n";

        Parser parser(source, "int_literals.glsl");
        std::unique_ptr<TrUnit> tr_unit = parser.Parse(eTrUnitType::Compute);
        require_fatal(tr_unit != nullptr);

        std::stringstream ss;
        WriterHLSL().Write(tr_unit.get(), ss);
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
                                     "    float32_t test_f32 = 1.5;\n"
                                     "    float64_t test_f64_lf = 1.3lf;\n"
                                     "    float16_t test_f16_hf_lower = 1.5hf;\n"
                                     "    float16_t test_f16_hf_upper = 1.5HF;\n"
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
                                       "    float test_f32 = 1.5;\n"
                                       "    double test_f64_lf = 1.3lf;\n"
                                       "    half test_f16_hf_lower = 1.5hf;\n"
                                       "    half test_f16_hf_upper = 1.5hf;\n"
                                       "}\n";

        Parser parser(source, "float_literals.glsl");
        std::unique_ptr<TrUnit> tr_unit = parser.Parse(eTrUnitType::Compute);
        require_fatal(tr_unit != nullptr);

        std::stringstream ss;
        WriterHLSL().Write(tr_unit.get(), ss);
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
                                     "void main_func( )\n"
                                     "{\n"
                                     "    bar a, b, c;\n"
                                     "    float d = c.b.a.x;\n"
                                     "    float e = c.b.a[0];\n"
                                     "    e = d = 1.0;\n"
                                     "}\n";
        static const char expected[] = "struct foo {\n"
                                       "    float3 a;\n"
                                       "    float2 b;\n"
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
                                       "    ray_hash_t h = (ray_hash_t)(-1, -1);\n"
                                       "}\n"
                                       "void main_func() {\n"
                                       "    bar a;\n"
                                       "    bar b;\n"
                                       "    bar c;\n"
                                       "    float d = c.b.a.x;\n"
                                       "    float e = c.b.a[0];\n"
                                       "    e = d = 1.0;\n"
                                       "}\n";

        Parser parser(source, "structures.glsl");
        std::unique_ptr<TrUnit> tr_unit = parser.Parse(eTrUnitType::Compute);
        require_fatal(tr_unit != nullptr);

        std::stringstream ss;
        WriterHLSL().Write(tr_unit.get(), ss);
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
                                     "void func_main() {\n"
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
                                       "static const float b[1][2] = { { 0.227027029, 0.194594592 } };\n"
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
                                       "void func_main() {\n"
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

        Parser parser(source, "arrays.glsl");
        std::unique_ptr<TrUnit> tr_unit = parser.Parse(eTrUnitType::Compute);
        require_fatal(tr_unit != nullptr);

        std::stringstream ss;
        WriterHLSL().Write(tr_unit.get(), ss);
        require(ss.str() == expected);
    }
    { // if statement
        static const char source[] = "void test() {\n"
                                     "    [[flatten]] if (true) {}\n"
                                     "    [[branch]] if (false) {}\n"
                                     "}\n";
        static const char expected[] = "void test() {\n"
                                       "    [flatten] if (true) {\n"
                                       "    }\n"
                                       "    [branch] if (false) {\n"
                                       "    }\n"
                                       "}\n";

        Parser parser(source, "if_statement.glsl");
        std::unique_ptr<TrUnit> tr_unit = parser.Parse(eTrUnitType::Compute);
        require_fatal(tr_unit != nullptr);

        std::stringstream ss;
        WriterHLSL().Write(tr_unit.get(), ss);
        require(ss.str() == expected);
    }
    { // do statement
        static const char source[] = "void test() {\n"
                                     "    [[unroll, dependency_infinite]] do a(); while (true);\n"
                                     "    [[dont_unroll, dependency_length(4)]] do { } while (true);\n"
                                     "}\n";
        static const char expected[] = "void test() {\n"
                                       "    do a();\n"
                                       "    while (true);\n"
                                       "    do {\n"
                                       "    }\n"
                                       "    while (true);\n"
                                       "}\n";

        Parser parser(source, "do_statement.glsl");
        std::unique_ptr<TrUnit> tr_unit = parser.Parse(eTrUnitType::Compute);
        require_fatal(tr_unit != nullptr);

        std::stringstream ss;
        WriterHLSL().Write(tr_unit.get(), ss);
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
                                       "    while (true) {\n"
                                       "    }\n"
                                       "    while ((i < 10)) {\n"
                                       "    }\n"
                                       "}\n";

        Parser parser(source, "while_statement.glsl");
        std::unique_ptr<TrUnit> tr_unit = parser.Parse(eTrUnitType::Compute);
        require_fatal(tr_unit != nullptr);

        std::stringstream ss;
        WriterHLSL().Write(tr_unit.get(), ss);
        require(ss.str() == expected);
    }
    { // for statement
        static const char source[] = "void func_main() {\n"
                                     "    int i = 0;\n"
                                     "    for(;;) { }\n"
                                     "    for(i = 0;; ) { }\n"
                                     "    [[unroll, dependency_infinite]] for(int i = 0; i < 10;) { }\n"
                                     "    [[dont_unroll, dependency_length(4)]] for(int i = 0; i < 10; i++) { }\n"
                                     "}\n";
        static const char expected[] = "void func_main() {\n"
                                       "    int i = 0;\n"
                                       "    for (;;) {\n"
                                       "    }\n"
                                       "    for (i = 0;;) {\n"
                                       "    }\n"
                                       "    [unroll] for (int i = 0; (i < 10);) {\n"
                                       "    }\n"
                                       "    [loop] for (int i = 0; (i < 10); i++) {\n"
                                       "    }\n"
                                       "}\n";

        Parser parser(source, "for_statement.glsl");
        std::unique_ptr<TrUnit> tr_unit = parser.Parse(eTrUnitType::Compute);
        require_fatal(tr_unit != nullptr);

        std::stringstream ss;
        WriterHLSL().Write(tr_unit.get(), ss);
        require(ss.str() == expected);
    }
    { // ternary
        static const char source[] = "void func_main() {\n"
                                     "    float a = 0, b = 0, c = 0, d = 0;\n"
                                     "    float y = (a < b / 2 ? a - 1 : b - 2);\n"
                                     "    float w = (a ? a,b : b,c);\n"
                                     "    float z = a ? b ? c : d : w;\n"
                                     "}\n";
        static const char expected[] = "void func_main() {\n"
                                       "    float a = 0;\n"
                                       "    float b = 0;\n"
                                       "    float c = 0;\n"
                                       "    float d = 0;\n"
                                       "    float y = ((a < (b / 2)) ? (a - 1) : (b - 2));\n"
                                       "    float w = (a ? (a, b) : (b, c));\n"
                                       "    float z = (a ? (b ? c : d) : w);\n"
                                       "}\n";

        Parser parser(source, "ternary.glsl");
        std::unique_ptr<TrUnit> tr_unit = parser.Parse(eTrUnitType::Compute);
        require_fatal(tr_unit != nullptr);

        std::stringstream ss;
        WriterHLSL().Write(tr_unit.get(), ss);
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
                                       "    [flatten] switch (i) {\n"
                                       "        case 0:\n"
                                       "        break;\n"
                                       "        default:\n"
                                       "        break;\n"
                                       "    }\n"
                                       "    [branch] switch (i) {\n"
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

        Parser parser(source, "switch.glsl");
        std::unique_ptr<TrUnit> tr_unit = parser.Parse(eTrUnitType::Compute);
        require_fatal(tr_unit != nullptr);

        std::stringstream ss;
        WriterHLSL().Write(tr_unit.get(), ss);
        require(ss.str() == expected);
    }
    { // vector stuff
        static const char source[] = "const vec4 g = {1.0, 2.0, 3.0, 4.0};\n"
                                     "vec4 f() { return g; }\n"
                                     "float func(const vec3 color, float x, float y) {\n"
                                     "    vec2 s = {x, y};\n"
                                     "    float t = f()[2];\n"
                                     "    return 0.212671 * color[0] + 0.715160 * color.y + 0.072169 * color.z;\n"
                                     "}\n";
        static const char expected[] =
            "static const float4 g = { 1.0, 2.0, 3.0, 4.0 };\n"
            "float4 f() {\n"
            "    return g;\n"
            "}\n"
            "float func(const float3 color, float x, float y) {\n"
            "    float2 s = { x, y };\n"
            "    float t = f()[2];\n"
            "    return (((0.212670997 * color[0]) + (0.715160012 * color.y)) + (0.0721689984 * color.z));\n"
            "}\n";

        Parser parser(source, "vector.glsl");
        std::unique_ptr<TrUnit> tr_unit = parser.Parse(eTrUnitType::Compute);
        require_fatal(tr_unit != nullptr);

        std::stringstream ss;
        WriterHLSL().Write(tr_unit.get(), ss);
        require(ss.str() == expected);
    }
    { // push constant
        static const char source[] = "struct Params {\n"
                                     "    uvec4 rect;\n"
                                     "    float inv_gamma;\n"
                                     "    int tonemap_mode;\n"
                                     "    float variance_threshold;\n"
                                     "    int iteration;\n"
                                     "};\n"
                                     "layout(push_constant) uniform UniformParams {\n"
                                     "    Params g_params;\n"
                                     "};\n";
        static const char expected[] = "struct Params {\n"
                                       "    uint4 rect;\n"
                                       "    float inv_gamma;\n"
                                       "    int tonemap_mode;\n"
                                       "    float variance_threshold;\n"
                                       "    int iteration;\n"
                                       "};\n"
                                       "cbuffer UniformParams {\n"
                                       "    Params g_params : packoffset(c0);\n"
                                       "};\n";

        Parser parser(source, "push_constant.glsl");
        std::unique_ptr<TrUnit> tr_unit = parser.Parse(eTrUnitType::Compute);
        require_fatal(tr_unit != nullptr);

        std::stringstream ss;
        WriterHLSL().Write(tr_unit.get(), ss);
        require(ss.str() == expected);
    }
    { // uniform buffer
        static const char source[] = "struct atmosphere_params_t {\n"
                                     "    vec4 rayleigh_scattering;\n"
                                     "    vec4 mie_scattering;\n"
                                     "    vec4 mie_extinction;\n"
                                     "    vec4 mie_absorption;\n"
                                     "};\n"
                                     "layout (binding = 3, std140) uniform AtmosphereParams {\n"
                                     "    atmosphere_params_t g_atmosphere_params;\n"
                                     "};\n";
        static const char expected[] = "struct atmosphere_params_t {\n"
                                       "    float4 rayleigh_scattering;\n"
                                       "    float4 mie_scattering;\n"
                                       "    float4 mie_extinction;\n"
                                       "    float4 mie_absorption;\n"
                                       "};\n"
                                       "cbuffer AtmosphereParams : register(b3) {\n"
                                       "    atmosphere_params_t g_atmosphere_params : packoffset(c0);\n"
                                       "};\n";

        Parser parser(source, "push_constant.glsl");
        std::unique_ptr<TrUnit> tr_unit = parser.Parse(eTrUnitType::Compute);
        require_fatal(tr_unit != nullptr);

        std::stringstream ss;
        WriterHLSL().Write(tr_unit.get(), ss);
        require(ss.str() == expected);
    }
    { // compute shader variables
        static const char source[] = "layout (local_size_x = 8, local_size_y = 8, local_size_z = 1) in;\n"
                                     "void main() {\n"
                                     "}\n";
        static const char expected[] =
            "uint4 texelFetch(Texture2D<uint4> t, int2 P, int lod) {\n"
            "    return t.Load(int3(P, lod));\n"
            "}\n"
            "float4 texelFetch(Texture2D<float4> t, int2 P, int lod) {\n"
            "    return t.Load(int3(P, lod));\n"
            "}\n"
            "float4 texelFetch(Texture2D<float4> t, SamplerState s, int2 P, int lod) {\n"
            "    return t.Load(int3(P, lod));\n"
            "}\n"
            "float4 texelFetch(Texture2DArray<float4> t, SamplerState s, int3 P, int lod) {\n"
            "    return t.Load(int4(P, lod));\n"
            "}\n"
            "float4 texelFetch(Texture3D<float4> t, SamplerState s, int3 P, int lod) {\n"
            "    return t.Load(int4(P, lod));\n"
            "}\n"
            "float4 textureLod(Texture2D<float4> t, SamplerState s, float2 P, float lod) {\n"
            "    return t.SampleLevel(s, P, lod);\n"
            "}\n"
            "float4 textureLod(Texture3D<float4> t, SamplerState s, float3 P, float lod) {\n"
            "    return t.SampleLevel(s, P, lod);\n"
            "}\n"
            "float4 textureLod(Texture2DArray<float4> t, SamplerState s, float3 P, float lod) {\n"
            "    return t.SampleLevel(s, P, lod);\n"
            "}\n"
            "float4 textureLodOffset(Texture2D<float4> t, SamplerState s, float2 P, float lod, int2 offset) {\n"
            "    return t.SampleLevel(s, P, lod, offset);\n"
            "}\n"
            "int2 textureSize(Texture2D<float4> t, int lod) {\n"
            "    uint2 ret;\n"
            "    uint NumberOfLevels;\n"
            "    t.GetDimensions(lod, ret.x, ret.y, NumberOfLevels);\n"
            "    return ret;\n"
            "}\n"
            "float4 imageLoad(RWTexture2D<float> image, int2 P) { return float4(image[P], 0, 0, 0); }\n"
            "float4 imageLoad(RWTexture2D<float2> image, int2 P) { return float4(image[P], 0, 0); }\n"
            "float4 imageLoad(RWTexture2D<float4> image, int2 P) { return image[P]; }\n"
            "int4 imageLoad(RWTexture2D<int> image, int2 P) { return int4(image[P], 0, 0, 0); }\n"
            "int4 imageLoad(RWTexture2D<int2> image, int2 P) { return int4(image[P], 0, 0); }\n"
            "int4 imageLoad(RWTexture2D<int4> image, int2 P) { return image[P]; }\n"
            "uint4 imageLoad(RWTexture2D<uint> image, int2 P) { return uint4(image[P], 0, 0, 0); }\n"
            "uint4 imageLoad(RWTexture2D<uint2> image, int2 P) { return uint4(image[P], 0, 0); }\n"
            "uint4 imageLoad(RWTexture2D<uint4> image, int2 P) { return image[P]; }\n"
            "void imageStore(RWTexture2D<float> image, int2 P, float4 data) { image[P] = data.x; }\n"
            "void imageStore(RWTexture2D<float2> image, int2 P, float4 data) { image[P] = data.xy; }\n"
            "void imageStore(RWTexture2D<float4> image, int2 P, float4 data) { image[P] = data; }\n"
            "void imageStore(RWTexture2D<int> image, int2 P, int4 data) { image[P] = data.x; }\n"
            "void imageStore(RWTexture2D<int2> image, int2 P, int4 data) { image[P] = data.xy; }\n"
            "void imageStore(RWTexture2D<int4> image, int2 P, int4 data) { image[P] = data; }\n"
            "void imageStore(RWTexture2D<uint> image, int2 P, uint4 data) { image[P] = data.x; }\n"
            "void imageStore(RWTexture2D<uint2> image, int2 P, uint4 data) { image[P] = data.xy; }\n"
            "void imageStore(RWTexture2D<uint4> image, int2 P, uint4 data) { image[P] = data; }\n"
            "static const uint gl_RayQueryCandidateIntersectionTriangleEXT = CANDIDATE_NON_OPAQUE_TRIANGLE;\n"
            "static const uint gl_RayQueryCandidateIntersectionAABBEXT = CANDIDATE_PROCEDURAL_PRIMITIVE;\n"
            "static const uint gl_RayQueryCommittedIntersectionNoneEXT = COMMITTED_NOTHING;\n"
            "static const uint gl_RayQueryCommittedIntersectionTriangleEXT = COMMITTED_TRIANGLE_HIT;\n"
            "static const uint gl_RayQueryCommittedIntersectionGeneratedEXT = "
            "COMMITTED_PROCEDURAL_PRIMITIVE_HIT;\n"
            "void rayQueryInitializeEXT(RayQuery<RAY_FLAG_NONE> rayQuery,\n"
            "                           RaytracingAccelerationStructure topLevel,\n"
            "                           uint rayFlags, uint cullMask, float3 origin, float tMin,\n"
            "                           float3 direction, float tMax) {\n"
            "    RayDesc desc = {origin, tMin, direction, tMax};\n"
            "    rayQuery.TraceRayInline(topLevel, rayFlags, cullMask, desc);\n"
            "}\n"
            "bool rayQueryProceedEXT(RayQuery<RAY_FLAG_NONE> q) {\n"
            "    return q.Proceed();\n"
            "}\n"
            "uint rayQueryGetIntersectionTypeEXT(RayQuery<RAY_FLAG_NONE> q, bool committed) {\n"
            "    if (committed) {\n"
            "        return q.CommittedStatus();\n"
            "    } else {\n"
            "        return q.CandidateType();\n"
            "    }\n"
            "}\n"
            "void rayQueryConfirmIntersectionEXT(RayQuery<RAY_FLAG_NONE> q) {\n"
            "    q.CommitNonOpaqueTriangleHit();\n"
            "}\n"
            "int rayQueryGetIntersectionInstanceCustomIndexEXT(RayQuery<RAY_FLAG_NONE> q, bool committed) {\n"
            "    if (committed) {\n"
            "        return q.CommittedInstanceID();\n"
            "    } else {\n"
            "        return q.CandidateInstanceID();\n"
            "    }\n"
            "}\n"
            "int rayQueryGetIntersectionInstanceIdEXT(RayQuery<RAY_FLAG_NONE> q, bool committed) {\n"
            "    if (committed) {\n"
            "        return q.CommittedInstanceIndex();\n"
            "    } else {\n"
            "        return q.CandidateInstanceIndex();\n"
            "    }\n"
            "}\n"
            "int rayQueryGetIntersectionPrimitiveIndexEXT(RayQuery<RAY_FLAG_NONE> q, bool committed) {\n"
            "    if (committed) {\n"
            "        return q.CommittedPrimitiveIndex();\n"
            "    } else {\n"
            "        return q.CandidatePrimitiveIndex();\n"
            "    }\n"
            "}\n"
            "bool rayQueryGetIntersectionFrontFaceEXT(RayQuery<RAY_FLAG_NONE> q, bool committed) {\n"
            "    if (committed) {\n"
            "        return q.CommittedTriangleFrontFace();\n"
            "    } else {\n"
            "        return q.CandidateTriangleFrontFace();\n"
            "    }\n"
            "}\n"
            "float2 rayQueryGetIntersectionBarycentricsEXT(RayQuery<RAY_FLAG_NONE> q, bool committed) {\n"
            "    if (committed) {\n"
            "        return q.CommittedTriangleBarycentrics();\n"
            "    } else {\n"
            "        return q.CandidateTriangleBarycentrics();\n"
            "    }\n"
            "}\n"
            "float rayQueryGetIntersectionTEXT(RayQuery<RAY_FLAG_NONE> q, bool committed) {\n"
            "    if (committed) {\n"
            "        return q.CommittedRayT();\n"
            "    } else {\n"
            "        return q.CandidateTriangleRayT();\n"
            "    }\n"
            "}\n"
            "static uint3 gl_WorkGroupID;\n"
            "static uint3 gl_LocalInvocationID;\n"
            "static uint3 gl_GlobalInvocationID;\n"
            "static uint gl_LocalInvocationIndex;\n"
            "static uint gl_SubgroupSize;\n"
            "static uint gl_SubgroupInvocationID;\n"
            "struct GLSLX_Input {\n"
            "    uint3 gl_WorkGroupID : SV_GroupID;\n"
            "    uint3 gl_LocalInvocationID : SV_GroupThreadID;\n"
            "    uint3 gl_GlobalInvocationID : SV_DispatchThreadID;\n"
            "    uint gl_LocalInvocationIndex : SV_GroupIndex;\n"
            "};\n"
            "[numthreads(8, 8, 1)]\n"
            "void main(GLSLX_Input glslx_input) {\n"
            "    gl_WorkGroupID = glslx_input.gl_WorkGroupID;\n"
            "    gl_LocalInvocationID = glslx_input.gl_LocalInvocationID;\n"
            "    gl_GlobalInvocationID = glslx_input.gl_GlobalInvocationID;\n"
            "    gl_LocalInvocationIndex = glslx_input.gl_LocalInvocationIndex;\n"
            "    gl_SubgroupSize = WaveGetLaneCount();\n"
            "    gl_SubgroupInvocationID = WaveGetLaneIndex();\n"
            "}\n";

        Parser parser(source, "compute.glsl");
        std::unique_ptr<TrUnit> tr_unit = parser.Parse(eTrUnitType::Compute);
        require_fatal(tr_unit != nullptr);

        std::stringstream ss;
        WriterHLSL().Write(tr_unit.get(), ss);
        require(ss.str() == expected);
    }
    { // RO buffer
        static const char source[] = "layout(std430, binding = 0) readonly buffer Test {\n"
                                     "    uint g_test[];\n"
                                     "};\n"
                                     "struct data_t {\n"
                                     "    float arr[3];\n"
                                     "    uint i;\n"
                                     "    vec4 v;\n"
                                     "};\n"
                                     "struct data2_t {\n"
                                     "    float arr[3];\n"
                                     "    uint i;\n"
                                     "    float16_t h[8];\n"
                                     "    data_t s;\n"
                                     "};\n"
                                     "layout(std430, binding = 1) readonly buffer Test2 {\n"
                                     "    data2_t g_test2[];\n"
                                     "};\n"
                                     "void test() {\n"
                                     "    uint t = g_test[42];\n"
                                     "    float f = g_test2[24].s.arr[1];\n"
                                     "}\n";
        static const char expected[] = "struct data_t {\n"
                                       "    float arr[3];\n"
                                       "    uint i;\n"
                                       "    float4 v;\n"
                                       "};\n"
                                       "struct data2_t {\n"
                                       "    float arr[3];\n"
                                       "    uint i;\n"
                                       "    half h[8];\n"
                                       "    data_t s;\n"
                                       "};\n"
                                       "ByteAddressBuffer g_test : register(t0, space0);\n"
                                       "uint __load_g_test(int index) {\n"
                                       "    uint ret;\n"
                                       "    ret = g_test.Load(index * 4 + 0);\n"
                                       "    return ret;\n"
                                       "}\n"
                                       "ByteAddressBuffer g_test2 : register(t1, space0);\n"
                                       "data2_t __load_g_test2(int index) {\n"
                                       "    data2_t ret;\n"
                                       "    ret.arr[0] = asfloat(g_test2.Load(index * 64 + 0));\n"
                                       "    ret.arr[1] = asfloat(g_test2.Load(index * 64 + 4));\n"
                                       "    ret.arr[2] = asfloat(g_test2.Load(index * 64 + 8));\n"
                                       "    ret.i = g_test2.Load(index * 64 + 12);\n"
                                       "    ret.h[0] = g_test2.Load<half>(index * 64 + 16);\n"
                                       "    ret.h[1] = g_test2.Load<half>(index * 64 + 18);\n"
                                       "    ret.h[2] = g_test2.Load<half>(index * 64 + 20);\n"
                                       "    ret.h[3] = g_test2.Load<half>(index * 64 + 22);\n"
                                       "    ret.h[4] = g_test2.Load<half>(index * 64 + 24);\n"
                                       "    ret.h[5] = g_test2.Load<half>(index * 64 + 26);\n"
                                       "    ret.h[6] = g_test2.Load<half>(index * 64 + 28);\n"
                                       "    ret.h[7] = g_test2.Load<half>(index * 64 + 30);\n"
                                       "    ret.s.arr[0] = asfloat(g_test2.Load(index * 64 + 32));\n"
                                       "    ret.s.arr[1] = asfloat(g_test2.Load(index * 64 + 36));\n"
                                       "    ret.s.arr[2] = asfloat(g_test2.Load(index * 64 + 40));\n"
                                       "    ret.s.i = g_test2.Load(index * 64 + 44);\n"
                                       "    ret.s.v = asfloat(g_test2.Load4(index * 64 + 48));\n"
                                       "    return ret;\n"
                                       "}\n"
                                       "void test() {\n"
                                       "    uint t = __load_g_test(42);\n"
                                       "    float f = __load_g_test2(24).s.arr[1];\n"
                                       "}\n";

        Parser parser(source, "readonly_buffer.glsl");
        std::unique_ptr<TrUnit> tr_unit = parser.Parse(eTrUnitType::Compute);
        require_fatal(tr_unit != nullptr);

        std::stringstream ss;
        WriterHLSL().Write(tr_unit.get(), ss);
        require(ss.str() == expected);
    }
    { // RW buffer
        static const char source[] = "layout(std430, binding = 0) buffer Test {\n"
                                     "    uint g_test[];\n"
                                     "};\n"
                                     "struct data_t {\n"
                                     "    float arr[3];\n"
                                     "    uint i;\n"
                                     "};\n"
                                     "struct data2_t {\n"
                                     "    float arr[3];\n"
                                     "    uint i;\n"
                                     "    float16_t h[8];\n"
                                     "    data_t s;\n"
                                     "};\n"
                                     "layout(std430, binding = 1) buffer Test2 {\n"
                                     "    data2_t g_test2[];\n"
                                     "};\n"
                                     "void test() {\n"
                                     "    uint t = g_test[42];\n"
                                     "    g_test[42] = t + 1;\n"
                                     "    g_test2[24].s.i = t + 1;\n"
                                     "}\n";
        static const char expected[] = "struct data_t {\n"
                                       "    float arr[3];\n"
                                       "    uint i;\n"
                                       "};\n"
                                       "struct data2_t {\n"
                                       "    float arr[3];\n"
                                       "    uint i;\n"
                                       "    half h[8];\n"
                                       "    data_t s;\n"
                                       "};\n"
                                       "RWByteAddressBuffer g_test : register(u0, space0);\n"
                                       "uint __load_g_test(int index) {\n"
                                       "    uint ret;\n"
                                       "    ret = g_test.Load(index * 4 + 0);\n"
                                       "    return ret;\n"
                                       "}\n"
                                       "RWByteAddressBuffer g_test2 : register(u1, space0);\n"
                                       "data2_t __load_g_test2(int index) {\n"
                                       "    data2_t ret;\n"
                                       "    ret.arr[0] = asfloat(g_test2.Load(index * 48 + 0));\n"
                                       "    ret.arr[1] = asfloat(g_test2.Load(index * 48 + 4));\n"
                                       "    ret.arr[2] = asfloat(g_test2.Load(index * 48 + 8));\n"
                                       "    ret.i = g_test2.Load(index * 48 + 12);\n"
                                       "    ret.h[0] = g_test2.Load<half>(index * 48 + 16);\n"
                                       "    ret.h[1] = g_test2.Load<half>(index * 48 + 18);\n"
                                       "    ret.h[2] = g_test2.Load<half>(index * 48 + 20);\n"
                                       "    ret.h[3] = g_test2.Load<half>(index * 48 + 22);\n"
                                       "    ret.h[4] = g_test2.Load<half>(index * 48 + 24);\n"
                                       "    ret.h[5] = g_test2.Load<half>(index * 48 + 26);\n"
                                       "    ret.h[6] = g_test2.Load<half>(index * 48 + 28);\n"
                                       "    ret.h[7] = g_test2.Load<half>(index * 48 + 30);\n"
                                       "    ret.s.arr[0] = asfloat(g_test2.Load(index * 48 + 32));\n"
                                       "    ret.s.arr[1] = asfloat(g_test2.Load(index * 48 + 36));\n"
                                       "    ret.s.arr[2] = asfloat(g_test2.Load(index * 48 + 40));\n"
                                       "    ret.s.i = g_test2.Load(index * 48 + 44);\n"
                                       "    return ret;\n"
                                       "}\n"
                                       "void test() {\n"
                                       "    uint t = __load_g_test(42);\n"
                                       "    uint __temp0 = (t + 1);\n"
                                       "    uint __offset0 = 42 * 4 + 0;\n"
                                       "    g_test.Store(__offset0 + 0, __temp0);\n"
                                       "    uint __temp1 = (t + 1);\n"
                                       "    uint __offset1 = 24 * 48 + 32;\n"
                                       "    g_test2.Store(__offset1 + 0, __temp1);\n"
                                       "}\n";

        Parser parser(source, "readwrite_buffer.glsl");
        std::unique_ptr<TrUnit> tr_unit = parser.Parse(eTrUnitType::Compute);
        require_fatal(tr_unit != nullptr);

        std::stringstream ss;
        WriterHLSL().Write(tr_unit.get(), ss);
        require(ss.str() == expected);
    }
    { // atomic operations
        static const char source[] = "layout(std430, binding = 0) buffer Test {\n"
                                     "    uint g_test[];\n"
                                     "};\n"
                                     "shared uint g_shared[16];\n"
                                     "void test() {\n"
                                     "    uint v = atomicAdd(g_shared[1], 1u);\n"
                                     "    v = atomicAdd(g_shared[2], 2u);"
                                     "    uint vv = atomicAdd(g_test[3], 1u);\n"
                                     "    vv = atomicAdd(g_test[4], 2u);\n"
                                     "}\n";
        static const char expected[] = "groupshared uint g_shared[16];\n"
                                       "RWByteAddressBuffer g_test : register(u0, space0);\n"
                                       "uint __load_g_test(int index) {\n"
                                       "    uint ret;\n"
                                       "    ret = g_test.Load(index * 4 + 0);\n"
                                       "    return ret;\n"
                                       "}\n"
                                       "void test() {\n"
                                       "    uint __temp0;\n"
                                       "    InterlockedAdd(g_shared[1], 1u, __temp0);\n"
                                       "    uint v = __temp0;\n"
                                       "    uint __temp1;\n"
                                       "    InterlockedAdd(g_shared[2], 2u, __temp1);\n"
                                       "    v = __temp1;\n"
                                       "    uint __temp2;\n"
                                       "    g_test.InterlockedAdd(4 * 3, 1u, __temp2);\n"
                                       "    uint vv = __temp2;\n"
                                       "    uint __temp3;\n"
                                       "    g_test.InterlockedAdd(4 * 4, 2u, __temp3);\n"
                                       "    vv = __temp3;\n"
                                       "}\n";

        Parser parser(source, "atomic.glsl");
        std::unique_ptr<TrUnit> tr_unit = parser.Parse(eTrUnitType::Compute);
        require_fatal(tr_unit != nullptr);

        std::stringstream ss;
        WriterHLSL().Write(tr_unit.get(), ss);
        require(ss.str() == expected);
    }
    { // matrix multiply
        static const char source[] = "vec3 test(vec3 n, mat4 xform) {\n"
                                     "    return (transpose(xform) * vec4(n, 0.0)).xyz;\n"
                                     "}\n";
        static const char expected[] = "float3 test(float3 n, row_major float4x4 xform) {\n"
                                       "    return mul(float4(n, 0.0), transpose(xform)).xyz;\n"
                                       "}\n";

        Parser parser(source, "atomic.glsl");
        std::unique_ptr<TrUnit> tr_unit = parser.Parse(eTrUnitType::Compute);
        require_fatal(tr_unit != nullptr);

        std::stringstream ss;
        WriterHLSL().Write(tr_unit.get(), ss);
        require(ss.str() == expected);
    }

    printf("OK\n");
}
