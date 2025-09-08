#version 430 core

#include "_cs_common.glsl"
#include "debug_gbuffer_interface.h"

#pragma multi_compile _ DEPTH NORMALS ROUGHNESS METALLIC

LAYOUT_PARAMS uniform UniformParams {
    Params g_params;
};

layout(binding = DEPTH_TEX_SLOT) uniform sampler2D g_depth_tex;
layout(binding = ALBEDO_TEX_SLOT) uniform sampler2D g_albedo_tex;
layout(binding = NORM_TEX_SLOT) uniform usampler2D g_normal_tex;
layout(binding = SPEC_TEX_SLOT) uniform usampler2D g_specular_tex;

layout(binding = OUT_IMG_SLOT, rgba8) uniform restrict writeonly image2D g_out_img;

layout (local_size_x = LOCAL_GROUP_SIZE_X, local_size_y = LOCAL_GROUP_SIZE_Y, local_size_z = 1) in;

void main() {
    const uvec2 px_coords = gl_GlobalInvocationID.xy;
    if (px_coords.x >= g_params.img_size.x || px_coords.y >= g_params.img_size.y) {
        return;
    }

	const float depth = texelFetch(g_depth_tex, ivec2(px_coords), 0).x;
	const vec4 normal = UnpackNormalAndRoughness(texelFetch(g_normal_tex, ivec2(px_coords), 0).x);
	const uint packed_mat_params = texelFetch(g_specular_tex, ivec2(px_coords), 0).x;

	vec4 mat_params0, mat_params1;
    UnpackMaterialParams(packed_mat_params, mat_params0, mat_params1);

    const float roughness = normal.w;
    const float sheen = mat_params0.x;
    const float sheen_tint = mat_params0.y;
    const float specular = mat_params0.z;
    const float specular_tint = mat_params0.w;
    const float metallic = mat_params1.x;
    const float transmission = mat_params1.y;
    const float clearcoat = mat_params1.z;
    const float clearcoat_roughness = mat_params1.w;

	vec4 out_color = vec4(0.0);

#if defined(DEPTH)
	out_color = vec4(depth);
#elif defined(NORMALS)
	out_color = vec4(0.5 * normal.xyz + 0.5, 0);
#elif defined(ROUGHNESS)
	out_color = vec4(roughness);
#elif defined(METALLIC)
	out_color = vec4(metallic);
#endif

    imageStore(g_out_img, ivec2(px_coords), out_color);
}
