#version 450
#extension GL_GOOGLE_include_directive : require
#if HWRT
#extension GL_EXT_ray_query : require
#endif
#if SUBGROUP
#extension GL_KHR_shader_subgroup_vote : require
#endif

#include "intersect_scene_shadow_interface.h"
#include "common.glsl"
#include "texture.glsl"
#include "light_bvh.glsl"

layout(push_constant) uniform UniformParams {
    Params g_params;
};

layout(std430, binding = TRIS_BUF_SLOT) readonly buffer Tris {
    tri_accel_t g_tris[];
};

layout(std430, binding = TRI_INDICES_BUF_SLOT) readonly buffer TriIndices {
    uint g_tri_indices[];
};

layout(std430, binding = TRI_MATERIALS_BUF_SLOT) readonly buffer TriMaterials {
    uint g_tri_materials[];
};

layout(std430, binding = MATERIALS_BUF_SLOT) readonly buffer Materials {
    material_t g_materials[];
};

layout(std430, binding = NODES_BUF_SLOT) readonly buffer Nodes {
    bvh2_node_t g_nodes[];
};

layout(std430, binding = MESH_INSTANCES_BUF_SLOT) readonly buffer MeshInstances {
    mesh_instance_t g_mesh_instances[];
};

layout(std430, binding = VERTICES_BUF_SLOT) readonly buffer Vertices {
    vertex_t g_vertices[];
};

layout(std430, binding = VTX_INDICES_BUF_SLOT) readonly buffer VtxIndices {
    uint g_vtx_indices[];
};

layout(std430, binding = SH_RAYS_BUF_SLOT) readonly buffer Rays {
    shadow_ray_t g_sh_rays[];
};

layout(std430, binding = COUNTERS_BUF_SLOT) readonly buffer Counters {
    uint g_counters[];
};

layout(std430, binding = LIGHTS_BUF_SLOT) readonly buffer Lights {
    light_t g_lights[];
};

layout(std430, binding = LIGHT_CWNODES_BUF_SLOT) readonly buffer LightCWNodes {
    light_cwbvh_node_t g_light_cwnodes[];
};

layout(std430, binding = RANDOM_SEQ_BUF_SLOT) readonly buffer Random {
    uint g_random_seq[];
};

#if HWRT
layout(binding = TLAS_SLOT) uniform accelerationStructureEXT g_tlas;
#endif

layout(binding = INOUT_IMG_SLOT, rgba32f) uniform image2D g_inout_img;

vec2 get_scrambled_2d_rand(const uint dim, const uint seed, const int _sample) {
    const uint i_seed = hash_combine(seed, dim),
               x_seed = hash_combine(seed, 2 * dim + 0),
               y_seed = hash_combine(seed, 2 * dim + 1);

    const uint shuffled_dim = uint(nested_uniform_scramble_base2(dim, seed) & (RAND_DIMS_COUNT - 1));
    const uint shuffled_i = uint(nested_uniform_scramble_base2(_sample, i_seed) & (RAND_SAMPLES_COUNT - 1));
    return vec2(scramble_unorm(x_seed, g_random_seq[shuffled_dim * 2 * RAND_SAMPLES_COUNT + 2 * shuffled_i + 0]),
                scramble_unorm(y_seed, g_random_seq[shuffled_dim * 2 * RAND_SAMPLES_COUNT + 2 * shuffled_i + 1]));
}

#define FETCH_TRI(j) g_tris[j]
#include "traverse_bvh.glsl"

layout (local_size_x = LOCAL_GROUP_SIZE_X, local_size_y = LOCAL_GROUP_SIZE_Y, local_size_z = 1) in;

#if HWRT
shared uint g_stack[LOCAL_GROUP_SIZE_X * LOCAL_GROUP_SIZE_Y][MAX_LTREE_STACK_SIZE];
#else
shared uint g_stack[LOCAL_GROUP_SIZE_X * LOCAL_GROUP_SIZE_Y][MAX_STACK_SIZE];
#endif

