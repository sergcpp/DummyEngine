#version 430 core
#if !defined(VULKAN) && !defined(NO_BINDLESS)
#extension GL_ARB_bindless_texture : enable
#endif

#include "_fs_common.glsl"
#include "texturing_common.glsl"
#include "depth_peel_interface.h"

#pragma multi_compile _ NO_BINDLESS

#if defined(NO_BINDLESS) && defined(VULKAN)
    #pragma dont_compile
#endif

layout (binding = BIND_UB_SHARED_DATA_BUF, std140) uniform SharedDataBlock {
    SharedData g_shrd_data;
};

layout(binding = OUT_IMG_BUF_SLOT, r32ui) uniform restrict coherent uimageBuffer g_out_depth;

layout (early_fragment_tests) in;

void main() {
    // https://on-demand.gputechconf.com/gtc/2014/presentations/S4385-order-independent-transparency-opengl.pdf
    int frag_index = int(gl_FragCoord.y) * g_shrd_data.ires_and_ifres.x + int(gl_FragCoord.x);
    uint ztest = floatBitsToUint(gl_FragCoord.z);
    for (int i = 0; i < OIT_LAYERS_COUNT; ++i) {
        const uint zold = imageAtomicMax(g_out_depth, frag_index, ztest);
        if (zold == 0u || zold == ztest) {
            break;
        }
        ztest = min(zold, ztest);
        frag_index += g_shrd_data.ires_and_ifres.x * g_shrd_data.ires_and_ifres.y;
    }
}
