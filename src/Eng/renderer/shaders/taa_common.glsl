#ifndef TAA_COMMON_GLSL
#define TAA_COMMON_GLSL

// http://twvideo01.ubm-us.net/o1/vault/gdc2016/Presentations/Pedersen_LasseJonFuglsang_TemporalReprojectionAntiAliasing.pdf
vec3 ClipAABB(vec3 aabb_min, vec3 aabb_max, vec3 q) {
    vec3 p_clip = 0.5 * (aabb_max + aabb_min);
    vec3 e_clip = 0.5 * (aabb_max - aabb_min) + 0.0001;

    vec3 v_clip = q - p_clip;
    vec3 v_unit = v_clip.xyz / e_clip;
    vec3 a_unit = abs(v_unit);
    float ma_unit = max3(a_unit.x, a_unit.y, a_unit.z);

    if (ma_unit > 1.0) {
        return p_clip + v_clip / ma_unit;
    } else {
        return q; // point inside aabb
    }
}

// https://software.intel.com/en-us/node/503873
vec3 RGB_to_YCoCg(vec3 c) {
    // Y = R/4 + G/2 + B/4
    // Co = R/2 - B/2
    // Cg = -R/4 + G/2 - B/4
    return vec3(
         c.x/4.0 + c.y/2.0 + c.z/4.0,
         c.x/2.0 - c.z/2.0,
        -c.x/4.0 + c.y/2.0 - c.z/4.0
    );
}

vec3 YCoCg_to_RGB(vec3 c) {
    // R = Y + Co - Cg
    // G = Y + Cg
    // B = Y - Co - Cg
    return max(vec3(
        c.x + c.y - c.z,
        c.x + c.z,
        c.x - c.y - c.z
    ), vec3(0.0));
}

vec3 FindClosestFragment_3x3(sampler2D dtex, const vec2 uv, const vec2 texel_size) {
    const vec3 dtl = vec3(-1, -1, textureLodOffset(dtex, uv, 0.0, ivec2(-1, -1)).x);
    const vec3 dtc = vec3( 0, -1, textureLodOffset(dtex, uv, 0.0, ivec2( 0, -1)).x);
    const vec3 dtr = vec3( 1, -1, textureLodOffset(dtex, uv, 0.0, ivec2( 1, -1)).x);

    const vec3 dml = vec3(-1,  0, textureLodOffset(dtex, uv, 0.0, ivec2(-1,  0)).x);
    const vec3 dmc = vec3( 0,  0, textureLodOffset(dtex, uv, 0.0, ivec2( 0,  0)).x);
    const vec3 dmr = vec3( 1,  0, textureLodOffset(dtex, uv, 0.0, ivec2( 1,  0)).x);

    const vec3 dbl = vec3(-1,  1, textureLodOffset(dtex, uv, 0.0, ivec2(-1,  1)).x);
    const vec3 dbc = vec3( 0,  1, textureLodOffset(dtex, uv, 0.0, ivec2( 0,  1)).x);
    const vec3 dbr = vec3( 1,  1, textureLodOffset(dtex, uv, 0.0, ivec2( 1,  1)).x);

    vec3 dmin = dtl;
    if (dmin.z < dtc.z) dmin = dtc;
    if (dmin.z < dtr.z) dmin = dtr;

    if (dmin.z < dml.z) dmin = dml;
    if (dmin.z < dmc.z) dmin = dmc;
    if (dmin.z < dmr.z) dmin = dmr;

    if (dmin.z < dbl.z) dmin = dbl;
    if (dmin.z < dbc.z) dmin = dbc;
    if (dmin.z < dbr.z) dmin = dbr;

    return vec3(uv + texel_size * dmin.xy, dmin.z);
}

