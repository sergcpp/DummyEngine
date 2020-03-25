R"(#version 310 es
#extension GL_ARB_texture_multisample : enable
#extension GL_EXT_texture_buffer : enable
#extension GL_EXT_texture_cube_map_array : enable

#ifdef GL_ES
    precision mediump float;
#endif

)"
#include "_fs_common.glsl"
R"(

/*
UNIFORM_BLOCKS
    SharedDataBlock : )" AS_STR(REN_UB_SHARED_DATA_LOC) R"(
*/

)" __ADDITIONAL_DEFINES_STR__ R"(

layout (std140) uniform SharedDataBlock {
    SharedData shrd_data;
};

#if defined(MSAA_4)
layout(binding = REN_REFL_SPEC_TEX_SLOT) uniform mediump sampler2DMS s_spec_texture;
layout(binding = REN_REFL_DEPTH_TEX_SLOT) uniform mediump sampler2DMS s_depth_texture;
layout(binding = REN_REFL_NORM_TEX_SLOT) uniform mediump sampler2DMS s_norm_texture;
#else
layout(binding = REN_REFL_SPEC_TEX_SLOT) uniform mediump sampler2D s_spec_texture;
layout(binding = REN_REFL_DEPTH_TEX_SLOT) uniform mediump sampler2D s_depth_texture;
layout(binding = REN_REFL_NORM_TEX_SLOT) uniform mediump sampler2D s_norm_texture;
#endif
layout(binding = REN_REFL_DEPTH_LOW_TEX_SLOT) uniform mediump sampler2D depth_low_texture;
layout(binding = REN_REFL_SSR_TEX_SLOT) uniform mediump sampler2D source_texture;

layout(binding = REN_REFL_PREV_TEX_SLOT) uniform mediump sampler2D prev_texture;
layout(binding = REN_REFL_BRDF_TEX_SLOT) uniform sampler2D brdf_lut_texture;
layout(binding = REN_ENV_TEX_SLOT) uniform mediump samplerCubeArray probe_textures;
layout(binding = REN_CELLS_BUF_SLOT) uniform highp usamplerBuffer cells_buffer;
layout(binding = REN_ITEMS_BUF_SLOT) uniform highp usamplerBuffer items_buffer;

in vec2 aVertexUVs_;

out vec4 outColor;

void main() {
    outColor = vec4(0.0);

    ivec2 icoord = ivec2(gl_FragCoord.xy);
    ivec2 icoord_low = ivec2(gl_FragCoord.xy) / 2;

#if defined(MSAA_4)
    vec4 specular = 0.25 * (texelFetch(s_spec_texture, icoord, 0) +
                            texelFetch(s_spec_texture, icoord, 1) +
                            texelFetch(s_spec_texture, icoord, 2) +
                            texelFetch(s_spec_texture, icoord, 3));
#else
    vec4 specular = texelFetch(s_spec_texture, icoord, 0);
#endif
    if ((specular.r + specular.g + specular.b) < 0.0001) return;

    float depth = texelFetch(s_depth_texture, icoord, 0).r;
    float d0 = LinearizeDepth(depth, shrd_data.uClipInfo);
 
    float d1 = abs(d0 - texelFetch(depth_low_texture, icoord_low + ivec2(0, 0), 0).r);
    float d2 = abs(d0 - texelFetch(depth_low_texture, icoord_low + ivec2(0, 1), 0).r);
    float d3 = abs(d0 - texelFetch(depth_low_texture, icoord_low + ivec2(1, 0), 0).r);
    float d4 = abs(d0 - texelFetch(depth_low_texture, icoord_low + ivec2(1, 1), 0).r);
 
    float dmin = min(min(d1, d2), min(d3, d4));
 
    vec3 ssr_uvs;

    if (dmin < 0.05) {
        ssr_uvs = texture(source_texture, aVertexUVs_ * shrd_data.uResAndFRes.xy / shrd_data.uResAndFRes.zw).rgb;
    } else {
        if (dmin == d1) {
            ssr_uvs = texelFetch(source_texture, icoord_low + ivec2(0, 0), 0).rgb;
        } else if (dmin == d2) {
            ssr_uvs = texelFetch(source_texture, icoord_low + ivec2(0, 1), 0).rgb;
        } else if (dmin == d3) {
            ssr_uvs = texelFetch(source_texture, icoord_low + ivec2(1, 0), 0).rgb;
        } else {
            ssr_uvs = texelFetch(source_texture, icoord_low + ivec2(1, 1), 0).rgb;
        }
    }

    vec3 normal = 2.0 * texelFetch(s_norm_texture, icoord, 0).xyz - 1.0;

    float tex_lod = 6.0 * specular.a;
    float N_dot_V;

    vec3 c0 = vec3(0.0);
    vec2 brdf;

    {   // apply cubemap contribution
        vec4 ray_origin_cs = vec4(2.0 * vec3(aVertexUVs_.xy, depth) - 1.0, 1.0);

        vec4 ray_origin_vs = shrd_data.uInvProjMatrix * ray_origin_cs;
        ray_origin_vs /= ray_origin_vs.w;

        vec3 view_ray_ws = normalize((shrd_data.uInvViewMatrix * vec4(ray_origin_vs.xyz, 0.0)).xyz);
        vec3 refl_ray_ws = reflect(view_ray_ws, normal);

        vec4 ray_origin_ws = shrd_data.uInvViewMatrix * ray_origin_vs;
        ray_origin_ws /= ray_origin_ws.w;

        highp float k = log2(d0 / shrd_data.uClipInfo[1]) / shrd_data.uClipInfo[3];
        int slice = int(floor(k * float(REN_GRID_RES_Z)));
    
        int ix = icoord.x, iy = icoord.y;
        int cell_index = slice * REN_GRID_RES_X * REN_GRID_RES_Y + (iy * REN_GRID_RES_Y / int(shrd_data.uResAndFRes.y)) * REN_GRID_RES_X + (ix * REN_GRID_RES_X / int(shrd_data.uResAndFRes.x));
        
        highp uvec2 cell_data = texelFetch(cells_buffer, cell_index).xy;
        highp uint offset = bitfieldExtract(cell_data.x, 0, 24);
        highp uint pcount = bitfieldExtract(cell_data.y, 8, 8);

        float total_fade = 0.0;

        for (uint i = offset; i < offset + pcount; i++) {
            highp uint item_data = texelFetch(items_buffer, int(i)).x;
            int pi = int(bitfieldExtract(item_data, 24, 8));

            float dist = distance(shrd_data.uProbes[pi].pos_and_radius.xyz, ray_origin_ws.xyz);
            float fade = 1.0 - smoothstep(0.9, 1.0, dist / shrd_data.uProbes[pi].pos_and_radius.w);
            c0 += fade * RGBMDecode(textureLod(probe_textures, vec4(refl_ray_ws, shrd_data.uProbes[pi].unused_and_layer.w), tex_lod));
            total_fade += fade;
        }

        c0 /= max(total_fade, 1.0);

        N_dot_V = clamp(dot(normal, -view_ray_ws), 0.0, 1.0);
        brdf = texture(brdf_lut_texture, vec2(N_dot_V, specular.a)).xy;
    }

    vec3 kS = FresnelSchlickRoughness(N_dot_V, specular.rgb, specular.a);

    c0 = mix(c0, textureLod(prev_texture, ssr_uvs.rg, 0.0).xyz, ssr_uvs.b);
    outColor = vec4(c0 * (kS * brdf.x + brdf.y), 1.0);
}
)"
