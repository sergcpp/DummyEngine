#version 310 es
#extension GL_EXT_texture_buffer : enable

#include "_vs_common.glsl"
#include "_vs_instance_index_emu.glsl"
#include "_texturing.glsl"

/*
UNIFORM_BLOCKS
    SharedDataBlock : $ubSharedDataLoc
PERM @TRANSPARENT_PERM
PERM @MOVING_PERM
PERM @OUTPUT_VELOCITY
PERM @OUTPUT_VELOCITY;TRANSPARENT_PERM
PERM @MOVING_PERM;OUTPUT_VELOCITY
PERM @MOVING_PERM;OUTPUT_VELOCITY;TRANSPARENT_PERM
*/

layout(location = REN_VTX_POS_LOC) in vec3 g_in_vtx_pos;
#ifdef TRANSPARENT_PERM
layout(location = REN_VTX_UV1_LOC) in vec2 g_in_vtx_uvs0;
#endif
layout(location = REN_VTX_AUX_LOC) in uint g_in_vtx_color_packed;

#if defined(VULKAN) || defined(GL_SPIRV)
layout (binding = REN_UB_SHARED_DATA_LOC, std140)
#else
layout (std140)
#endif
uniform SharedDataBlock {
    SharedData g_shrd_data;
};

layout(binding = REN_INST_BUF_SLOT) uniform samplerBuffer g_instances_buffer;

layout(binding = REN_INST_INDICES_BUF_SLOT, std430) readonly buffer InstanceIndices {
    ivec2 g_instance_indices[];
};

layout(binding = REN_NOISE_TEX_SLOT) uniform sampler2D g_noise_texture;

layout(binding = REN_MATERIALS_SLOT, std430) readonly buffer Materials {
    MaterialData g_materials[];
};

#ifdef OUTPUT_VELOCITY
    LAYOUT(location = 0) out vec3 g_vtx_pos_cs_curr;
    LAYOUT(location = 1) out vec3 g_vtx_pos_cs_prev;
#endif // OUTPUT_VELOCITY
#ifdef TRANSPARENT_PERM
    LAYOUT(location = 2) out vec2 g_vtx_uvs0;
    #ifdef HASHED_TRANSPARENCY
        LAYOUT(location = 3) out vec3 g_vtx_pos_ls;
    #endif // HASHED_TRANSPARENCY
    #if defined(BINDLESS_TEXTURES)
        LAYOUT(location = 4) out flat TEX_HANDLE g_alpha_texture;
    #endif // BINDLESS_TEXTURES
#endif // TRANSPARENT_PERM

invariant gl_Position;

void main() {
    ivec2 instance = g_instance_indices[gl_InstanceIndex];
    mat4 model_matrix_curr = FetchModelMatrix(g_instances_buffer, instance.x);

#ifdef MOVING_PERM
    mat4 model_matrix_prev = FetchModelMatrix(g_instances_buffer, instance.x + 1);
#endif

    // load vegetation properties
    vec4 veg_params = texelFetch(g_instances_buffer, instance.x * INSTANCE_BUF_STRIDE + 3);

    vec4 vtx_color = unpackUnorm4x8(g_in_vtx_color_packed);

    vec3 obj_pos_ws = model_matrix_curr[3].xyz;
    vec4 wind_scroll = g_shrd_data.wind_scroll + vec4(VEGE_NOISE_SCALE_LF * obj_pos_ws.xz, VEGE_NOISE_SCALE_HF * obj_pos_ws.xz);
    vec4 wind_params = unpackUnorm4x8(floatBitsToUint(veg_params.x));
    vec4 wind_vec_ls = vec4(unpackHalf2x16(floatBitsToUint(veg_params.y)), unpackHalf2x16(floatBitsToUint(veg_params.z)));

    vec3 vtx_pos_ls = TransformVegetation(g_in_vtx_pos, vtx_color, wind_scroll, wind_params, wind_vec_ls, g_noise_texture);
    vec3 vtx_pos_ws = (model_matrix_curr * vec4(vtx_pos_ls, 1.0)).xyz;

#ifdef TRANSPARENT_PERM
    g_vtx_uvs0 = g_in_vtx_uvs0;

#if defined(BINDLESS_TEXTURES)
    MaterialData mat = g_materials[instance.y];
    g_alpha_texture = GET_HANDLE(mat.texture_indices[3]);
#endif // BINDLESS_TEXTURES
#endif // TRANSPARENT_PERM

    gl_Position = g_shrd_data.view_proj_matrix * vec4(vtx_pos_ws, 1.0);
#if defined(VULKAN)
    gl_Position.y = -gl_Position.y;
#endif

#ifdef OUTPUT_VELOCITY
    vec4 wind_scroll_prev = g_shrd_data.wind_scroll_prev + vec4(VEGE_NOISE_SCALE_LF * obj_pos_ws.xz, VEGE_NOISE_SCALE_HF * obj_pos_ws.xz);
    vec3 vtx_pos_ls_prev = TransformVegetation(g_in_vtx_pos, vtx_color, wind_scroll_prev, wind_params, wind_vec_ls, g_noise_texture);
#ifdef MOVING_PERM
    vec3 vtx_pos_ws_prev = (model_matrix_prev * vec4(vtx_pos_ls_prev, 1.0)).xyz;
#else // MOVING_PERM
    vec3 vtx_pos_ws_prev = (model_matrix_curr * vec4(vtx_pos_ls_prev, 1.0)).xyz;
#endif // MOVING_PERM

    g_vtx_pos_cs_curr = gl_Position.xyw;
    g_vtx_pos_cs_prev = (g_shrd_data.view_proj_prev_matrix * vec4(vtx_pos_ws_prev, 1.0)).xyw;
#if defined(VULKAN)
    g_vtx_pos_cs_prev.y = -g_vtx_pos_cs_prev.y;
#endif // VULKAN
#endif // OUTPUT_VELOCITY
}

