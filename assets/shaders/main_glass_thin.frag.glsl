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

layout(binding = $MatTex1Slot) uniform sampler2D normals_texture;
layout(binding = $MatTex2Slot) uniform sampler2D specular_texture;
layout(binding = $SSAOTexSlot) uniform sampler2D ao_texture;
layout(binding = $EnvTexSlot) uniform mediump samplerCubeArray env_texture;
layout(binding = $LightBufSlot) uniform mediump samplerBuffer lights_buffer;
layout(binding = $DecalBufSlot) uniform mediump samplerBuffer decals_buffer;
layout(binding = $CellsBufSlot) uniform highp usamplerBuffer cells_buffer;
layout(binding = $ItemsBufSlot) uniform highp usamplerBuffer items_buffer;
layout(binding = $Moments0TexSlot) uniform mediump sampler2D moments0_texture;
layout(binding = $Moments1TexSlot) uniform mediump sampler2D moments1_texture;
layout(binding = $Moments2TexSlot) uniform mediump sampler2D moments2_texture;
layout(binding = $Moments0MsTexSlot) uniform mediump sampler2DMS moments0_texture_ms;
layout(binding = $Moments1MsTexSlot) uniform mediump sampler2DMS moments1_texture_ms;
layout(binding = $Moments2MsTexSlot) uniform mediump sampler2DMS moments2_texture_ms;

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
    mat4 uViewMatrix, uProjMatrix, uViewProjMatrix, uViewProjPrevMatrix;
    mat4 uInvViewMatrix, uInvProjMatrix, uInvViewProjMatrix, uDeltaMatrix;
    ShadowMapRegion uShadowMapRegions[$MaxShadowMaps];
    vec4 uSunDir, uSunCol;
    vec4 uClipInfo, uCamPosAndGamma;
    vec4 uResAndFRes, uTranspParamsAndTime;
	vec4 uWindScroll, uWindScrollPrev;
    ProbeItem uProbes[$MaxProbes];
};

#if defined(VULKAN) || defined(GL_SPIRV)
layout(location = 0) in highp vec3 aVertexPos_;
layout(location = 1) in mediump vec2 aVertexUVs_;
layout(location = 2) in mediump vec3 aVertexNormal_;
layout(location = 3) in mediump vec3 aVertexTangent_;
#else
in highp vec3 aVertexPos_;
in mediump vec2 aVertexUVs_;
in mediump vec3 aVertexNormal_;
in mediump vec3 aVertexTangent_;
#endif

layout(location = $OutColorIndex) out vec4 outColor;
layout(location = $OutNormIndex) out vec4 outNormal;
layout(location = $OutSpecIndex) out vec4 outSpecular;

#include "common.glsl"

void main(void) {
    highp float lin_depth = uClipInfo[0] / (gl_FragCoord.z * (uClipInfo[1] - uClipInfo[2]) + uClipInfo[2]);
    
    // remapped depth in [-1; 1] range used for moments calculation
    highp float transp_z =
        2.0 * (log(lin_depth) - uTranspParamsAndTime[0]) / uTranspParamsAndTime[1] - 1.0;
    
    vec3 normal_color = texture(normals_texture, aVertexUVs_).wyz;
    
    vec3 normal = normal_color * 2.0 - 1.0;
    normal = normalize(mat3(aVertexTangent_, cross(aVertexNormal_, aVertexTangent_), aVertexNormal_) * normal);
    
    vec3 view_ray_ws = normalize(aVertexPos_ - uCamPosAndGamma.xyz);
    
    const float R0 = 0.15f;
    float factor = pow5(clamp(1.0 - dot(normal, -view_ray_ws), 0.0, 1.0));
    float fresnel = clamp(R0 + (1.0 - R0) * factor, 0.0, 1.0);
    
    if (uTranspParamsAndTime[2] < 1.5 || uTranspParamsAndTime[2] > 2.5) {
        highp float k = log2(lin_depth / uClipInfo[1]) / uClipInfo[3];
        int slice = int(floor(k * $ItemGridResZ.0));
        
        int ix = int(gl_FragCoord.x), iy = int(gl_FragCoord.y);
        int cell_index = slice * $ItemGridResX * $ItemGridResY + (iy * $ItemGridResY / int(uResAndFRes.y)) * $ItemGridResX + ix * $ItemGridResX / int(uResAndFRes.x);
        
        highp uvec2 cell_data = texelFetch(cells_buffer, cell_index).xy;
        highp uint offset = bitfieldExtract(cell_data.x, 0, 24);
        highp uint pcount = bitfieldExtract(cell_data.y, 8, 8);
         
        vec4 specular_color = texture(specular_texture, aVertexUVs_);
        vec3 refl_ray_ws = reflect(view_ray_ws, normal);
        
        vec3 reflected_color = vec3(0.0);
        float total_fade = 0.0;
        
        for (uint i = offset; i < offset + pcount; i++) {
            highp uint item_data = texelFetch(items_buffer, int(i)).x;
            int pi = int(bitfieldExtract(item_data, 24, 8));
            
            float dist = distance(uProbes[pi].pos_and_radius.xyz, aVertexPos_);
            float fade = 1.0 - smoothstep(0.9, 1.0, dist / uProbes[pi].pos_and_radius.w);
            
            reflected_color += fade * RGBMDecode(textureLod(env_texture, vec4(refl_ray_ws, uProbes[pi].unused_and_layer.w), 0.0));
            total_fade += fade;
        }
        
        if (total_fade > 1.0) {
            reflected_color /= total_fade;
        }
        
        if (uTranspParamsAndTime[2] < 0.5) {
            outColor = vec4(reflected_color * specular_color.rgb, fresnel);
        } else if (uTranspParamsAndTime[2] < 1.5) {
            outColor = vec4(reflected_color * specular_color.rgb, fresnel) * TransparentDepthWeight(gl_FragCoord.z, fresnel);
            outNormal = vec4(fresnel);
        } else {
            float b_0;
            vec4 b_1234;
                               
            if (uTranspParamsAndTime[2] < 3.5) {
                b_0 = texelFetch(moments0_texture, ivec2(ix, iy), 0).x;
                b_1234 = vec4(texelFetch(moments1_texture, ivec2(ix, iy), 0).xy,
                              texelFetch(moments2_texture, ivec2(ix, iy), 0).xy);
            } else {
                b_0 = texelFetch(moments0_texture_ms, ivec2(ix, iy), 0).x;
                b_1234 = vec4(texelFetch(moments1_texture_ms, ivec2(ix, iy), 0).xy,
                              texelFetch(moments2_texture_ms, ivec2(ix, iy), 0).xy);
            }
            
            float transmittance_at_depth;
            float total_transmittance;
            ResolveMoments(transp_z, b_0, b_1234, transmittance_at_depth, total_transmittance);
            
            outColor = vec4(fresnel * transmittance_at_depth * reflected_color * specular_color.rgb, fresnel * transmittance_at_depth);
        }
    } else {
        float b_0;
        vec4 b_1234;
        GenerateMoments(transp_z, 1.0 - fresnel, b_0, b_1234);
        
        outColor.x = b_0;
        outNormal.xy = b_1234.xy;
        outSpecular.xy = b_1234.zw;
    }
}
