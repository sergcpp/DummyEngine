#version 310 es
#extension GL_EXT_texture_buffer : enable
#extension GL_OES_texture_buffer : enable
#extension GL_EXT_texture_cube_map_array : enable
//#extension GL_EXT_control_flow_attributes : enable

$ModifyWarning

#ifdef GL_ES
    precision mediump float;
    precision mediump sampler2DShadow;
#endif

#define LIGHT_ATTEN_CUTOFF 0.004

layout(binding = $MatTex0Slot) uniform sampler2D diffuse_texture;
layout(binding = $MatTex1Slot) uniform sampler2D normals_texture;
layout(binding = $MatTex2Slot) uniform sampler2D specular_texture;
layout(binding = $ShadTexSlot) uniform sampler2DShadow shadow_texture;
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

#if defined(VULKAN) || defined(GL_SPIRV)
layout (binding = 0, std140)
#else
layout (std140)
#endif
uniform SharedDataBlock {
    mat4 uViewMatrix, uProjMatrix, uViewProjMatrix;
    mat4 uInvViewMatrix, uInvProjMatrix, uInvViewProjMatrix, uDeltaMatrix;
    ShadowMapRegion uShadowMapRegions[$MaxShadowMaps];
    vec4 uSunDir, uSunCol;
    vec4 uClipInfo, uCamPosAndGamma;
    vec4 uResAndFRes, uTranspDepthRangeAndUnused;
    ProbeItem uProbes[$MaxProbes];
};

#if defined(VULKAN) || defined(GL_SPIRV)
layout(location = 0) in highp vec3 aVertexPos_;
layout(location = 1) in mediump vec2 aVertexUVs_;
layout(location = 2) in mediump vec3 aVertexNormal_;
layout(location = 3) in mediump vec3 aVertexTangent_;
layout(location = 4) in highp vec3 aVertexShUVs_[4];
#else
in highp vec3 aVertexPos_;
in mediump vec2 aVertexUVs_;
in mediump vec3 aVertexNormal_;
in mediump vec3 aVertexTangent_;
in highp vec3 aVertexShUVs_[4];
#endif

layout(location = $OutColorIndex) out vec4 outColor;
layout(location = $OutNormIndex) out vec4 outNormal;
layout(location = $OutSpecIndex) out vec4 outSpecular;

#include "common.glsl"

float median(float r, float g, float b) {
    return max(min(r, g), min(max(r, g), b));
}

void main(void) {
    highp float lin_depth = uClipInfo[0] / (gl_FragCoord.z * (uClipInfo[1] - uClipInfo[2]) + uClipInfo[2]);
    highp float k = log2(lin_depth / uClipInfo[1]) / uClipInfo[3];
    int slice = int(floor(k * $ItemGridResZ.0));
    
    int ix = int(gl_FragCoord.x), iy = int(gl_FragCoord.y);
    int cell_index = slice * $ItemGridResX * $ItemGridResY + (iy * $ItemGridResY / int(uResAndFRes.y)) * $ItemGridResX + ix * $ItemGridResX / int(uResAndFRes.x);
    
    highp uvec2 cell_data = texelFetch(cells_buffer, cell_index).xy;
    highp uvec2 offset_and_lcount = uvec2(bitfieldExtract(cell_data.x, 0, 24), bitfieldExtract(cell_data.x, 24, 8));
    highp uvec2 dcount_and_pcount = uvec2(bitfieldExtract(cell_data.y, 0, 8), bitfieldExtract(cell_data.y, 8, 8));
    
    vec3 albedo_color = texture(diffuse_texture, aVertexUVs_).rgb;
    
    vec2 duv_dx = dFdx(aVertexUVs_), duv_dy = dFdy(aVertexUVs_);
    vec3 normal_color = texture(normals_texture, aVertexUVs_).wyz;
    vec4 specular_color = texture(specular_texture, aVertexUVs_);

    vec3 normal = normal_color * 2.0 - 1.0;
    normal = normalize(mat3(aVertexTangent_, cross(aVertexNormal_, aVertexTangent_), aVertexNormal_) * normal);
    
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
                
                atten *= SampleShadowPCF5x5(shadow_texture, pp.xyz);
            }
            
            additional_light += col_and_index.xyz * atten * smoothstep(dir_and_spot.w, dir_and_spot.w + 0.2, _dot2);
        }
    }
    
    vec3 indirect_col = vec3(0.0);
    float total_fade = 0.0;
    
    for (uint i = offset_and_lcount.x; i < offset_and_lcount.x + dcount_and_pcount.y; i++) {
        highp uint item_data = texelFetch(items_buffer, int(i)).x;
        int pi = int(bitfieldExtract(item_data, 24, 8));
        
        const float SH_A0 = 0.886226952; // PI / sqrt(4.0f * Pi)
        const float SH_A1 = 1.02332675;  // sqrt(PI / 3.0f)
        
        float dist = distance(uProbes[pi].pos_and_radius.xyz, aVertexPos_);
        float fade = 1.0 - smoothstep(0.9, 1.0, dist / uProbes[pi].pos_and_radius.w);
        vec4 vv = fade * vec4(SH_A0, SH_A1 * normal.yzx);

        indirect_col.r += dot(uProbes[pi].sh_coeffs[0], vv);
        indirect_col.g += dot(uProbes[pi].sh_coeffs[1], vv);
        indirect_col.b += dot(uProbes[pi].sh_coeffs[2], vv);
        total_fade += fade;
    }
    
    indirect_col /= max(total_fade, 1.0);
    indirect_col = max(4.0 * indirect_col, vec3(0.0));
    
    float lambert = clamp(dot(normal, uSunDir.xyz), 0.0, 1.0);
    float visibility = 0.0;
    if (lambert > 0.00001) {
        visibility = GetSunVisibility(lin_depth, shadow_texture, aVertexShUVs_);
    }
    
    vec4 text_color = textureLod(specular_texture, aVertexUVs_, 0.0);
    
    {   // SDF drawing
        float sig_dist = median(text_color.r, text_color.g, text_color.b);
        
        float s = sig_dist - 0.5;
        float v = s / fwidth(s);
        
        albedo_color = mix(albedo_color, vec3(0.0), clamp(v + 0.5, 0.0, 1.0));
    }
    
    vec2 ao_uvs = vec2(ix, iy) / uResAndFRes.zw;
    float ambient_occlusion = textureLod(ao_texture, ao_uvs, 0.0).r;
    vec3 diffuse_color = albedo_color * (uSunCol.xyz * lambert * visibility + ambient_occlusion * indirect_col + additional_light);
    
    outColor = vec4(diffuse_color, 1.0);
    outNormal = vec4(normal * 0.5 + 0.5, 0.0);
    outSpecular = vec4(0.0);
}
