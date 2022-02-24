#ifndef BLIT_COMBINE_INTERFACE_GLSL
#define BLIT_COMBINE_INTERFACE_GLSL

#include "_interface_common.glsl"

INTERFACE_START(BlitCombine)

struct Params {
    VEC4_TYPE transform;
    VEC2_TYPE tex_size;
    float tonemap;
    float gamma;
    float exposure;
    float fade;
};

DEF_CONST_INT(HDR_TEX_SLOT, 0)
DEF_CONST_INT(BLURED_TEX_SLOT, 1)

INTERFACE_END

#endif // BLIT_COMBINE_INTERFACE_GLSL