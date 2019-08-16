#version 310 es
#extension GL_EXT_texture_buffer : enable

#include "_vs_common.glsl"
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

layout(location = REN_VTX_POS_LOC) in vec3 aVertexPosition;
#ifdef TRANSPARENT_PERM
layout(location = REN_VTX_UV1_LOC) in vec2 aVertexUVs1;
#endif
layout(location = REN_VTX_AUX_LOC) in uint aVertexColorPacked;

#if defined(VULKAN) || defined(GL_SPIRV)
layout (binding = REN_UB_SHARED_DATA_LOC, std140)
#else
layout (std140)
#endif
uniform SharedDataBlock {
    SharedData shrd_data;
};

layout(binding = REN_INST_BUF_SLOT) uniform samplerBuffer instances_buffer;
layout(binding = REN_NOISE_TEX_SLOT) uniform sampler2D noise_texture;

#if defined(VULKAN)
layout(push_constant) uniform PushConstants {
    ivec2 uInstanceIndices[REN_MAX_BATCH_SIZE];
};
#else
layout(location = REN_U_INSTANCES_LOC) uniform ivec2 uInstanceIndices[REN_MAX_BATCH_SIZE];
#endif

layout(binding = REN_MATERIALS_SLOT) readonly buffer Materials {
	MaterialData materials[];
};

#ifdef OUTPUT_VELOCITY
	LAYOUT(location = 0) out vec3 aVertexCSCurr_;
	LAYOUT(location = 1) out vec3 aVertexCSPrev_;
#endif // OUTPUT_VELOCITY
#ifdef TRANSPARENT_PERM
	LAYOUT(location = 2) out vec2 aVertexUVs1_;
	#ifdef HASHED_TRANSPARENCY
		LAYOUT(location = 3) out vec3 aVertexObjCoord_;
	#endif // HASHED_TRANSPARENCY
	#if defined(BINDLESS_TEXTURES)
		LAYOUT(location = 4) out flat TEX_HANDLE alpha_texture;
	#endif // BINDLESS_TEXTURES
#endif // TRANSPARENT_PERM

invariant gl_Position;

void main() {
    ivec2 instance = uInstanceIndices[gl_InstanceIndex];
    mat4 model_matrix_curr = FetchModelMatrix(instances_buffer, instance.x);

#ifdef MOVING_PERM
    mat4 model_matrix_prev = FetchModelMatrix(instances_buffer, instance.x + 1);
#endif

    // load vegetation properties
    vec4 veg_params = texelFetch(instances_buffer, instance.x * INSTANCE_BUF_STRIDE + 3);

    vec4 vtx_color = unpackUnorm4x8(aVertexColorPacked);

    vec3 obj_pos_ws = model_matrix_curr[3].xyz;
    vec4 wind_scroll = shrd_data.uWindScroll + vec4(VEGE_NOISE_SCALE_LF * obj_pos_ws.xz, VEGE_NOISE_SCALE_HF * obj_pos_ws.xz);
    vec4 wind_params = unpackUnorm4x8(floatBitsToUint(veg_params.x));
    vec4 wind_vec_ls = vec4(unpackHalf2x16(floatBitsToUint(veg_params.y)), unpackHalf2x16(floatBitsToUint(veg_params.z)));

    vec3 vtx_pos_ls = TransformVegetation(aVertexPosition, vtx_color, wind_scroll, wind_params, wind_vec_ls, noise_texture);
    vec3 vtx_pos_ws = (model_matrix_curr * vec4(vtx_pos_ls, 1.0)).xyz;

#ifdef TRANSPARENT_PERM
    aVertexUVs1_ = aVertexUVs1;
	
#if defined(BINDLESS_TEXTURES)
	MaterialData mat = materials[instance.y];
	alpha_texture = GET_HANDLE(mat.texture_indices[0]);
#endif // BINDLESS_TEXTURES
#endif // TRANSPARENT_PERM

    gl_Position = shrd_data.uViewProjMatrix * vec4(vtx_pos_ws, 1.0);
#if defined(VULKAN)
    gl_Position.y = -gl_Position.y;
#endif
    
#ifdef OUTPUT_VELOCITY
    vec4 wind_scroll_prev = shrd_data.uWindScrollPrev + vec4(VEGE_NOISE_SCALE_LF * obj_pos_ws.xz, VEGE_NOISE_SCALE_HF * obj_pos_ws.xz);
    vec3 vtx_pos_ls_prev = TransformVegetation(aVertexPosition, vtx_color, wind_scroll_prev, wind_params, wind_vec_ls, noise_texture);
#ifdef MOVING_PERM
    vec3 vtx_pos_ws_prev = (model_matrix_prev * vec4(vtx_pos_ls_prev, 1.0)).xyz;
#else // MOVING_PERM
    vec3 vtx_pos_ws_prev = (model_matrix_curr * vec4(vtx_pos_ls_prev, 1.0)).xyz;
#endif // MOVING_PERM

    aVertexCSCurr_ = gl_Position.xyw;
    aVertexCSPrev_ = (shrd_data.uViewProjPrevMatrix * vec4(vtx_pos_ws_prev, 1.0)).xyw;
#if defined(VULKAN)
    aVertexCSPrev_.y = -aVertexCSPrev_.y;
#endif // VULKAN
#endif // OUTPUT_VELOCITY
} 

