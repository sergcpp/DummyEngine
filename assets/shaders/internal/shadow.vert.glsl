#version 310 es
#extension GL_EXT_texture_buffer : enable
#extension GL_ARB_bindless_texture: enable

#include "_vs_common.glsl"

layout(location = REN_VTX_POS_LOC) in vec3 aVertexPosition;
#ifdef TRANSPARENT_PERM
layout(location = REN_VTX_UV1_LOC) in vec2 aVertexUVs1;
#endif

layout(binding = REN_INST_BUF_SLOT) uniform highp samplerBuffer instances_buffer;

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
out vec2 aVertexUVs1_;
#if defined(GL_ARB_bindless_texture)
out flat uvec2 alpha_texture;
#endif // GL_ARB_bindless_texture
#endif

void main() {
    int instance = uInstanceIndices[gl_InstanceID / 4][gl_InstanceID % 4];

    mat4 MMatrix;
    MMatrix[0] = texelFetch(instances_buffer, instance * 4 + 0);
    MMatrix[1] = texelFetch(instances_buffer, instance * 4 + 1);
    MMatrix[2] = texelFetch(instances_buffer, instance * 4 + 2);
    MMatrix[3] = vec4(0.0, 0.0, 0.0, 1.0);

    MMatrix = transpose(MMatrix);

#ifdef TRANSPARENT_PERM
    aVertexUVs1_ = aVertexUVs1;
	
#if defined(GL_ARB_bindless_texture)
	MaterialData mat = materials[uMaterialIndex];
	alpha_texture = texture_handles[mat.texture_indices[0]];
#endif // GL_ARB_bindless_texture
#endif

    vec3 vertex_position_ws = (MMatrix * vec4(aVertexPosition, 1.0)).xyz;
    gl_Position = uShadowViewProjMatrix * vec4(vertex_position_ws, 1.0);
} 