bool Traverse_BLAS_WithStack(vec3 ro, vec3 rd, vec3 inv_d, int obj_index, uint node_index, uint stack_size,
                                  inout hit_data_t inter) {
    vec3 neg_inv_do = -inv_d * ro;

    uint initial_stack_size = stack_size;
    g_stack[gl_LocalInvocationIndex][stack_size++] = 0x1fffffffu;

    uint cur = node_index;
    while (stack_size != initial_stack_size) {
        uint leaf_node = 0;
        while (stack_size != initial_stack_size && (cur & BVH2_PRIM_COUNT_BITS) == 0) {
            const bvh2_node_t n = g_nodes[cur];

            uvec2 children = n.child.xy;

            const vec3 ch0_min = vec3(n.ch_data0[0], n.ch_data0[2], n.ch_data2[0]);
            const vec3 ch0_max = vec3(n.ch_data0[1], n.ch_data0[3], n.ch_data2[1]);

            const vec3 ch1_min = vec3(n.ch_data1[0], n.ch_data1[2], n.ch_data2[2]);
            const vec3 ch1_max = vec3(n.ch_data1[1], n.ch_data1[3], n.ch_data2[3]);

            float ch0_dist, ch1_dist;
            const bool ch0_res = bbox_test(inv_d, neg_inv_do, inter.t, ch0_min, ch0_max, ch0_dist);
            const bool ch1_res = bbox_test(inv_d, neg_inv_do, inter.t, ch1_min, ch1_max, ch1_dist);

            if (!ch0_res && !ch1_res) {
                cur = g_stack[gl_LocalInvocationIndex][--stack_size];
            } else {
                cur = ch0_res ? children[0] : children[1];
                if (ch0_res && ch1_res) {
                    if (ch1_dist < ch0_dist) {
                        const uint temp = cur;
                        cur = children[1];
                        children[1] = temp;
                    }
                    g_stack[gl_LocalInvocationIndex][stack_size++] = children[1];
                }
            }
            if ((cur & BVH2_PRIM_COUNT_BITS) != 0 && (leaf_node & BVH2_PRIM_COUNT_BITS) == 0) {
                leaf_node = cur;
                cur = g_stack[gl_LocalInvocationIndex][--stack_size];
            }
#if SUBGROUP
            if (subgroupAll((leaf_node & BVH2_PRIM_COUNT_BITS) != 0)) {
                break;
            }
#else
            if ((leaf_node & BVH2_PRIM_COUNT_BITS) != 0) {
                break;
            }
#endif
        }
        while ((leaf_node & BVH2_PRIM_COUNT_BITS) != 0) {
            const int tri_start = int(leaf_node & BVH2_PRIM_INDEX_BITS),
                      tri_end = int(tri_start + ((leaf_node & BVH2_PRIM_COUNT_BITS) >> 29) + 1);

            const bool hit_found = IntersectTris_AnyHit(ro, rd, tri_start, tri_end, obj_index, inter);
            if (hit_found) {
                const bool is_backfacing = (inter.prim_index < 0);
                const uint prim_index = is_backfacing ? -inter.prim_index - 1 : inter.prim_index;
                const uint tri_index = g_tri_indices[prim_index];

                const uint front_mi = (g_tri_materials[tri_index] & 0xffff);
                const uint back_mi = (g_tri_materials[tri_index] >> 16u) & 0xffff;

                if ((!is_backfacing && (front_mi & MATERIAL_SOLID_BIT) != 0) ||
                    (is_backfacing && (back_mi & MATERIAL_SOLID_BIT) != 0)) {
                    return true;
                }
            }

            leaf_node = cur;
            if ((cur & BVH2_PRIM_COUNT_BITS) != 0) {
                cur = g_stack[gl_LocalInvocationIndex][--stack_size];
            }
        }
    }

    return false;
}

