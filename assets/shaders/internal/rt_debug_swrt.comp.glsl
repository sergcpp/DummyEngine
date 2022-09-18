#version 310 es

#if defined(GL_ES) || defined(VULKAN)
    precision highp int;
    precision highp float;
#endif

#include "_rt_common.glsl"
#include "_swrt_common.glsl"
#include "_texturing.glsl"

#include "rt_debug_interface.glsl"

/*
UNIFORM_BLOCKS
    SharedDataBlock : $ubSharedDataLoc
    UniformParams : $ubUnifParamLoc
*/

LAYOUT_PARAMS uniform UniformParams {
    Params g_params;
};

#if defined(VULKAN) || defined(GL_SPIRV)
layout (binding = REN_UB_SHARED_DATA_LOC, std140)
#else
layout (std140)
#endif
uniform SharedDataBlock {
    SharedData g_shrd_data;
};

layout(std430, binding = BLAS_BUF_SLOT) readonly buffer Blas {
    bvh_node_t g_blas_nodes[];
};

layout(std430, binding = TLAS_BUF_SLOT) readonly buffer Tlas {
    bvh_node_t g_tlas_nodes[];
};

layout(std430, binding = GEO_DATA_BUF_SLOT) readonly buffer GeometryData {
    RTGeoInstance g_geometries[];
};

layout(std430, binding = MATERIAL_BUF_SLOT) readonly buffer Materials {
    MaterialData g_materials[];
};

layout(std430, binding = VTX_BUF1_SLOT) readonly buffer VtxData0 {
    vec4 g_vtx_data0[];
};

layout(std430, binding = VTX_BUF2_SLOT) readonly buffer VtxData1 {
    uvec4 g_vtx_data1[];
};

layout(std430, binding = NDX_BUF_SLOT) readonly buffer VtxNdxData {
    uint g_vtx_indices[];
};

layout(std430, binding = PRIM_NDX_BUF_SLOT) readonly buffer PrimNdxData {
    uint g_prim_indices[];
};

layout(std430, binding = MESHES_BUF_SLOT) readonly buffer MeshesData {
    mesh_t g_meshes[];
};

layout(std430, binding = MESH_INSTANCES_BUF_SLOT) readonly buffer MeshInstancesData {
    mesh_instance_t g_mesh_instances[];
};

layout(binding = LMAP_TEX_SLOTS) uniform sampler2D g_lm_textures[5];

layout(binding = ENV_TEX_SLOT) uniform samplerCube g_env_tex;

layout(binding = OUT_IMG_SLOT, r11f_g11f_b10f) uniform image2D g_out_image;

shared uint g_stack[LOCAL_GROUP_SIZE_X * LOCAL_GROUP_SIZE_Y][MAX_STACK_SIZE];

void IntersectTris_ClosestHit(vec3 ro, vec3 rd, int tri_start, int tri_end, uint first_vertex,
                              int obj_index, inout hit_data_t out_inter, int geo_index, int geo_count) {
    for (int i = tri_start; i < tri_end; ++i) {
        uint j = g_prim_indices[i];

        float t, u, v;
        if (IntersectTri(ro, rd, g_vtx_data0[first_vertex + g_vtx_indices[3 * j + 0]].xyz,
                                 g_vtx_data0[first_vertex + g_vtx_indices[3 * j + 1]].xyz,
                                 g_vtx_data0[first_vertex + g_vtx_indices[3 * j + 2]].xyz, t, u, v) && t > 0.0 && t < out_inter.t) {
            out_inter.mask = -1;
            out_inter.prim_index = int(j);
            out_inter.obj_index = obj_index;
            out_inter.geo_index = geo_index;
            out_inter.geo_count = geo_count;
            out_inter.t = t;
            out_inter.u = u;
            out_inter.v = v;
        }
    }
}

void Traverse_MicroTree_WithStack(vec3 ro, vec3 rd, vec3 inv_d, int obj_index, uint node_index,
                                  uint first_vertex, uint stack_size, inout hit_data_t inter,
                                  int geo_index, int geo_count) {
    vec3 neg_inv_do = -inv_d * ro;

    uint initial_stack_size = stack_size;
    g_stack[gl_LocalInvocationIndex][stack_size++] = node_index;

    while (stack_size != initial_stack_size) {
        uint cur = g_stack[gl_LocalInvocationIndex][--stack_size];

        bvh_node_t n = g_blas_nodes[cur];

        if (!_bbox_test_fma(inv_d, neg_inv_do, inter.t, n.bbox_min.xyz, n.bbox_max.xyz)) {
            continue;
        }

        if ((floatBitsToUint(n.bbox_min.w) & LEAF_NODE_BIT) == 0) {
            g_stack[gl_LocalInvocationIndex][stack_size++] = far_child(rd, n);
            g_stack[gl_LocalInvocationIndex][stack_size++] = near_child(rd, n);
        } else {
            int tri_beg = int(floatBitsToUint(n.bbox_min.w) & PRIM_INDEX_BITS);
            int tri_end = tri_beg + floatBitsToInt(n.bbox_max.w);

            IntersectTris_ClosestHit(ro, rd, tri_beg, tri_end, first_vertex, obj_index, inter, geo_index, geo_count);
        }
    }
}

