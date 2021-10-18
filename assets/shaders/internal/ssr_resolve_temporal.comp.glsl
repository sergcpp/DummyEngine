#version 310 es
#extension GL_KHR_shader_subgroup_basic : require
#extension GL_KHR_shader_subgroup_ballot : require
#extension GL_KHR_shader_subgroup_arithmetic : require

#if defined(GL_ES) || defined(VULKAN)
    precision highp int;
    precision highp float;
#endif

#include "_common.glsl"
#include "ssr_common.glsl"
#include "taa_common.glsl"
#include "ssr_resolve_temporal_interface.glsl"

/*
UNIFORM_BLOCKS
    SharedDataBlock : $ubSharedDataLoc
    UniformParams : $ubUnifParamLoc
*/

LAYOUT_PARAMS uniform UniformParams {
    Params params;
};

#if defined(VULKAN) || defined(GL_SPIRV)
layout (binding = REN_UB_SHARED_DATA_LOC, std140)
#else
layout (std140)
#endif
uniform SharedDataBlock {
    SharedData shrd_data;
};

layout(binding = DEPTH_TEX_SLOT) uniform sampler2D depth_texture;
layout(binding = NORM_TEX_SLOT) uniform sampler2D normal_texture;
layout(binding = ROUGH_TEX_SLOT) uniform sampler2D rough_texture;
layout(binding = NORM_HIST_TEX_SLOT) uniform sampler2D normal_history;
layout(binding = ROUGH_HIST_TEX_SLOT) uniform sampler2D rough_history;
layout(binding = VELOCITY_TEX_SLOT) uniform sampler2D velocity_texture;
layout(binding = REFL_TEX_SLOT) uniform sampler2D refl_texture;
layout(binding = REFL_HIST_TEX_SLOT) uniform sampler2D refl_history;
layout(binding = RAY_LEN_TEX_SLOT) uniform sampler2D ray_len_texture;

layout(std430, binding = TILE_METADATA_MASK_SLOT) readonly buffer TileMetadataMask {
    uint g_tile_metadata_mask[];
};

layout(std430, binding = TEMP_VARIANCE_MASK_SLOT) writeonly buffer TempVarianceMask {
    uint g_temporal_variance_mask[];
};

layout(binding = OUT_DENOISED_IMG_SLOT, r11f_g11f_b10f) uniform image2D out_denoised_img;

layout (local_size_x = LOCAL_GROUP_SIZE_X, local_size_y = LOCAL_GROUP_SIZE_Y, local_size_z = 1) in;

bool IsGlossyReflection(float roughness) {
    return roughness < params.thresholds.x;
}

bool IsMirrorReflection(float roughness) {
    return roughness < params.thresholds.y; //0.0001;
}

vec3 EstimateNeighbouhoodDeviation(ivec2 dispatch_thread_id) {
    vec3 color_sum = vec3(0.0);
    vec3 color_sum_squared = vec3(0.0);

    int radius = 1;
    float weight = (radius * 2.0 + 1.0) * (radius * 2.0 + 1.0);

    for (int dx = -radius; dx <= radius; dx++) {
        for (int dy = -radius; dy <= radius; dy++) {
            ivec2 texel_coords = dispatch_thread_id + ivec2(dx, dy);
            vec3 value = texelFetch(refl_texture, texel_coords, 0).rgb;
            color_sum += value;
            color_sum_squared += value * value;
        }
    }

    vec3 color_std = (color_sum_squared - color_sum * color_sum / weight) / (weight - 1.0);
    return sqrt(max(color_std, 0.0));
}

