#version 310 es
#extension GL_EXT_texture_buffer : enable
#extension GL_OES_texture_buffer : enable
#extension GL_ARB_bindless_texture: enable
//#extension GL_EXT_control_flow_attributes : enable

$ModifyWarning

#ifdef GL_ES
    precision mediump float;
    precision mediump sampler2DShadow;
#else
    #define lowp
    #define mediump
    #define highp
#endif

#include "internal/_fs_common.glsl"

#define LIGHT_ATTEN_CUTOFF 0.004

#if !defined(GL_ARB_bindless_texture)
layout(binding = REN_MAT_TEX0_SLOT) uniform sampler2D diff_texture;
layout(binding = REN_MAT_TEX1_SLOT) uniform sampler2D norm_texture;
layout(binding = REN_MAT_TEX2_SLOT) uniform sampler2D spec_texture;
#endif // GL_ARB_bindless_texture
layout(binding = REN_SHAD_TEX_SLOT) uniform sampler2DShadow shadow_texture;
layout(binding = REN_LMAP_SH_SLOT) uniform sampler2D lm_indirect_sh_texture[4];
layout(binding = REN_DECAL_TEX_SLOT) uniform sampler2D decals_texture;
layout(binding = REN_SSAO_TEX_SLOT) uniform sampler2D ao_texture;
layout(binding = REN_LIGHT_BUF_SLOT) uniform mediump samplerBuffer lights_buffer;
layout(binding = REN_DECAL_BUF_SLOT) uniform mediump samplerBuffer decals_buffer;
layout(binding = REN_CELLS_BUF_SLOT) uniform highp usamplerBuffer cells_buffer;
layout(binding = REN_ITEMS_BUF_SLOT) uniform highp usamplerBuffer items_buffer;
layout(binding = REN_CONE_RT_LUT_SLOT) uniform lowp sampler2D cone_rt_lut;

#if defined(VULKAN) || defined(GL_SPIRV)
layout (binding = 0, std140)
#else
layout (std140)
#endif
uniform SharedDataBlock {
    SharedData shrd_data;
};

#if defined(VULKAN) || defined(GL_SPIRV)
layout(location = 0) in highp vec3 aVertexPos_;
layout(location = 1) in mediump vec4 aVertexUVs_;
layout(location = 2) in mediump vec3 aVertexNormal_;
layout(location = 3) in mediump vec3 aVertexTangent_;
layout(location = 4) in highp vec3 aVertexShUVs_[4];
#if defined(GL_ARB_bindless_texture)
layout(location = 8) in flat uvec2 diff_texture;
layout(location = 9) in flat uvec2 norm_texture;
layout(location = 10) in flat uvec2 spec_texture;
#endif // GL_ARB_bindless_texture
#else
in highp vec3 aVertexPos_;
in mediump vec4 aVertexUVs_;
in mediump vec3 aVertexNormal_;
in mediump vec3 aVertexTangent_;
in highp vec3 aVertexShUVs_[4];
#if defined(GL_ARB_bindless_texture)
in flat uvec2 diff_texture;
in flat uvec2 norm_texture;
in flat uvec2 spec_texture;
#endif // GL_ARB_bindless_texture
#endif

layout(location = REN_OUT_COLOR_INDEX) out vec4 outColor;
layout(location = REN_OUT_NORM_INDEX) out vec4 outNormal;
layout(location = REN_OUT_SPEC_INDEX) out vec4 outSpecular;