void Traverse_MacroTree_WithStack(vec3 orig_ro, vec3 orig_rd, vec3 orig_inv_rd, uint node_index,
                                  inout hit_data_t inter) {
    vec3 orig_neg_inv_do = -orig_inv_rd * orig_ro;

    uint stack_size = 0;
    g_stack[gl_LocalInvocationIndex][stack_size++] = node_index;

    while (stack_size != 0) {
        uint cur = g_stack[gl_LocalInvocationIndex][--stack_size];

        bvh_node_t n = g_tlas_nodes[cur];

        if (!_bbox_test_fma(orig_inv_rd, orig_neg_inv_do, inter.t, n.bbox_min.xyz, n.bbox_max.xyz)) {
            continue;
        }

        if ((floatBitsToUint(n.bbox_min.w) & LEAF_NODE_BIT) == 0) {
            g_stack[gl_LocalInvocationIndex][stack_size++] = far_child(orig_rd, n);
            g_stack[gl_LocalInvocationIndex][stack_size++] = near_child(orig_rd, n);
        } else {
            uint prim_index = (floatBitsToUint(n.bbox_min.w) & PRIM_INDEX_BITS);
            uint prim_count = floatBitsToUint(n.bbox_max.w);
            for (uint i = prim_index; i < prim_index + prim_count; ++i) {
                mesh_instance_t mi = g_mesh_instances[i];
                mesh_t m = g_meshes[floatBitsToUint(mi.bbox_max.w)];

                if (!_bbox_test_fma(orig_inv_rd, orig_neg_inv_do, inter.t, mi.bbox_min.xyz, mi.bbox_max.xyz)) {
                    continue;
                }

                mat4x3 inv_transform = transpose(mi.inv_transform);

                vec3 ro = (inv_transform * vec4(orig_ro, 1.0)).xyz;
                vec3 rd = (inv_transform * vec4(orig_rd, 0.0)).xyz;
                vec3 inv_d = safe_invert(rd);

                uint geo_index = floatBitsToUint(mi.bbox_min.w);
                uint geo_count = m.geo_count;

                Traverse_MicroTree_WithStack(ro, rd, inv_d, int(i), m.node_index, m.vert_index,
                                             stack_size, inter, int(geo_index), int(geo_count));
            }
        }
    }
}

int hash(const int x) {
    uint ret = uint(x);
    ret = ((ret >> 16) ^ ret) * 0x45d9f3b;
    ret = ((ret >> 16) ^ ret) * 0x45d9f3b;
    ret = (ret >> 16) ^ ret;
    return int(ret);
}

float construct_float(uint m) {
    const uint ieeeMantissa = 0x007FFFFFu; // binary32 mantissa bitmask
    const uint ieeeOne      = 0x3F800000u; // 1.0 in IEEE binary32

    m &= ieeeMantissa;                     // Keep only mantissa bits (fractional part)
    m |= ieeeOne;                          // Add fractional part to 1.0

    const float  f = uintBitsToFloat(m);   // Range [1:2]
    return f - 1.0;                        // Range [0:1]
}

layout (local_size_x = LOCAL_GROUP_SIZE_X, local_size_y = LOCAL_GROUP_SIZE_Y, local_size_z = 1) in;

