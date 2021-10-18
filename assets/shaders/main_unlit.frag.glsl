#version 310 es
#extension GL_EXT_texture_buffer : enable
#extension GL_OES_texture_buffer : enable
#extension GL_EXT_texture_cube_map_array : enable
//#extension GL_EXT_control_flow_attributes : enable

$ModifyWarning

#if defined(GL_ES) || defined(VULKAN)
    precision highp int;
    precision highp float;
    precision mediump sampler2DShadow;
#endif

#include "internal/_fs_common.glsl"
#include "internal/_texturing.glsl"

#define LIGHT_ATTEN_CUTOFF 0.004

#if !defined(BINDLESS_TEXTURES)
layout(binding = REN_MAT_TEX0_SLOT) uniform sampler2D diff_texture;
layout(binding = REN_MAT_TEX1_SLOT) uniform sampler2D norm_texture;
layout(binding = REN_MAT_TEX2_SLOT) uniform sampler2D spec_texture;
#endif // BINDLESS_TEXTURES
layout(binding = REN_SHAD_TEX_SLOT) uniform sampler2DShadow shadow_texture;
layout(binding = REN_LMAP_SH_SLOT) uniform sampler2D lm_indirect_sh_texture[4];
layout(binding = REN_DECAL_TEX_SLOT) uniform sampler2D decals_texture;
layout(binding = REN_SSAO_TEX_SLOT) uniform sampler2D ao_texture;
layout(binding = REN_ENV_TEX_SLOT) uniform mediump samplerCubeArray env_texture;
layout(binding = REN_LIGHT_BUF_SLOT) uniform mediump samplerBuffer lights_buffer;
layout(binding = REN_DECAL_BUF_SLOT) uniform mediump samplerBuffer decals_buffer;
layout(binding = REN_CELLS_BUF_SLOT) uniform highp usamplerBuffer cells_buffer;
layout(binding = REN_ITEMS_BUF_SLOT) uniform highp usamplerBuffer items_buffer;
layout(binding = REN_CONE_RT_LUT_SLOT) uniform lowp sampler2D cone_rt_lut;

#if defined(VULKAN) || defined(GL_SPIRV)
layout (binding = REN_UB_SHARED_DATA_LOC, std140)
#else
layout (std140)
#endif
uniform SharedDataBlock {
    SharedData shrd_data;
};

LAYOUT(location = 1) in vec2 aVertexUVs_;
LAYOUT(location = 2) in mediump vec3 aVertexNormal_;
#if defined(BINDLESS_TEXTURES)
    LAYOUT(location = 8) in flat TEX_HANDLE diff_texture;
#endif // BINDLESS_TEXTURES

layout(location = REN_OUT_COLOR_INDEX) out vec4 outColor;
layout(location = REN_OUT_NORM_INDEX) out vec4 outNormal;
layout(location = REN_OUT_SPEC_INDEX) out vec4 outSpecular;

void main(void) {
    vec3 albedo_color = texture(SAMPLER2D(diff_texture), aVertexUVs_).rgb;
    
    outColor = vec4(albedo_color, 1.0);
    outNormal = vec4(0.5 * aVertexNormal_ + 0.5, 0.0);
}
