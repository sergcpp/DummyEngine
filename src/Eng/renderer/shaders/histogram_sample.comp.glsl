#version 430 core

#include "_cs_common.glsl"
#include "histogram_sample_interface.h"

LAYOUT_PARAMS uniform UniformParams {
    Params g_params;
};

layout(binding = HDR_TEX_SLOT) uniform sampler2D g_hdr_tex;

layout(binding = OUT_IMG_SLOT, r32ui) uniform uimage2D g_out_img;

#define SAMPLE_WEIGHT 10.0

vec2 hammersley(const uint index, const uint count) {
	const float E1 = fract(float(index) / float(count));
	const uint revert_sample = bitfieldReverse(index);
	return vec2(E1, float(revert_sample) * 2.3283064365386963e-10);
}

float histogram_from_luma(const float luma) {
	return log2(luma) * 0.03 + 0.556;
}

layout (local_size_x = GRP_SIZE_X, local_size_y = GRP_SIZE_Y, local_size_z = 1) in;

void main() {
    const ivec2 grp = ivec2(gl_WorkGroupID.xy);
    const vec2 uv_corner = vec2(grp) / vec2(16.0, 8.0);
    const vec2 uv_sample = uv_corner + hammersley(gl_LocalInvocationIndex, GRP_SIZE_X * GRP_SIZE_Y) / vec2(16.0, 8.0);

    const vec3 color = textureLod(g_hdr_tex, uv_sample, 0.0).xyz / g_params.pre_exposure;

    const float luma = lum(color);
    const float bucketId = saturate(histogram_from_luma(luma)) * (EXPOSURE_HISTOGRAM_RES - 1) * (1.0 - 1e-5);
    const uint bucket = uint(bucketId);
    const float weight = 1.0 - fract(bucketId);

    if (bucket > 0) {
        imageAtomicAdd(g_out_img, ivec2(bucket, 0), uint(SAMPLE_WEIGHT * weight));
        imageAtomicAdd(g_out_img, ivec2(bucket + 1, 0), uint(SAMPLE_WEIGHT * (1.0 - weight)));
        imageAtomicAdd(g_out_img, ivec2(EXPOSURE_HISTOGRAM_RES, 0), min(uint(SAMPLE_WEIGHT), uint(SAMPLE_WEIGHT * weight) + uint(SAMPLE_WEIGHT * (1.0 - weight))));
    }
}
