#version 310 es
#extension GL_EXT_texture_buffer : enable

#include "_vs_common.glsl"
#include "_texturing.glsl"

#include "shadow_interface.glsl"

/*
PERM @TRANSPARENT_PERM
*/

layout(location = REN_VTX_POS_LOC) in vec3 aVertexPosition;
#ifdef TRANSPARENT_PERM
layout(location = REN_VTX_UV1_LOC) in vec2 aVertexUVs1;
#endif

layout(binding = REN_INST_BUF_SLOT) uniform samplerBuffer instances_buffer;

#if defined(VULKAN)
layout(push_constant) uniform PushConstants {
    mat4 uShadowViewProjMatrix;
    ivec2 uInstanceIndices[REN_MAX_BATCH_SIZE];
};
#else
layout(location = REN_U_INSTANCES_LOC) uniform ivec2 uInstanceIndices[REN_MAX_BATCH_SIZE];
layout(location = U_M_MATRIX_LOC) uniform mat4 uShadowViewProjMatrix;
#endif

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

#ifdef TRANSPARENT_PERM
    aVertexUVs1_ = aVertexUVs1;
    
#if defined(BINDLESS_TEXTURES)
    MaterialData mat = materials[instance.y];
    alpha_texture = GET_HANDLE(mat.texture_indices[0]);
#endif // BINDLESS_TEXTURES
#endif

    vec3 vertex_position_ws = (MMatrix * vec4(aVertexPosition, 1.0)).xyz;
    gl_Position = uShadowViewProjMatrix * vec4(vertex_position_ws, 1.0);
#if defined(VULKAN)
    gl_Position.y = -gl_Position.y;
#endif
} 
