#version 430 core
#if defined(HWRT)
#extension GL_EXT_ray_query : require
#endif
#extension GL_EXT_control_flow_attributes : enable
#if !defined(VULKAN)
#extension GL_ARB_bindless_texture : enable
#endif
#if !defined(NO_SUBGROUP)
#extension GL_KHR_shader_subgroup_vote : require
#define SWRT_SUBGROUP 1
#endif

#include "rt_common.glsl"
#include "swrt_common.glsl"
#include "texturing_common.glsl"
#include "principled_common.glsl"
#include "pmj_common.glsl"
#include "ssr_common.glsl"
#include "light_bvh_common.glsl"
#include "sample_lights_interface.h"

#pragma multi_compile _ HWRT
#pragma multi_compile _ NO_SUBGROUP

const int TRANSPARENCY_LIMIT = 4;

LAYOUT_PARAMS uniform UniformParams {
    Params g_params;
};

layout (binding = BIND_UB_SHARED_DATA_BUF, std140) uniform SharedDataBlock {
    shared_data_t g_shrd_data;
};

layout(binding = ALBEDO_TEX_SLOT) uniform sampler2D g_albedo_tex;
layout(binding = DEPTH_TEX_SLOT) uniform sampler2D g_depth_tex;
layout(binding = NORM_TEX_SLOT) uniform usampler2D g_normal_tex;
layout(binding = SPEC_TEX_SLOT) uniform usampler2D g_specular_tex;

layout(binding = RANDOM_SEQ_BUF_SLOT) uniform usamplerBuffer g_random_seq;
layout(binding = LIGHTS_BUF_SLOT) uniform usamplerBuffer g_lights_buf;
layout(binding = LIGHT_NODES_BUF_SLOT) uniform samplerBuffer g_light_nodes_buf;

layout(std430, binding = GEO_DATA_BUF_SLOT) readonly buffer GeometryData {
    rt_geo_instance_t g_geometries[];
};

layout(std430, binding = MATERIAL_BUF_SLOT) readonly buffer Materials {
    material_data_t g_materials[];
};

layout(binding = VTX_BUF1_SLOT) uniform samplerBuffer g_vtx_data0;
layout(binding = NDX_BUF_SLOT) uniform usamplerBuffer g_vtx_indices;

#if defined(HWRT)
    layout(binding = TLAS_SLOT) uniform accelerationStructureEXT g_tlas;
#else
    layout(binding = BLAS_BUF_SLOT) uniform samplerBuffer g_blas_nodes;
    layout(binding = TLAS_BUF_SLOT) uniform samplerBuffer g_tlas_nodes;

    layout(binding = PRIM_NDX_BUF_SLOT) uniform usamplerBuffer g_prim_indices;
    layout(binding = MESH_INSTANCES_BUF_SLOT) uniform samplerBuffer g_mesh_instances;
#endif

layout(binding = OUT_DIFFUSE_IMG_SLOT, rgba16f) uniform restrict image2D g_out_diffuse_img;
layout(binding = OUT_SPECULAR_IMG_SLOT, rgba16f) uniform restrict image2D g_out_specular_img;

layout (local_size_x = GRP_SIZE_X, local_size_y = GRP_SIZE_Y, local_size_z = 1) in;

