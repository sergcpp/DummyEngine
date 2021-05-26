#version 310 es
#extension GL_EXT_texture_buffer : enable
#extension GL_ARB_bindless_texture: enable

#include "_vs_common.glsl"

/*
UNIFORM_BLOCKS
    SharedDataBlock : $ubSharedDataLoc
PERM @MOVING_PERM
PERM @TRANSPARENT_PERM
PERM @TRANSPARENT_PERM;MOVING_PERM
*/

layout(location = REN_VTX_POS_LOC) in vec3 aVertexPosition;
#ifdef TRANSPARENT_PERM
layout(location = REN_VTX_UV1_LOC) in vec2 aVertexUVs1;
#endif

#if defined(VULKAN) || defined(GL_SPIRV)
layout (binding = REN_UB_SHARED_DATA_LOC, std140)
#else
layout (std140)
#endif
uniform SharedDataBlock {
    SharedData shrd_data;
};

layout(binding = REN_INST_BUF_SLOT) uniform mediump samplerBuffer instances_buffer;

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

#if defined(VULKAN) || defined(GL_SPIRV)
	#ifdef MOVING_PERM
    layout(location = 0) out vec3 aVertexCSCurr_;
    layout(location = 1) out vec3 aVertexCSPrev_;
    #endif // MOVING_PERM
    #ifdef TRANSPARENT_PERM
	layout(location = 2) out vec2 aVertexUVs1_;
    #ifdef HASHED_TRANSPARENCY
    layout(location = 3) out vec3 aVertexObjCoord_;
    #endif // HASHED_TRANSPARENCY
	#if defined(GL_ARB_bindless_texture)
	layout(location = 4) out flat uvec2 alpha_texture;
	#endif // GL_ARB_bindless_texture
    #endif // TRANSPARENT_PERM
#else
	#ifdef MOVING_PERM
    out vec3 aVertexCSCurr_;
    out vec3 aVertexCSPrev_;
    #endif // MOVING_PERM
    #ifdef TRANSPARENT_PERM
    out vec2 aVertexUVs1_;
    #ifdef HASHED_TRANSPARENCY
    out vec3 aVertexObjCoord_;
    #endif // HASHED_TRANSPARENCY
	#if defined(GL_ARB_bindless_texture)
	out flat uvec2 alpha_texture;
	#endif // GL_ARB_bindless_texture
    #endif // TRANSPARENT_PERM
#endif

invariant gl_Position;

void main() {
    int instance_curr = uInstanceIndices[gl_InstanceID / 4][gl_InstanceID % 4];

    mat4 model_matrix_curr = FetchModelMatrix(instances_buffer, instance_curr);

#ifdef MOVING_PERM
    int instance_prev = instance_curr + 1;
    mat4 model_matrix_prev = FetchModelMatrix(instances_buffer, instance_prev);
#endif // MOVING_PERM

#ifdef TRANSPARENT_PERM
    aVertexUVs1_ = aVertexUVs1;
	
#if defined(GL_ARB_bindless_texture)
	MaterialData mat = materials[uMaterialIndex];
	alpha_texture = texture_handles[mat.texture_indices[0]];
#endif // GL_ARB_bindless_texture
#ifdef HASHED_TRANSPARENCY
    aVertexObjCoord_ = aVertexPosition;
#endif // HASHED_TRANSPARENCY
#endif // TRANSPARENT_PERM

    vec3 vtx_pos_ws_curr = (model_matrix_curr * vec4(aVertexPosition, 1.0)).xyz;
    gl_Position = shrd_data.uViewProjMatrix * vec4(vtx_pos_ws_curr, 1.0);

#ifdef MOVING_PERM
    aVertexCSCurr_ = gl_Position.xyw;

    vec3 vtx_pos_ws_prev = (model_matrix_prev * vec4(aVertexPosition, 1.0)).xyz;
    aVertexCSPrev_ = (shrd_data.uViewProjPrevMatrix * vec4(vtx_pos_ws_prev, 1.0)).xyw;
#endif // MOVING_PERM

} 
