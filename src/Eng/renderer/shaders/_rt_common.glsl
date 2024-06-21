
#include "_cs_common.glsl"

const float NormalBiasConstant = 0.00025;
const float NormalBiasPosAddition = 0.000025;
const float NormalBiasViewAddition = 0.000025;

const uint RTGeoProbeBits = 0xff;
const uint RTGeoLightmappedBit = (1u << 8u);

struct RTGeoInstance {
    uint indices_start;
    uint vertices_start;
    uint material_index;
    uint flags;
    vec4 lmap_transform;
};

vec3 offset_ray(vec3 p, vec3 n) {
    const float Origin = 1.0 / 32.0;
    const float FloatScale = 1.0 / 65536.0;
    const float IntScale = 256.0;

    ivec3 of_i = ivec3(IntScale * n);

    vec3 p_i = vec3(intBitsToFloat(floatBitsToInt(p.x) + ((p.x < 0.0) ? -of_i.x : of_i.x)),
                    intBitsToFloat(floatBitsToInt(p.y) + ((p.y < 0.0) ? -of_i.y : of_i.y)),
                    intBitsToFloat(floatBitsToInt(p.z) + ((p.z < 0.0) ? -of_i.z : of_i.z)));

    return vec3(abs(p[0]) < Origin ? (p[0] + FloatScale * n[0]) : p_i[0],
                abs(p[1]) < Origin ? (p[1] + FloatScale * n[1]) : p_i[1],
                abs(p[2]) < Origin ? (p[2] + FloatScale * n[2]) : p_i[2]);
}

float GetHitDistanceNormalization(float viewZ, float roughness) {
    // (units) - constant value
    const float A = 3.0;
    // (> 0) - viewZ based linear scale (1 m - 10 cm, 10 m - 1 m, 100 m - 10 m)
    const float B = 0.1;
    // (>= 1) - roughness based scale, use values > 1 to get bigger hit distance for low roughness
    const float C = 20.0;
    // (<= 0) - absolute value should be big enough to collapse "exp2( D * roughness ^ 2 )" to "~0" for roughness = 1
    const float D = -25.0;

    return (A + abs(viewZ) * B) * mix(1.0, C, saturate(exp2(D * roughness * roughness)));
}

float GetNormHitDist(float hitDist, float viewZ, float roughness) {
    return saturate(hitDist / GetHitDistanceNormalization(viewZ, roughness));
}
