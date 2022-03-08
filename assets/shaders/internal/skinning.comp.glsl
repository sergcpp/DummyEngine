#version 310 es

#include "_common.glsl"
#include "skinning_interface.glsl"

/*
UNIFORM_BLOCKS
    UniformParams : $ubUnifParamLoc
*/

struct InVertex {
    highp vec4 p_and_nxy;
    highp uvec2 nz_and_b;
    highp uvec2 t0_and_t1;
    highp uvec2 bone_indices;
    highp uvec2 bone_weights;
};

struct InDelta {
    highp vec2 dpxy;
    highp vec2 dpz_dnxy;
    highp uvec2 dnz_and_db;
};

struct OutVertexData0 {
    highp vec4 p_and_t0;
};

struct OutVertexData1 {
    highp uvec2 n_and_bx;
    highp uvec2 byz_and_t1;
};

layout(std430, binding = IN_VERTICES_SLOT) readonly buffer Input0 {
    InVertex vertices[];
} g_in_data0;

layout(std430, binding = IN_MATRICES_SLOT) readonly buffer Input1 {
    highp mat3x4 matrices[];
} g_in_data1;

layout(std430, binding = IN_SHAPE_KEYS_SLOT) readonly buffer Input2 {
    highp uint shape_keys[];
} g_in_data2;

layout(std430, binding = IN_DELTAS_SLOT) readonly buffer Input3 {
    InDelta deltas[];
} g_in_data3;

layout(std430, binding = OUT_VERTICES0) writeonly buffer Output0 {
    OutVertexData0 vertices[];
} g_out_data0;

layout(std430, binding = OUT_VERTICES1) writeonly buffer Output1 {
    OutVertexData1 vertices[];
} g_out_data1;

LAYOUT_PARAMS uniform UniformParams {
    Params g_params;
};

layout (local_size_x = LOCAL_GROUP_SIZE, local_size_y = 1, local_size_z = 1) in;