void main(void) {
    highp float lin_depth = LinearizeDepth(gl_FragCoord.z, shrd_data.uClipInfo);
    highp float k = log2(lin_depth / shrd_data.uClipInfo[1]) / shrd_data.uClipInfo[3];
    int slice = int(floor(k * float(REN_GRID_RES_Z)));
    
    int ix = int(gl_FragCoord.x), iy = int(gl_FragCoord.y);
    int cell_index = GetCellIndex(ix, iy, slice, shrd_data.uResAndFRes.xy);
    
    highp uvec2 cell_data = texelFetch(cells_buffer, cell_index).xy;
    highp uvec2 offset_and_lcount = uvec2(bitfieldExtract(cell_data.x, 0, 24),
                                          bitfieldExtract(cell_data.x, 24, 8));
    highp uvec2 dcount_and_pcount = uvec2(bitfieldExtract(cell_data.y, 0, 8),
                                          bitfieldExtract(cell_data.y, 8, 8));
    
    vec3 diff_color = texture(SAMPLER2D(diff_texture), aVertexUVs_.xy).rgb;
    vec3 norm_color = texture(SAMPLER2D(norm_texture), aVertexUVs_.xy).wyz;
    vec4 spec_color = texture(SAMPLER2D(spec_texture), aVertexUVs_.xy);
    
    vec3 dp_dx = dFdx(aVertexPos_);
    vec3 dp_dy = dFdy(aVertexPos_);
    
    for (uint i = offset_and_lcount.x; i < offset_and_lcount.x + dcount_and_pcount.x; i++) {
        highp uint item_data = texelFetch(items_buffer, int(i)).x;
        int di = int(bitfieldExtract(item_data, 12, 12));
        
        mat4 de_proj;
        de_proj[0] = texelFetch(decals_buffer, di * 6 + 0);
        de_proj[1] = texelFetch(decals_buffer, di * 6 + 1);
        de_proj[2] = texelFetch(decals_buffer, di * 6 + 2);
        de_proj[3] = vec4(0.0, 0.0, 0.0, 1.0);
        de_proj = transpose(de_proj);
        
        vec4 pp = de_proj * vec4(aVertexPos_, 1.0);
        pp /= pp[3];
        
        vec3 app = abs(pp.xyz);
        vec2 uvs = pp.xy * 0.5 + 0.5;
        
        vec2 duv_dx = 0.5 * (de_proj * vec4(dp_dx, 0.0)).xy;
        vec2 duv_dy = 0.5 * (de_proj * vec4(dp_dy, 0.0)).xy;
        
        /*[[branch]]*/ if (app.x < 1.0 && app.y < 1.0 && app.z < 1.0) {
            vec4 diff_uvs_tr = texelFetch(decals_buffer, di * 6 + 3);
            float decal_influence = 0.0;
            
            if (diff_uvs_tr.z > 0.0) {
                vec2 diff_uvs = diff_uvs_tr.xy + diff_uvs_tr.zw * uvs;
                
                vec2 _duv_dx = diff_uvs_tr.zw * duv_dx;
                vec2 _duv_dy = diff_uvs_tr.zw * duv_dy;
            
                vec4 decal_diff = textureGrad(decals_texture, diff_uvs, _duv_dx, _duv_dy);
                decal_influence = decal_diff.a;
                diff_color = mix(diff_color, SRGBToLinear(decal_diff.rgb), decal_influence);
            }
            
            vec4 norm_uvs_tr = texelFetch(decals_buffer, di * 6 + 4);
            
            if (norm_uvs_tr.z > 0.0) {
                vec2 norm_uvs = norm_uvs_tr.xy + norm_uvs_tr.zw * uvs;
                
                vec2 _duv_dx = 2.0 * norm_uvs_tr.zw * duv_dx;
                vec2 _duv_dy = 2.0 * norm_uvs_tr.zw * duv_dy;
            
                vec3 decal_norm = textureGrad(decals_texture, norm_uvs, _duv_dx, _duv_dy).wyz;
                norm_color = mix(norm_color, decal_norm, decal_influence);
            }
            
            vec4 spec_uvs_tr = texelFetch(decals_buffer, di * 6 + 5);
            
            if (spec_uvs_tr.z > 0.0) {
                vec2 spec_uvs = spec_uvs_tr.xy + spec_uvs_tr.zw * uvs;
                
                vec2 _duv_dx = spec_uvs_tr.zw * duv_dx;
                vec2 _duv_dy = spec_uvs_tr.zw * duv_dy;
            
                vec4 decal_spec = textureGrad(decals_texture, spec_uvs, _duv_dx, _duv_dy);
                spec_color = mix(spec_color, decal_spec, decal_influence);
            }
        }
    }
    
    vec3 normal = norm_color * 2.0 - 1.0;
    normal = normalize(mat3(cross(aVertexTangent_, aVertexNormal_), aVertexTangent_, aVertexNormal_) * normal);
    
    vec3 additional_light = vec3(0.0, 0.0, 0.0);

    for (uint i = offset_and_lcount.x; i < offset_and_lcount.x + offset_and_lcount.y; i++) {
        highp uint item_data = texelFetch(items_buffer, int(i)).x;
        int li = int(bitfieldExtract(item_data, 0, 12));

        vec4 pos_and_radius = texelFetch(lights_buffer, li * 3 + 0);
        highp vec4 col_and_index = texelFetch(lights_buffer, li * 3 + 1);
        vec4 dir_and_spot = texelFetch(lights_buffer, li * 3 + 2);
        
        vec3 L = pos_and_radius.xyz - aVertexPos_;
        float dist = length(L);
        float d = max(dist - pos_and_radius.w, 0.0);
        L /= dist;
        
        highp float denom = d / pos_and_radius.w + 1.0;
        highp float atten = 1.0 / (denom * denom);
        
        highp float brightness = max(col_and_index.x, max(col_and_index.y, col_and_index.z));
        
        highp float factor = LIGHT_ATTEN_CUTOFF / brightness;
        atten = (atten - factor) / (1.0 - LIGHT_ATTEN_CUTOFF);
        atten = max(atten, 0.0);
        
        float _dot1 = max(dot(L, normal), 0.0);
        float _dot2 = dot(L, dir_and_spot.xyz);
        
        atten = _dot1 * atten;
        if (_dot2 > dir_and_spot.w && (brightness * atten) > FLT_EPS) {
            int shadowreg_index = floatBitsToInt(col_and_index.w);
            /*[[branch]]*/ if (shadowreg_index != -1) {
                vec4 reg_tr = shrd_data.uShadowMapRegions[shadowreg_index].transform;
                
                highp vec4 pp = shrd_data.uShadowMapRegions[shadowreg_index].clip_from_world * vec4(aVertexPos_, 1.0);
                pp /= pp.w;
                pp.xyz = pp.xyz * 0.5 + vec3(0.5);
                pp.xy = reg_tr.xy + pp.xy * reg_tr.zw;
                
                atten *= SampleShadowPCF5x5(shadow_texture, pp.xyz);
            }
            
            additional_light += col_and_index.xyz * atten *
                smoothstep(dir_and_spot.w, dir_and_spot.w + 0.2, _dot2);
        }
    }
    
    vec3 sh_l_00 = RGBMDecode(texture(lm_indirect_sh_texture[0], aVertexUVs_.zw));
    vec3 sh_l_10 = 2.0 * texture(lm_indirect_sh_texture[1], aVertexUVs_.zw).rgb - vec3(1.0);
    vec3 sh_l_11 = 2.0 * texture(lm_indirect_sh_texture[2], aVertexUVs_.zw).rgb - vec3(1.0);
    vec3 sh_l_12 = 2.0 * texture(lm_indirect_sh_texture[3], aVertexUVs_.zw).rgb - vec3(1.0);
    
    vec3 cone_origin_ws = aVertexPos_;
    // use green channel of L1 band as cone tracing direction
    vec3 cone_dir_ws = normalize(vec3(sh_l_12.g, sh_l_10.g, sh_l_11.g));
    
    float cone_occlusion = 1.0;
    float sph_occlusion = 1.0;

    highp uint ecount = bitfieldExtract(cell_data.y, 16, 8);

    for (uint i = offset_and_lcount.x; i < offset_and_lcount.x + ecount; i++) {
        highp uint item_data = texelFetch(items_buffer, int(i)).y;
        int ei = int(bitfieldExtract(item_data, 0, 8));

        vec4 pos_and_radius = shrd_data.uEllipsoids[ei].pos_and_radius;
        vec4 axis_and_perp = shrd_data.uEllipsoids[ei].axis_and_perp;

#if 1
        float axis_len = length(axis_and_perp.xyz);

        mat3 sph_ls;
        sph_ls[0] = vec3(0.0);
        sph_ls[0][floatBitsToInt(axis_and_perp.w)] = 1.0;
        sph_ls[1] = axis_and_perp.xyz;// / axis_len;
        sph_ls[2] = normalize(cross(sph_ls[0], sph_ls[1]));
        sph_ls[0] = normalize(cross(sph_ls[1], sph_ls[2]));
        sph_ls = inverse(sph_ls);

        vec3 cone_origin_ls = sph_ls * (cone_origin_ws.xyz - pos_and_radius.xyz);
        vec3 cone_dir_ls = sph_ls * cone_dir_ws;
        
        //cone_origin_ls[1] /= axis_len;
        //cone_dir_ls[1] /= axis_len;
        cone_dir_ls = normalize(cone_dir_ls);

        vec3 dir = -cone_origin_ls;
        float dist = length(dir);

        float sin_omega = pos_and_radius.w / sqrt(pos_and_radius.w * pos_and_radius.w + dist * dist);
        float cos_phi = dot(dir, cone_dir_ls) / dist;

        cone_occlusion *= textureLod(cone_rt_lut, vec2(cos_phi, sin_omega), 0.0).r;
        sph_occlusion *= clamp(1.0 - (pos_and_radius.w / dist) * (pos_and_radius.w / dist), 0.0, 1.0);
#else
        vec3 dir = pos_and_radius.xyz - cone_origin_ws;
        float dist = length(dir);

        float sin_omega = pos_and_radius.w / sqrt(pos_and_radius.w * pos_and_radius.w +
                                                  dist * dist);
        float cos_phi = dot(dir, cone_dir_ws) / dist;
        
        cone_occlusion *= textureLod(cone_rt_lut, vec2(cos_phi, sin_omega), 0.0).g;
        sph_occlusion *= clamp(1.0 - (pos_and_radius.w / dist) * (pos_and_radius.w / dist), 0.0, 1.0);
#endif
    }
    
    // squared to compensate gamma
    cone_occlusion *= cone_occlusion;
    
    vec3 indirect_col = sph_occlusion * EvalSHIrradiance(cone_occlusion * normal, sh_l_00, sh_l_10, sh_l_11, sh_l_12);
    
    float lambert = clamp(dot(normal, shrd_data.uSunDir.xyz), 0.0, 1.0);
    float visibility = 0.0;
    /*[[branch]]*/ if (lambert > 0.00001) {
        visibility = GetSunVisibility(lin_depth, shadow_texture, aVertexShUVs_);
    }

    vec2 ao_uvs = vec2(ix, iy) / shrd_data.uResAndFRes.zw;
    float ambient_occlusion = textureLod(ao_texture, ao_uvs, 0.0).r;
    vec3 diffuse_color = diff_color * (shrd_data.uSunCol.xyz * lambert * visibility +
                                         ambient_occlusion * ambient_occlusion * indirect_col +
                                         additional_light);
    
    vec3 view_ray_ws = normalize(shrd_data.uCamPosAndGamma.xyz - aVertexPos_);
    float N_dot_V = clamp(dot(normal, view_ray_ws), 0.0, 1.0);

    vec3 kD = 1.0 - FresnelSchlickRoughness(N_dot_V, spec_color.xyz, spec_color.a);
    
    outColor = vec4(diffuse_color * kD, 1.0);
    outNormal = vec4(normal * 0.5 + 0.5, 0.0);
    outSpecular = spec_color;
}