// Taken from http://vec3.ca/bicubic-filtering-in-fewer-taps/
vec4 SampleCatmulRom4x4_9Tap(const sampler2D tex, const vec2 uv, const vec4 tex_size_inv_size) {
    const vec2 sample_pos = uv * tex_size_inv_size.xy;
    const vec2 tex_pos1 = floor(sample_pos - 0.5) + 0.5;

    const vec2 f = sample_pos - tex_pos1;

    vec2 w0 = f * (-0.5 + f * (1.0 - 0.5 * f));
    vec2 w1 = 1.0 + f * f * (-2.5 + 1.5 * f);
    vec2 w2 = f * (0.5 + f * (2.0 - 1.5 * f));
    vec2 w3 = f * f * (-0.5 + 0.5 * f);

    vec2 w12 = w1 + w2;
    vec2 offset12 = w2 / (w1 + w2);

    vec2 tex_pos0 = tex_pos1 - 1.0;
    vec2 tex_pos3 = tex_pos1 + 2.0;
    vec2 tex_pos12 = tex_pos1 + offset12;

    tex_pos0 *= tex_size_inv_size.zw;
    tex_pos3 *= tex_size_inv_size.zw;
    tex_pos12 *= tex_size_inv_size.zw;

    vec4 result = vec4(0.0);

    result += textureLod(tex, vec2(tex_pos0.x,  tex_pos0.y), 0.0) * w0.x * w0.y;
    result += textureLod(tex, vec2(tex_pos12.x, tex_pos0.y), 0.0) * w12.x * w0.y;
    result += textureLod(tex, vec2(tex_pos3.x,  tex_pos0.y), 0.0) * w3.x * w0.y;

    result += textureLod(tex, vec2(tex_pos0.x,  tex_pos12.y), 0.0) * w0.x * w12.y;
    result += textureLod(tex, vec2(tex_pos12.x, tex_pos12.y), 0.0) * w12.x * w12.y;
    result += textureLod(tex, vec2(tex_pos3.x,  tex_pos12.y), 0.0) * w3.x * w12.y;

    result += textureLod(tex, vec2(tex_pos0.x,  tex_pos3.y), 0.0) * w0.x * w3.y;
    result += textureLod(tex, vec2(tex_pos12.x, tex_pos3.y), 0.0) * w12.x * w3.y;
    result += textureLod(tex, vec2(tex_pos3.x,  tex_pos3.y), 0.0) * w3.x * w3.y;

    return result;
}

// Modified version from: https://gist.github.com/TheRealMJP/c83b8c0f46b63f3a88a5986f4fa982b1
// Corners are skipped (as Jimenez does in SMAA), alpha is taken from center sample only
vec4 SampleCatmulRom4x4_5Tap(const sampler2D tex, const vec2 uv, const vec4 tex_size_inv_size) {
    const vec2 pos = uv * tex_size_inv_size.xy;
    const vec2 center_pos = floor(pos - 0.5) + 0.5;
    const vec2 f = pos - center_pos;

    const vec2 w0 = f * (-0.5 + f * (1.0 - 0.5 * f));
    const vec2 w1 = 1.0 + f * f * (1.5 * f - 2.5);
    const vec2 w2 = f * (0.5 + f * (2.0 - 1.5 * f));
    const vec2 w3 = f * f * (0.5 * f - 0.5);

    const vec2 w12 = w1 + w2;
    const vec2 tc12 = (center_pos + w2 / w12) * tex_size_inv_size.zw;
    const vec2 tc0 = (center_pos - 1.0) * tex_size_inv_size.zw;
    const vec2 tc3 = (center_pos + 2.0) * tex_size_inv_size.zw;

    // We use alpha channel from center sample only as an optimization
    const vec4 center_value = textureLod(tex, vec2(tc12.x, tc12.y), 0.0);

    const vec4 result = vec4(textureLod(tex, vec2(tc12.x, tc0.y ), 0.0).xyz, 1.0) * (w12.x * w0.y ) +
                        vec4(textureLod(tex, vec2(tc0.x,  tc12.y), 0.0).xyz, 1.0) * (w0.x  * w12.y) +
                        vec4(center_value.xyz, 1.0) * (w12.x * w12.y) +
                        vec4(textureLod(tex, vec2(tc3.x,  tc12.y), 0.0).xyz, 1.0) * (w3.x  * w12.y) +
                        vec4(textureLod(tex, vec2(tc12.x, tc3.y ), 0.0).xyz, 1.0) * (w12.x * w3.y );

    return vec4(result.xyz * rcp(result.w), center_value.w);
}

#endif // TAA_COMMON_GLSL