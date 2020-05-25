R"(#version )" GLSL_VERSION_STR R"(

#define REN_MAX_SKIN_VERTICES_TOTAL )" AS_STR(REN_MAX_SKIN_VERTICES_TOTAL) R"(

struct InVertex {
    highp vec4 p_and_nxy;
    highp uvec2 nz_and_b;
    highp uvec2 t0_and_t1;
    highp uvec2 bone_indices;
    highp uvec2 bone_weights;
};

struct OutVertexData0 {
    highp vec4 p_and_t0;
};

struct OutVertexData1 {
    highp uvec2 n_and_bx;
    highp uvec2 byz_and_t1;
};

layout(std430, binding = 0) readonly buffer Input0 {
    InVertex vertices[];
} in_data0;

layout(std430, binding = 1) readonly buffer Input1 {
    highp mat3x4 matrices[];
} in_data1;

layout(std430, binding = 2) writeonly buffer Output0 {
    OutVertexData0 vertices[];
} out_data0;

layout(std430, binding = 3) writeonly buffer Output1 {
    OutVertexData1 vertices[];
} out_data1;

layout(location = 0) uniform highp uvec4 uParams;

layout (local_size_x = 256, local_size_y = 1, local_size_z = 1) in;

void main() {
    if (gl_GlobalInvocationID.x >= uParams.y) {
        return;
    }

    highp uint in_ndx = uParams.x + gl_GlobalInvocationID.x;
    mediump uint xform_offset = uParams.z;

    highp vec3 p = in_data0.vertices[in_ndx].p_and_nxy.xyz;

    highp uint _nxy = floatBitsToUint(in_data0.vertices[in_ndx].p_and_nxy.w);
    highp vec2 nxy = unpackSnorm2x16(_nxy);

    highp uint _nz_and_bx = in_data0.vertices[in_ndx].nz_and_b.x;
    highp vec2 nz_and_bx = unpackSnorm2x16(_nz_and_bx);

    highp uint _byz = in_data0.vertices[in_ndx].nz_and_b.y;
    highp vec2 byz = unpackSnorm2x16(_byz);

    highp vec3 n = vec3(nxy, nz_and_bx.x),
               b = vec3(nz_and_bx.y, byz);

    mediump uvec4 bone_indices = uvec4(
        bitfieldExtract(in_data0.vertices[in_ndx].bone_indices.x, 0, 16),
        bitfieldExtract(in_data0.vertices[in_ndx].bone_indices.x, 16, 16),
        bitfieldExtract(in_data0.vertices[in_ndx].bone_indices.y, 0, 16),
        bitfieldExtract(in_data0.vertices[in_ndx].bone_indices.y, 16, 16)
    );
    mediump vec4 bone_weights = vec4(
        unpackUnorm2x16(in_data0.vertices[in_ndx].bone_weights.x),
        unpackUnorm2x16(in_data0.vertices[in_ndx].bone_weights.y)
    );

    highp mat3x4 mat_curr = mat3x4(0.0);
    highp mat3x4 mat_prev = mat3x4(0.0);

    for (int j = 0; j < 4; j++) {
        if (bone_weights[j] > 0.0) {
            mat_curr += in_data1.matrices[2 * (xform_offset + bone_indices[j]) + 0] * bone_weights[j];
            mat_prev += in_data1.matrices[2 * (xform_offset + bone_indices[j]) + 1] * bone_weights[j];
        }
    }

    highp mat4x3 tr_mat_curr = transpose(mat_curr);

    highp vec3 p_curr = tr_mat_curr * vec4(p, 1.0);
    mediump vec3 n_curr = tr_mat_curr * vec4(n, 0.0);
    mediump vec3 b_curr = tr_mat_curr * vec4(b, 0.0);

    highp uint out_ndx_curr = uParams.w + gl_GlobalInvocationID.x;

    out_data0.vertices[out_ndx_curr].p_and_t0.xyz = p_curr;
    // copy texture coordinates unchanged
    out_data0.vertices[out_ndx_curr].p_and_t0.w = uintBitsToFloat(in_data0.vertices[in_ndx].t0_and_t1.x);

    out_data1.vertices[out_ndx_curr].n_and_bx.x = packSnorm2x16(n_curr.xy);
    out_data1.vertices[out_ndx_curr].n_and_bx.y = packSnorm2x16(vec2(n_curr.z, b_curr.x));
    out_data1.vertices[out_ndx_curr].byz_and_t1.x = packSnorm2x16(b_curr.yz);
    // copy texture coordinates unchanged
    out_data1.vertices[out_ndx_curr].byz_and_t1.y = in_data0.vertices[in_ndx].t0_and_t1.y;

    highp mat4x3 tr_mat_prev = transpose(mat_prev);
    highp vec3 p_prev = tr_mat_prev * vec4(p, 1.0);

    highp uint out_ndx_prev = out_ndx_curr + REN_MAX_SKIN_VERTICES_TOTAL;
    out_data0.vertices[out_ndx_prev].p_and_t0.xyz = p_prev;
}

)"
