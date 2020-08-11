#version 310 es
#extension GL_EXT_texture_buffer : enable

#include "_vs_common.glsl"

/*
UNIFORM_BLOCKS
    SharedDataBlock : $ubSharedDataLoc
*/

layout(location = REN_VTX_POS_LOC) in vec3 aVertexPosition;
#ifdef TRANSPARENT_PERM
layout(location = REN_VTX_UV1_LOC) in vec2 aVertexUVs1;
#endif
layout(location = REN_VTX_AUX_LOC) in uint aVertexColorPacked;

layout (std140) uniform SharedDataBlock {
    SharedData shrd_data;
};

layout(binding = REN_INST_BUF_SLOT) uniform mediump samplerBuffer instances_buffer;
layout(binding = REN_NOISE_TEX_SLOT) uniform sampler2D noise_texture;
layout(location = REN_U_INSTANCES_LOC) uniform ivec4 uInstanceIndices[REN_MAX_BATCH_SIZE / 4];

#ifdef TRANSPARENT_PERM
out vec2 aVertexUVs1_;
#endif
#ifdef OUTPUT_VELOCITY
out vec3 aVertexCSCurr_;
out vec3 aVertexCSPrev_;
#endif

invariant gl_Position;

void main() {
    int instance_curr = uInstanceIndices[gl_InstanceID / 4][gl_InstanceID % 4];
    mat4 model_matrix_curr = FetchModelMatrix(instances_buffer, instance_curr);

#ifdef MOVING_PERM
    int instance_prev = instance_curr + 1;
    mat4 model_matrix_prev = FetchModelMatrix(instances_buffer, instance_prev);
#endif

    // load vegetation properties
    vec4 veg_params = texelFetch(instances_buffer, instance_curr * 4 + 3);

    vec4 vtx_color = unpackUnorm4x8(aVertexColorPacked);

    vec3 obj_pos_ws = model_matrix_curr[3].xyz;
    vec4 wind_scroll = shrd_data.uWindScroll + vec4(VEGE_NOISE_SCALE_LF * obj_pos_ws.xz, VEGE_NOISE_SCALE_HF * obj_pos_ws.xz);
    vec4 wind_params = unpackUnorm4x8(floatBitsToUint(veg_params.x));
    vec4 wind_vec_ls = vec4(unpackHalf2x16(floatBitsToUint(veg_params.y)), unpackHalf2x16(floatBitsToUint(veg_params.z)));

    vec3 vtx_pos_ls = TransformVegetation(aVertexPosition, vtx_color, wind_scroll, wind_params, wind_vec_ls, noise_texture);
    vec3 vtx_pos_ws = (model_matrix_curr * vec4(vtx_pos_ls, 1.0)).xyz;

#ifdef TRANSPARENT_PERM
    aVertexUVs1_ = aVertexUVs1;
#endif

    gl_Position = shrd_data.uViewProjMatrix * vec4(vtx_pos_ws, 1.0);
    
#ifdef OUTPUT_VELOCITY
    vec4 wind_scroll_prev = shrd_data.uWindScrollPrev + vec4(VEGE_NOISE_SCALE_LF * obj_pos_ws.xz, VEGE_NOISE_SCALE_HF * obj_pos_ws.xz);
    vec3 vtx_pos_ls_prev = TransformVegetation(aVertexPosition, vtx_color, wind_scroll_prev, wind_params, wind_vec_ls, noise_texture);
#ifdef MOVING_PERM
    vec3 vtx_pos_ws_prev = (model_matrix_prev * vec4(vtx_pos_ls_prev, 1.0)).xyz;
#else
    vec3 vtx_pos_ws_prev = (model_matrix_curr * vec4(vtx_pos_ls_prev, 1.0)).xyz;
#endif

    aVertexCSCurr_ = gl_Position.xyw;
    aVertexCSPrev_ = (shrd_data.uViewProjPrevMatrix * vec4(vtx_pos_ws_prev, 1.0)).xyw;
#endif
} 

