R"(
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

bvec2 lessThan(vec2 x, vec2 y) { return bvec2(x[0] < y[0], x[1] < y[1]); }
bvec3 lessThan(vec3 x, vec3 y) { return bvec3(x[0] < y[0], x[1] < y[1], x[2] < y[2]); }
bvec4 lessThan(vec4 x, vec4 y) { return bvec4(x[0] < y[0], x[1] < y[1], x[2] < y[2], x[3] < y[3]); }
bvec2 lessThan(ivec2 x, ivec2 y) { return bvec2(x[0] < y[0], x[1] < y[1]); }
bvec3 lessThan(ivec3 x, ivec3 y) { return bvec3(x[0] < y[0], x[1] < y[1], x[2] < y[2]); }
bvec4 lessThan(ivec4 x, ivec4 y) { return bvec4(x[0] < y[0], x[1] < y[1], x[2] < y[2], x[3] < y[3]); }
bvec2 lessThan(uvec2 x, uvec2 y) { return bvec2(x[0] < y[0], x[1] < y[1]); }
bvec3 lessThan(uvec3 x, uvec3 y) { return bvec3(x[0] < y[0], x[1] < y[1], x[2] < y[2]); }
bvec4 lessThan(uvec4 x, uvec4 y) { return bvec4(x[0] < y[0], x[1] < y[1], x[2] < y[2], x[3] < y[3]); }

bvec2 lessThanEqual(vec2 x, vec2 y) { return bvec2(x[0] <= y[0], x[1] <= y[1]); }
bvec3 lessThanEqual(vec3 x, vec3 y) { return bvec3(x[0] <= y[0], x[1] <= y[1], x[2] <= y[2]); }
bvec4 lessThanEqual(vec4 x, vec4 y) { return bvec4(x[0] <= y[0], x[1] <= y[1], x[2] <= y[2], x[3] <= y[3]); }
bvec2 lessThanEqual(ivec2 x, ivec2 y) { return bvec2(x[0] <= y[0], x[1] <= y[1]); }
bvec3 lessThanEqual(ivec3 x, ivec3 y) { return bvec3(x[0] <= y[0], x[1] <= y[1], x[2] <= y[2]); }
bvec4 lessThanEqual(ivec4 x, ivec4 y) { return bvec4(x[0] <= y[0], x[1] <= y[1], x[2] <= y[2], x[3] <= y[3]); }
bvec2 lessThanEqual(uvec2 x, uvec2 y) { return bvec2(x[0] <= y[0], x[1] <= y[1]); }
bvec3 lessThanEqual(uvec3 x, uvec3 y) { return bvec3(x[0] <= y[0], x[1] <= y[1], x[2] <= y[2]); }
bvec4 lessThanEqual(uvec4 x, uvec4 y) { return bvec4(x[0] <= y[0], x[1] <= y[1], x[2] <= y[2], x[3] <= y[3]); }

bvec2 greaterThan(vec2 x, vec2 y) { return bvec2(x[0] > y[0], x[1] > y[1]); }
bvec3 greaterThan(vec3 x, vec3 y) { return bvec3(x[0] > y[0], x[1] > y[1], x[2] > y[2]); }
bvec4 greaterThan(vec4 x, vec4 y) { return bvec4(x[0] > y[0], x[1] > y[1], x[2] > y[2], x[3] > y[3]); }
bvec2 greaterThan(ivec2 x, ivec2 y) { return bvec2(x[0] > y[0], x[1] > y[1]); }
bvec3 greaterThan(ivec3 x, ivec3 y) { return bvec3(x[0] > y[0], x[1] > y[1], x[2] > y[2]); }
bvec4 greaterThan(ivec4 x, ivec4 y) { return bvec4(x[0] > y[0], x[1] > y[1], x[2] > y[2], x[3] > y[3]); }
bvec2 greaterThan(uvec2 x, uvec2 y) { return bvec2(x[0] > y[0], x[1] > y[1]); }
bvec3 greaterThan(uvec3 x, uvec3 y) { return bvec3(x[0] > y[0], x[1] > y[1], x[2] > y[2]); }
bvec4 greaterThan(uvec4 x, uvec4 y) { return bvec4(x[0] > y[0], x[1] > y[1], x[2] > y[2], x[3] > y[3]); }

bvec2 greaterThanEqual(vec2 x, vec2 y) { return bvec2(x[0] >= y[0], x[1] >= y[1]); }
bvec3 greaterThanEqual(vec3 x, vec3 y) { return bvec3(x[0] >= y[0], x[1] >= y[1], x[2] >= y[2]); }
bvec4 greaterThanEqual(vec4 x, vec4 y) { return bvec4(x[0] >= y[0], x[1] >= y[1], x[2] >= y[2], x[3] >= y[3]); }
bvec2 greaterThanEqual(ivec2 x, ivec2 y) { return bvec2(x[0] >= y[0], x[1] >= y[1]); }
bvec3 greaterThanEqual(ivec3 x, ivec3 y) { return bvec3(x[0] >= y[0], x[1] >= y[1], x[2] >= y[2]); }
bvec4 greaterThanEqual(ivec4 x, ivec4 y) { return bvec4(x[0] >= y[0], x[1] >= y[1], x[2] >= y[2], x[3] >= y[3]); }
bvec2 greaterThanEqual(uvec2 x, uvec2 y) { return bvec2(x[0] >= y[0], x[1] >= y[1]); }
bvec3 greaterThanEqual(uvec3 x, uvec3 y) { return bvec3(x[0] >= y[0], x[1] >= y[1], x[2] >= y[2]); }
bvec4 greaterThanEqual(uvec4 x, uvec4 y) { return bvec4(x[0] >= y[0], x[1] >= y[1], x[2] >= y[2], x[3] >= y[3]); }