void main() {
    if (gl_GlobalInvocationID.x >= g_params.uSkinParams.y) {
        return;
    }

    highp uint in_ndx = g_params.uSkinParams.x + gl_GlobalInvocationID.x;
    mediump uint xform_offset = g_params.uSkinParams.z;

    highp vec3 p = g_in_data0.vertices[in_ndx].p_and_nxy.xyz;

    highp uint _nxy = floatBitsToUint(g_in_data0.vertices[in_ndx].p_and_nxy.w);
    highp vec2 nxy = unpackSnorm2x16(_nxy);

    highp uint _nz_and_bx = g_in_data0.vertices[in_ndx].nz_and_b.x;
    highp vec2 nz_and_bx = unpackSnorm2x16(_nz_and_bx);

    highp uint _byz = g_in_data0.vertices[in_ndx].nz_and_b.y;
    highp vec2 byz = unpackSnorm2x16(_byz);

    highp vec3 n = vec3(nxy, nz_and_bx.x), b = vec3(nz_and_bx.y, byz);

    highp vec3 p2 = p;

    for (uint j = g_params.uShapeParamsCurr.x; j < g_params.uShapeParamsCurr.x + g_params.uShapeParamsCurr.y; j++) {
        highp uint shape_data = g_in_data2.shape_keys[j];
        mediump uint shape_index = bitfieldExtract(shape_data, 0, 16);
        mediump float shape_weight = unpackUnorm2x16(shape_data).y;

        int sh_i = int(g_params.uShapeParamsCurr.z + shape_index * g_params.uSkinParams.y + gl_GlobalInvocationID.x);
        p += shape_weight * vec3(g_in_data3.deltas[sh_i].dpxy, g_in_data3.deltas[sh_i].dpz_dnxy.x);
        highp uint _dnxy = floatBitsToUint(g_in_data3.deltas[sh_i].dpz_dnxy.y);
        mediump vec2 _dnz_and_dbx = unpackSnorm2x16(g_in_data3.deltas[sh_i].dnz_and_db.x);
        n += shape_weight * vec3(unpackSnorm2x16(_dnxy), _dnz_and_dbx.x);
        mediump vec2 _dbyz = unpackSnorm2x16(g_in_data3.deltas[sh_i].dnz_and_db.y);
        b += shape_weight * vec3(_dnz_and_dbx.y, _dbyz);
    }

    for (uint j = g_params.uShapeParamsPrev.x; j < g_params.uShapeParamsPrev.x + g_params.uShapeParamsPrev.y; j++) {
        highp uint shape_data = g_in_data2.shape_keys[j];
        mediump uint shape_index = bitfieldExtract(shape_data, 0, 16);
        mediump float shape_weight = unpackUnorm2x16(shape_data).y;

        int sh_i = int(g_params.uShapeParamsPrev.z + shape_index * g_params.uSkinParams.y + gl_GlobalInvocationID.x);
        p2 += shape_weight * vec3(g_in_data3.deltas[sh_i].dpxy, g_in_data3.deltas[sh_i].dpz_dnxy.x);
    }

    mediump uvec4 bone_indices = uvec4(
        bitfieldExtract(g_in_data0.vertices[in_ndx].bone_indices.x, 0, 16),
        bitfieldExtract(g_in_data0.vertices[in_ndx].bone_indices.x, 16, 16),
        bitfieldExtract(g_in_data0.vertices[in_ndx].bone_indices.y, 0, 16),
        bitfieldExtract(g_in_data0.vertices[in_ndx].bone_indices.y, 16, 16)
    );
    mediump vec4 bone_weights = vec4(
        unpackUnorm2x16(g_in_data0.vertices[in_ndx].bone_weights.x),
        unpackUnorm2x16(g_in_data0.vertices[in_ndx].bone_weights.y)
    );

    highp mat3x4 mat_curr = mat3x4(0.0);
    highp mat3x4 mat_prev = mat3x4(0.0);

    for (int j = 0; j < 4; j++) {
        if (bone_weights[j] > 0.0) {
            mat_curr += g_in_data1.matrices[2u * (xform_offset + bone_indices[j]) + 0u] * bone_weights[j];
            mat_prev += g_in_data1.matrices[2u * (xform_offset + bone_indices[j]) + 1u] * bone_weights[j];
        }
    }

    highp mat4x3 tr_mat_curr = transpose(mat_curr);

    highp vec3 p_curr = tr_mat_curr * vec4(p, 1.0);
    mediump vec3 n_curr = tr_mat_curr * vec4(n, 0.0);
    mediump vec3 b_curr = tr_mat_curr * vec4(b, 0.0);

    highp uint out_ndx_curr = g_params.uSkinParams.w + gl_GlobalInvocationID.x;

    g_out_data0.vertices[out_ndx_curr].p_and_t0.xyz = p_curr;
    // copy texture coordinates unchanged
    g_out_data0.vertices[out_ndx_curr].p_and_t0.w = uintBitsToFloat(g_in_data0.vertices[in_ndx].t0_and_t1.x);

    g_out_data1.vertices[out_ndx_curr].n_and_bx.x = packSnorm2x16(n_curr.xy);
    g_out_data1.vertices[out_ndx_curr].n_and_bx.y = packSnorm2x16(vec2(n_curr.z, b_curr.x));
    g_out_data1.vertices[out_ndx_curr].byz_and_t1.x = packSnorm2x16(b_curr.yz);
    // copy texture coordinates unchanged
    g_out_data1.vertices[out_ndx_curr].byz_and_t1.y = g_in_data0.vertices[in_ndx].t0_and_t1.y;

    highp mat4x3 tr_mat_prev = transpose(mat_prev);
    highp vec3 p_prev = tr_mat_prev * vec4(p2, 1.0);

    highp uint out_ndx_prev = out_ndx_curr + uint(REN_MAX_SKIN_VERTICES_TOTAL);
    g_out_data0.vertices[out_ndx_prev].p_and_t0.xyz = p_prev;
}
