#version 460
#extension GL_EXT_ray_tracing : require

#include "rt_common.glsl"
#include "texturing_common.glsl"
#include "rt_debug_interface.h"

layout(std430, binding = GEO_DATA_BUF_SLOT) readonly buffer GeometryData {
    rt_geo_instance_t g_geometries[];
};

layout(std430, binding = MATERIAL_BUF_SLOT) readonly buffer Materials {
    material_data_t g_materials[];
};

layout(std430, binding = VTX_BUF1_SLOT) readonly buffer VtxData0 {
    uvec4 g_vtx_data0[];
};

layout(std430, binding = NDX_BUF_SLOT) readonly buffer NdxData {
    uint g_vtx_indices[];
};

layout(location = 0) rayPayloadInEXT RayPayload g_pld;

hitAttributeEXT vec2 bary_coord;

void main() {
    const rt_geo_instance_t geo = g_geometries[gl_InstanceCustomIndexEXT + gl_GeometryIndexEXT];
    uint mat_index = (geo.material_index & 0xffff);
    if (gl_HitKindEXT == gl_HitKindBackFacingTriangleEXT) {
        mat_index = (geo.material_index >> 16);
    }
    const material_data_t mat = g_materials[mat_index & MATERIAL_INDEX_BITS];

    const uint i0 = g_vtx_indices[geo.indices_start + 3 * gl_PrimitiveID + 0];
    const uint i1 = g_vtx_indices[geo.indices_start + 3 * gl_PrimitiveID + 1];
    const uint i2 = g_vtx_indices[geo.indices_start + 3 * gl_PrimitiveID + 2];

    const vec2 uv0 = unpackHalf2x16(g_vtx_data0[geo.vertices_start + i0].w);
    const vec2 uv1 = unpackHalf2x16(g_vtx_data0[geo.vertices_start + i1].w);
    const vec2 uv2 = unpackHalf2x16(g_vtx_data0[geo.vertices_start + i2].w);

    const vec2 uv = uv0 * (1.0 - bary_coord.x - bary_coord.y) + uv1 * bary_coord.x + uv2 * bary_coord.y;
    const float alpha = (1.0 - mat.params[3].x) * textureLod(SAMPLER2D(mat.texture_indices[MAT_TEX_ALPHA]), uv, 0.0).x;
    if (alpha < 0.5) {
        ignoreIntersectionEXT;
    }
    if (mat.params[2].y > 0) {
        const vec3 base_color = mat.params[0].xyz * SRGBToLinear(YCoCg_to_RGB(textureLod(SAMPLER2D(mat.texture_indices[MAT_TEX_BASECOLOR]), uv, 0.0)));
        g_pld.throughput = min(g_pld.throughput, mix(vec3(1.0), 0.8 * mat.params[2].y * base_color, alpha));
        g_pld.throughput_dist = min(g_pld.throughput_dist, gl_HitTEXT);
        if (dot(g_pld.throughput, vec3(0.333)) > 0.1) {
            ignoreIntersectionEXT;
        }
    }
}
