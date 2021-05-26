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
layout(location = REN_VTX_NOR_LOC) in vec4 aVertexNormal;
layout(location = REN_VTX_TAN_LOC) in vec2 aVertexTangent;
layout(location = REN_VTX_UV1_LOC) in vec2 aVertexUVs1;
layout(location = REN_VTX_AUX_LOC) in uint aVertexUnused;

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
layout(location = 0) out highp vec3 aVertexPos_;
layout(location = 1) out mediump vec2 aVertexUVs_;
layout(location = 2) out mediump vec3 aVertexNormal_;
layout(location = 3) out mediump vec3 aVertexTangent_;
layout(location = 4) out highp vec3 aVertexShUVs_[4];
#if defined(GL_ARB_bindless_texture)
layout(location = 8) out flat uvec2 diff_texture;
layout(location = 9) out flat uvec2 norm_texture;
layout(location = 10) out flat uvec2 spec_texture;
#endif // GL_ARB_bindless_texture
#else
out highp vec3 aVertexPos_;
out mediump vec2 aVertexUVs_;
out mediump vec3 aVertexNormal_;
out mediump vec3 aVertexTangent_;
out highp vec3 aVertexShUVs_[4];
#if defined(GL_ARB_bindless_texture)
out flat uvec2 diff_texture;
out flat uvec2 norm_texture;
out flat uvec2 spec_texture;
#endif // GL_ARB_bindless_texture
#endif

#ifdef VULKAN
    #define gl_InstanceID gl_InstanceIndex
#endif

invariant gl_Position;

void main(void) {
    int instance = uInstanceIndices[gl_InstanceID / 4][gl_InstanceID % 4];

    mat4 model_matrix = FetchModelMatrix(instances_buffer, instance);

    vec3 vtx_pos_ws = (model_matrix * vec4(aVertexPosition, 1.0)).xyz;
    vec3 vtx_nor_ws = normalize((model_matrix * vec4(aVertexNormal.xyz, 0.0)).xyz);
    vec3 vtx_tan_ws = normalize((model_matrix * vec4(aVertexNormal.w, aVertexTangent, 0.0)).xyz);

    aVertexPos_ = vtx_pos_ws;
    aVertexNormal_ = vtx_nor_ws;
    aVertexTangent_ = vtx_tan_ws;
    aVertexUVs_ = aVertexUVs1;
    
    const vec2 offsets[4] = vec2[4](
        vec2(0.0, 0.0),
        vec2(0.25, 0.0),
        vec2(0.0, 0.5),
        vec2(0.25, 0.5)
    );
    
    /*[[unroll]]*/ for (int i = 0; i < 4; i++) {
        aVertexShUVs_[i] = (shrd_data.uShadowMapRegions[i].clip_from_world *
                            vec4(vtx_pos_ws, 1.0)).xyz;
        aVertexShUVs_[i] = 0.5 * aVertexShUVs_[i] + 0.5;
        aVertexShUVs_[i].xy *= vec2(0.25, 0.5);
        aVertexShUVs_[i].xy += offsets[i];
    }
	
#if defined(GL_ARB_bindless_texture)
	MaterialData mat = materials[uMaterialIndex];
	diff_texture = texture_handles[mat.texture_indices[0]];
	norm_texture = texture_handles[mat.texture_indices[1]];
	spec_texture = texture_handles[mat.texture_indices[2]];
#endif // GL_ARB_bindless_texture
    
    gl_Position = shrd_data.uViewProjMatrix * vec4(vtx_pos_ws, 1.0);
} 
