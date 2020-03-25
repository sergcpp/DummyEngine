R"(#version )" GLSL_VERSION_STR R"(

struct InVertex {
    highp vec4 p_and_nxy;
    highp uvec2 nz_and_b;
    highp uvec2 t0_and_t1;
    highp uvec2 bone_indices;
    highp uvec2 bone_weights;
};

struct SkinRegion {
    highp uint in_vtx_offset, out_vtx_offset;
    highp uint xform_offset_and_vertex_count;
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

layout(std430, binding = 2) readonly buffer Input2 {
    SkinRegion skin_regions[];
} in_data2;

layout(std430, binding = 3) writeonly buffer Output0 {
    OutVertexData0 vertices[];
} out_data0;

layout(std430, binding = 4) writeonly buffer Output1 {
    OutVertexData1 vertices[];
} out_data1;

layout (local_size_x = 256, local_size_y = 1, local_size_z = 1) in;

void main() {
    int reg_id = int(gl_WorkGroupID.x);
    highp uint in_ndx = in_data2.skin_regions[reg_id].in_vtx_offset + gl_LocalInvocationID.x;
    mediump uint xform_offset = bitfieldExtract(in_data2.skin_regions[reg_id].xform_offset_and_vertex_count, 0, 16);
    mediump uint vertex_count = bitfieldExtract(in_data2.skin_regions[reg_id].xform_offset_and_vertex_count, 16, 16);

    if (gl_LocalInvocationID.x >= vertex_count) {
        return;
    }

    highp vec3 p = in_data0.vertices[in_ndx].p_and_nxy.xyz;

    highp uint _nxy = floatBitsToUint(in_data0.vertices[in_ndx].p_and_nxy.w);
    highp vec2 nxy = unpackSnorm2x16(_nxy);

    highp uint _nz_and_bx = in_data0.vertices[in_ndx].nz_and_b.x;
    highp vec2 nz_and_bx = unpackSnorm2x16(_nz_and_bx);

    highp uint _byz = in_data0.vertices[in_ndx].nz_and_b.y;
    highp vec2 byz = unpackSnorm2x16(_byz);

    highp vec3 n = vec3(nxy, nz_and_bx.x),
               b = vec3(nz_and_bx.y, byz);

    mediump uvec4 bone_indices = uvec4(bitfieldExtract(in_data0.vertices[in_ndx].bone_indices.x, 0, 16),
                                       bitfieldExtract(in_data0.vertices[in_ndx].bone_indices.x, 16, 16),
                                       bitfieldExtract(in_data0.vertices[in_ndx].bone_indices.y, 0, 16),
                                       bitfieldExtract(in_data0.vertices[in_ndx].bone_indices.y, 16, 16));
    mediump vec4 bone_weights = vec4(unpackUnorm2x16(in_data0.vertices[in_ndx].bone_weights.x),
                                     unpackUnorm2x16(in_data0.vertices[in_ndx].bone_weights.y));

    highp mat3x4 _mat = mat3x4(0.0);

    for (int j = 0; j < 4; j++) {
        if (bone_weights[j] > 0.0) {
            _mat = _mat + in_data1.matrices[xform_offset + bone_indices[j]] * bone_weights[j];
        }
    }

    highp mat4x3 _tr_mat = transpose(_mat);

    highp vec3 tr_p = _tr_mat * vec4(p, 1.0);
    mediump vec3 tr_n = _tr_mat * vec4(n, 0.0);
    mediump vec3 tr_b = _tr_mat * vec4(b, 0.0);

    highp uint out_ndx = in_data2.skin_regions[reg_id].out_vtx_offset + gl_LocalInvocationID.x;

    out_data0.vertices[out_ndx].p_and_t0.xyz = tr_p;
    // copy texture coordinates unchanged
    out_data0.vertices[out_ndx].p_and_t0.w = uintBitsToFloat(in_data0.vertices[in_ndx].t0_and_t1.x);

    out_data1.vertices[out_ndx].n_and_bx.x = packSnorm2x16(tr_n.xy);
    out_data1.vertices[out_ndx].n_and_bx.y = packSnorm2x16(vec2(tr_n.z, tr_b.x));
    out_data1.vertices[out_ndx].byz_and_t1.x = packSnorm2x16(tr_b.yz);
    // copy texture coordinates unchanged
    out_data1.vertices[out_ndx].byz_and_t1.y = in_data0.vertices[in_ndx].t0_and_t1.y;
}

)"
