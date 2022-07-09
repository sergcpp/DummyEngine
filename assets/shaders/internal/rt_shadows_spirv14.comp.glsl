#version 460
#extension GL_KHR_shader_subgroup_arithmetic : require
#extension GL_EXT_ray_query : require

#if defined(GL_ES) || defined(VULKAN)
    precision highp int;
    precision highp float;
#endif

#include "_rt_common.glsl"
#include "_texturing.glsl"
#include "rt_shadows_interface.glsl"
#include "rt_shadow_common.glsl.inl"

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

layout(binding = NOISE_TEX_SLOT) uniform sampler2D g_noise_tex;
layout(binding = DEPTH_TEX_SLOT) uniform sampler2D g_depth_tex;
layout(binding = NORM_TEX_SLOT) uniform sampler2D g_norm_tex;

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

layout(std430, binding = TILE_LIST_SLOT) readonly buffer TileList {
    uvec4 g_tile_list[];
};

layout(binding = OUT_SHADOW_IMG_SLOT, r32ui) uniform restrict uimage2D g_out_shadow_img;

vec3 offset_ray(vec3 p, vec3 n) { // TODO: avoid duplication!
    const float Origin = 1.0f / 32.0f;
    const float FloatScale = 1.0f / 65536.0f;
    const float IntScale = 256.0f;

    ivec3 of_i = ivec3(IntScale * n);

    vec3 p_i = vec3(intBitsToFloat(floatBitsToInt(p.x) + ((p.x < 0.0) ? -of_i.x : of_i.x)),
                    intBitsToFloat(floatBitsToInt(p.y) + ((p.y < 0.0) ? -of_i.y : of_i.y)),
                    intBitsToFloat(floatBitsToInt(p.z) + ((p.z < 0.0) ? -of_i.z : of_i.z)));

    return vec3(abs(p[0]) < Origin ? (p[0] + FloatScale * n[0]) : p_i[0],
                abs(p[1]) < Origin ? (p[1] + FloatScale * n[1]) : p_i[1],
                abs(p[2]) < Origin ? (p[2] + FloatScale * n[2]) : p_i[2]);
}

mat2x3 CreateTangentVectors(vec3 normal) {
	vec3 up = abs(normal.z) < 0.99999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);

	mat2x3 tangents;

	tangents[0] = normalize(cross(up, normal));
	tangents[1] = cross(normal, tangents[0]);

	return tangents;
}

vec3 MapToCone(vec2 u, vec3 n, float radius) {
	vec2 offset = 2.0 * u - vec2(1.0);

	if (offset.x == 0.0 && offset.y == 0.0) {
		return n;
	}

	float theta, r;

	if (abs(offset.x) > abs(offset.y)) {
		r = offset.x;
		theta = 0.25 * M_PI * (offset.y / offset.x);
	} else {
		r = offset.y;
		theta = 0.5 * M_PI * (1.0 - 0.5 * (offset.x / offset.y));
	}

	vec2 uv = vec2(radius * r * cos(theta), radius * r * sin(theta));

	mat2x3 tangents = CreateTangentVectors(n);

	return n + uv.x * tangents[0] + uv.y * tangents[1];
}


layout (local_size_x = TILE_SIZE_X, local_size_y = TILE_SIZE_Y, local_size_z = 1) in;

