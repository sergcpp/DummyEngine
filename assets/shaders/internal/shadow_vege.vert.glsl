#version 310 es
#extension GL_EXT_texture_buffer : enable

#include "_vs_common.glsl"
#include "_texturing.glsl"

#include "shadow_interface.glsl"

/*
PERM @TRANSPARENT_PERM
*/

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
layout(location = REN_VTX_AUX_LOC) in uint aVertexColorPacked;

layout(binding = REN_INST_BUF_SLOT) uniform samplerBuffer instances_buffer;
layout(binding = REN_NOISE_TEX_SLOT) uniform sampler2D noise_texture;

#if defined(VULKAN)
layout(push_constant) uniform PushConstants {
	mat4 uShadowViewProjMatrix;
    ivec2 uInstanceIndices[REN_MAX_BATCH_SIZE];
};
#else // VULKAN
layout(location = REN_U_INSTANCES_LOC) uniform ivec2 uInstanceIndices[REN_MAX_BATCH_SIZE];
layout(location = U_M_MATRIX_LOC) uniform mat4 uShadowViewProjMatrix;
#endif // VULKAN

layout(binding = REN_MATERIALS_SLOT) readonly buffer Materials {
	MaterialData materials[];
};

#ifdef TRANSPARENT_PERM
	LAYOUT(location = 0) out vec2 aVertexUVs1_;
	#if defined(BINDLESS_TEXTURES)
		LAYOUT(location = 1) out flat TEX_HANDLE alpha_texture;
	#endif // BINDLESS_TEXTURES
#endif // TRANSPARENT_PERM

void main() {
    ivec2 instance = uInstanceIndices[gl_InstanceIndex];
    mat4 MMatrix = FetchModelMatrix(instances_buffer, instance.x);

    vec4 veg_params = texelFetch(instances_buffer, instance.x * INSTANCE_BUF_STRIDE + 3);

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
	
#if defined(BINDLESS_TEXTURES)
	MaterialData mat = materials[instance.y];
	alpha_texture = GET_HANDLE(mat.texture_indices[0]);
#endif // BINDLESS_TEXTURES
#endif // TRANSPARENT_PERM

    gl_Position = uShadowViewProjMatrix * vec4(vtx_pos_ws, 1.0);
#if defined(VULKAN)
    gl_Position.y = -gl_Position.y;
#endif
} 
