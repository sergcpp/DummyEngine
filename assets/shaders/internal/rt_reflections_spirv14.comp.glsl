#version 460
#extension GL_EXT_ray_query : require

#if defined(GL_ES) || defined(VULKAN)
    precision highp int;
    precision highp float;
#endif

#include "_rt_common.glsl"
#include "_texturing.glsl"
#include "ssr_common.glsl"
#include "rt_reflections_interface.glsl"

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

layout(binding = DEPTH_TEX_SLOT) uniform sampler2D g_depth_tex;
layout(binding = NORM_TEX_SLOT) uniform sampler2D g_norm_tex;
layout(binding = ENV_TEX_SLOT) uniform samplerCube g_env_tex;

layout(binding = TLAS_SLOT) uniform accelerationStructureEXT g_tlas;

layout(std430, binding = GEO_DATA_BUF_SLOT) readonly buffer GeometryData {
    RTGeoInstance g_geometries[];
};

layout(std430, binding = MATERIAL_BUF_SLOT) readonly buffer Materials {
    MaterialData g_materials[];
};

layout(std430, binding = VTX_BUF1_SLOT) readonly buffer VtxData0 {
    uvec4 g_vtx_data0[];
};

layout(std430, binding = VTX_BUF2_SLOT) readonly buffer VtxData1 {
    uvec4 g_vtx_data1[];
};

layout(std430, binding = NDX_BUF_SLOT) readonly buffer NdxData {
    uint g_indices[];
};

layout(binding = LMAP_TEX_SLOTS) uniform sampler2D g_lm_textures[5];

layout(std430, binding = RAY_COUNTER_SLOT) readonly buffer RayCounter {
    uint g_ray_counter[];
};

layout(std430, binding = RAY_LIST_SLOT) readonly buffer RayList {
    uint g_ray_list[];
};

layout(binding = NOISE_TEX_SLOT) uniform lowp sampler2D g_noise_tex;

layout(binding = OUT_REFL_IMG_SLOT, r11f_g11f_b10f) uniform writeonly restrict image2D g_out_color_img;
layout(binding = OUT_RAYLEN_IMG_SLOT, r16f) uniform writeonly restrict image2D g_out_raylen_img;

//layout(location = 0) rayPayloadEXT RayPayload g_pld;

vec3 SampleReflectionVector(vec3 view_direction, vec3 normal, float roughness, ivec2 dispatch_thread_id) {
    mat3 tbn_transform = CreateTBN(normal);
    vec3 view_direction_tbn = tbn_transform * (-view_direction);

    vec2 u = texelFetch(g_noise_tex, ivec2(dispatch_thread_id) % 128, 0).rg;

    vec3 sampled_normal_tbn = Sample_GGX_VNDF_Hemisphere(view_direction_tbn, roughness, u.x, u.y);
#ifdef PERFECT_REFLECTIONS
    sampled_normal_tbn = vec3(0.0, 0.0, 1.0); // Overwrite normal sample to produce perfect reflection.
#endif

    vec3 reflected_direction_tbn = reflect(-view_direction_tbn, sampled_normal_tbn);

    // Transform reflected_direction back to the initial space.
    mat3 inv_tbn_transform = transpose(tbn_transform);
    return (inv_tbn_transform * reflected_direction_tbn);
}

layout (local_size_x = 64, local_size_y = 1, local_size_z = 1) in;