void main() {
    if (gl_GlobalInvocationID.x >= g_params.img_size.x || gl_GlobalInvocationID.y >= g_params.img_size.y) {
        return;
    }

    const ivec2 icoord = ivec2(gl_GlobalInvocationID.xy);
    const vec2 norm_uvs = (vec2(icoord) + 0.5) * g_shrd_data.ren_res.zw;

    const float depth = texelFetch(g_depth_tex, icoord, 0).x;
    if (depth == 0.0) {
        imageStore(g_out_specular_img, icoord, vec4(0.0, 0.0, 0.0, -1.0));
        return;
    }
    const float lin_depth = LinearizeDepth(depth, g_shrd_data.clip_info);

    const vec4 pos_cs = vec4(2.0 * norm_uvs - 1.0, depth, 1.0);
    const vec3 pos_vs = TransformFromClipSpace(g_shrd_data.view_from_clip, pos_cs);
    const vec3 pos_ws = TransformFromClipSpace(g_shrd_data.world_from_clip, pos_cs);

    const vec3 base_color = texelFetch(g_albedo_tex, icoord, 0).xyz;
    const vec4 normal = UnpackNormalAndRoughness(texelFetch(g_normal_tex, icoord, 0).x);
    const uint packed_mat_params = texelFetch(g_specular_tex, icoord, 0).x;

    //

    const vec3 P = pos_ws.xyz;
    const vec3 I = normalize(g_shrd_data.cam_pos_and_exp.xyz - P);
    const vec3 N = normal.xyz;
    const float N_dot_V = saturate(dot(N, I));

    vec3 tint_color = vec3(0.0);

    const float base_color_lum = lum(base_color);
    if (base_color_lum > 0.0) {
        tint_color = base_color / base_color_lum;
    }

    vec4 mat_params0, mat_params1;
    UnpackMaterialParams(packed_mat_params, mat_params0, mat_params1);

    const float roughness = normal.w;
    const float sheen = mat_params0.x;
    const float sheen_tint = mat_params0.y;
    const float specular = mat_params0.z;
    const float specular_tint = mat_params0.w;
    const float metallic = mat_params1.x;
    const float transmission = mat_params1.y;
    const float clearcoat = mat_params1.z;
    const float clearcoat_roughness = mat_params1.w;

    vec3 spec_tmp_col = mix(vec3(1.0), tint_color, specular_tint);
    spec_tmp_col = mix(specular * 0.08 * spec_tmp_col, base_color, metallic);

    const float spec_ior = (2.0 / (1.0 - sqrt(0.08 * specular))) - 1.0;
    const float spec_F0 = fresnel_dielectric_cos(1.0, spec_ior);

    // Approximation of FH (using shading normal)
    const float FN = (fresnel_dielectric_cos(dot(I, N), spec_ior) - spec_F0) / (1.0 - spec_F0);

    const vec3 approx_spec_col = mix(spec_tmp_col, vec3(1.0), FN * (1.0 - roughness));
    const float spec_color_lum = lum(approx_spec_col);

    lobe_masks_t lobe_masks = get_lobe_masks(mix(base_color_lum, 1.0, sheen), spec_color_lum, specular, metallic, transmission, clearcoat);

    const vec3 sheen_color = sheen * mix(vec3(1.0), tint_color, sheen_tint);

    const float clearcoat_ior = (2.0 / (1.0 - sqrt(0.08 * clearcoat))) - 1.0;
    const float clearcoat_F0 = fresnel_dielectric_cos(1.0, clearcoat_ior);
    const float clearcoat_roughness2 = clearcoat_roughness * clearcoat_roughness;

    // Approximation of FH (using shading normal)
    const float clearcoat_FN = (fresnel_dielectric_cos(dot(I, N), clearcoat_ior) - clearcoat_F0) / (1.0 - clearcoat_F0);
    const vec3 approx_clearcoat_col = vec3(mix(/*clearcoat * 0.08*/ 0.04, 1.0, clearcoat_FN));

    //

    const uint px_hash = hash((gl_GlobalInvocationID.x << 16) | gl_GlobalInvocationID.y);
    const float light_pick_rand = get_scrambled_2d_rand(g_random_seq, RAND_DIM_LIGHT_PICK, px_hash, int(g_params.frame_index)).x;

    float pdf_factor;
    const int li = PickLightSource(P, g_light_nodes_buf, g_params.lights_count, light_pick_rand, pdf_factor);
    if (li == -1) {
        imageStore(g_out_specular_img, icoord, vec4(0.0));
        return;
    }

    //

    _light_item_t litem = FetchLightItem(g_lights_buf, li);
    const bool is_diffuse = (floatBitsToUint(litem.col_and_type.w) & LIGHT_DIFFUSE_BIT) != 0;
    const bool is_specular = (floatBitsToUint(litem.col_and_type.w) & LIGHT_SPECULAR_BIT) != 0;
    const bool is_doublesided = (floatBitsToUint(litem.col_and_type.w) & LIGHT_DOUBLESIDED_BIT) != 0;

    [[flatten]] if (!is_diffuse) lobe_masks.bits &= ~LOBE_DIFFUSE_BIT;
    [[flatten]] if (!is_specular) lobe_masks.bits &= ~(LOBE_SPECULAR_BIT | LOBE_CLEARCOAT_BIT);

    const vec3 p1 = litem.pos_and_radius.xyz,
               p2 = litem.u_and_reg.xyz,
               p3 = litem.v_and_blend.xyz;
    const vec2 uv1 = vec2(litem.pos_and_radius.w, litem.dir_and_spot.x),
               uv2 = litem.dir_and_spot.yz,
               uv3 = vec2(litem.dir_and_spot.w, litem.v_and_blend.w);
    const vec3 e1 = p2 - p1, e2 = p3 - p1;
    float light_fwd_len;
    vec3 light_forward = normalize_len(cross(e1, e2), light_fwd_len);

    // Simple area sampling
    const vec2 rand_light_uv = get_scrambled_2d_rand(g_random_seq, RAND_DIM_LIGHT_UV, px_hash, int(g_params.frame_index));
    const float r1 = sqrt(rand_light_uv.x), r2 = rand_light_uv.y;
    const vec2 luv = uv1 * (1.0 - r1) + r1 * (uv2 * (1.0 - r2) + uv3 * r2);
    const vec3 lp = p1 * (1.0 - r1) + r1 * (p2 * (1.0 - r2) + p3 * r2);

#if defined(BINDLESS_TEXTURES)
    litem.col_and_type.xyz *= SRGBToLinear(YCoCg_to_RGB(textureLod(SAMPLER2D(GET_HANDLE(floatBitsToInt(litem.u_and_reg.w))), luv, 0.0)));
#endif

    float ls_dist;
    const vec3 L = normalize_len(lp - P, ls_dist);

    float cos_theta = -dot(L, light_forward);
    if (is_doublesided) {
        cos_theta = abs(cos_theta);
    }
    const float ls_pdf = pdf_factor * (ls_dist * ls_dist) / (0.5 * light_fwd_len * cos_theta);

    if (hsum(litem.col_and_type.xyz) * g_shrd_data.cam_pos_and_exp.w / ls_pdf < 3.0 * GI_LIGHT_CUTOFF) {
        imageStore(g_out_specular_img, icoord, vec4(0.0));
        return;
    }

    //

    vec3 ro = pos_ws + (NormalBiasConstant + abs(pos_ws) * NormalBiasPosAddition + (-pos_vs.z) * NormalBiasViewAddition) * N;
    vec3 inv_d = safe_invert(L);

    float visibility = 1.0;
#ifdef HWRT
    rayQueryEXT rq;
    rayQueryInitializeEXT(rq,                       // rayQuery
                          g_tlas,                   // topLevel
                          0,                        // rayFlags
                          (1u << RAY_TYPE_SHADOW),  // cullMask
                          ro,                       // origin
                          0.0,                      // tMin
                          L,                        // direction
                          ls_dist - 0.001           // tMax
                          );

    int transp_depth = 0;
    while(rayQueryProceedEXT(rq)) {
        if (rayQueryGetIntersectionTypeEXT(rq, false) == gl_RayQueryCandidateIntersectionTriangleEXT) {
            if (transp_depth++ < TRANSPARENCY_LIMIT) {
                // perform alpha test
                const int custom_index = rayQueryGetIntersectionInstanceCustomIndexEXT(rq, false);
                const int geo_index = rayQueryGetIntersectionGeometryIndexEXT(rq, false);
                const int tri_index = rayQueryGetIntersectionPrimitiveIndexEXT(rq, false);
                const vec2 bary_coord = rayQueryGetIntersectionBarycentricsEXT(rq, false);
                const bool backfacing = !rayQueryGetIntersectionFrontFaceEXT(rq, false);

                const rt_geo_instance_t geo = g_geometries[custom_index + geo_index];
                const uint mat_index = backfacing ? (geo.material_index >> 16) : (geo.material_index & 0xffff);
                const material_data_t mat = g_materials[mat_index & MATERIAL_INDEX_BITS];

                const uint i0 = texelFetch(g_vtx_indices, int(geo.indices_start + 3 * tri_index + 0)).x;
                const uint i1 = texelFetch(g_vtx_indices, int(geo.indices_start + 3 * tri_index + 1)).x;
                const uint i2 = texelFetch(g_vtx_indices, int(geo.indices_start + 3 * tri_index + 2)).x;

                const vec4 p0 = texelFetch(g_vtx_data0, int(geo.vertices_start + i0));
                const vec4 p1 = texelFetch(g_vtx_data0, int(geo.vertices_start + i1));
                const vec4 p2 = texelFetch(g_vtx_data0, int(geo.vertices_start + i2));

                const vec2 uv0 = unpackHalf2x16(floatBitsToUint(p0.w));
                const vec2 uv1 = unpackHalf2x16(floatBitsToUint(p1.w));
                const vec2 uv2 = unpackHalf2x16(floatBitsToUint(p2.w));

                const vec2 uv = uv0 * (1.0 - bary_coord.x - bary_coord.y) + uv1 * bary_coord.x + uv2 * bary_coord.y;
                const float alpha = textureLod(SAMPLER2D(mat.texture_indices[MAT_TEX_ALPHA]), uv, 0.0).x;
                if (alpha < 0.5) {
                    continue;
                }
            }
            rayQueryConfirmIntersectionEXT(rq);
        }
    }

    if (rayQueryGetIntersectionTypeEXT(rq, true) != gl_RayQueryCommittedIntersectionNoneEXT ||
        transp_depth >= TRANSPARENCY_LIMIT) {
        visibility = 0.0;
    }
#else // HWRT
    hit_data_t inter;
    inter.mask = 0;
    inter.obj_index = inter.prim_index = 0;
    inter.geo_index_count = 0;
    inter.t = ls_dist - 0.001;
    inter.u = inter.v = 0.0;

    float inter_t = inter.t;
    int transp_depth = 0;
    while (true) {
        Traverse_TLAS_WithStack(g_tlas_nodes, g_blas_nodes, g_mesh_instances, g_vtx_data0, g_vtx_indices, g_prim_indices,
                                ro, L, inv_d, (1u << RAY_TYPE_SHADOW), 0 /* root_node */, inter);
        if (inter.mask != 0) {
            if (transp_depth++ < TRANSPARENCY_LIMIT) {
                // perform alpha test
                const bool backfacing = (inter.prim_index < 0);
                const int tri_index = backfacing ? -inter.prim_index - 1 : inter.prim_index;

                int i = int(inter.geo_index_count & 0x00ffffffu);
                for (; i < int(inter.geo_index_count & 0x00ffffffu) + int((inter.geo_index_count >> 24) & 0xffu); ++i) {
                    const int tri_start = int(g_geometries[i].indices_start) / 3;
                    if (tri_start > tri_index) {
                        break;
                    }
                }

                const int geo_index = i - 1;

                const rt_geo_instance_t geo = g_geometries[geo_index];
                const uint mat_index = backfacing ? (geo.material_index >> 16) : (geo.material_index & 0xffff);
                if ((mat_index & MATERIAL_SOLID_BIT) != 0) {
                    break;
                }
                const material_data_t mat = g_materials[mat_index & MATERIAL_INDEX_BITS];

                const uint i0 = texelFetch(g_vtx_indices, 3 * tri_index + 0).x;
                const uint i1 = texelFetch(g_vtx_indices, 3 * tri_index + 1).x;
                const uint i2 = texelFetch(g_vtx_indices, 3 * tri_index + 2).x;

                const vec4 p0 = texelFetch(g_vtx_data0, int(geo.vertices_start + i0));
                const vec4 p1 = texelFetch(g_vtx_data0, int(geo.vertices_start + i1));
                const vec4 p2 = texelFetch(g_vtx_data0, int(geo.vertices_start + i2));

                const vec2 uv0 = unpackHalf2x16(floatBitsToUint(p0.w));
                const vec2 uv1 = unpackHalf2x16(floatBitsToUint(p1.w));
                const vec2 uv2 = unpackHalf2x16(floatBitsToUint(p2.w));

                const vec2 uv = uv0 * (1.0 - inter.u - inter.v) + uv1 * inter.u + uv2 * inter.v;
    #if defined(BINDLESS_TEXTURES)
                const float alpha = textureLod(SAMPLER2D(GET_HANDLE(mat.texture_indices[MAT_TEX_ALPHA])), uv, 0.0).x;
                if (alpha < 0.5) {
                    ro += (inter.t + 0.0005) * L;
                    inter.mask = 0;
                    inter_t -= inter.t + 0.0005;
                    inter.t = inter_t;
                    continue;
                }
    #endif
            }
        }
        break;
    }

    if (inter.mask != 0 || transp_depth >= TRANSPARENCY_LIMIT) {
        visibility = 0.0;
    }
#endif // HWRT

    //

    vec3 diffuse_col = vec3(0.0);
    vec4 specular_col = vec4(0.0, 0.0, 0.0, -1.0);

    if ((lobe_masks.bits & LOBE_DIFFUSE_BIT) != 0 && ls_pdf > 1e-7) {
        diffuse_col += litem.col_and_type.xyz * saturate(dot(N, L)) / (ls_pdf * M_PI);
        diffuse_col *= (1.0 - metallic) * (1.0 - transmission);

        const float bsdf_pdf = saturate(dot(N, L)) / M_PI;
        const float mis_weight = power_heuristic(ls_pdf, bsdf_pdf);
        diffuse_col *= mis_weight;
        //diffuse_col = limit_intensity(diffuse_col, 10.0);
    }

    if ((lobe_masks.bits & LOBE_SPECULAR_BIT) != 0 && roughness * roughness > 1e-7 && ls_pdf > 1e-7) {
        const mat3 tbn_transform = CreateTBN(N);
        const vec3 view_dir_ts = tbn_transform * I;
        const vec3 sampled_normal_ts = tbn_transform * normalize(L + I);
        const vec3 reflected_dir_ts = tbn_transform * L;

        const float D = D_GGX(sampled_normal_ts, vec2(roughness * roughness));
        const float bsdf_pdf = GGX_VNDF_Reflection_Bounded_PDF(D, view_dir_ts, vec2(roughness * roughness));
        const float mis_weight = power_heuristic(ls_pdf, bsdf_pdf);

        const float denom = 4.0 * abs(view_dir_ts[2] * reflected_dir_ts[2]);
        specular_col.xyz = litem.col_and_type.xyz * D * mis_weight * saturate(reflected_dir_ts[2]) / (/*denom */ ls_pdf);
        specular_col.w = dot(specular_col.xyz, specular_col.xyz) > 1e-7 ? ls_dist : -1.0;
    }

    const vec4 prev_diff_val = imageLoad(g_out_diffuse_img, icoord);
    imageStore(g_out_diffuse_img, icoord, vec4(prev_diff_val.xyz + compress_hdr(visibility * diffuse_col, g_shrd_data.cam_pos_and_exp.w), prev_diff_val.w));
    imageStore(g_out_specular_img, icoord, vec4(compress_hdr(visibility * specular_col.xyz, g_shrd_data.cam_pos_and_exp.w), specular_col.w));
}
