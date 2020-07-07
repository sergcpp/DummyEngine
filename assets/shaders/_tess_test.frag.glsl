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

#include "common_fs.glsl"

#define LIGHT_ATTEN_CUTOFF 0.004

layout(binding = REN_MAT_TEX0_SLOT) uniform sampler2D diffuse_texture;
layout(binding = REN_MAT_TEX1_SLOT) uniform sampler2D normals_texture;
layout(binding = REN_MAT_TEX2_SLOT) uniform sampler2D specular_texture;
layout(binding = REN_MAT_TEX3_SLOT) uniform sampler2D bump_texture;
layout(binding = REN_SHAD_TEX_SLOT) uniform sampler2DShadow shadow_texture;
layout(binding = REN_DECAL_TEX_SLOT) uniform sampler2D decals_texture;
layout(binding = REN_SSAO_TEX_SLOT) uniform sampler2D ao_texture;
layout(binding = REN_BRDF_TEX_SLOT) uniform sampler2D brdf_lut_texture;
layout(binding = REN_LIGHT_BUF_SLOT) uniform mediump samplerBuffer lights_buffer;
layout(binding = REN_DECAL_BUF_SLOT) uniform mediump samplerBuffer decals_buffer;
layout(binding = REN_CELLS_BUF_SLOT) uniform highp usamplerBuffer cells_buffer;
layout(binding = REN_ITEMS_BUF_SLOT) uniform highp usamplerBuffer items_buffer;

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
layout(location = 4) in highp vec3 aVertexShUVs_[4];
layout(location = 8) in lowp float tex_height;
#else
in highp vec3 aVertexPos_;
in mediump vec2 aVertexUVs_;
in mediump vec3 aVertexNormal_;
in mediump vec3 aVertexTangent_;
in highp vec3 aVertexShUVs_[4];
in lowp float tex_height;
#endif

layout(location = REN_OUT_COLOR_INDEX) out vec4 outColor;
layout(location = REN_OUT_NORM_INDEX) out vec4 outNormal;
layout(location = REN_OUT_SPEC_INDEX) out vec4 outSpecular;

#include "common.glsl"

vec2 ReliefParallaxMapping(vec3 dir, vec2 uvs) {
	const float ParallaxScale = 0.008;// * fract(shrd_data.uTranspParamsAndTime.w);
	
	const float MinLayers = 4.0;
	const float MaxLayers = 32.0;
	float layer_count = mix(MaxLayers, MinLayers, clamp(abs(dir.z), 0.0, 1.0));
	
	float layer_height = 1.0 / layer_count;
	float cur_layer_height = 1.0 - tex_height;
	
	vec2 duvs = ParallaxScale * dir.xy / dir.z / layer_count;
	vec2 cur_uvs = uvs;
	
	float height = texture(bump_texture, cur_uvs).r;
	
	while (height > cur_layer_height) {
		cur_layer_height += layer_height;
		cur_uvs += duvs;
		height = texture(bump_texture, cur_uvs).r;
	}
	
	duvs = 0.5 * duvs;
	layer_height = 0.5 * layer_height;
	
	cur_uvs -= duvs;
	cur_layer_height -= layer_height;
	
	const int BinSearchInterations = 8;
	for (int i = 0; i < BinSearchInterations; i++) {
		duvs = 0.5 * duvs;
		layer_height = 0.5 * layer_height;
		height = texture(bump_texture, cur_uvs).r;
		if (height > cur_layer_height) {
			cur_uvs += duvs;
			cur_layer_height += layer_height;
		} else {
			cur_uvs -= duvs;
			cur_layer_height -= layer_height;
		}
	}
	
	return cur_uvs;
}