void main() {
    if (gl_GlobalInvocationID.x >= g_params.img_size.x || gl_GlobalInvocationID.y >= g_params.img_size.y) {
        return;
    }

    const vec2 px_center = vec2(gl_GlobalInvocationID.xy) + vec2(0.5);
    const vec2 in_uv = px_center / vec2(g_params.img_size);
    vec2 d = in_uv * 2.0 - 1.0;
#if defined(VULKAN)
    d.y = -d.y;
#endif

    vec4 origin = g_shrd_data.inv_view_matrix * vec4(0, 0, 0, 1);
    origin /= origin.w;
    vec4 target = g_shrd_data.inv_proj_matrix * vec4(d.xy, 1, 1);
    target /= target.w;
    vec4 direction = g_shrd_data.inv_view_matrix * vec4(normalize(target.xyz), 0);

    vec3 inv_d = safe_invert(direction.xyz);

    hit_data_t inter;
    inter.mask = 0;
    inter.obj_index = inter.prim_index = 0;
    inter.t = MAX_DIST;
    inter.u = inter.v = 0.0;

    Traverse_MacroTree_WithStack(origin.xyz, direction.xyz, inv_d, 0 /* root_node */, inter);

    vec3 col;
    if (inter.mask == 0) {
        col = RGBMDecode(texture(g_env_tex, direction.xyz));
    } else {
        int i = inter.geo_index;
        for (; i < inter.geo_index + inter.geo_count; ++i) {
            int tri_start = int(g_geometries[i].indices_start) / 3;
            if (tri_start > inter.prim_index) {
                break;
            }
        }
    
        int geo_index = i - 1;
        
        RTGeoInstance geo = g_geometries[geo_index];
        MaterialData mat = g_materials[geo.material_index];

        uint i0 = g_vtx_indices[3 * inter.prim_index + 0];
        uint i1 = g_vtx_indices[3 * inter.prim_index + 1];
        uint i2 = g_vtx_indices[3 * inter.prim_index + 2];

        vec3 p0 = g_vtx_data0[geo.vertices_start + i0].xyz;
        vec3 p1 = g_vtx_data0[geo.vertices_start + i1].xyz;
        vec3 p2 = g_vtx_data0[geo.vertices_start + i2].xyz;

        vec2 uv0 = unpackHalf2x16(floatBitsToUint(g_vtx_data0[geo.vertices_start + i0].w));
        vec2 uv1 = unpackHalf2x16(floatBitsToUint(g_vtx_data0[geo.vertices_start + i1].w));
        vec2 uv2 = unpackHalf2x16(floatBitsToUint(g_vtx_data0[geo.vertices_start + i2].w));

        vec2 uv = uv0 * (1.0 - inter.u - inter.v) + uv1 * inter.u + uv2 * inter.v;
#if defined(BINDLESS_TEXTURES)
        mat4x3 inv_transform = transpose(g_mesh_instances[inter.obj_index].inv_transform);
        vec3 direction_obj_space = (inv_transform * vec4(direction.xyz, 0.0)).xyz;

        vec2 tex_res = textureSize(SAMPLER2D(mat.texture_indices[0]), 0).xy;
        float ta = abs((uv1.x - uv0.x) * (uv2.y - uv0.y) - (uv2.x - uv0.x) * (uv1.y - uv0.y));

        vec3 tri_normal = cross(p1 - p0, p2 - p0);
        float pa = length(tri_normal);
        tri_normal /= pa;

        float cone_width = g_params.pixel_spread_angle * inter.t;

        float tex_lod = 0.5 * log2(ta/pa);
        tex_lod += log2(cone_width);
        tex_lod += 0.5 * log2(tex_res.x * tex_res.y);
        tex_lod -= log2(abs(dot(direction_obj_space, tri_normal)));

        col = SRGBToLinear(YCoCg_to_RGB(textureLod(SAMPLER2D(mat.texture_indices[0]), uv, tex_lod)));
#else
        // TODO: Fallback to shared texture atlas
#endif

        if ((geo.flags & RTGeoLightmappedBit) != 0u) {
            vec2 lm_uv0 = unpackHalf2x16(g_vtx_data1[geo.vertices_start + i0].w);
            vec2 lm_uv1 = unpackHalf2x16(g_vtx_data1[geo.vertices_start + i1].w);
            vec2 lm_uv2 = unpackHalf2x16(g_vtx_data1[geo.vertices_start + i2].w);

            vec2 lm_uv = lm_uv0 * (1.0 - inter.u - inter.v) + lm_uv1 * inter.u + lm_uv2 * inter.v;
            lm_uv = geo.lmap_transform.xy + geo.lmap_transform.zw * lm_uv;

            vec3 direct_lm = RGBMDecode(textureLod(g_lm_textures[0], lm_uv, 0.0));
            vec3 indirect_lm = 2.0 * RGBMDecode(textureLod(g_lm_textures[1], lm_uv, 0.0));
            col *= (direct_lm + indirect_lm);
        } else {
            vec3 normal0 = vec3(unpackSnorm2x16(g_vtx_data1[geo.vertices_start + i0].x), unpackSnorm2x16(g_vtx_data1[geo.vertices_start + i0].y).x);
            vec3 normal1 = vec3(unpackSnorm2x16(g_vtx_data1[geo.vertices_start + i1].x), unpackSnorm2x16(g_vtx_data1[geo.vertices_start + i1].y).x);
            vec3 normal2 = vec3(unpackSnorm2x16(g_vtx_data1[geo.vertices_start + i2].x), unpackSnorm2x16(g_vtx_data1[geo.vertices_start + i2].y).x);

            vec3 normal = normal0 * (1.0 - inter.u - inter.v) + normal1 * inter.u + normal2 * inter.v;

            col *= EvalSHIrradiance_NonLinear(normal,
                                              g_shrd_data.probes[geo.flags & RTGeoProbeBits].sh_coeffs[0],
                                              g_shrd_data.probes[geo.flags & RTGeoProbeBits].sh_coeffs[1],
                                              g_shrd_data.probes[geo.flags & RTGeoProbeBits].sh_coeffs[2]);
        }
    }

    imageStore(g_out_image, ivec2(gl_GlobalInvocationID.xy), vec4(col, 1.0));
}