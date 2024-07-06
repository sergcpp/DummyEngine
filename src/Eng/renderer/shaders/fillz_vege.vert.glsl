#version 430 core
#if !defined(VULKAN) && !defined(NO_BINDLESS)
#extension GL_ARB_bindless_texture : enable
#endif

#include "_vs_common.glsl"
#include "_vs_instance_index_emu.glsl"
#include "_texturing.glsl"
#include "_vegetation.glsl"

#pragma multi_compile _ MOVING
#pragma multi_compile _ OUTPUT_VELOCITY
#pragma multi_compile _ TRANSPARENT
#pragma multi_compile _ NO_BINDLESS

#if defined(NO_BINDLESS) && defined(VULKAN)
    #pragma dont_compile
#endif

layout(location = VTX_POS_LOC) in vec3 g_in_vtx_pos;
#ifdef TRANSPARENT
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

#if defined(NO_BINDLESS)
layout(binding = BIND_MAT_TEX4) uniform sampler2D g_pp_pos_tex;
layout(binding = BIND_MAT_TEX5) uniform sampler2D g_pp_dir_tex;
#endif

#ifdef OUTPUT_VELOCITY
    layout(location = 0) out vec3 g_vtx_pos_cs_curr;
    layout(location = 1) out vec3 g_vtx_pos_cs_prev;
    layout(location = 2) out vec2 g_vtx_z_vs_curr;
    layout(location = 3) out vec2 g_vtx_z_vs_prev;
#endif // OUTPUT_VELOCITY
#ifdef TRANSPARENT
    layout(location = 4) out vec2 g_vtx_uvs0;
    layout(location = 5) out vec3 g_vtx_pos_ls;
    layout(location = 6) out flat float g_alpha;
    #if !defined(NO_BINDLESS)
        layout(location = 7) out flat TEX_HANDLE g_alpha_tex;
    #endif // !NO_BINDLESS
#endif // TRANSPARENT

invariant gl_Position;

void main() {
    const ivec2 instance = g_instance_indices[gl_InstanceIndex];
    const mat4 model_matrix_curr = FetchModelMatrix(g_instances_buf, instance.x);

#ifdef MOVING
    const mat4 model_matrix_prev = FetchModelMatrix(g_instances_buf, instance.x + 1);
#endif

    // load vegetation properties
    const vec4 veg_params = texelFetch(g_instances_buf, instance.x * INSTANCE_BUF_STRIDE + 3);
    const vec2 pp_vtx_uvs = unpackHalf2x16(g_in_vtx_uvs1_packed);

    const MaterialData mat = g_materials[instance.y];
#if !defined(NO_BINDLESS)
    const TEX_HANDLE g_pp_pos_tex = GET_HANDLE(mat.texture_indices[4]);
    const TEX_HANDLE g_pp_dir_tex = GET_HANDLE(mat.texture_indices[5]);
#endif // !NO_BINDLESS
    const HierarchyData hdata_curr = FetchHierarchyData(SAMPLER2D(g_pp_pos_tex), SAMPLER2D(g_pp_dir_tex), pp_vtx_uvs);

    const vec3 obj_pos_ws = model_matrix_curr[3].xyz;
    const vec4 wind_scroll = g_shrd_data.wind_scroll + vec4(VEGE_NOISE_SCALE_LF * obj_pos_ws.xz, VEGE_NOISE_SCALE_HF * obj_pos_ws.xz);
    const vec4 wind_params = unpackUnorm4x8(floatBitsToUint(veg_params.x));
    const vec4 wind_vec_ls = vec4(unpackHalf2x16(floatBitsToUint(veg_params.y)), unpackHalf2x16(floatBitsToUint(veg_params.z)));

    vec3 _unused = vec3(0.0);
    const vec3 vtx_pos_ls_curr = TransformVegetation(g_in_vtx_pos, _unused, _unused, g_noise_tex, wind_scroll, wind_params, wind_vec_ls, hdata_curr);

#ifdef TRANSPARENT
    g_vtx_uvs0 = g_in_vtx_uvs0;
    g_alpha = 1.0 - mat.params[3].x;
#if !defined(NO_BINDLESS)
    g_alpha_tex = GET_HANDLE(mat.texture_indices[3]);
#endif // !NO_BINDLESS
#endif // TRANSPARENT

    const vec3 vtx_pos_ws_curr = (model_matrix_curr * vec4(vtx_pos_ls_curr, 1.0)).xyz;
    gl_Position = g_shrd_data.clip_from_world * vec4(vtx_pos_ws_curr, 1.0);

#ifdef OUTPUT_VELOCITY
    const vec4 wind_scroll_prev = g_shrd_data.wind_scroll_prev + vec4(VEGE_NOISE_SCALE_LF * obj_pos_ws.xz, VEGE_NOISE_SCALE_HF * obj_pos_ws.xz);
#ifdef MOVING
    const HierarchyData hdata_prev = FetchHierarchyData(SAMPLER2D(g_pp_pos_tex), SAMPLER2D(g_pp_dir_tex), pp_vtx_uvs);
#else // MOVING
    const HierarchyData hdata_prev = hdata_curr;
#endif // MOVING

    const vec3 vtx_pos_ls_prev = TransformVegetation(g_in_vtx_pos, _unused, _unused, g_noise_tex, wind_scroll_prev, wind_params, wind_vec_ls, hdata_prev);

#ifdef MOVING
    const vec3 vtx_pos_ws_prev = (model_matrix_prev * vec4(vtx_pos_ls_prev, 1.0)).xyz;
#else // MOVING
    const vec3 vtx_pos_ws_prev = (model_matrix_curr * vec4(vtx_pos_ls_prev, 1.0)).xyz;
#endif // MOVING

    g_vtx_pos_cs_curr = gl_Position.xyw;
    g_vtx_pos_cs_prev = (g_shrd_data.prev_clip_from_world * vec4(vtx_pos_ws_prev, 1.0)).xyw;
#if defined(VULKAN)
    g_vtx_pos_cs_prev.y = -g_vtx_pos_cs_prev.y;
#endif // VULKAN
    const vec4 vtx_pos_vs_curr = g_shrd_data.view_from_world * vec4(vtx_pos_ws_curr, 1.0);
    const vec4 vtx_pos_vs_prev = g_shrd_data.prev_view_from_world * vec4(vtx_pos_ws_prev, 1.0);
    g_vtx_z_vs_curr = vtx_pos_vs_curr.zw;
    g_vtx_z_vs_prev = vtx_pos_vs_prev.zw;
#endif // OUTPUT_VELOCITY
}

