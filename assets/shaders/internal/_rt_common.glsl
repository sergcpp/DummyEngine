
#include "_cs_common.glsl"

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