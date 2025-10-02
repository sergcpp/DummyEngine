#version 430 core

#include "_cs_common.glsl"
#include "taa_common.glsl"
#include "prepare_disocclusion_interface.h"

LAYOUT_PARAMS uniform UniformParams {
    Params g_params;
};

layout(binding = DILATED_DEPTH_TEX_SLOT) uniform sampler2D g_dilated_depth;
layout(binding = DILATED_VELOCITY_TEX_SLOT) uniform sampler2D g_dilated_velocity;
layout(binding = RECONSTRUCTED_DEPTH_TEX_SLOT) uniform usampler2D g_reconstructed_depth;
layout(binding = VELOCITY_TEX_SLOT) uniform sampler2D g_velocity;

layout(binding = OUT_IMG_SLOT, rg8) uniform image2D g_out_img;

layout (local_size_x = GRP_SIZE_X, local_size_y = GRP_SIZE_Y, local_size_z = 1) in;

void main() {
    const ivec2 icoord = ivec2(gl_GlobalInvocationID.xy);
    if (icoord.x >= g_params.img_size.x || icoord.y >= g_params.img_size.y) {
        return;
    }

    const vec2 norm_uvs = (vec2(icoord) + 0.5) * g_params.texel_size;
    const vec2 closest_vel = texelFetch(g_dilated_velocity, icoord, 0).xy;
    const float dilated_depth = texelFetch(g_dilated_depth, icoord, 0).x;
    const float dilated_depth_lin = LinearizeDepth(dilated_depth, g_params.clip_info);

    //
    // Calculate disocclusion
    //
    const vec2 reproj_uvs = norm_uvs - closest_vel * g_params.texel_size;
    const float uvx = fract(float(g_params.img_size.x) * reproj_uvs.x + 0.5);
    const float uvy = fract(float(g_params.img_size.y) * reproj_uvs.y + 0.5);

    vec4 w = vec4(1.0);
    // Bilinear weights
    w.x *= (1.0 - uvx) * (1.0 - uvy);
    w.y *= (uvx) * (1.0 - uvy);
    w.z *= (1.0 - uvx) * (uvy);
    w.w *= (uvx) * (uvy);

    const ivec2 offsets[4] = ivec2[4](
        ivec2(0, 0),
        ivec2(1, 0),
        ivec2(0, 1),
        ivec2(1, 1)
    );

    const ivec2 base_pos = ivec2(floor(reproj_uvs * vec2(g_params.img_size) - 0.5));

    float total_depth = 0.0, total_weight = 0.0;
    for (int i = 0; i < 4; ++i) {
        const ivec2 sample_pos = base_pos + offsets[i];
        if (all(greaterThanEqual(sample_pos, ivec2(0))) && all(lessThan(sample_pos, g_params.img_size))) {
            const float weight = w[i];
            if (weight > 0.01) {
                const float prev_depth = uintBitsToFloat(texelFetch(g_reconstructed_depth, sample_pos, 0).x);
                const float prev_depth_lin = LinearizeDepth(prev_depth, g_params.clip_info);

                const float depth_diff = dilated_depth_lin - prev_depth_lin;
                if (depth_diff > 0.0) {
                    const float plane_depth_lin = max(prev_depth_lin, dilated_depth_lin);

                    const vec3 center = ReconstructViewPosition_YFlip(vec2(0.5), g_params.frustum_info, -plane_depth_lin, 0.0 /* is_ortho */);
                    const vec3 corner = ReconstructViewPosition_YFlip(vec2(0.0), g_params.frustum_info, -plane_depth_lin, 0.0 /* is_ortho */);

                    const float viewport_width = sqrt(float(g_params.img_size.x * g_params.img_size.y));
                    const float depth_threshold = max(dilated_depth_lin, prev_depth_lin);

                    const float Ksep = 1.37e-05;
                    const float Kfov = length(corner) / length(center);
                    const float req_depth_separation = Ksep * Kfov * viewport_width * depth_threshold;

                    const float resolusion_factor = saturate(viewport_width / length(vec2(1920.0, 1080.0)));
                    const float power = 1.0 + 2.0 * resolusion_factor;

                    total_depth += pow(saturate(req_depth_separation / depth_diff), power) * weight;
                    total_weight += weight;
                }
            }
        }
    }
    float out_disocclusion = (total_weight > 0.0) ? saturate(1.0 - total_depth / total_weight) : 0.0;
    if (dilated_depth < FLT_EPS) {
        // Mark sky pixels
        out_disocclusion = 1.0;
    }

    //
    // Compute motion divergence
    //
    const vec2 center_motion = texelFetch(g_velocity, icoord, 0).xy * g_params.texel_size;
    float max_velocity_uv = length(center_motion);

    float min_convergence = 1.0;
    if (max_velocity_uv > 0.00001) {
        for (int dy = -1; dy <= 1; ++dy) {
            for (int dx = -1; dx <= 1; ++dx) {
                const ivec2 sample_pos = clamp(icoord + ivec2(dx, dy), ivec2(0), g_params.img_size);

                const vec2 motion = texelFetch(g_velocity, sample_pos, 0).xy * g_params.texel_size;
                float velocity_uv = length(motion);

                max_velocity_uv = max(max_velocity_uv, velocity_uv);
                velocity_uv = max_velocity_uv;

                min_convergence = min(min_convergence, dot(motion / velocity_uv, center_motion / velocity_uv));
            }
        }
    }
    const float out_motion_divergence = saturate(1.0 - min_convergence) * saturate(100.0 * max_velocity_uv);

    imageStore(g_out_img, icoord, vec4(out_disocclusion, out_motion_divergence, 0.0, 0.0));
}