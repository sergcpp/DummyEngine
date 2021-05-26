#version 310 es
#extension GL_EXT_texture_buffer : enable
#extension GL_OES_texture_buffer : enable
#extension GL_ARB_bindless_texture: enable
//#extension GL_EXT_control_flow_attributes : enable

$ModifyWarning

#include "internal/_vs_common.glsl"

/*
UNIFORM_BLOCKS
    SharedDataBlock : $ubSharedDataLoc
*/

layout(location = REN_VTX_POS_LOC) in vec3 aVertexPosition;
layout(location = REN_VTX_UV1_LOC) in vec2 aVertexUVs1;

#if defined(VULKAN) || defined(GL_SPIRV)
layout (binding = 0, std140)
#else
layout (std140)
#endif
uniform SharedDataBlock {
    SharedData shrd_data;
};

layout(location = REN_U_MAT_INDEX_LOC) uniform uint uMaterialIndex;
layout (location = REN_U_INSTANCES_LOC) uniform ivec4 uInstanceIndices[REN_MAX_BATCH_SIZE / 4];

layout(binding = REN_INST_BUF_SLOT) uniform highp samplerBuffer instances_buffer;

layout(binding = REN_MATERIALS_SLOT) buffer Materials {
	MaterialData materials[];
};

#if defined(GL_ARB_bindless_texture)
layout(binding = REN_BINDLESS_TEX_SLOT) buffer TextureHandles {
	uvec2 texture_handles[];
};
#endif

#if defined(VULKAN) || defined(GL_SPIRV)
layout(location = 4) out vec2 aVertexUVs1_;
#if defined(GL_ARB_bindless_texture)
layout(location = 8) out flat uvec2 mat0_texture;
layout(location = 9) out flat uvec2 mat1_texture;
#endif // GL_ARB_bindless_texture
#else
out vec2 aVertexUVs1_;
#if defined(GL_ARB_bindless_texture)
out flat uvec2 mat0_texture;
out flat uvec2 mat1_texture;
#endif // GL_ARB_bindless_texture
#endif

#ifdef VULKAN
    #define gl_InstanceID gl_InstanceIndex
#endif

invariant gl_Position;

void main(void) {
    int instance = uInstanceIndices[gl_InstanceID / 4][gl_InstanceID % 4];

    mat4 model_matrix = FetchModelMatrix(instances_buffer, instance);

    aVertexUVs1_ = aVertexUVs1;
    
#if defined(GL_ARB_bindless_texture)
	MaterialData mat = materials[uMaterialIndex];
	mat0_texture = texture_handles[mat.texture_indices[0]];
	mat1_texture = texture_handles[mat.texture_indices[1]];
#endif // GL_ARB_bindless_texture
	
    vec3 vtx_pos_ws = (model_matrix * vec4(aVertexPosition, 1.0)).xyz;
    gl_Position = shrd_data.uViewProjMatrix * vec4(vtx_pos_ws, 1.0);
} 