bool Traverse_TLAS_WithStack(vec3 orig_ro, vec3 orig_rd, vec3 orig_inv_rd, uint node_index, inout hit_data_t inter) {
    vec3 orig_neg_inv_do = -orig_inv_rd * orig_ro;

    uint stack_size = 0;
    g_stack[gl_LocalInvocationIndex][stack_size++] = 0x1fffffffu;

    uint cur = node_index;
    while (stack_size != 0) {
        uint leaf_node = 0;
        while (stack_size != 0 && (cur & BVH2_PRIM_COUNT_BITS) == 0) {
            const bvh2_node_t n = g_nodes[cur];

            uvec2 children = n.child.xy;

            const vec3 ch0_min = vec3(n.ch_data0[0], n.ch_data0[2], n.ch_data2[0]);
            const vec3 ch0_max = vec3(n.ch_data0[1], n.ch_data0[3], n.ch_data2[1]);

            const vec3 ch1_min = vec3(n.ch_data1[0], n.ch_data1[2], n.ch_data2[2]);
            const vec3 ch1_max = vec3(n.ch_data1[1], n.ch_data1[3], n.ch_data2[3]);

            float ch0_dist, ch1_dist;
            const bool ch0_res = bbox_test(orig_inv_rd, orig_neg_inv_do, inter.t, ch0_min, ch0_max, ch0_dist);
            const bool ch1_res = bbox_test(orig_inv_rd, orig_neg_inv_do, inter.t, ch1_min, ch1_max, ch1_dist);

            if (!ch0_res && !ch1_res) {
                cur = g_stack[gl_LocalInvocationIndex][--stack_size];
            } else {
                cur = ch0_res ? children[0] : children[1];
                if (ch0_res && ch1_res) {
                    if (ch1_dist < ch0_dist) {
                        const uint temp = cur;
                        cur = children[1];
                        children[1] = temp;
                    }
                    g_stack[gl_LocalInvocationIndex][stack_size++] = children[1];
                }
            }
            if ((cur & BVH2_PRIM_COUNT_BITS) != 0 && (leaf_node & BVH2_PRIM_COUNT_BITS) == 0) {
                leaf_node = cur;
                cur = g_stack[gl_LocalInvocationIndex][--stack_size];
            }
#if SUBGROUP
            if (subgroupAll((leaf_node & BVH2_PRIM_COUNT_BITS) != 0)) {
                break;
            }
#else
            if ((leaf_node & BVH2_PRIM_COUNT_BITS) != 0) {
                break;
            }
#endif
        }
        while ((leaf_node & BVH2_PRIM_COUNT_BITS) != 0) {
            const uint mi_index = (leaf_node & BVH2_PRIM_INDEX_BITS);
            const mesh_instance_t mi = g_mesh_instances[mi_index];
            if ((mi.data.w & RAY_TYPE_SHADOW_BIT) != 0) {
                const vec3 ro = (mi.inv_xform * vec4(orig_ro, 1.0)).xyz;
                const vec3 rd = (mi.inv_xform * vec4(orig_rd, 0.0)).xyz;
                const vec3 inv_d = safe_invert(rd);

                const bool solid_hit_found = Traverse_BLAS_WithStack(ro, rd, inv_d, int(mi_index), mi.data.y, stack_size, inter);
                if (solid_hit_found) {
                    return true;
                }
            }
            leaf_node = cur;
            if ((cur & BVH2_PRIM_COUNT_BITS) != 0) {
                cur = g_stack[gl_LocalInvocationIndex][--stack_size];
            }
        }
    }

    return false;
}

