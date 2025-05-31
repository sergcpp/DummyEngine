#ifndef _CS_COMMON_GLSL
#define _CS_COMMON_GLSL

#include "_common.glsl"

#if defined(VULKAN)
#define GetCellIndex(ix, iy, slice, res) \
    (slice * ITEM_GRID_RES_X * ITEM_GRID_RES_Y + ((int(res.y) - 1 - iy) * ITEM_GRID_RES_Y / int(res.y)) * ITEM_GRID_RES_X + ix * ITEM_GRID_RES_X / int(res.x))
#else
#define GetCellIndex(ix, iy, slice, res) \
    (slice * ITEM_GRID_RES_X * ITEM_GRID_RES_Y + (iy * ITEM_GRID_RES_Y / int(res.y)) * ITEM_GRID_RES_X + ix * ITEM_GRID_RES_X / int(res.x))
#endif

// Rounds value to the nearest multiple of 8
uvec2 RoundUp8(uvec2 value) {
    uvec2 round_down = value & ~7u; // 0b111
    return (round_down == value) ? value : value + 8;
}

/*vec3 EvalSHIrradiance(vec3 normal, vec3 sh_l_00, vec3 sh_l_10, vec3 sh_l_11,
                      vec3 sh_l_12) {
    return max((0.5 + (sh_l_10 * normal.y + sh_l_11 * normal.z +
                       sh_l_12 * normal.x)) * sh_l_00 * 2.0, vec3(0.0));
}

vec3 EvalSHIrradiance_NonLinear(vec3 normal, vec3 sh_l_00, vec3 sh_l_10, vec3 sh_l_11,
                                vec3 sh_l_12) {
    vec3 l = sqrt(sh_l_10 * sh_l_10 + sh_l_11 * sh_l_11 + sh_l_12 * sh_l_12);
    vec3 inv_l = mix(vec3(0.0), vec3(1.0) / l, step(l, vec3(FLT_EPS)));

    vec3 q = 0.5 * (vec3(1.0) + (sh_l_10 * normal.y + sh_l_11 * normal.z +
                                 sh_l_12 * normal.x) * inv_l);
    vec3 p = vec3(1.0) + 2.0 * l;
    vec3 a = (vec3(1.0) - l) / (vec3(1.0) + l);

    return sh_l_00 * (a + (vec3(1.0) - a) * (p + vec3(1.0)) * pow(q, p));
}

vec3 EvalSHIrradiance_NonLinear(vec3 dir, vec4 sh_r, vec4 sh_g, vec4 sh_b) {
    vec3 R1_len = vec3(length(sh_r.yzw), length(sh_g.yzw), length(sh_b.yzw));
    vec3 R1_inv_len = mix(vec3(0.0), vec3(1.0) / R1_len, step(vec3(FLT_EPS), R1_len));
    vec3 R0 = vec3(sh_r.x, sh_g.x, sh_b.x);

    vec3 q = 0.5 * (vec3(1.0) + vec3(dot(dir.yzx, sh_r.yzw), dot(dir.yzx, sh_g.yzw),
                                     dot(dir.yzx, sh_b.yzw)) * R1_inv_len);
    vec3 p = vec3(1.0) + 2.0 * R1_len / R0;
    vec3 a = (vec3(1.0) - R1_len / R0) / (vec3(1.0) + R1_len / R0);

    return R0 * (a + (vec3(1.0) - a) * (p + vec3(1.0)) * pow(q, p));
}

vec3 EvaluateSH(const vec3 normal, const  vec4 sh_coeffs[3]) {
    const float SH_A0 = 0.886226952; // PI / sqrt(4.0 * Pi)
    const float SH_A1 = 1.02332675;  // sqrt(PI / 3.0)

    const vec4 vv = vec4(SH_A0, SH_A1 * normal.yzx);

    return vec3(dot(sh_coeffs[0], vv), dot(sh_coeffs[1], vv), dot(sh_coeffs[2], vv));
}
*/

//
// Index remapping (nested quads)
//
//  00 01 02 03 04 05 06 07           00 01 08 09 10 11 18 19
//  08 09 0a 0b 0c 0d 0e 0f           02 03 0a 0b 12 13 1a 1b
//  10 11 12 13 14 15 16 17           04 05 0c 0d 14 15 1c 1d
//  18 19 1a 1b 1c 1d 1e 1f   ---->   06 07 0e 0f 16 17 1e 1f
//  20 21 22 23 24 25 26 27           20 21 28 29 30 31 38 39
//  28 29 2a 2b 2c 2d 2e 2f           22 23 2a 2b 32 33 3a 3b
//  30 31 32 33 34 35 36 37           24 25 2c 2d 34 35 3c 3d
//  38 39 3a 3b 3c 3d 3e 3f           26 27 2e 2f 36 37 3e 3f
uvec2 RemapLane8x8(const uint lane) {
    return uvec2(bitfieldInsert(bitfieldExtract(lane, 2, 3), lane, 0, 1),
                 bitfieldInsert(bitfieldExtract(lane, 3, 3), bitfieldExtract(lane, 1, 2), 0, 2));
}

#endif // _CS_COMMON_GLSL