vec2 GetHitPositionReprojection(ivec2 dispatch_thread_id, vec2 uv, float reflected_ray_length) {
    float z = texelFetch(depth_texture, dispatch_thread_id, 0).r;
    vec3 ray_vs = vec3(uv, z);
    
    vec2 unjitter = shrd_data.uTaaInfo.xy;
#if defined(VULKAN)
    unjitter.y = -unjitter.y;
#endif
    
    { // project from screen space to view space
        ray_vs.y = (1.0 - ray_vs.y);
        ray_vs.xy = 2.0 * ray_vs.xy - 1.0;
        ray_vs.xy -= unjitter;
        vec4 projected = shrd_data.uInvProjMatrix * vec4(ray_vs, 1.0);
        ray_vs = projected.xyz / projected.w;
    }

    // We start out with reconstructing the ray length in view space. 
    // This includes the portion from the camera to the reflecting surface as well as the portion from the surface to the hit position.
    float surface_depth = length(ray_vs);
    float ray_length = surface_depth + reflected_ray_length;

    // We then perform a parallax correction by shooting a ray of the same length "straight through" the reflecting surface and reprojecting the tip of that ray to the previous frame.
    ray_vs /= surface_depth; // == normalize(ray_vs)
    ray_vs *= ray_length;
    vec3 hit_position_ws; // This is the "fake" hit position if we would follow the ray straight through the surface.
    { // project from view space to world space
        vec4 projected = shrd_data.uInvViewMatrix * vec4(ray_vs, 1.0);
        hit_position_ws = projected.xyz;
    }
    
    vec2 prev_hit_position;
    { // project to screen space of previous frame
        vec4 projected = shrd_data.uViewProjPrevMatrix * vec4(hit_position_ws, 1.0);
        projected.xyz /= projected.w;
        projected.xy = 0.5 * projected.xy + 0.5;
        projected.y = (1.0 - projected.y);
        prev_hit_position = projected.xy;
    }
    return prev_hit_position;
}

float SampleHistory(vec2 uv, uvec2 screen_size, vec3 normal, float roughness, vec3 radiance_min, vec3 radiance_max, float roughness_sigma_min, float roughness_sigma_max, float temporal_stability_factor, out vec3 radiance) {
    ivec2 texel_coords = ivec2(screen_size * uv);
    radiance = texelFetch(refl_history, texel_coords, 0).rgb;
    radiance = clip_aabb(radiance_min, radiance_max, radiance);

    vec3 history_normal = texelFetch(normal_history, texel_coords, 0).xyz;
    float history_roughness = texelFetch(rough_history, texel_coords, 0).r;

    const float normal_sigma = 8.0;

    float accumulation_speed = temporal_stability_factor
        * GetEdgeStoppingNormalWeight(normal, history_normal, normal_sigma)
        * GetEdgeStoppingRoughnessWeight(roughness, history_roughness, roughness_sigma_min, roughness_sigma_max)
        * GetRoughnessAccumulationWeight(roughness)
        ;

    return clamp(accumulation_speed, 0.0, 1.0);
}

float Luma(vec3 col) {
    return max(dot(col, vec3(0.299, 0.587, 0.114)), 0.00001);
}

float ComputeTemporalVariance(vec3 history_radiance, vec3 radiance) {
    // Check temporal variance. 
    float history_luminance = Luma(history_radiance);
    float luminance = Luma(radiance);
    return abs(history_luminance - luminance) / max(max(history_luminance, luminance), 0.00001);
}

shared uint g_shared_temp_variance_mask[2];

void WriteTemporalVarianceMask(uint mask_write_index, uint has_temporal_variance_mask) {
    // All lanes write to the same index, so we combine the masks within the wave and do a single write
    uint s_has_temporal_variance_mask = subgroupOr(has_temporal_variance_mask);
    if (subgroupElect()) {
        g_temporal_variance_mask[mask_write_index] = s_has_temporal_variance_mask;
    }
}

uint GetBitMaskFromPixelPosition(uvec2 pixel_pos) {
    uint lane_index = (pixel_pos.y % 4u) * 8u + (pixel_pos.x % 8u);
    return (1u << lane_index);
}