vec3 IntersectSceneShadow(shadow_ray_t r) {
    vec3 ro = vec3(r.o[0], r.o[1], r.o[2]);
    vec3 rd = vec3(r.d[0], r.d[1], r.d[2]);
    vec3 rc = vec3(r.c[0], r.c[1], r.c[2]);
    int depth = get_transp_depth(r.depth);

    const uint px_hash = hash(r.xy);
    const uint rand_hash = hash_combine(px_hash, g_params.rand_seed);

    uint rand_dim = RAND_DIM_BASE_COUNT + get_total_depth(r.depth) * RAND_DIM_BOUNCE_COUNT;

    float dist = r.dist > 0.0 ? r.dist : MAX_DIST;
#if !HWRT
    const vec3 inv_d = safe_invert(rd);

    while (dist > HIT_BIAS) {
        hit_data_t sh_inter;
        sh_inter.t = dist;
        sh_inter.v = -1.0;

        const bool solid_hit = Traverse_TLAS_WithStack(ro, rd, inv_d, g_params.node_index, sh_inter);
        if (solid_hit || depth > g_params.max_transp_depth) {
            return vec3(0.0);
        }

        if (sh_inter.v < 0.0) {
            return rc;
        }

        const bool is_backfacing = (sh_inter.prim_index < 0);
        const uint prim_index = is_backfacing ? -sh_inter.prim_index - 1 : sh_inter.prim_index;

        const uint tri_index = g_tri_indices[prim_index];

        const uint front_mi = (g_tri_materials[tri_index] & 0xffff);
        const uint back_mi = (g_tri_materials[tri_index] >> 16u) & 0xffff;

        const uint mat_index = (is_backfacing ? back_mi : front_mi) & MATERIAL_INDEX_BITS;

        const vertex_t v1 = g_vertices[g_vtx_indices[tri_index * 3 + 0]];
        const vertex_t v2 = g_vertices[g_vtx_indices[tri_index * 3 + 1]];
        const vertex_t v3 = g_vertices[g_vtx_indices[tri_index * 3 + 2]];

        const float w = 1.0 - sh_inter.u - sh_inter.v;
        const vec2 sh_uvs =
            vec2(v1.t[0], v1.t[1]) * w + vec2(v2.t[0], v2.t[1]) * sh_inter.u + vec2(v3.t[0], v3.t[1]) * sh_inter.v;

        // reuse traversal stack
        g_stack[gl_LocalInvocationIndex][0] = mat_index;
        g_stack[gl_LocalInvocationIndex][1] = floatBitsToUint(1.0);
        int stack_size = 1;

        const vec2 tex_rand = get_scrambled_2d_rand(rand_dim + RAND_DIM_TEX, rand_hash, g_params.iteration - 1);

        vec3 throughput = vec3(0.0);
        while (stack_size-- != 0) {
            material_t mat = g_materials[g_stack[gl_LocalInvocationIndex][2 * stack_size + 0]];
            float weight = uintBitsToFloat(g_stack[gl_LocalInvocationIndex][2 * stack_size + 1]);

            // resolve mix material
            if (mat.type == MixNode) {
                float mix_val = mat.tangent_rotation_or_strength;
                if (mat.textures[BASE_TEXTURE] != 0xffffffff) {
                    mix_val *= SampleBilinear(mat.textures[BASE_TEXTURE], sh_uvs, 0, tex_rand, true /* YCoCg */, true /* SRGB */).r;
                }

                g_stack[gl_LocalInvocationIndex][2 * stack_size + 0] = mat.textures[MIX_MAT1];
                g_stack[gl_LocalInvocationIndex][2 * stack_size + 1] = floatBitsToUint(weight * (1.0 - mix_val));
                ++stack_size;
                g_stack[gl_LocalInvocationIndex][2 * stack_size + 0] = mat.textures[MIX_MAT2];
                g_stack[gl_LocalInvocationIndex][2 * stack_size + 1] = floatBitsToUint(weight * mix_val);
                ++stack_size;
            } else if (mat.type == TransparentNode) {
                throughput += weight * vec3(mat.base_color[0], mat.base_color[1], mat.base_color[2]);
            }
        }

        rc *= throughput;
        if (lum(rc) < FLT_EPS) {
            break;
        }

        const float t = sh_inter.t + HIT_BIAS;
        ro += rd * t;
        dist -= t;

        ++depth;
        rand_dim += RAND_DIM_BOUNCE_COUNT;
    }

    return rc;
#else
    while (dist > HIT_BIAS) {
        rayQueryEXT rq;
        rayQueryInitializeEXT(rq,                       // rayQuery
                              g_tlas,                   // topLevel
                              0,                        // rayFlags
                              RAY_TYPE_SHADOW_BIT,      // cullMask
                              ro,                       // origin
                              0.0,                      // tMin
                              rd,                       // direction
                              dist                      // tMax
                              );
        while (rayQueryProceedEXT(rq)) {
            // NOTE: All instances are opaque, no need to confirm intersection
        }

        if (rayQueryGetIntersectionTypeEXT(rq, true) != gl_RayQueryCommittedIntersectionNoneEXT) {
            // perform alpha test
            const int primitive_offset = rayQueryGetIntersectionInstanceCustomIndexEXT(rq, true);
            const int obj_index = rayQueryGetIntersectionInstanceIdEXT(rq, true);
            const int tri_index = primitive_offset + rayQueryGetIntersectionPrimitiveIndexEXT(rq, true);

            const uint front_mi = (g_tri_materials[tri_index] & 0xffff);
            const uint back_mi = (g_tri_materials[tri_index] >> 16u) & 0xffff;

            const bool is_backfacing = !rayQueryGetIntersectionFrontFaceEXT(rq, true);
            bool solid_hit = !is_backfacing && (front_mi & MATERIAL_SOLID_BIT) != 0;
            if (is_backfacing) {
                solid_hit = solid_hit || (back_mi & MATERIAL_SOLID_BIT) != 0;
            }
            if (solid_hit || depth > g_params.max_transp_depth) {
                return vec3(0.0);
            }

            const uint mat_index = (is_backfacing ? back_mi : front_mi) & MATERIAL_INDEX_BITS;

            const vertex_t v1 = g_vertices[g_vtx_indices[tri_index * 3 + 0]];
            const vertex_t v2 = g_vertices[g_vtx_indices[tri_index * 3 + 1]];
            const vertex_t v3 = g_vertices[g_vtx_indices[tri_index * 3 + 2]];

            const vec2 uv = rayQueryGetIntersectionBarycentricsEXT(rq, true);
            const float w = 1.0 - uv.x - uv.y;
            const vec2 sh_uvs = vec2(v1.t[0], v1.t[1]) * w + vec2(v2.t[0], v2.t[1]) * uv.x + vec2(v3.t[0], v3.t[1]) * uv.y;

            // reuse traversal stack
            g_stack[gl_LocalInvocationIndex][0] = mat_index;
            g_stack[gl_LocalInvocationIndex][1] = floatBitsToUint(1.0);
            int stack_size = 1;

            const vec2 tex_rand = get_scrambled_2d_rand(rand_dim + RAND_DIM_TEX, rand_hash, g_params.iteration - 1);

            vec3 throughput = vec3(0.0);
            while (stack_size-- != 0) {
                material_t mat = g_materials[g_stack[gl_LocalInvocationIndex][2 * stack_size + 0]];
                float weight = uintBitsToFloat(g_stack[gl_LocalInvocationIndex][2 * stack_size + 1]);

                // resolve mix material
                if (mat.type == MixNode) {
                    float mix_val = mat.tangent_rotation_or_strength;
                    if (mat.textures[BASE_TEXTURE] != 0xffffffff) {
                        mix_val *= SampleBilinear(mat.textures[BASE_TEXTURE], sh_uvs, 0, tex_rand, true /* YCoCg */, true /* SRGB */).r;
                    }

                    g_stack[gl_LocalInvocationIndex][2 * stack_size + 0] = mat.textures[MIX_MAT1];
                    g_stack[gl_LocalInvocationIndex][2 * stack_size + 1] = floatBitsToUint(weight * (1.0 - mix_val));
                    ++stack_size;
                    g_stack[gl_LocalInvocationIndex][2 * stack_size + 0] = mat.textures[MIX_MAT2];
                    g_stack[gl_LocalInvocationIndex][2 * stack_size + 1] = floatBitsToUint(weight * mix_val);
                    ++stack_size;
                } else if (mat.type == TransparentNode) {
                    throughput += weight * vec3(mat.base_color[0], mat.base_color[1], mat.base_color[2]);
                }
            }

            rc *= throughput;
            if (lum(rc) < FLT_EPS) {
                break;
            }
        }

        const float t = rayQueryGetIntersectionTEXT(rq, true) + HIT_BIAS;
        ro += rd * t;
        dist -= t;

        ++depth;
        rand_dim += RAND_DIM_BOUNCE_COUNT;
    }

    return rc;
#endif
}

