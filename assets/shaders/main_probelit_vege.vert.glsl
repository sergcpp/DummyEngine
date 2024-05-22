#version 320 es
#extension GL_EXT_texture_buffer : enable
#extension GL_OES_texture_buffer : enable
//#extension GL_EXT_control_flow_attributes : enable
#if !defined(VULKAN) && !defined(GL_SPIRV)
#extension GL_ARB_bindless_texture : enable
#endif

#include "internal/_vs_common.glsl"
#include "internal/_vs_instance_index_emu.glsl"
#include "internal/_texturing.glsl"
#include "internal/_vegetation.glsl"

layout(location = VTX_POS_LOC) in vec3 g_in_vtx_pos;
layout(location = VTX_NOR_LOC) in vec4 g_in_vtx_normal;
layout(location = VTX_TAN_LOC) in vec2 g_in_vtx_tangent;
layout(location = VTX_UV1_LOC) in vec2 g_in_vtx_uvs0;
layout(location = VTX_AUX_LOC) in uint g_in_vtx_uvs1_packed;

layout (binding = BIND_UB_SHARED_DATA_BUF, std140) uniform SharedDataBlock {
    SharedData g_shrd_data;
};

layout(binding = BIND_INST_NDX_BUF, std430) readonly buffer InstanceIndices {
    ivec2 g_instance_indices[];
};

layout(binding = BIND_INST_BUF) uniform samplerBuffer g_instances_buf;
layout(binding = BIND_NOISE_TEX) uniform sampler2D g_noise_tex;

layout(binding = BIND_MATERIALS_BUF, std430) readonly buffer Materials {
    MaterialData g_materials[];
};

#if !defined(BINDLESS_TEXTURES)
layout(binding = BIND_MAT_TEX4) uniform sampler2D g_pp_pos_tex;
layout(binding = BIND_MAT_TEX5) uniform sampler2D g_pp_dir_tex;
#endif

layout(location = 0) out highp vec3 g_vtx_pos;
layout(location = 1) out mediump vec2 g_vtx_uvs;
layout(location = 2) out mediump vec3 g_vtx_normal;
layout(location = 3) out mediump vec3 g_vtx_tangent;
layout(location = 4) out highp vec4 g_vtx_sh_uvs0;
layout(location = 5) out highp vec4 g_vtx_sh_uvs1;
layout(location = 6) out highp vec4 g_vtx_sh_uvs2;
#if defined(BINDLESS_TEXTURES)
    layout(location = 7) out flat TEX_HANDLE g_diff_tex;
    layout(location = 8) out flat TEX_HANDLE g_norm_tex;
    layout(location = 9) out flat TEX_HANDLE g_spec_tex;
    layout(location = 10) out flat TEX_HANDLE g_mat3_tex; // unused
#endif // BINDLESS_TEXTURES
layout(location = 11) out vec4 g_debug_color;

invariant gl_Position;

void main(void) {
    ivec2 instance = g_instance_indices[gl_InstanceIndex];

    mat4 model_matrix = FetchModelMatrix(g_instances_buf, instance.x);

    // load vegetation properties
    vec4 veg_params = texelFetch(g_instances_buf, instance.x * INSTANCE_BUF_STRIDE + 3);
    vec2 pp_vtx_uvs = unpackHalf2x16(g_in_vtx_uvs1_packed);

#if defined(BINDLESS_TEXTURES)
    MaterialData mat = g_materials[instance.y];
    TEX_HANDLE g_pp_pos_tex = GET_HANDLE(mat.texture_indices[4]);
    TEX_HANDLE g_pp_dir_tex = GET_HANDLE(mat.texture_indices[5]);
#endif // BINDLESS_TEXTURES
    HierarchyData hdata = FetchHierarchyData(SAMPLER2D(g_pp_pos_tex), SAMPLER2D(g_pp_dir_tex), pp_vtx_uvs);

    vec3 obj_pos_ws = model_matrix[3].xyz;
    vec4 wind_scroll = g_shrd_data.wind_scroll + vec4(VEGE_NOISE_SCALE_LF * obj_pos_ws.xz, VEGE_NOISE_SCALE_HF * obj_pos_ws.xz);
    vec4 wind_params = unpackUnorm4x8(floatBitsToUint(veg_params.x));
    vec4 wind_vec_ls = vec4(unpackHalf2x16(floatBitsToUint(veg_params.y)), unpackHalf2x16(floatBitsToUint(veg_params.z)));

    vec3 vtx_nor_ls = g_in_vtx_normal.xyz, vtx_tan_ls = vec3(g_in_vtx_normal.w, g_in_vtx_tangent);
    vec3 vtx_pos_ls = TransformVegetation(g_in_vtx_pos, vtx_nor_ls, vtx_tan_ls, g_noise_tex, wind_scroll, wind_params, wind_vec_ls, hdata);

    vec3 vtx_pos_ws = (model_matrix * vec4(vtx_pos_ls, 1.0)).xyz;
    vec3 vtx_nor_ws = normalize((model_matrix * vec4(vtx_nor_ls, 0.0)).xyz);
    vec3 vtx_tan_ws = normalize((model_matrix * vec4(vtx_tan_ls, 0.0)).xyz);

    gl_Position = g_shrd_data.clip_from_world_no_translation * vec4(vtx_pos_ws - g_shrd_data.cam_pos_and_exp.xyz, 1.0);
#if defined(VULKAN)
    gl_Position.y = -gl_Position.y;
#endif

    g_vtx_pos = vtx_pos_ws;
    g_vtx_normal = vtx_nor_ws;
    g_vtx_tangent = vtx_tan_ws;
    g_vtx_uvs = g_in_vtx_uvs0;

    const vec2 offsets[4] = vec2[4](
        vec2(0.0, 0.0),
        vec2(0.25, 0.0),
        vec2(0.0, 0.5),
        vec2(0.25, 0.5)
    );

    /*[[unroll]]*/ for (int i = 0; i < 4; i++) {
        vec3 shadow_uvs = (g_shrd_data.shadowmap_regions[i].clip_from_world * vec4(vtx_pos_ws, 1.0)).xyz;
#if defined(VULKAN)
        shadow_uvs.xy = 0.5 * shadow_uvs.xy + 0.5;
#else // VULKAN
        shadow_uvs = 0.5 * shadow_uvs + 0.5;
#endif // VULKAN
        shadow_uvs.xy *= vec2(0.25, 0.5);
        shadow_uvs.xy += offsets[i];
#if defined(VULKAN)
        shadow_uvs.y = 1.0 - shadow_uvs.y;
#endif // VULKAN
        g_vtx_sh_uvs0[i] = shadow_uvs[0];
        g_vtx_sh_uvs1[i] = shadow_uvs[1];
        g_vtx_sh_uvs2[i] = shadow_uvs[2];
    }

#if defined(BINDLESS_TEXTURES)
    g_diff_tex = GET_HANDLE(mat.texture_indices[0]);
    g_norm_tex = GET_HANDLE(mat.texture_indices[1]);
    g_spec_tex = GET_HANDLE(mat.texture_indices[2]);
#endif // BINDLESS_TEXTURES
}