void WriteTemporalVariance(ivec2 dispatch_thread_id, ivec2 group_thread_id, uvec2 screen_size, bool has_temporal_variance) {
    uint mask_write_index = GetTemporalVarianceIndex(dispatch_thread_id, screen_size.x);
    uint lane_mask = GetBitMaskFromPixelPosition(dispatch_thread_id);
    uint has_temporal_variance_mask = has_temporal_variance ? lane_mask : 0;

    if (gl_SubgroupSize == 32) {
        WriteTemporalVarianceMask(mask_write_index, has_temporal_variance_mask);
    } else if (gl_SubgroupSize == 64) { // The lower 32 lanes write to a different index than the upper 32 lanes.
        if (gl_SubgroupInvocationID < 32) {
            WriteTemporalVarianceMask(mask_write_index, has_temporal_variance_mask); // Write lower
        } else {
            WriteTemporalVarianceMask(mask_write_index, has_temporal_variance_mask); // Write upper
        }
    } else { // Use shared memory for all other wave sizes
        uint mask_index = group_thread_id.y / 4;
        g_shared_temp_variance_mask[mask_index] = 0;
       
        groupMemoryBarrier();
        barrier();
       
        atomicOr(g_shared_temp_variance_mask[mask_index], has_temporal_variance_mask);
        
        groupMemoryBarrier();
        barrier();

        if (all(equal(group_thread_id, ivec2(0)))) {
            g_temporal_variance_mask[mask_write_index + 0] = g_shared_temp_variance_mask[0];
            g_temporal_variance_mask[mask_write_index + 1] = g_shared_temp_variance_mask[1];
        }
    }
}

void ResolveTemporal(ivec2 dispatch_thread_id, ivec2 group_thread_id, uvec2 screen_size, float temporal_stability_factor, float temporal_variance_threshold) {
    uint tile_meta_data_index = GetTileMetaDataIndex(dispatch_thread_id, screen_size.x);
    tile_meta_data_index = subgroupBroadcastFirst(tile_meta_data_index);
    bool needs_denoiser = g_tile_metadata_mask[tile_meta_data_index] != 0u;
    
    bool has_temporal_variance = false;
    
    if (needs_denoiser) {
        float roughness = texelFetch(rough_texture, dispatch_thread_id, 0).r;
        if (!IsGlossyReflection(roughness) || IsMirrorReflection(roughness)) {
            return;
        }
        
        vec2 uv = (vec2(dispatch_thread_id) + 0.5) / vec2(params.img_size);
        
        vec3 normal = texelFetch(normal_texture, dispatch_thread_id, 0).xyz;
        vec3 radiance = texelFetch(refl_texture, dispatch_thread_id, 0).rgb;
        vec3 radiance_hist = texelFetch(refl_history, dispatch_thread_id, 0).rgb;
        float ray_len = texelFetch(ray_len_texture, dispatch_thread_id, 0).r;
        
        vec2 velocity = texelFetch(velocity_texture, dispatch_thread_id, 0).xy;
        vec3 color_deviation = EstimateNeighbouhoodDeviation(dispatch_thread_id);
        color_deviation *= 2.2;
        
        vec3 radiance_min = radiance - color_deviation;
        vec3 radiance_max = radiance + color_deviation;
        
        // reproject point on the reflecting surface
        vec2 surf_repr_uv = uv - velocity;
        
        // reproject hit point
        vec2 hit_repr_uv = GetHitPositionReprojection(dispatch_thread_id, uv, ray_len);
        
        vec2 repr_uv = (roughness < 0.05) ? hit_repr_uv : surf_repr_uv;
        
        vec3 reprojection = vec3(0.0);
        float weight = 0.0;
        if (all(greaterThan(repr_uv, vec2(0.0))) && all(lessThan(repr_uv, vec2(1.0)))) {
            weight = SampleHistory(repr_uv, screen_size, normal, roughness, radiance_min, radiance_max, RoughnessSigmaMin, RoughnessSigmaMax, temporal_stability_factor, reprojection);
        }
        
        radiance = mix(radiance, reprojection, weight);
        has_temporal_variance = ComputeTemporalVariance(radiance_hist, radiance) > temporal_variance_threshold;
        
        imageStore(out_denoised_img, dispatch_thread_id, vec4(radiance, 1.0));
    }
    
    WriteTemporalVariance(dispatch_thread_id, group_thread_id, screen_size, has_temporal_variance);
}

void main() {
    ResolveTemporal(ivec2(gl_GlobalInvocationID.xy), ivec2(gl_LocalInvocationID.xy), params.img_size, 0.99 /* temporal_stability_factor */, 0.002/* temporal_variance_threshold */);
}
