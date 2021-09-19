#version 460
#extension GL_EXT_ray_tracing : require

#include "_rt_common.glsl"
#include "_texturing.glsl"
#include "rt_reflections_interface.glsl"

layout(std430, binding = GEO_DATA_BUF_SLOT) readonly buffer GeometryData {
    RTGeoInstance geometries[];
};

layout(std430, binding = MATERIAL_BUF_SLOT) readonly buffer Materials {
    MaterialData materials[];
};

layout(std430, binding = VTX_BUF1_SLOT) readonly buffer VtxData0 {
    uvec4 vtx_data0[];
};

layout(std430, binding = NDX_BUF_SLOT) readonly buffer NdxData {
    uint indices[];
};

hitAttributeEXT vec2 bary_coord;

void main() {
    RTGeoInstance geo = geometries[gl_InstanceCustomIndexEXT + gl_GeometryIndexEXT];
    MaterialData mat = materials[geo.material_index];

    uint i0 = indices[geo.indices_start + 3 * gl_PrimitiveID + 0];
    uint i1 = indices[geo.indices_start + 3 * gl_PrimitiveID + 1];
    uint i2 = indices[geo.indices_start + 3 * gl_PrimitiveID + 2];

    vec2 uv0 = unpackHalf2x16(vtx_data0[geo.vertices_start + i0].w);
    vec2 uv1 = unpackHalf2x16(vtx_data0[geo.vertices_start + i1].w);
    vec2 uv2 = unpackHalf2x16(vtx_data0[geo.vertices_start + i2].w);

    vec2 uv = uv0 * (1.0 - bary_coord.x - bary_coord.y) + uv1 * bary_coord.x + uv2 * bary_coord.y;
    float alpha = textureLod(SAMPLER2D(mat.texture_indices[0]), uv, 0.0).a;
    if (alpha < 0.5) {
        ignoreIntersectionEXT;
    }
}