vec2 ParallaxOcclusionMapping(vec3 dir, vec2 uvs) {
	const float ParallaxScale = 0.008;
	
	const float MinLayers = 4.0;
	const float MaxLayers = 16.0;
	float layer_count = mix(MaxLayers, MinLayers, clamp(abs(dir.z), 0.0, 1.0));
	
	float layer_height = 1.0 / layer_count;
	float cur_layer_height = 0.0;//1.0 - tex_height;
	
	vec2 duvs = ParallaxScale * dir.xy / dir.z / layer_count;
	vec2 cur_uvs = uvs;
	
	float height = texture(bump_texture, cur_uvs).r;
	
	while (height > cur_layer_height) {
		cur_layer_height += layer_height;
		cur_uvs += duvs;
		height = texture(bump_texture, cur_uvs).r;
	}
	
	vec2 prev_uvs = cur_uvs - duvs;
	
	float next_height = height - cur_layer_height;
	float prev_height = texture(bump_texture, prev_uvs).r - cur_layer_height + layer_height;
	
	float weight = next_height / (next_height - prev_height);
	vec2 final_uvs = mix(cur_uvs, prev_uvs, weight);
	
	return final_uvs;
}

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
										  
	vec3 view_ray_ws = normalize(shrd_data.uCamPosAndGamma.xyz - aVertexPos_);
	vec3 view_ray_ts = vec3(
		dot(view_ray_ws, cross(aVertexTangent_, aVertexNormal_)),
		-dot(view_ray_ws, aVertexTangent_),
		dot(view_ray_ws, aVertexNormal_)
	);
    //vec2 modified_uvs = aVertexUVs_;//ReliefParallaxMapping(view_ray_ts, aVertexUVs_);
	vec2 modified_uvs = ParallaxOcclusionMapping(view_ray_ts, aVertexUVs_);
	
    vec3 albedo_color = texture(diffuse_texture, modified_uvs).rgb;
    
    vec2 duv_dx = dFdx(modified_uvs), duv_dy = dFdy(modified_uvs);
    vec3 normal_color = texture(normals_texture, modified_uvs).wyz;
    vec4 specular_color = texture(specular_texture, modified_uvs);
    
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
                albedo_color = mix(albedo_color, SRGBToLinear(decal_diff.rgb), decal_influence);
            }
            
            vec4 norm_uvs_tr = texelFetch(decals_buffer, di * 6 + 4);
            
            if (norm_uvs_tr.z > 0.0) {
                vec2 norm_uvs = norm_uvs_tr.xy + norm_uvs_tr.zw * uvs;
                
                vec2 _duv_dx = 2.0 * norm_uvs_tr.zw * duv_dx;
                vec2 _duv_dy = 2.0 * norm_uvs_tr.zw * duv_dy;
            
                vec3 decal_norm = textureGrad(decals_texture, norm_uvs, _duv_dx, _duv_dy).wyz;
                normal_color = mix(normal_color, decal_norm, decal_influence);
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
    normal = normalize(mat3(cross(aVertexTangent_, aVertexNormal_), aVertexTangent_,
                            aVertexNormal_) * normal);
    
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
            if (shadowreg_index != -1) {
                vec4 reg_tr = shrd_data.uShadowMapRegions[shadowreg_index].transform;
                
                highp vec4 pp =
                    shrd_data.uShadowMapRegions[shadowreg_index].clip_from_world * vec4(aVertexPos_, 1.0);
                pp /= pp.w;
                pp.xyz = pp.xyz * 0.5 + vec3(0.5);
                pp.xy = reg_tr.xy + pp.xy * reg_tr.zw;
                
                atten *= SampleShadowPCF5x5(shadow_texture, pp.xyz);
            }
            
            additional_light += col_and_index.xyz * atten *
                smoothstep(dir_and_spot.w, dir_and_spot.w + 0.2, _dot2);
        }
    }
    
    vec3 indirect_col = vec3(0.0);
    float total_fade = 0.0;
    
    for (uint i = offset_and_lcount.x; i < offset_and_lcount.x + dcount_and_pcount.y; i++) {
        highp uint item_data = texelFetch(items_buffer, int(i)).x;
        int pi = int(bitfieldExtract(item_data, 24, 8));
        
        float dist = distance(shrd_data.uProbes[pi].pos_and_radius.xyz, aVertexPos_);
        float fade = 1.0 - smoothstep(0.9, 1.0, dist / shrd_data.uProbes[pi].pos_and_radius.w);
        
        indirect_col += fade * EvalSHIrradiance_NonLinear(normal,
                                                          shrd_data.uProbes[pi].sh_coeffs[0],
                                                          shrd_data.uProbes[pi].sh_coeffs[1],
                                                          shrd_data.uProbes[pi].sh_coeffs[2]);
        total_fade += fade;
    }
    
    indirect_col /= max(total_fade, 1.0);
    indirect_col = max(1.0 * indirect_col, vec3(0.0));
    
    float lambert = clamp(dot(normal, shrd_data.uSunDir.xyz), 0.0, 1.0);
    float visibility = 0.0;
    if (lambert > 0.00001) {
        visibility = GetSunVisibility(lin_depth, shadow_texture, aVertexShUVs_);
    }
    
    vec2 ao_uvs = vec2(ix, iy) / shrd_data.uResAndFRes.zw;
    float ambient_occlusion = textureLod(ao_texture, ao_uvs, 0.0).r;
    vec3 diffuse_color = albedo_color * (shrd_data.uSunCol.xyz * lambert * visibility +
                                         ambient_occlusion * ambient_occlusion * indirect_col +
                                         additional_light);
    
    float N_dot_V = clamp(dot(normal, view_ray_ws), 0.0, 1.0);
    
    vec3 kD = 1.0 - FresnelSchlickRoughness(N_dot_V, specular_color.rgb, specular_color.a);

    outColor = vec4(diffuse_color * kD, 1.0);
    outNormal = vec4(normal * 0.5 + 0.5, 1.0);
    outSpecular = specular_color;
	
	//vec3 view_vec_rec = normalize(mat3(cross(aVertexTangent_, aVertexNormal_), aVertexTangent_,
	//							  aVertexNormal_) * view_ray_ts);
	//outColor.rgb = 0.0001 * outColor.rgb + 100.0 * abs(view_vec_rec - view_ray_ws);
}
