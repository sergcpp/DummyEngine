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

layout(binding = BLAS_BUF_SLOT) uniform samplerBuffer g_blas_nodes;
layout(binding = TLAS_BUF_SLOT) uniform samplerBuffer g_tlas_nodes;

layout(std430, binding = GEO_DATA_BUF_SLOT) readonly buffer GeometryData {
    RTGeoInstance g_geometries[];
};

layout(std430, binding = MATERIAL_BUF_SLOT) readonly buffer Materials {
    MaterialData g_materials[];
};

layout(binding = VTX_BUF1_SLOT) uniform samplerBuffer g_vtx_data0;
layout(binding = VTX_BUF2_SLOT) uniform usamplerBuffer g_vtx_data1;
layout(binding = NDX_BUF_SLOT) uniform usamplerBuffer g_vtx_indices;

layout(binding = PRIM_NDX_BUF_SLOT) uniform usamplerBuffer g_prim_indices;
layout(binding = MESHES_BUF_SLOT) uniform usamplerBuffer g_meshes;
layout(binding = MESH_INSTANCES_BUF_SLOT) uniform samplerBuffer g_mesh_instances;

layout(binding = LMAP_TEX_SLOTS) uniform sampler2D g_lm_textures[5];

layout(binding = ENV_TEX_SLOT) uniform samplerCube g_env_tex;

layout(binding = OUT_IMG_SLOT, r11f_g11f_b10f) uniform image2D g_out_image;

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
    inter.geo_index = inter.geo_count = 0;
    inter.t = MAX_DIST;
    inter.u = inter.v = 0.0;

    Traverse_MacroTree_WithStack(g_tlas_nodes, g_blas_nodes, g_mesh_instances, g_meshes, g_vtx_data0,
                                 g_vtx_indices, g_prim_indices, origin.xyz, direction.xyz, inv_d, 0 /* root_node */, inter);

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

        uint i0 = texelFetch(g_vtx_indices, 3 * inter.prim_index + 0).x;
        uint i1 = texelFetch(g_vtx_indices, 3 * inter.prim_index + 1).x;
        uint i2 = texelFetch(g_vtx_indices, 3 * inter.prim_index + 2).x;

        vec4 p0 = texelFetch(g_vtx_data0, int(geo.vertices_start + i0));
        vec4 p1 = texelFetch(g_vtx_data0, int(geo.vertices_start + i1));
        vec4 p2 = texelFetch(g_vtx_data0, int(geo.vertices_start + i2));

        vec2 uv0 = unpackHalf2x16(floatBitsToUint(p0.w));
        vec2 uv1 = unpackHalf2x16(floatBitsToUint(p1.w));
        vec2 uv2 = unpackHalf2x16(floatBitsToUint(p2.w));

        vec2 uv = uv0 * (1.0 - inter.u - inter.v) + uv1 * inter.u + uv2 * inter.v;
#if defined(BINDLESS_TEXTURES)
        mat4x3 inv_transform = transpose(mat3x4(texelFetch(g_mesh_instances, int(5 * inter.obj_index + 2)),
                                                texelFetch(g_mesh_instances, int(5 * inter.obj_index + 3)),
                                                texelFetch(g_mesh_instances, int(5 * inter.obj_index + 4))));
        vec3 direction_obj_space = (inv_transform * vec4(direction.xyz, 0.0)).xyz;

        vec2 tex_res = textureSize(SAMPLER2D(GET_HANDLE(mat.texture_indices[0])), 0).xy;
        float ta = abs((uv1.x - uv0.x) * (uv2.y - uv0.y) - (uv2.x - uv0.x) * (uv1.y - uv0.y));

        vec3 tri_normal = cross(p1.xyz - p0.xyz, p2.xyz - p0.xyz);
        float pa = length(tri_normal);
        tri_normal /= pa;

        float cone_width = g_params.pixel_spread_angle * inter.t;

        float tex_lod = 0.5 * log2(ta/pa);
        tex_lod += log2(cone_width);
        tex_lod += 0.5 * log2(tex_res.x * tex_res.y);
        tex_lod -= log2(abs(dot(direction_obj_space, tri_normal)));

        col = SRGBToLinear(YCoCg_to_RGB(textureLod(SAMPLER2D(GET_HANDLE(mat.texture_indices[0])), uv, tex_lod)));
#else
        // TODO: Fallback to shared texture atlas
#endif

        if ((geo.flags & RTGeoLightmappedBit) != 0u) {
            vec2 lm_uv0 = unpackHalf2x16(texelFetch(g_vtx_data1, int(geo.vertices_start + i0)).w);
            vec2 lm_uv1 = unpackHalf2x16(texelFetch(g_vtx_data1, int(geo.vertices_start + i1)).w);
            vec2 lm_uv2 = unpackHalf2x16(texelFetch(g_vtx_data1, int(geo.vertices_start + i2)).w);

            vec2 lm_uv = lm_uv0 * (1.0 - inter.u - inter.v) + lm_uv1 * inter.u + lm_uv2 * inter.v;
            lm_uv = geo.lmap_transform.xy + geo.lmap_transform.zw * lm_uv;

            vec3 direct_lm = RGBMDecode(textureLod(g_lm_textures[0], lm_uv, 0.0));
            vec3 indirect_lm = 2.0 * RGBMDecode(textureLod(g_lm_textures[1], lm_uv, 0.0));
            col *= (direct_lm + indirect_lm);
        } else {
            uvec2 packed0 = texelFetch(g_vtx_data1, int(geo.vertices_start + i0)).xy;
            uvec2 packed1 = texelFetch(g_vtx_data1, int(geo.vertices_start + i1)).xy;
            uvec2 packed2 = texelFetch(g_vtx_data1, int(geo.vertices_start + i2)).xy;

            vec3 normal0 = vec3(unpackSnorm2x16(packed0.x), unpackSnorm2x16(packed0.y).x);
            vec3 normal1 = vec3(unpackSnorm2x16(packed1.x), unpackSnorm2x16(packed1.y).x);
            vec3 normal2 = vec3(unpackSnorm2x16(packed2.x), unpackSnorm2x16(packed2.y).x);

            vec3 normal = normal0 * (1.0 - inter.u - inter.v) + normal1 * inter.u + normal2 * inter.v;

            col *= EvalSHIrradiance_NonLinear(normal,
                                              g_shrd_data.probes[geo.flags & RTGeoProbeBits].sh_coeffs[0],
                                              g_shrd_data.probes[geo.flags & RTGeoProbeBits].sh_coeffs[1],
                                              g_shrd_data.probes[geo.flags & RTGeoProbeBits].sh_coeffs[2]);
        }
    }

    imageStore(g_out_image, ivec2(gl_GlobalInvocationID.xy), vec4(col, 1.0));
}