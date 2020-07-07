#version 310 es
#extension GL_EXT_texture_buffer : enable
#extension GL_EXT_texture_cube_map_array : enable
//#extension GL_EXT_control_flow_attributes : enable

$ModifyWarning

#ifdef GL_ES
    precision mediump float;
    precision mediump sampler2DShadow;
#endif

#include "common_fs.glsl"
#include "common.glsl"

layout(binding = REN_MAT_TEX0_SLOT) uniform sampler2D diffuse_texture;
layout(binding = REN_MAT_TEX1_SLOT) uniform sampler2D normals_texture;
layout(binding = REN_MAT_TEX2_SLOT) uniform sampler2D specular_texture;
layout(binding = REN_SSAO_TEX_SLOT) uniform sampler2D ao_texture;
layout(binding = REN_ENV_TEX_SLOT) uniform mediump samplerCubeArray env_texture;
layout(binding = REN_LIGHT_BUF_SLOT) uniform mediump samplerBuffer lights_buffer;
layout(binding = REN_DECAL_BUF_SLOT) uniform mediump samplerBuffer decals_buffer;
layout(binding = REN_CELLS_BUF_SLOT) uniform highp usamplerBuffer cells_buffer;
layout(binding = REN_ITEMS_BUF_SLOT) uniform highp usamplerBuffer items_buffer;
layout(binding = REN_MOMENTS0_TEX_SLOT) uniform mediump sampler2D moments0_texture;
layout(binding = REN_MOMENTS1_TEX_SLOT) uniform mediump sampler2D moments1_texture;
layout(binding = REN_MOMENTS2_TEX_SLOT) uniform mediump sampler2D moments2_texture;
layout(binding = REN_MOMENTS0_MS_TEX_SLOT) uniform mediump sampler2DMS moments0_texture_ms;
layout(binding = REN_MOMENTS1_MS_TEX_SLOT) uniform mediump sampler2DMS moments1_texture_ms;
layout(binding = REN_MOMENTS2_MS_TEX_SLOT) uniform mediump sampler2DMS moments2_texture_ms;

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
layout(location = 1) in mediump vec2 aVertexUVs_;
layout(location = 2) in mediump vec3 aVertexNormal_;
layout(location = 3) in mediump vec3 aVertexTangent_;
#else
in highp vec3 aVertexPos_;
in mediump vec2 aVertexUVs_;
in mediump vec3 aVertexNormal_;
in mediump vec3 aVertexTangent_;
#endif

layout(location = REN_OUT_COLOR_INDEX) out vec4 outColor;
layout(location = REN_OUT_NORM_INDEX) out vec4 outNormal;
layout(location = REN_OUT_SPEC_INDEX) out vec4 outSpecular;

void main(void) {
    highp float lin_depth = LinearizeDepth(gl_FragCoord.z, shrd_data.uClipInfo);
    
    // remapped depth in [-1; 1] range used for moments calculation
    highp float transp_z =
        2.0 * (log(lin_depth) - shrd_data.uTranspParamsAndTime[0]) /
            shrd_data.uTranspParamsAndTime[1] - 1.0;
    
    vec3 normal_color = texture(normals_texture, aVertexUVs_).wyz;
        
    vec3 normal = normal_color * 2.0 - 1.0;
    normal = normalize(mat3(aVertexTangent_, cross(aVertexNormal_, aVertexTangent_),
                            aVertexNormal_) * normal);
    
    vec3 view_ray_ws = normalize(aVertexPos_ - shrd_data.uCamPosAndGamma.xyz);
    
    float val = shrd_data.uTranspParamsAndTime[3] + aVertexPos_.y * 10.0;
    float kk = 0.75 + 0.25 * step(val - floor(val), 0.5);
    
    float tr = 0.75 * clamp(1.2 - dot(normal, -view_ray_ws), 0.0, 1.0);
    
    if (shrd_data.uTranspParamsAndTime[2] < 1.5 || shrd_data.uTranspParamsAndTime[2] > 2.5) {
        highp float k = log2(lin_depth / shrd_data.uClipInfo[1]) / shrd_data.uClipInfo[3];
        int slice = int(floor(k * float(REN_GRID_RES_Z)));
        
        int ix = int(gl_FragCoord.x), iy = int(gl_FragCoord.y);
        int cell_index = GetCellIndex(ix, iy, slice, shrd_data.uResAndFRes.xy);
        
        highp uvec2 cell_data = texelFetch(cells_buffer, cell_index).xy;
        highp uint offset = bitfieldExtract(cell_data.x, 0, 24);
        highp uint pcount = bitfieldExtract(cell_data.y, 8, 8);
        
        vec3 diffuse_color = texture(diffuse_texture, aVertexUVs_).xyz;
        vec4 specular_color = texture(specular_texture, aVertexUVs_);
        
        vec3 refl_ray_ws = reflect(view_ray_ws, normal);
        
        vec3 reflected_color = vec3(0.0);
        float total_fade = 0.0;
        
        for (uint i = offset; i < offset + pcount; i++) {
            highp uint item_data = texelFetch(items_buffer, int(i)).x;
            int pi = int(bitfieldExtract(item_data, 24, 8));
            
            float dist = distance(shrd_data.uProbes[pi].pos_and_radius.xyz, aVertexPos_);
            float fade = 1.0 - smoothstep(0.9, 1.0,
                                          dist / shrd_data.uProbes[pi].pos_and_radius.w);
            
            reflected_color += fade * RGBMDecode(textureLod(env_texture,
                vec4(refl_ray_ws, shrd_data.uProbes[pi].unused_and_layer.w), 0.0));
            total_fade += fade;
        }
        
        if (total_fade > 1.0) {
            reflected_color /= total_fade;
        }
        
        float alpha = tr * kk;
        
        if (shrd_data.uTranspParamsAndTime[2] < 0.5) {
            outColor = vec4(diffuse_color, alpha);
        } else if (shrd_data.uTranspParamsAndTime[2] < 1.5) {
            outColor = vec4(diffuse_color, alpha) * TransparentDepthWeight(gl_FragCoord.z,
                                                                           alpha);
            outNormal = vec4(alpha);
        } else {
            float b_0;
            vec4 b_1234;
                               
            if (shrd_data.uTranspParamsAndTime[2] < 3.5) {
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
        
            outColor = vec4(alpha * transmittance_at_depth * diffuse_color,
                            alpha * transmittance_at_depth);
        }
    } else {
        float alpha = tr * kk;
        
        // Store moments into render target
        float b_0;
        vec4 b_1234;
        GenerateMoments(transp_z, 1.0 - alpha, b_0, b_1234);
        
        outColor.x = b_0;
        outNormal.xy = b_1234.xy;
        outSpecular.xy = b_1234.zw;
    }
}
