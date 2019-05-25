#version 310 es
#extension GL_EXT_texture_buffer : enable
#extension GL_EXT_texture_cube_map_array : enable

$ModifyWarning

#ifdef GL_ES
    precision mediump float;
    precision mediump sampler2DShadow;
#endif

#define LIGHT_ATTEN_CUTOFF 0.004f

layout(binding = $DiffTexSlot) uniform sampler2D diffuse_texture;
layout(binding = $NormTexSlot) uniform sampler2D normals_texture;
layout(binding = $SpecTexSlot) uniform sampler2D specular_texture;
layout(binding = $ShadTexSlot) uniform sampler2DShadow shadow_texture;
layout(binding = $LmapSHSlot) uniform sampler2D lm_indirect_sh_texture[4];
layout(binding = $DecalTexSlot) uniform sampler2D decals_texture;
layout(binding = $SSAOTexSlot) uniform sampler2D ao_texture;
layout(binding = $LightBufSlot) uniform mediump samplerBuffer lights_buffer;
layout(binding = $DecalBufSlot) uniform mediump samplerBuffer decals_buffer;
layout(binding = $CellsBufSlot) uniform highp usamplerBuffer cells_buffer;
layout(binding = $ItemsBufSlot) uniform highp usamplerBuffer items_buffer;

struct ShadowMapRegion {
    vec4 transform;
    mat4 clip_from_world;
};

struct ProbeItem {
    vec4 pos_and_radius;
    vec4 unused_and_layer;
    vec4 sh_coeffs[3];
};

layout (std140) uniform SharedDataBlock {
    mat4 uViewMatrix, uProjMatrix, uViewProjMatrix;
    mat4 uInvViewMatrix, uInvProjMatrix, uInvViewProjMatrix, uDeltaMatrix;
    ShadowMapRegion uShadowMapRegions[$MaxShadowMaps];
    vec4 uSunDir, uSunCol;
    vec4 uClipInfo, uCamPosAndGamma;
    vec4 uResAndFRes;
    ProbeItem uProbes[$MaxProbes];
};

#ifdef VULKAN
layout(location = 0) in vec3 aVertexPos_;
layout(location = 1) in mat3 aVertexTBN_;
layout(location = 4) in vec2 aVertexUVs1_;
layout(location = 5) in vec2 aVertexUVs2_;
layout(location = 6) in vec3 aVertexShUVs_[4];
#else
in vec3 aVertexPos_;
in mat3 aVertexTBN_;
in vec2 aVertexUVs1_;
in vec2 aVertexUVs2_;
in vec3 aVertexShUVs_[4];
#endif

layout(location = $OutColorIndex) out vec4 outColor;
layout(location = $OutNormIndex) out vec3 outNormal;
layout(location = $OutSpecIndex) out vec4 outSpecular;

#include "common.glsl"