void main() {
    uint ray_index = gl_WorkGroupID.x * 64 + gl_LocalInvocationIndex;
    if (ray_index >= g_ray_counter[5]) return;
    uint packed_coords = g_ray_list[ray_index];

    uvec2 ray_coords;
    bool copy_horizontal, copy_vertical, copy_diagonal;
    UnpackRayCoords(packed_coords, ray_coords, copy_horizontal, copy_vertical, copy_diagonal);

    ivec2 icoord = ivec2(ray_coords);
    float depth = texelFetch(g_depth_tex, icoord, 0).r;
    vec4 normal_roughness = UnpackNormalAndRoughness(texelFetch(g_norm_tex, icoord, 0));
    vec3 normal_ws = normal_roughness.xyz;
    vec3 normal_vs = normalize((g_shrd_data.view_matrix * vec4(normal_ws, 0.0)).xyz);

    float roughness = normal_roughness.w;

    const vec2 px_center = vec2(icoord) + vec2(0.5);
    const vec2 in_uv = px_center / vec2(g_params.img_size);

#if defined(VULKAN)
    vec4 ray_origin_cs = vec4(2.0 * in_uv - 1.0, depth, 1.0);
    ray_origin_cs.y = -ray_origin_cs.y;
#else // VULKAN
    vec4 ray_origin_cs = vec4(2.0 * vec3(in_uv, depth) - 1.0, 1.0);
#endif // VULKAN

    vec4 ray_origin_vs = g_shrd_data.inv_proj_matrix * ray_origin_cs;
    ray_origin_vs /= ray_origin_vs.w;

    vec3 view_ray_vs = normalize(ray_origin_vs.xyz);
    vec3 refl_ray_vs = SampleReflectionVector(view_ray_vs, normal_vs, roughness, icoord);
    vec3 refl_ray_ws = (g_shrd_data.inv_view_matrix * vec4(refl_ray_vs.xyz, 0.0)).xyz;

    vec4 ray_origin_ws = g_shrd_data.inv_view_matrix * ray_origin_vs;
    ray_origin_ws /= ray_origin_ws.w;

    vec3 col;

    const uint ray_flags = gl_RayFlagsCullBackFacingTrianglesEXT;
    const float t_min = 0.001;
    const float t_max = 1000.0;

    float _cone_width = g_params.pixel_spread_angle * (-ray_origin_vs.z);

    rayQueryEXT rq;
    rayQueryInitializeEXT(rq,               // rayQuery
                          g_tlas,           // topLevel
                          ray_flags,        // rayFlags
                          0xff,             // cullMask
                          ray_origin_ws.xyz,// origin
                          t_min,            // tMin
                          refl_ray_ws.xyz,  // direction
                          t_max             // tMax
                          );
    while(rayQueryProceedEXT(rq)) {
        if (rayQueryGetIntersectionTypeEXT(rq, false) == gl_RayQueryCandidateIntersectionTriangleEXT) {
            // perform alpha test
            int custom_index = rayQueryGetIntersectionInstanceCustomIndexEXT(rq, false);
            int geo_index = rayQueryGetIntersectionGeometryIndexEXT(rq, false);
            int prim_id = rayQueryGetIntersectionPrimitiveIndexEXT(rq, false);
            vec2 bary_coord = rayQueryGetIntersectionBarycentricsEXT(rq, false);

            RTGeoInstance geo = g_geometries[custom_index + geo_index];
            MaterialData mat = g_materials[geo.material_index];

            uint i0 = g_indices[geo.indices_start + 3 * prim_id + 0];
            uint i1 = g_indices[geo.indices_start + 3 * prim_id + 1];
            uint i2 = g_indices[geo.indices_start + 3 * prim_id + 2];

            vec2 uv0 = unpackHalf2x16(g_vtx_data0[geo.vertices_start + i0].w);
            vec2 uv1 = unpackHalf2x16(g_vtx_data0[geo.vertices_start + i1].w);
            vec2 uv2 = unpackHalf2x16(g_vtx_data0[geo.vertices_start + i2].w);

            vec2 uv = uv0 * (1.0 - bary_coord.x - bary_coord.y) + uv1 * bary_coord.x + uv2 * bary_coord.y;
            float alpha = textureLod(SAMPLER2D(mat.texture_indices[3]), uv, 0.0).r;
            if (alpha >= 0.5) {
                rayQueryConfirmIntersectionEXT(rq);
            }
        }
    }

    float ray_len = 0.0;

    if (rayQueryGetIntersectionTypeEXT(rq, true) == gl_RayQueryCommittedIntersectionNoneEXT) {
        col = clamp(RGBMDecode(textureLod(g_env_tex, refl_ray_ws.xyz, 0.0)), vec3(0.0), vec3(4.0)); // clamp is temporary workaround
    } else {
        int custom_index = rayQueryGetIntersectionInstanceCustomIndexEXT(rq, true);
        int geo_index = rayQueryGetIntersectionGeometryIndexEXT(rq, true);
        float hit_t = rayQueryGetIntersectionTEXT(rq, true);
        int prim_id = rayQueryGetIntersectionPrimitiveIndexEXT(rq, true);
        vec2 bary_coord = rayQueryGetIntersectionBarycentricsEXT(rq, true);

        RTGeoInstance geo = g_geometries[custom_index + geo_index];
        MaterialData mat = g_materials[geo.material_index];

        uint i0 = g_indices[geo.indices_start + 3 * prim_id + 0];
        uint i1 = g_indices[geo.indices_start + 3 * prim_id + 1];
        uint i2 = g_indices[geo.indices_start + 3 * prim_id + 2];

        vec3 p0 = uintBitsToFloat(g_vtx_data0[geo.vertices_start + i0].xyz);
        vec3 p1 = uintBitsToFloat(g_vtx_data0[geo.vertices_start + i1].xyz);
        vec3 p2 = uintBitsToFloat(g_vtx_data0[geo.vertices_start + i2].xyz);

        vec2 uv0 = unpackHalf2x16(g_vtx_data0[geo.vertices_start + i0].w);
        vec2 uv1 = unpackHalf2x16(g_vtx_data0[geo.vertices_start + i1].w);
        vec2 uv2 = unpackHalf2x16(g_vtx_data0[geo.vertices_start + i2].w);

        vec2 uv = uv0 * (1.0 - bary_coord.x - bary_coord.y) + uv1 * bary_coord.x + uv2 * bary_coord.y;

        vec2 tex_res = textureSize(SAMPLER2D(mat.texture_indices[0]), 0).xy;
        float ta = abs((uv1.x - uv0.x) * (uv2.y - uv0.y) - (uv2.x - uv0.x) * (uv1.y - uv0.y));

        vec3 tri_normal = cross(p1 - p0, p2 - p0);
        float pa = length(tri_normal);
        tri_normal /= pa;

        float cone_width = _cone_width + g_params.pixel_spread_angle * hit_t;

        float tex_lod = 0.5 * log2(ta/pa);
        tex_lod += log2(cone_width);
        tex_lod += 0.5 * log2(tex_res.x * tex_res.y);
        tex_lod -= log2(abs(dot(rayQueryGetIntersectionObjectRayDirectionEXT(rq, true), tri_normal)));
        col = SRGBToLinear(YCoCg_to_RGB(textureLod(SAMPLER2D(mat.texture_indices[0]), uv, tex_lod)));

        if ((geo.flags & RTGeoLightmappedBit) != 0u) {
            vec2 lm_uv0 = unpackHalf2x16(g_vtx_data1[geo.vertices_start + i0].w);
            vec2 lm_uv1 = unpackHalf2x16(g_vtx_data1[geo.vertices_start + i1].w);
            vec2 lm_uv2 = unpackHalf2x16(g_vtx_data1[geo.vertices_start + i2].w);

            vec2 lm_uv = lm_uv0 * (1.0 - bary_coord.x - bary_coord.y) + lm_uv1 * bary_coord.x + lm_uv2 * bary_coord.y;
            lm_uv = geo.lmap_transform.xy + geo.lmap_transform.zw * lm_uv;

            vec3 direct_lm = RGBMDecode(textureLod(g_lm_textures[0], lm_uv, 0.0));
            vec3 indirect_lm = 2.0 * RGBMDecode(textureLod(g_lm_textures[1], lm_uv, 0.0));

            col *= (direct_lm + indirect_lm);
        } else {
            vec3 normal0 = vec3(unpackSnorm2x16(g_vtx_data1[geo.vertices_start + i0].x), unpackSnorm2x16(g_vtx_data1[geo.vertices_start + i0].y).x);
            vec3 normal1 = vec3(unpackSnorm2x16(g_vtx_data1[geo.vertices_start + i1].x), unpackSnorm2x16(g_vtx_data1[geo.vertices_start + i1].y).x);
            vec3 normal2 = vec3(unpackSnorm2x16(g_vtx_data1[geo.vertices_start + i2].x), unpackSnorm2x16(g_vtx_data1[geo.vertices_start + i2].y).x);

            vec3 normal = normal0 * (1.0 - bary_coord.x - bary_coord.y) + normal1 * bary_coord.x + normal2 * bary_coord.y;

            col *= EvalSHIrradiance_NonLinear(normal,
                                              g_shrd_data.probes[geo.flags & RTGeoProbeBits].sh_coeffs[0],
                                              g_shrd_data.probes[geo.flags & RTGeoProbeBits].sh_coeffs[1],
                                              g_shrd_data.probes[geo.flags & RTGeoProbeBits].sh_coeffs[2]);
        }

        ray_len = hit_t;
    }

    imageStore(g_out_color_img, icoord, vec4(col, 1.0));
    imageStore(g_out_raylen_img, icoord, vec4(ray_len));

    ivec2 copy_target = icoord ^ 1; // flip last bit to find the mirrored coords along the x and y axis within a quad
    if (copy_horizontal) {
        ivec2 copy_coords = ivec2(copy_target.x, icoord.y);
        imageStore(g_out_color_img, copy_coords, vec4(col, 0.0));
        imageStore(g_out_raylen_img, copy_coords, vec4(ray_len));
    }
    if (copy_vertical) {
        ivec2 copy_coords = ivec2(icoord.x, copy_target.y);
        imageStore(g_out_color_img, copy_coords, vec4(col, 0.0));
        imageStore(g_out_raylen_img, copy_coords, vec4(ray_len));
    }
    if (copy_diagonal) {
        ivec2 copy_coords = copy_target;
        imageStore(g_out_color_img, copy_coords, vec4(col, 0.0));
        imageStore(g_out_raylen_img, copy_coords, vec4(ray_len));
    }
}
