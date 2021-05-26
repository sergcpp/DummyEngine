#version 310 es
#extension GL_EXT_texture_buffer : enable
#extension GL_ARB_bindless_texture: enable

#include "_vs_common.glsl"

/*
UNIFORM_BLOCKS
    SharedDataBlock : $ubSharedDataLoc
*/

#if defined(VULKAN) || defined(GL_SPIRV)
layout (binding = REN_UB_SHARED_DATA_LOC, std140)
#else
layout (std140)
#endif
uniform SharedDataBlock {
    SharedData shrd_data;
};

layout(location = REN_VTX_POS_LOC) in vec3 aVertexPosition;
#ifdef TRANSPARENT_PERM
layout(location = REN_VTX_UV1_LOC) in vec2 aVertexUVs1;
#endif
//layout(location = REN_VTX_NOR_LOC) in vec3 aVertexNormal;
layout(location = REN_VTX_AUX_LOC) in uint aVertexColorPacked;

layout(binding = REN_INST_BUF_SLOT) uniform highp samplerBuffer instances_buffer;
layout(binding = REN_NOISE_TEX_SLOT) uniform sampler2D noise_texture;

layout(location = REN_U_M_MATRIX_LOC) uniform mat4 uShadowViewProjMatrix;

layout(location = REN_U_MAT_INDEX_LOC) uniform uint uMaterialIndex;
layout(location = REN_U_INSTANCES_LOC) uniform ivec4 uInstanceIndices[REN_MAX_BATCH_SIZE / 4];

layout(binding = REN_MATERIALS_SLOT) buffer Materials {
	MaterialData materials[];
};

#if defined(GL_ARB_bindless_texture)
layout(binding = REN_BINDLESS_TEX_SLOT) buffer TextureHandles {
	uvec2 texture_handles[];
};
#endif

#ifdef TRANSPARENT_PERM
#if defined(VULKAN) || defined(GL_SPIRV)
layout(location = 0) out vec2 aVertexUVs1_;
#if defined(GL_ARB_bindless_texture)
layout(location = 1) out flat uvec2 alpha_texture;
#endif // GL_ARB_bindless_texture
#else
out vec2 aVertexUVs1_;
#if defined(GL_ARB_bindless_texture)
out flat uvec2 alpha_texture;
#endif // GL_ARB_bindless_texture
#endif
#endif

void main() {
    int instance = uInstanceIndices[gl_InstanceID / 4][gl_InstanceID % 4];

    mat4 MMatrix;
    MMatrix[0] = texelFetch(instances_buffer, instance * 4 + 0);
    MMatrix[1] = texelFetch(instances_buffer, instance * 4 + 1);
    MMatrix[2] = texelFetch(instances_buffer, instance * 4 + 2);
    MMatrix[3] = vec4(0.0, 0.0, 0.0, 1.0);

    MMatrix = transpose(MMatrix);

    vec4 veg_params = texelFetch(instances_buffer, instance * 4 + 3);

    vec3 vtx_pos_ls = aVertexPosition;
    vec4 vtx_color = unpackUnorm4x8(aVertexColorPacked);

    vec3 obj_pos_ws = MMatrix[3].xyz;
    vec4 wind_scroll = shrd_data.uWindScroll + vec4(VEGE_NOISE_SCALE_LF * obj_pos_ws.xz, VEGE_NOISE_SCALE_HF * obj_pos_ws.xz);
    vec4 wind_params = unpackUnorm4x8(floatBitsToUint(veg_params.x));
    vec4 wind_vec_ls = vec4(unpackHalf2x16(floatBitsToUint(veg_params.y)), unpackHalf2x16(floatBitsToUint(veg_params.z)));

    vtx_pos_ls = TransformVegetation(vtx_pos_ls, vtx_color, wind_scroll, wind_params, wind_vec_ls, noise_texture);

    vec3 vtx_pos_ws = (MMatrix * vec4(vtx_pos_ls, 1.0)).xyz;

#ifdef TRANSPARENT_PERM
    aVertexUVs1_ = aVertexUVs1;
	
#if defined(GL_ARB_bindless_texture)
	MaterialData mat = materials[uMaterialIndex];
	alpha_texture = texture_handles[mat.texture_indices[0]];
#endif // GL_ARB_bindless_texture
#endif

    gl_Position = uShadowViewProjMatrix * vec4(vtx_pos_ws, 1.0);
} 
