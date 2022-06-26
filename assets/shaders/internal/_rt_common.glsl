
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
