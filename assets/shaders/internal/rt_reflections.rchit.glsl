#version 460
#extension GL_EXT_ray_tracing : require

#include "_rt_common.glsl"
#include "_texturing.glsl"
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

layout(location = 0) rayPayloadInEXT RayPayload g_pld;

hitAttributeEXT vec2 bary_coord;

void main() {
    RTGeoInstance geo = g_geometries[gl_InstanceCustomIndexEXT + gl_GeometryIndexEXT];
    MaterialData mat = g_materials[geo.material_index];

    uint i0 = g_indices[geo.indices_start + 3 * gl_PrimitiveID + 0];
    uint i1 = g_indices[geo.indices_start + 3 * gl_PrimitiveID + 1];
    uint i2 = g_indices[geo.indices_start + 3 * gl_PrimitiveID + 2];

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

    float cone_width = g_pld.cone_width + g_params.pixel_spread_angle * gl_HitTEXT;

    float tex_lod = 0.5 * log2(ta/pa);
    tex_lod += log2(cone_width);
    tex_lod += 0.5 * log2(tex_res.x * tex_res.y);
    tex_lod -= log2(abs(dot(gl_ObjectRayDirectionEXT, tri_normal)));
    g_pld.col = textureLod(SAMPLER2D(mat.texture_indices[0]), uv, tex_lod).xyz;

    if ((geo.flags & RTGeoLightmappedBit) != 0u) {
        vec2 lm_uv0 = unpackHalf2x16(g_vtx_data1[geo.vertices_start + i0].w);
        vec2 lm_uv1 = unpackHalf2x16(g_vtx_data1[geo.vertices_start + i1].w);
        vec2 lm_uv2 = unpackHalf2x16(g_vtx_data1[geo.vertices_start + i2].w);

        vec2 lm_uv = lm_uv0 * (1.0 - bary_coord.x - bary_coord.y) + lm_uv1 * bary_coord.x + lm_uv2 * bary_coord.y;
        lm_uv = geo.lmap_transform.xy + geo.lmap_transform.zw * lm_uv;

        vec3 direct_lm = RGBMDecode(textureLod(g_lm_textures[0], lm_uv, 0.0));
        vec3 indirect_lm = 2.0 * RGBMDecode(textureLod(g_lm_textures[1], lm_uv, 0.0));
        g_pld.col *= (direct_lm + indirect_lm);
    } else {
        vec3 normal0 = vec3(unpackSnorm2x16(g_vtx_data1[geo.vertices_start + i0].x), unpackSnorm2x16(g_vtx_data1[geo.vertices_start + i0].y).x);
        vec3 normal1 = vec3(unpackSnorm2x16(g_vtx_data1[geo.vertices_start + i1].x), unpackSnorm2x16(g_vtx_data1[geo.vertices_start + i1].y).x);
        vec3 normal2 = vec3(unpackSnorm2x16(g_vtx_data1[geo.vertices_start + i2].x), unpackSnorm2x16(g_vtx_data1[geo.vertices_start + i2].y).x);

        vec3 normal = normal0 * (1.0 - bary_coord.x - bary_coord.y) + normal1 * bary_coord.x + normal2 * bary_coord.y;

        g_pld.col *= EvalSHIrradiance_NonLinear(normal,
                                              g_shrd_data.probes[geo.flags & RTGeoProbeBits].sh_coeffs[0],
                                              g_shrd_data.probes[geo.flags & RTGeoProbeBits].sh_coeffs[1],
                                              g_shrd_data.probes[geo.flags & RTGeoProbeBits].sh_coeffs[2]);
    }
    g_pld.cone_width = gl_HitTEXT;
}