bvec2 equal(vec2 x, vec2 y) { return bvec2(x[0] == y[0], x[1] == y[1]); }
bvec3 equal(vec3 x, vec3 y) { return bvec3(x[0] == y[0], x[1] == y[1], x[2] == y[2]); }
bvec4 equal(vec4 x, vec4 y) { return bvec4(x[0] == y[0], x[1] == y[1], x[2] == y[2], x[3] == y[3]); }
bvec2 equal(ivec2 x, ivec2 y) { return bvec2(x[0] == y[0], x[1] == y[1]); }
bvec3 equal(ivec3 x, ivec3 y) { return bvec3(x[0] == y[0], x[1] == y[1], x[2] == y[2]); }
bvec4 equal(ivec4 x, ivec4 y) { return bvec4(x[0] == y[0], x[1] == y[1], x[2] == y[2], x[3] == y[3]); }
bvec2 equal(uvec2 x, uvec2 y) { return bvec2(x[0] == y[0], x[1] == y[1]); }
bvec3 equal(uvec3 x, uvec3 y) { return bvec3(x[0] == y[0], x[1] == y[1], x[2] == y[2]); }
bvec4 equal(uvec4 x, uvec4 y) { return bvec4(x[0] == y[0], x[1] == y[1], x[2] == y[2], x[3] == y[3]); }
bvec2 equal(bvec2 x, bvec2 y) { return bvec2(x[0] == y[0], x[1] == y[1]); }
bvec3 equal(bvec3 x, bvec3 y) { return bvec3(x[0] == y[0], x[1] == y[1], x[2] == y[2]); }
bvec4 equal(bvec4 x, bvec4 y) { return bvec4(x[0] == y[0], x[1] == y[1], x[2] == y[2], x[3] == y[3]); }

bvec2 notEqual(vec2 x, vec2 y) { return bvec2(x[0] != y[0], x[1] != y[1]); }
bvec3 notEqual(vec3 x, vec3 y) { return bvec3(x[0] != y[0], x[1] != y[1], x[2] != y[2]); }
bvec4 notEqual(vec4 x, vec4 y) { return bvec4(x[0] != y[0], x[1] != y[1], x[2] != y[2], x[3] != y[3]); }
bvec2 notEqual(ivec2 x, ivec2 y) { return bvec2(x[0] != y[0], x[1] != y[1]); }
bvec3 notEqual(ivec3 x, ivec3 y) { return bvec3(x[0] != y[0], x[1] != y[1], x[2] != y[2]); }
bvec4 notEqual(ivec4 x, ivec4 y) { return bvec4(x[0] != y[0], x[1] != y[1], x[2] != y[2], x[3] != y[3]); }
bvec2 notEqual(uvec2 x, uvec2 y) { return bvec2(x[0] != y[0], x[1] != y[1]); }
bvec3 notEqual(uvec3 x, uvec3 y) { return bvec3(x[0] != y[0], x[1] != y[1], x[2] != y[2]); }
bvec4 notEqual(uvec4 x, uvec4 y) { return bvec4(x[0] != y[0], x[1] != y[1], x[2] != y[2], x[3] != y[3]); }
bvec2 notEqual(bvec2 x, bvec2 y) { return bvec2(x[0] != y[0], x[1] != y[1]); }
bvec3 notEqual(bvec3 x, bvec3 y) { return bvec3(x[0] != y[0], x[1] != y[1], x[2] != y[2]); }
bvec4 notEqual(bvec4 x, bvec4 y) { return bvec4(x[0] != y[0], x[1] != y[1], x[2] != y[2], x[3] != y[3]); }

bool all(bvec2 x) { return x[0] || x[1]; }
bool all(bvec3 x) { return x[0] || x[1] || x[2]; }
bool all(bvec4 x) { return x[0] || x[1] || x[2] || x[3]; }

bool all(bvec2 x) { return x[0] && x[1]; }
bool all(bvec3 x) { return x[0] && x[1] && x[2]; }
bool all(bvec4 x) { return x[0] && x[1] && x[2] && x[3]; }

bvec2 not(bvec2 x) { return bvec2(!x[0], !x[1]); }
bvec3 not(bvec3 x) { return bvec3(!x[0], !x[1], !x[2]); }
bvec4 not(bvec4 x) { return bvec4(!x[0], !x[1], !x[2], !x[3]); }
)";
