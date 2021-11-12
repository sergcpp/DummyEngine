#version 310 es
#extension GL_EXT_texture_buffer : enable
#extension GL_OES_texture_buffer : enable
#extension GL_EXT_texture_cube_map_array : enable
//#extension GL_EXT_control_flow_attributes : enable

$ModifyWarning

#if defined(GL_ES) || defined(VULKAN)
    precision highp int;
    precision highp float;
    precision mediump sampler2DShadow;
#endif

#include "internal/_fs_common.glsl"
#include "internal/_texturing.glsl"

#if !defined(BINDLESS_TEXTURES)
layout(binding = REN_MAT_TEX0_SLOT) uniform sampler2D diff_texture;
layout(binding = REN_MAT_TEX1_SLOT) uniform sampler2D norm_texture;
layout(binding = REN_MAT_TEX2_SLOT) uniform sampler2D spec_texture;
#endif // BINDLESS_TEXTURES
layout(binding = REN_SHAD_TEX_SLOT) uniform sampler2DShadow shadow_texture;
layout(binding = REN_LMAP_SH_SLOT) uniform sampler2D lm_indirect_sh_texture[4];
layout(binding = REN_DECAL_TEX_SLOT) uniform sampler2D decals_texture;
layout(binding = REN_SSAO_TEX_SLOT) uniform sampler2D ao_texture;
layout(binding = REN_ENV_TEX_SLOT) uniform mediump samplerCubeArray env_texture;
layout(binding = REN_LIGHT_BUF_SLOT) uniform mediump samplerBuffer lights_buffer;
layout(binding = REN_DECAL_BUF_SLOT) uniform mediump samplerBuffer decals_buffer;
layout(binding = REN_CELLS_BUF_SLOT) uniform highp usamplerBuffer cells_buffer;
layout(binding = REN_ITEMS_BUF_SLOT) uniform highp usamplerBuffer items_buffer;
layout(binding = REN_CONE_RT_LUT_SLOT) uniform lowp sampler2D cone_rt_lut;

#if defined(VULKAN) || defined(GL_SPIRV)
layout (binding = REN_UB_SHARED_DATA_LOC, std140)
#else
layout (std140)
#endif
uniform SharedDataBlock {
    SharedData shrd_data;
};

LAYOUT(location = 0) in highp vec3 aVertexPos_;
LAYOUT(location = 1) in mediump vec2 aVertexUVs_;
LAYOUT(location = 2) in mediump vec3 aVertexNormal_;
LAYOUT(location = 3) in mediump vec3 aVertexTangent_;
#if defined(BINDLESS_TEXTURES)
    LAYOUT(location = 9) in flat TEX_HANDLE norm_texture;
    LAYOUT(location = 10) in flat TEX_HANDLE spec_texture;
#endif // BINDLESS_TEXTURES

layout(location = REN_OUT_COLOR_INDEX) out vec4 outColor;
layout(location = REN_OUT_NORM_INDEX) out vec4 outNormal;
layout(location = REN_OUT_SPEC_INDEX) out vec4 outSpecular;

void main(void) {
    highp float lin_depth = LinearizeDepth(gl_FragCoord.z, shrd_data.uClipInfo);

    // remapped depth in [-1; 1] range used for moments calculation
    highp float transp_z = 2.0 * (log(lin_depth) - shrd_data.uTranspParamsAndTime[0]) /
                           shrd_data.uTranspParamsAndTime[1] - 1.0;

    vec3 normal_color = texture(SAMPLER2D(norm_texture), aVertexUVs_).wyz;

    vec3 normal = normal_color * 2.0 - 1.0;
    normal = normalize(mat3(aVertexTangent_, cross(aVertexNormal_, aVertexTangent_),
                            aVertexNormal_) * normal);

    vec3 view_ray_ws = normalize(aVertexPos_ - shrd_data.uCamPosAndGamma.xyz);

    const float R0 = 0.04f;
    float factor = pow5(clamp(1.0 - dot(normal, -view_ray_ws), 0.0, 1.0));
    float fresnel = clamp(R0 + (1.0 - R0) * factor, 0.0, 1.0);

    highp float k = log2(lin_depth / shrd_data.uClipInfo[1]) / shrd_data.uClipInfo[3];
    int slice = int(floor(k * float(REN_GRID_RES_X)));

    int ix = int(gl_FragCoord.x), iy = int(gl_FragCoord.y);
    int cell_index = GetCellIndex(ix, iy, slice, shrd_data.uResAndFRes.xy);

    highp uvec2 cell_data = texelFetch(cells_buffer, cell_index).xy;
    highp uint offset = bitfieldExtract(cell_data.x, 0, 24);
    highp uint pcount = bitfieldExtract(cell_data.y, 8, 8);

    vec4 specular_color = texture(SAMPLER2D(spec_texture), aVertexUVs_);
    vec3 refl_ray_ws = reflect(view_ray_ws, normal);

    vec3 reflected_color = vec3(0.0);
    float total_fade = 0.0;

    for (uint i = offset; i < offset + pcount; i++) {
        highp uint item_data = texelFetch(items_buffer, int(i)).x;
        int pi = int(bitfieldExtract(item_data, 24, 8));

        float dist = distance(shrd_data.uProbes[pi].pos_and_radius.xyz, aVertexPos_);
        float fade = 1.0 - smoothstep(0.9, 1.0,
                                      dist / shrd_data.uProbes[pi].pos_and_radius.w);

        reflected_color += fade * RGBMDecode(
            textureLod(env_texture, vec4(refl_ray_ws,
                                         shrd_data.uProbes[pi].unused_and_layer.w), 0.0));
        total_fade += fade;
    }

    if (total_fade > 1.0) {
        reflected_color /= total_fade;
    }

    outColor = vec4(reflected_color * specular_color.rgb, fresnel);
}
