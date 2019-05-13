#version 310 es
#extension GL_EXT_texture_buffer : enable
#extension GL_EXT_texture_cube_map_array : enable

$ModifyWarning

#ifdef GL_ES
    precision mediump float;
    precision mediump sampler2DShadow;
#endif

layout(binding = $NormTexSlot) uniform sampler2D normals_texture;
layout(binding = $SpecTexSlot) uniform sampler2D specular_texture;
layout(binding = $SSAOTexSlot) uniform sampler2D ao_texture;
layout(binding = $EnvTexSlot) uniform mediump samplerCubeArray env_texture;
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
#else
in vec3 aVertexPos_;
in mat3 aVertexTBN_;
in vec2 aVertexUVs1_;
#endif

layout(location = $OutColorIndex) out vec4 outColor;

#include "common.glsl"

void main(void) {
    highp float lin_depth = uClipInfo[0] / (gl_FragCoord.z * (uClipInfo[1] - uClipInfo[2]) + uClipInfo[2]);
    highp float k = log2(lin_depth / uClipInfo[1]) / uClipInfo[3];
    int slice = int(floor(k * $ItemGridResZ.0));
    
    int ix = int(gl_FragCoord.x), iy = int(gl_FragCoord.y);
    int cell_index = slice * $ItemGridResX * $ItemGridResY + (iy * $ItemGridResY / int(uResAndFRes.y)) * $ItemGridResX + ix * $ItemGridResX / int(uResAndFRes.x);
    
    highp uvec2 cell_data = texelFetch(cells_buffer, cell_index).xy;
    highp uint offset = bitfieldExtract(cell_data.x, 0, 24);
    highp uint pcount = bitfieldExtract(cell_data.y, 8, 8);
    
    vec2 duv_dx = dFdx(aVertexUVs1_), duv_dy = dFdy(aVertexUVs1_);
    vec3 normal_color = textureGrad(normals_texture, aVertexUVs1_, 2.0 * duv_dx, 2.0 * duv_dy).xyz;
    vec4 specular_color = texture(specular_texture, aVertexUVs1_);
    
    vec3 normal = normal_color * 2.0 - 1.0;
    normal = aVertexTBN_ * normal;
    
    vec3 view_ray_ws = normalize(aVertexPos_ - uCamPosAndGamma.xyz);
    vec3 refl_ray_ws = reflect(view_ray_ws, normal);
    
    vec3 reflected_color = vec3(0.0);
    float total_dist = 0.0;
    
    for (uint i = offset; i < offset + pcount; i++) {
        highp uint item_data = texelFetch(items_buffer, int(i)).x;
        int pi = int(bitfieldExtract(item_data, 24, 8));
        
        float dist = distance(uProbes[pi].pos_and_radius.xyz, aVertexPos_);
        
        reflected_color += dist * RGBMDecode(texture(env_texture, vec4(refl_ray_ws, uProbes[pi].unused_and_layer.w)));
        total_dist += dist;
    }
    
    if (pcount != 0u) {
        reflected_color /= total_dist;
    }
    
    const float R0 = 0.25f;
    float factor = pow(clamp(1.0 - dot(normal, -view_ray_ws), 0.0, 1.0), 5.0);
    float fresnel = R0 + (1.0 - R0) * factor;
    
    outColor = vec4(fresnel * reflected_color * specular_color.xyz, 0.5);
}
