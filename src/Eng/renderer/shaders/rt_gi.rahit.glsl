#version 460
#extension GL_EXT_ray_tracing : require

#include "rt_common.glsl"
#include "texturing_common.glsl"
#include "rt_gi_interface.h"

layout(std430, binding = GEO_DATA_BUF_SLOT) readonly buffer GeometryData {
    RTGeoInstance g_geometries[];
};

layout(std430, binding = MATERIAL_BUF_SLOT) readonly buffer Materials {
    MaterialData g_materials[];
};

layout(std430, binding = VTX_BUF1_SLOT) readonly buffer VtxData0 {
    uvec4 g_vtx_data0[];
};

layout(std430, binding = NDX_BUF_SLOT) readonly buffer NdxData {
    uint g_indices[];
};

hitAttributeEXT vec2 bary_coord;

void main() {
    RTGeoInstance geo = g_geometries[gl_InstanceCustomIndexEXT + gl_GeometryIndexEXT];
    MaterialData mat = g_materials[geo.material_index & MATERIAL_INDEX_BITS];

    uint i0 = g_indices[geo.indices_start + 3 * gl_PrimitiveID + 0];
    uint i1 = g_indices[geo.indices_start + 3 * gl_PrimitiveID + 1];
    uint i2 = g_indices[geo.indices_start + 3 * gl_PrimitiveID + 2];

    vec2 uv0 = unpackHalf2x16(g_vtx_data0[geo.vertices_start + i0].w);
    vec2 uv1 = unpackHalf2x16(g_vtx_data0[geo.vertices_start + i1].w);
    vec2 uv2 = unpackHalf2x16(g_vtx_data0[geo.vertices_start + i2].w);

    vec2 uv = uv0 * (1.0 - bary_coord.x - bary_coord.y) + uv1 * bary_coord.x + uv2 * bary_coord.y;
    float alpha = textureLod(SAMPLER2D(mat.texture_indices[4]), uv, 0.0).r;
    if (alpha < 0.5) {
        ignoreIntersectionEXT;
    }
}