void main() {
    uvec2 group_thread_id = RemapLane8x8(gl_LocalInvocationIndex);
    uvec4 tile = g_tile_list[gl_WorkGroupID.x];

    uvec2 tile_coord;
    uint mask;
    float min_t, max_t;
    UnpackTile(tile, tile_coord, mask, min_t, max_t);

    ivec2 icoord = ivec2(tile_coord * uvec2(TILE_SIZE_X, TILE_SIZE_Y) + group_thread_id);

    bool is_lit = true;

    uint bit_index = LaneIdToBitShift(group_thread_id);
    bool is_active = ((1u << bit_index) & mask) != 0u;
    if (is_active) {
        float depth = texelFetch(g_depth_tex, icoord, 0).r;
        vec3 normal_ws = UnpackNormalAndRoughness(texelFetch(g_norm_tex, icoord, 0)).xyz;
        vec3 normal_vs = normalize((g_shrd_data.view_matrix * vec4(normal_ws, 0.0)).xyz);

        vec2 px_center = vec2(icoord) + 0.5;
        vec2 in_uv = px_center / vec2(g_params.img_size);

#if defined(VULKAN)
        vec4 ray_origin_cs = vec4(2.0 * in_uv - 1.0, depth, 1.0);
        ray_origin_cs.y = -ray_origin_cs.y;
#else // VULKAN
        vec4 ray_origin_cs = vec4(2.0 * vec3(in_uv, depth) - 1.0, 1.0);
#endif // VULKAN

        vec4 ray_origin_vs = g_shrd_data.inv_proj_matrix * ray_origin_cs;
        ray_origin_vs /= ray_origin_vs.w;

        vec3 view_ray_vs = normalize(ray_origin_vs.xyz);
        vec3 shadow_ray_ws = g_shrd_data.sun_dir.xyz;

        vec2 u = texelFetch(g_noise_tex, icoord % 128, 0).rg;
        shadow_ray_ws = MapToCone(u, shadow_ray_ws, 0.01);

        vec4 ray_origin_ws = g_shrd_data.inv_view_matrix * ray_origin_vs;
        ray_origin_ws /= ray_origin_ws.w;

        // Bias to avoid self-intersection
        // TODO: use flat normal here
        ray_origin_ws.xyz = offset_ray(ray_origin_ws.xyz, normal_ws);

        const uint ray_flags = gl_RayFlagsCullBackFacingTrianglesEXT;

        float _cone_width = g_params.pixel_spread_angle * (-ray_origin_vs.z);

        rayQueryEXT rq;
        rayQueryInitializeEXT(rq,               // rayQuery
                              g_tlas,           // topLevel
                              ray_flags,        // rayFlags
                              0xff,             // cullMask
                              ray_origin_ws.xyz,// origin
                              min_t,            // tMin
                              shadow_ray_ws,    // direction
                              max_t             // tMax
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

        if (rayQueryGetIntersectionTypeEXT(rq, true) == gl_RayQueryCommittedIntersectionNoneEXT) {
            is_lit = false;
        }
    }

    uint new_mask = uint(is_lit) << bit_index;
    new_mask = subgroupOr(new_mask);
    if (gl_LocalInvocationIndex == 0) {
        uint old_mask = imageLoad(g_out_shadow_img, ivec2(tile_coord)).r;
        imageStore(g_out_shadow_img, ivec2(tile_coord), uvec4(old_mask & new_mask));
    }

#if 0
    uint ray_index = gl_WorkGroupID.x * 64 + gl_LocalInvocationIndex;
    if (ray_index >= g_ray_counter[1]) return;
    uint packed_coords = g_ray_list[ray_index];

    uvec2 ray_coords;
    bool copy_horizontal, copy_vertical, copy_diagonal;
    UnpackRayCoords(packed_coords, ray_coords, copy_horizontal, copy_vertical, copy_diagonal);

    ivec2 icoord = ivec2(ray_coords);
    float depth = texelFetch(g_depth_tex, icoord, 0).r;
    vec3 normal_ws = UnpackNormalAndRoughness(texelFetch(g_norm_tex, icoord, 0)).xyz;
    vec3 normal_vs = normalize((g_shrd_data.view_matrix * vec4(normal_ws, 0.0)).xyz);

    vec2 px_center = vec2(icoord) + 0.5;
    vec2 in_uv = px_center / vec2(g_params.img_size);

#if defined(VULKAN)
    vec4 ray_origin_cs = vec4(2.0 * in_uv - 1.0, depth, 1.0);
    ray_origin_cs.y = -ray_origin_cs.y;
#else // VULKAN
    vec4 ray_origin_cs = vec4(2.0 * vec3(in_uv, depth) - 1.0, 1.0);
#endif // VULKAN

    vec4 ray_origin_vs = g_shrd_data.inv_proj_matrix * ray_origin_cs;
    ray_origin_vs /= ray_origin_vs.w;

    vec3 view_ray_vs = normalize(ray_origin_vs.xyz);
    vec3 gi_ray_vs = SampleDiffuseVector(normal_vs, icoord);
    vec3 gi_ray_ws = (g_shrd_data.inv_view_matrix * vec4(gi_ray_vs.xyz, 0.0)).xyz;

    vec4 ray_origin_ws = g_shrd_data.inv_view_matrix * ray_origin_vs;
    ray_origin_ws /= ray_origin_ws.w;

    // Bias to avoid self-intersection
    // TODO: use flat normal here
    ray_origin_ws.xyz = offset_ray(ray_origin_ws.xyz, normal_ws);

    const uint ray_flags = gl_RayFlagsCullBackFacingTrianglesEXT;
    const float t_min = 0.0;
    const float t_max = 100.0;

    float _cone_width = g_params.pixel_spread_angle * (-ray_origin_vs.z);

    rayQueryEXT rq;
    rayQueryInitializeEXT(rq,               // rayQuery
                          g_tlas,           // topLevel
                          ray_flags,        // rayFlags
                          0xff,             // cullMask
                          ray_origin_ws.xyz,// origin
                          t_min,            // tMin
                          gi_ray_ws.xyz,  // direction
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

    vec3 col = vec3(0.0);
    float ray_len = t_max;

    if (rayQueryGetIntersectionTypeEXT(rq, true) == gl_RayQueryCommittedIntersectionNoneEXT) {
        col = clamp(RGBMDecode(textureLod(g_env_tex, gi_ray_ws.xyz, 4.0)), vec3(0.0), vec3(4.0)); // clamp is temporary workaround
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
            vec3 indirect = vec3(0.0);//2.0 * RGBMDecode(textureLod(g_lm_textures[1], lm_uv, 0.0));

            #if 0 // fake
                vec3 normal0 = vec3(unpackSnorm2x16(g_vtx_data1[geo.vertices_start + i0].x), unpackSnorm2x16(g_vtx_data1[geo.vertices_start + i0].y).x);
                vec3 normal1 = vec3(unpackSnorm2x16(g_vtx_data1[geo.vertices_start + i1].x), unpackSnorm2x16(g_vtx_data1[geo.vertices_start + i1].y).x);
                vec3 normal2 = vec3(unpackSnorm2x16(g_vtx_data1[geo.vertices_start + i2].x), unpackSnorm2x16(g_vtx_data1[geo.vertices_start + i2].y).x);

                vec3 normal = normal0 * (1.0 - bary_coord.x - bary_coord.y) + normal1 * bary_coord.x + normal2 * bary_coord.y;

                indirect += 0.1 * EvalSHIrradiance_NonLinear(normal,
                                              g_shrd_data.probes[geo.flags & RTGeoProbeBits].sh_coeffs[0],
                                              g_shrd_data.probes[geo.flags & RTGeoProbeBits].sh_coeffs[1],
                                              g_shrd_data.probes[geo.flags & RTGeoProbeBits].sh_coeffs[2]);
            #endif

            col *= (direct_lm + indirect);
        } else {
            vec3 normal0 = vec3(unpackSnorm2x16(g_vtx_data1[geo.vertices_start + i0].x), unpackSnorm2x16(g_vtx_data1[geo.vertices_start + i0].y).x);
            vec3 normal1 = vec3(unpackSnorm2x16(g_vtx_data1[geo.vertices_start + i1].x), unpackSnorm2x16(g_vtx_data1[geo.vertices_start + i1].y).x);
            vec3 normal2 = vec3(unpackSnorm2x16(g_vtx_data1[geo.vertices_start + i2].x), unpackSnorm2x16(g_vtx_data1[geo.vertices_start + i2].y).x);

            vec3 normal = normal0 * (1.0 - bary_coord.x - bary_coord.y) + normal1 * bary_coord.x + normal2 * bary_coord.y;

            col *= 0.0; /*EvalSHIrradiance_NonLinear(normal,
                                              g_shrd_data.probes[geo.flags & RTGeoProbeBits].sh_coeffs[0],
                                              g_shrd_data.probes[geo.flags & RTGeoProbeBits].sh_coeffs[1],
                                              g_shrd_data.probes[geo.flags & RTGeoProbeBits].sh_coeffs[2])*/;
        }

        ray_len = hit_t;
    }

    imageStore(g_out_color_img, icoord, vec4(col, ray_len));

    ivec2 copy_target = icoord ^ 1; // flip last bit to find the mirrored coords along the x and y axis within a quad
    if (copy_horizontal) {
        ivec2 copy_coords = ivec2(copy_target.x, icoord.y);
        imageStore(g_out_color_img, copy_coords, vec4(col, ray_len));
    }
    if (copy_vertical) {
        ivec2 copy_coords = ivec2(icoord.x, copy_target.y);
        imageStore(g_out_color_img, copy_coords, vec4(col, ray_len));
    }
    if (copy_diagonal) {
        ivec2 copy_coords = copy_target;
        imageStore(g_out_color_img, copy_coords, vec4(col, ray_len));
    }
#endif
}
