#version 320 es
#extension GL_EXT_texture_buffer : enable
#if !defined(VULKAN) && !defined(GL_SPIRV)
#extension GL_ARB_bindless_texture : enable
#endif

#include "_vs_common.glsl"
#include "_vs_instance_index_emu.glsl"
#include "_texturing.glsl"
#include "_vegetation.glsl"

#pragma multi_compile _ MOVING_PERM
#pragma multi_compile _ OUTPUT_VELOCITY
#pragma multi_compile _ TRANSPARENT_PERM
#pragma multi_compile _ HASHED_TRANSPARENCY

layout(location = VTX_POS_LOC) in vec3 g_in_vtx_pos;
#ifdef TRANSPARENT_PERM
layout(location = VTX_UV1_LOC) in vec2 g_in_vtx_uvs0;
#endif
layout(location = VTX_AUX_LOC) in uint g_in_vtx_uvs1_packed;

layout (binding = BIND_UB_SHARED_DATA_BUF, std140) uniform SharedDataBlock {
    SharedData g_shrd_data;
};

layout(binding = BIND_INST_BUF) uniform samplerBuffer g_instances_buf;

layout(binding = BIND_INST_NDX_BUF, std430) readonly buffer InstanceIndices {
    ivec2 g_instance_indices[];
};

layout(binding = BIND_NOISE_TEX) uniform sampler2D g_noise_tex;

layout(binding = BIND_MATERIALS_BUF, std430) readonly buffer Materials {
    MaterialData g_materials[];
};

#if !defined(BINDLESS_TEXTURES)
layout(binding = BIND_MAT_TEX4) uniform sampler2D g_pp_pos_tex;
layout(binding = BIND_MAT_TEX5) uniform sampler2D g_pp_dir_tex;
#endif

#ifdef OUTPUT_VELOCITY
    layout(location = 0) out vec3 g_vtx_pos_cs_curr;
    layout(location = 1) out vec3 g_vtx_pos_cs_prev;
#endif // OUTPUT_VELOCITY
#ifdef TRANSPARENT_PERM
    layout(location = 2) out vec2 g_vtx_uvs0;
    #ifdef HASHED_TRANSPARENCY
        layout(location = 3) out vec3 g_vtx_pos_ls;
    #endif // HASHED_TRANSPARENCY
    #if defined(BINDLESS_TEXTURES)
        layout(location = 4) out flat TEX_HANDLE g_alpha_tex;
    #endif // BINDLESS_TEXTURES
#endif // TRANSPARENT_PERM

invariant gl_Position;

void main() {
    ivec2 instance = g_instance_indices[gl_InstanceIndex];
    mat4 model_matrix_curr = FetchModelMatrix(g_instances_buf, instance.x);

#ifdef MOVING_PERM
    mat4 model_matrix_prev = FetchModelMatrix(g_instances_buf, instance.x + 1);
#endif

    // load vegetation properties
    vec4 veg_params = texelFetch(g_instances_buf, instance.x * INSTANCE_BUF_STRIDE + 3);
    vec2 pp_vtx_uvs = unpackHalf2x16(g_in_vtx_uvs1_packed);

#if defined(BINDLESS_TEXTURES)
    MaterialData mat = g_materials[instance.y];
    TEX_HANDLE g_pp_pos_tex = GET_HANDLE(mat.texture_indices[4]);
    TEX_HANDLE g_pp_dir_tex = GET_HANDLE(mat.texture_indices[5]);
#endif // BINDLESS_TEXTURES
    HierarchyData hdata_curr = FetchHierarchyData(SAMPLER2D(g_pp_pos_tex), SAMPLER2D(g_pp_dir_tex), pp_vtx_uvs);

    vec3 obj_pos_ws = model_matrix_curr[3].xyz;
    vec4 wind_scroll = g_shrd_data.wind_scroll + vec4(VEGE_NOISE_SCALE_LF * obj_pos_ws.xz, VEGE_NOISE_SCALE_HF * obj_pos_ws.xz);
    vec4 wind_params = unpackUnorm4x8(floatBitsToUint(veg_params.x));
    vec4 wind_vec_ls = vec4(unpackHalf2x16(floatBitsToUint(veg_params.y)), unpackHalf2x16(floatBitsToUint(veg_params.z)));

    vec3 _unused = vec3(0.0);
    vec3 vtx_pos_ls = TransformVegetation(g_in_vtx_pos, _unused, _unused, g_noise_tex, wind_scroll, wind_params, wind_vec_ls, hdata_curr);

#ifdef TRANSPARENT_PERM
    g_vtx_uvs0 = g_in_vtx_uvs0;

#if defined(BINDLESS_TEXTURES)
    g_alpha_tex = GET_HANDLE(mat.texture_indices[3]);
#endif // BINDLESS_TEXTURES
#endif // TRANSPARENT_PERM

    vec3 vtx_pos_ws = (model_matrix_curr * vec4(vtx_pos_ls, 1.0)).xyz;
    gl_Position = g_shrd_data.clip_from_world_no_translation * vec4(vtx_pos_ws - g_shrd_data.cam_pos_and_gamma.xyz, 1.0);
#if defined(VULKAN)
    gl_Position.y = -gl_Position.y;
#endif

#ifdef OUTPUT_VELOCITY
    vec4 wind_scroll_prev = g_shrd_data.wind_scroll_prev + vec4(VEGE_NOISE_SCALE_LF * obj_pos_ws.xz, VEGE_NOISE_SCALE_HF * obj_pos_ws.xz);
#ifdef MOVING_PERM
    HierarchyData hdata_prev = FetchHierarchyData(SAMPLER2D(g_pp_pos_tex), SAMPLER2D(g_pp_dir_tex), pp_vtx_uvs);
#else // MOVING_PERM
    HierarchyData hdata_prev = hdata_curr;
#endif // MOVING_PERM

    vec3 vtx_pos_ls_prev = TransformVegetation(g_in_vtx_pos, _unused, _unused, g_noise_tex, wind_scroll_prev, wind_params, wind_vec_ls, hdata_prev);

#ifdef MOVING_PERM
    vec3 vtx_pos_ws_prev = (model_matrix_prev * vec4(vtx_pos_ls_prev, 1.0)).xyz;
#else // MOVING_PERM
    vec3 vtx_pos_ws_prev = (model_matrix_curr * vec4(vtx_pos_ls_prev, 1.0)).xyz;
#endif // MOVING_PERM

    g_vtx_pos_cs_curr = gl_Position.xyw;
    g_vtx_pos_cs_prev = (g_shrd_data.prev_clip_from_world_no_translation * vec4(vtx_pos_ws_prev - g_shrd_data.prev_cam_pos.xyz, 1.0)).xyw;
#if defined(VULKAN)
    g_vtx_pos_cs_prev.y = -g_vtx_pos_cs_prev.y;
#endif // VULKAN
#endif // OUTPUT_VELOCITY
}