void main(void) {
    highp float lin_depth = uClipInfo[0] / (gl_FragCoord.z * (uClipInfo[1] - uClipInfo[2]) + uClipInfo[2]);
    highp float k = log2(lin_depth / uClipInfo[1]) / uClipInfo[3];
    int slice = int(floor(k * $ItemGridResZ.0));
    
    int ix = int(gl_FragCoord.x), iy = int(gl_FragCoord.y);
    int cell_index = slice * $ItemGridResX * $ItemGridResY + (iy * $ItemGridResY / int(uResAndFRes.y)) * $ItemGridResX + ix * $ItemGridResX / int(uResAndFRes.x);
    
    highp uvec2 cell_data = texelFetch(cells_buffer, cell_index).xy;
    highp uvec2 offset_and_lcount = uvec2(bitfieldExtract(cell_data.x, 0, 24), bitfieldExtract(cell_data.x, 24, 8));
    highp uvec2 dcount_and_pcount = uvec2(bitfieldExtract(cell_data.y, 0, 8), bitfieldExtract(cell_data.y, 8, 8));
    
    vec3 albedo_color = pow(texture(diffuse_texture, aVertexUVs1_).rgb, vec3(uCamPosAndGamma.w));
    vec3 normal_color = texture(normals_texture, aVertexUVs1_).xyz;
    vec4 specular_color = texture(specular_texture, aVertexUVs1_);
    
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
        
        if (app.x < 1.0 && app.y < 1.0 && app.z < 1.0) {
            vec4 diff_uvs_tr = texelFetch(decals_buffer, di * 6 + 3);
            float decal_influence = 0.0;
            
            if (diff_uvs_tr.z > 0.0) {
                vec2 diff_uvs = diff_uvs_tr.xy + diff_uvs_tr.zw * uvs;
                
                vec2 _duv_dx = diff_uvs_tr.zw * duv_dx;
                vec2 _duv_dy = diff_uvs_tr.zw * duv_dy;
            
                vec4 decal_diff = textureGrad(decals_texture, diff_uvs, _duv_dx, _duv_dy);
                decal_influence = decal_diff.a;
                albedo_color = mix(albedo_color, decal_diff.xyz, decal_influence);
            }
            
            vec4 norm_uvs_tr = texelFetch(decals_buffer, di * 6 + 4);
            
            if (norm_uvs_tr.z > 0.0) {
                vec2 norm_uvs = norm_uvs_tr.xy + norm_uvs_tr.zw * uvs;
                
                vec2 _duv_dx = 2.0 * norm_uvs_tr.zw * duv_dx;
                vec2 _duv_dy = 2.0 * norm_uvs_tr.zw * duv_dy;
            
                vec4 decal_norm = textureGrad(decals_texture, norm_uvs, _duv_dx, _duv_dy);
                normal_color = mix(normal_color, decal_norm.xyz, decal_influence);
            }
            
            vec4 spec_uvs_tr = texelFetch(decals_buffer, di * 6 + 5);
            
            if (spec_uvs_tr.z > 0.0) {
                vec2 spec_uvs = spec_uvs_tr.xy + spec_uvs_tr.zw * uvs;
                
                vec2 _duv_dx = spec_uvs_tr.zw * duv_dx;
                vec2 _duv_dy = spec_uvs_tr.zw * duv_dy;
            
                vec4 decal_spec = textureGrad(decals_texture, spec_uvs, _duv_dx, _duv_dy);
                specular_color = mix(specular_color, decal_spec, decal_influence);
            }
        }
    }
    
    vec3 normal = normal_color * 2.0 - 1.0;
    normal = aVertexTBN_ * normal;
    
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
        if (_dot2 > dir_and_spot.w && (brightness * atten) > $FltEps) {
            int shadowreg_index = floatBitsToInt(col_and_index.w);
            if (shadowreg_index != -1) {
                vec4 reg_tr = uShadowMapRegions[shadowreg_index].transform;
                
                highp vec4 pp = uShadowMapRegions[shadowreg_index].clip_from_world * vec4(aVertexPos_, 1.0);
                pp /= pp.w;
                pp.xyz = pp.xyz * 0.5 + vec3(0.5);
                pp.xy = reg_tr.xy + pp.xy * reg_tr.zw;
                
                const vec2 shadow_softness = vec2(3.0 / $ShadRes.0, 1.5 / $ShadRes.0);

                highp float r = M_PI * (-1.0 + 2.0 * rand(gl_FragCoord.xy));
                highp vec2 rx = vec2(cos(r), sin(r));
                highp vec2 ry = vec2(rx.y, -rx.x);
                
                float visibility = 0.0;
                
                int num_samples = min(int(16.0 * reg_tr.w), 8);
                
                highp float weight = 1.0 / float(num_samples);
                for (int i = 0; i < num_samples; i++) {
                    visibility += texture(shadow_texture, pp.xyz + vec3((rx * poisson_disk[i].x + ry * poisson_disk[i].y) * shadow_softness, 0.0));
                }
                visibility *= weight;
                
                atten *= visibility;
            }
            
            additional_light += col_and_index.xyz * atten * smoothstep(dir_and_spot.w, dir_and_spot.w + 0.2, _dot2);
        }
    }
    
    vec3 sh_l_00 = RGBMDecode(texture(lm_indirect_sh_texture[0], aVertexUVs2_));
    vec3 sh_l_10 = texture(lm_indirect_sh_texture[1], aVertexUVs2_).rgb;
    vec3 sh_l_11 = texture(lm_indirect_sh_texture[2], aVertexUVs2_).rgb;
    vec3 sh_l_12 = texture(lm_indirect_sh_texture[3], aVertexUVs2_).rgb;
    
    //indirect_col += sh_l_00 + sh_l_10 * normal.y + sh_l_11 * normal.z + sh_l_12 * normal.x;
    vec3 indirect_col = (0.5 + (sh_l_10 - vec3(0.5)) * normal.y +
                               (sh_l_11 - vec3(0.5)) * normal.z +
                               (sh_l_12 - vec3(0.5)) * normal.x) * sh_l_00 * 2.0;
    indirect_col = max(indirect_col, vec3(0.0));
    
    float lambert = max(dot(normal, uSunDir.xyz), 0.0);
    float visibility = 0.0;
    if (lambert > 0.00001) {
        visibility = GetSunVisibility(lin_depth, shadow_texture, aVertexShUVs_);
    }
    
    vec2 ao_uvs = gl_FragCoord.xy / uResAndFRes.xy;
    float dx = $SSAOBufResDiv.0 / uResAndFRes.z, dy = $SSAOBufResDiv.0 / uResAndFRes.w;
    float ambient_occlusion = texture(ao_texture, ao_uvs).r + texture(ao_texture, ao_uvs + vec2(dx, 0.0)).r +
                              texture(ao_texture, ao_uvs + vec2(0.0, dy)).r + texture(ao_texture, ao_uvs + vec2(dx, dy)).r;
    ambient_occlusion *= 0.25;
                              
    vec3 diffuse_color = albedo_color * (uSunCol.xyz * lambert * visibility + ambient_occlusion * indirect_col + additional_light);
    
    outColor = vec4(diffuse_color, 1.0);
    
    vec3 normal_vs = normalize((uViewMatrix * vec4(normal, 0.0)).xyz);
    outNormal = normal_vs * 0.5 + 0.5;
    
    outSpecular = vec4(vec3(ambient_occlusion), 1.0) * specular_color;
}
