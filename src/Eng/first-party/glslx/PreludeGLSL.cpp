#include "PreludeGLSL.h"

namespace glslx {
// https://registry.khronos.org/OpenGL/specs/gl/GLSLangSpec.4.60.html#built-in-functions
const char g_builtin_prototypes[] = 
#include "parser/BuiltinPrototypes.inl"
;

const char g_glsl_prelude[] = R"(
uint uaddCarry(uint x, uint y, uint carryIn, out uint carryOut) {
    carryOut = (x > ~y) || (x + ~y < ~carryIn);
    return x + y + carryIn;
}
uint usubBorrow(uint x, uint y, uint borrowIn, out uint borrowOut) {
    borrowOut = (x < y) || (x - y > borrowIn);
    return x - y - borrowIn;
}
int bitfieldReverse(int x) {
    x = (((x & 0xaaaaaaaa) >> 1) | ((x & 0x55555555) << 1));
    x = (((x & 0xcccccccc) >> 2) | ((x & 0x33333333) << 2));
    x = (((x & 0xf0f0f0f0) >> 4) | ((x & 0x0f0f0f0f) << 4));
    x = (((x & 0xff00ff00) >> 8) | ((x & 0x00ff00ff) << 8));
    return ((x >> 16) | (x << 16));
}
uint bitfieldReverse(uint x) {
    x = (((x & 0xaaaaaaaa) >> 1) | ((x & 0x55555555) << 1));
    x = (((x & 0xcccccccc) >> 2) | ((x & 0x33333333) << 2));
    x = (((x & 0xf0f0f0f0) >> 4) | ((x & 0x0f0f0f0f) << 4));
    x = (((x & 0xff00ff00) >> 8) | ((x & 0x00ff00ff) << 8));
    return ((x >> 16) | (x << 16));
}

//uint half2float(uint h) {
//	return ((h & uint(0x8000)) << uint(16)) | ((( h & uint(0x7c00)) + uint(0x1c000)) << uint(13)) | ((h & uint(0x03ff)) << uint(13));
//}

uint packUnorm1x16(float s) {
    return uint(round(clamp(s, 0.0, 1.0) * 65535.0));
}

float unpackUnorm1x16(uint p) {
    return float(p) * 0.000015259021;				// 1.0 / 65535.0 optimization
}

uint packSnorm1x16(float v) {
    return uint(round(clamp(v ,-1.0, 1.0) * 32767.0) + 32767.0);
}

float unpackSnorm1x16(uint p) {
    return clamp((float(p) - 32767.0) * 0.00003051851, -1.0, 1.0);	// 1.0 / 32767.0 optimization
}

//uint float2half(uint f) {
//	return	((f >> uint(16)) & uint(0x8000)) |
//		((((f & uint(0x7f800000)) - uint(0x38000000)) >> uint(13)) & uint(0x7c00)) |
//		((f >> uint(13)) & uint(0x03ff));
//}

uint packHalf2x16(vec2 v) {
    return f32tof16(v.x) | f32tof16(v.y) << uint(16);
}

vec2 unpackHalf2x16(uint v) {
    return vec2(f16tof32(v & uint(0xffff)),
                f16tof32(v >> uint(16)));
}

uint packUnorm2x16(vec2 v) {
    return packUnorm1x16(v.x) | packUnorm1x16(v.y) << uint(16);
}

vec2 unpackUnorm2x16(uint p) {
    return vec2(unpackUnorm1x16(p & uint(0xffff)),
                unpackUnorm1x16(p >> uint(16)));
}

uint packSnorm2x16(vec2 v) {
    return packSnorm1x16(v.x) | packSnorm1x16(v.y) << uint(16);
}

vec2 unpackSnorm2x16(uint p) {
    return vec2(unpackSnorm1x16(p & uint(0xffff)),
                unpackSnorm1x16(p >> uint(16)));
}

uint packUnorm4x8(vec4 value) {
    uvec4 Packed = uvec4(round(clamp(value, 0.0, 1.0) * 255.0));
    return Packed.x | (Packed.y << 8) | (Packed.z << 16) | (Packed.w << 24);
}

vec4 unpackUnorm4x8(uint value) {
    uvec4 Packed = uvec4(value & 0xff, (value >> 8) & 0xff, (value >> 16) & 0xff, value >> 24);
    return vec4(Packed) / 255.0;
}

float atan(float x, float y) { return atan2(x, y); }
)";
} // namespace glslx