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
    if (dmin.z > dtc.z) dmin = dtc;
    if (dmin.z > dtr.z) dmin = dtr;

    if (dmin.z > dml.z) dmin = dml;
    if (dmin.z > dmc.z) dmin = dmc;
    if (dmin.z > dmr.z) dmin = dmr;

    if (dmin.z > dbl.z) dmin = dbl;
    if (dmin.z > dbc.z) dmin = dbc;
    if (dmin.z > dbr.z) dmin = dbr;

    return vec3(uv + texel_size * dmin.xy, dmin.z);
}

#endif // TAA_COMMON_GLSL