float IntersectAreaLightsShadow(shadow_ray_t r) {
    vec3 ro = vec3(r.o[0], r.o[1], r.o[2]);
    vec3 rd = vec3(r.d[0], r.d[1], r.d[2]);

    float rdist = abs(r.dist);

    vec3 inv_d = safe_invert(rd);
    vec3 neg_inv_do = -inv_d * ro;

    uint stack_size = 0;
    g_stack[gl_LocalInvocationIndex][stack_size++] = g_params.lights_node_index;

    const uint LightTypesMask = (1u << LIGHT_TYPE_RECT) | (1u << LIGHT_TYPE_DISK);

    while (stack_size != 0) {
        uint cur = g_stack[gl_LocalInvocationIndex][--stack_size];

        if ((cur & LEAF_NODE_BIT) == 0) {
            const light_cwbvh_node_t n = g_light_cwnodes[cur];

            // Unpack bounds
            const float ext[3] = {(n.bbox_max[0] - n.bbox_min[0]) / 255.0,
                                  (n.bbox_max[1] - n.bbox_min[1]) / 255.0,
                                  (n.bbox_max[2] - n.bbox_min[2]) / 255.0};
            float bbox_min[3][8], bbox_max[3][8];
            for (int i = 0; i < 8; ++i) {
                bbox_min[0][i] = bbox_min[1][i] = bbox_min[2][i] = -MAX_DIST;
                bbox_max[0][i] = bbox_max[1][i] = bbox_max[2][i] = MAX_DIST;
                if (((n.ch_bbox_min[0][i / 4] >> (8u * (i % 4u))) & 0xffu) != 0xff ||
                    ((n.ch_bbox_max[0][i / 4] >> (8u * (i % 4u))) & 0xffu) != 0) {
                    bbox_min[0][i] = n.bbox_min[0] + float((n.ch_bbox_min[0][i / 4] >> (8u * (i % 4u))) & 0xffu) * ext[0];
                    bbox_min[1][i] = n.bbox_min[1] + float((n.ch_bbox_min[0][i / 4] >> (8u * (i % 4u))) & 0xffu) * ext[1];
                    bbox_min[2][i] = n.bbox_min[2] + float((n.ch_bbox_min[0][i / 4] >> (8u * (i % 4u))) & 0xffu) * ext[2];

                    bbox_max[0][i] = n.bbox_min[0] + float((n.ch_bbox_max[0][i / 4] >> (8u * (i % 4u))) & 0xffu) * ext[0];
                    bbox_max[1][i] = n.bbox_min[1] + float((n.ch_bbox_max[0][i / 4] >> (8u * (i % 4u))) & 0xffu) * ext[1];
                    bbox_max[2][i] = n.bbox_min[2] + float((n.ch_bbox_max[0][i / 4] >> (8u * (i % 4u))) & 0xffu) * ext[2];
                }
            }

            // TODO: loop in morton order based on ray direction
            for (int j = 0; j < 8; ++j) {
                if ((n.ch_bitmask[j / 4] & (LightTypesMask << (8u * (j % 4u)))) != 0 &&
                    bbox_test(inv_d, neg_inv_do, rdist,
                              vec3(bbox_min[0][j], bbox_min[1][j], bbox_min[2][j]),
                              vec3(bbox_max[0][j], bbox_max[1][j], bbox_max[2][j]))) {
                    g_stack[gl_LocalInvocationIndex][stack_size++] = n.child[j];
                }
            }
        } else {
            const int light_index = int(cur & PRIM_INDEX_BITS);
            light_t l = g_lights[light_index];
            [[dont_flatten]] if ((LIGHT_RAY_VISIBILITY(l) & RAY_TYPE_SHADOW_BIT) == 0) {
                // Skip non-blocking light
                continue;
            }
            [[dont_flatten]] if (LIGHT_SKY_PORTAL(l) && r.dist >= 0.0) {
                continue;
            }

            const uint light_type = LIGHT_TYPE(l);
            if (light_type == LIGHT_TYPE_RECT) {
                vec3 light_pos = l.RECT_POS;
                vec3 light_u = l.RECT_U;
                vec3 light_v = l.RECT_V;

                vec3 light_forward = normalize(cross(light_u, light_v));

                float plane_dist = dot(light_forward, light_pos);
                float cos_theta = dot(rd, light_forward);
                float t = (plane_dist - dot(light_forward, ro)) / cos_theta;

                if (cos_theta < 0.0 && t > HIT_EPS && t < rdist) {
                    light_u /= dot(light_u, light_u);
                    light_v /= dot(light_v, light_v);

                    vec3 p = ro + rd * t;
                    vec3 vi = p - light_pos;
                    float a1 = dot(light_u, vi);
                    if (a1 >= -0.5 && a1 <= 0.5) {
                        float a2 = dot(light_v, vi);
                        if (a2 >= -0.5 && a2 <= 0.5) {
                            return 0.0;
                        }
                    }
                }
            } else if (light_type == LIGHT_TYPE_DISK) {
                vec3 light_pos = l.DISK_POS;
                vec3 light_u = l.DISK_U;
                vec3 light_v = l.DISK_V;

                vec3 light_forward = normalize(cross(light_u, light_v));

                float plane_dist = dot(light_forward, light_pos);
                float cos_theta = dot(rd, light_forward);
                float t = (plane_dist - dot(light_forward, ro)) / cos_theta;

                if (cos_theta < 0.0 && t > HIT_EPS && t < rdist) {
                    light_u /= dot(light_u, light_u);
                    light_v /= dot(light_v, light_v);

                    vec3 p = ro + rd * t;
                    vec3 vi = p - light_pos;
                    float a1 = dot(light_u, vi);
                    float a2 = dot(light_v, vi);

                    if (sqrt(a1 * a1 + a2 * a2) <= 0.5) {
                        return 0.0;
                    }
                }
            }
        }
    }

    return 1.0;
}

void main() {
    const int index = int(gl_WorkGroupID.x * 64 + gl_LocalInvocationIndex);
    if (index >= g_counters[3]) {
        return;
    }

    shadow_ray_t sh_ray = g_sh_rays[index];

    vec3 rc = IntersectSceneShadow(sh_ray);
    if (g_params.blocker_lights_count != 0) {
        rc *= IntersectAreaLightsShadow(sh_ray);
    }
    const float sum = rc.r + rc.g + rc.b;
    if (sum > g_params.clamp_val) {
        rc *= (g_params.clamp_val / sum);
    }
    if (lum(rc) > 0.0) {
        const int x = int((sh_ray.xy >> 16) & 0xffff);
        const int y = int(sh_ray.xy & 0xffff);

        vec4 col = imageLoad(g_inout_img, ivec2(x, y));
        col.xyz += rc;
        imageStore(g_inout_img, ivec2(x, y), col);
    }
}