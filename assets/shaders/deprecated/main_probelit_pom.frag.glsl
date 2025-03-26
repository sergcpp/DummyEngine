#version 430 core
//#extension GL_EXT_control_flow_attributes : enable
#if !defined(VULKAN)
#extension GL_ARB_bindless_texture : enable
#endif

#include "../internal/_fs_common.glsl"
#include "../internal/texturing_common.glsl"

#define LIGHT_ATTEN_CUTOFF 0.004

#if !defined(BINDLESS_TEXTURES)
layout(binding = BIND_MAT_TEX0) uniform sampler2D g_diff_tex;
layout(binding = BIND_MAT_TEX1) uniform sampler2D g_norm_tex;
layout(binding = BIND_MAT_TEX2) uniform sampler2D g_spec_tex;
layout(binding = BIND_MAT_TEX3) uniform sampler2D g_mat3_tex;
#endif // BINDLESS_TEXTURES
layout(binding = BIND_SHAD_TEX) uniform sampler2DShadow g_shadow_tex;
layout(binding = BIND_DECAL_TEX) uniform sampler2D g_decals_tex;
layout(binding = BIND_SSAO_TEX_SLOT) uniform sampler2D g_ao_tex;
layout(binding = BIND_ENV_TEX) uniform samplerCubeArray g_env_tex;
layout(binding = BIND_LIGHT_BUF) uniform samplerBuffer g_lights_buf;
layout(binding = BIND_DECAL_BUF) uniform samplerBuffer g_decals_buf;
layout(binding = BIND_CELLS_BUF) uniform usamplerBuffer g_cells_buf;
layout(binding = BIND_ITEMS_BUF) uniform usamplerBuffer g_items_buf;

layout (binding = BIND_UB_SHARED_DATA_BUF, std140) uniform SharedDataBlock {
    shared_data_t g_shrd_data;
};

layout(location = 0) in vec3 g_vtx_pos;
layout(location = 1) in vec2 g_vtx_uvs;
layout(location = 2) in vec3 g_vtx_normal;
layout(location = 3) in vec3 g_vtx_tangent;
layout(location = 4) in vec3 g_vtx_sh_uvs[4];
#if defined(BINDLESS_TEXTURES)
    layout(location = 8) in flat TEX_HANDLE g_diff_tex;
    layout(location = 9) in flat TEX_HANDLE g_norm_tex;
    layout(location = 10) in flat TEX_HANDLE g_spec_tex;
    layout(location = 11) in flat TEX_HANDLE g_mat3_tex;
#endif // BINDLESS_TEXTURES

layout(location = LOC_OUT_COLOR) out vec4 g_out_color;
layout(location = LOC_OUT_NORM) out vec4 g_out_normal;
layout(location = LOC_OUT_SPEC) out vec4 g_out_specular;

vec2 ParallaxMapping(vec3 dir, vec2 uvs) {
    const float ParallaxScale = 0.01;

    const float MinLayers = 4.0;
    const float MaxLayers = 32.0;
    float layer_count = mix(MinLayers, MaxLayers, abs(dir.z));

    float layer_height = 1.0 / layer_count;
    float cur_layer_height = 0.0;

    vec2 duvs = ParallaxScale * dir.xy / dir.z / layer_count;
    vec2 cur_uvs = uvs;

    float height = texture(SAMPLER2D(g_mat3_tex), cur_uvs).x;

    while (height > cur_layer_height) {
        cur_layer_height += layer_height;
        cur_uvs -= duvs;
        height = texture(SAMPLER2D(g_mat3_tex), cur_uvs).x;
    }

    return cur_uvs;
}

vec2 ReliefParallaxMapping(vec3 dir, vec2 uvs) {
    const float ParallaxScale = 0.25;//0.008;

    const float MinLayers = 64.0;
    const float MaxLayers = 64.0;
    float layer_count = mix(MaxLayers, MinLayers, clamp(abs(dir.z), 0.0, 1.0));

    float layer_height = 1.0 / layer_count;
    float cur_layer_height = 1.0;

    vec2 duvs = ParallaxScale * dir.xy / dir.z / layer_count;
    vec2 cur_uvs = uvs;

    float height = texture(SAMPLER2D(g_mat3_tex), cur_uvs).x;

    while (height < cur_layer_height) {
        cur_layer_height -= layer_height;
        cur_uvs -= duvs;
        height = texture(SAMPLER2D(g_mat3_tex), cur_uvs).x;
    }

    duvs = 0.5 * duvs;
    layer_height = 0.5 * layer_height;

    cur_uvs += duvs;
    cur_layer_height += layer_height;

    const int BinSearchInterations = 5;
    for (int i = 0; i < BinSearchInterations; i++) {
        duvs = 0.5 * duvs;
        layer_height = 0.5 * layer_height;
        height = texture(SAMPLER2D(g_mat3_tex), cur_uvs).x;
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

vec2 ParallaxOcclusionMapping(vec3 dir, vec2 uvs, out float iterations) {
    const float MinLayers = 32.0;
    const float MaxLayers = 256.0;
    float layer_count = mix(MaxLayers, MinLayers, clamp(abs(dir.z), 0.0, 1.0));

    float layer_height = 1.0 / layer_count;
    float cur_layer_height = 1.0;

    vec2 duvs = dir.xy / dir.z / layer_count;
    vec2 cur_uvs = uvs;

    float height = 1.0 - texture(SAMPLER2D(g_mat3_tex), cur_uvs).y;

    while (height < cur_layer_height) {
        cur_layer_height -= layer_height;
        cur_uvs += duvs;
        height = 1.0 - texture(SAMPLER2D(g_mat3_tex), cur_uvs).y;
    }

    vec2 prev_uvs = cur_uvs - duvs;

    float next_height = height - cur_layer_height;
    float prev_height = 1.0 - texture(SAMPLER2D(g_mat3_tex), prev_uvs).y - cur_layer_height - layer_height;

    float weight = next_height / (next_height - prev_height);
    vec2 final_uvs = mix(cur_uvs, prev_uvs, weight);

    iterations = layer_count;
    return final_uvs;
}

vec2 ConeSteppingExact(vec3 dir, vec2 uvs) {
    ivec2 tex_size = textureSize(SAMPLER2D(g_mat3_tex), 0);
    float w = 1.0 / float(max(tex_size.x, tex_size.y));

    float iz = sqrt(1.0 - clamp(dir.z * dir.z, 0.0, 1.0));

    vec2 h = textureLod(SAMPLER2D(g_mat3_tex), uvs, 0.0).xy;
    h.y = max(h.y, 1.0/255.0);

    int counter = 0;

    float t = 0.0;
    while (1.0 - dir.z * t > h.x) {
        t += w + (1.0 - dir.z * t - h.x) / (dir.z + iz / (h.y * h.y));
        h = textureLod(SAMPLER2D(g_mat3_tex), uvs - t * dir.xy, 0.0).xy;
        h.y = max(h.y, 1.0/255.0);

        counter += 1;
        if (counter > 1000) {
            //discard;
            break;
        }
    }

    t -= w;

    return uvs - t * dir.xy;
}

vec2 ConeSteppingFixed(vec3 dir, vec2 uvs) {
    float iz = sqrt(1.0 - clamp(dir.z * dir.z, 0.0, 1.0));

    vec2 h = texture(SAMPLER2D(g_mat3_tex), uvs).xy;
    float t = (1.0 - h.x) / (dir.z + iz / (h.y * h.y));

    // repeate 4 times
    h = texture(SAMPLER2D(g_mat3_tex), uvs - t * dir.xy).xy;
    t += (1.0 - dir.z * t - h.x) / (dir.z + iz / (h.y * h.y));
    h = texture(SAMPLER2D(g_mat3_tex), uvs - t * dir.xy).xy;
    t += (1.0 - dir.z * t - h.x) / (dir.z + iz / (h.y * h.y));
    h = texture(SAMPLER2D(g_mat3_tex), uvs - t * dir.xy).xy;
    t += (1.0 - dir.z * t - h.x) / (dir.z + iz / (h.y * h.y));
    h = texture(SAMPLER2D(g_mat3_tex), uvs - t * dir.xy).xy;
    t += (1.0 - dir.z * t - h.x) / (dir.z + iz / (h.y * h.y));

    // and 5 more times
    h = texture(SAMPLER2D(g_mat3_tex), uvs - t * dir.xy).xy;
    t += (1.0 - dir.z * t - h.x) / (dir.z + iz / (h.y * h.y));
    h = texture(SAMPLER2D(g_mat3_tex), uvs - t * dir.xy).xy;
    t += (1.0 - dir.z * t - h.x) / (dir.z + iz / (h.y * h.y));
    h = texture(SAMPLER2D(g_mat3_tex), uvs - t * dir.xy).xy;
    t += (1.0 - dir.z * t - h.x) / (dir.z + iz / (h.y * h.y));
    h = texture(SAMPLER2D(g_mat3_tex), uvs - t * dir.xy).xy;
    t += (1.0 - dir.z * t - h.x) / (dir.z + iz / (h.y * h.y));
    h = texture(SAMPLER2D(g_mat3_tex), uvs - t * dir.xy).xy;
    t += (1.0 - dir.z * t - h.x) / (dir.z + iz / (h.y * h.y));

    return uvs - t * dir.xy;
}

vec2 ConeSteppingLoop(vec3 dir, vec2 uvs) {
    float iz = sqrt(1.0 - clamp(dir.z * dir.z, 0.0, 1.0));

    const float MinLayers = 64.0;
    const float MaxLayers = 64.0;
    int steps_count = int(mix(MinLayers, MaxLayers, iz));

    float t = 0.0;

    for (int i = 0; i < steps_count; i++) {
        vec2 h = textureLod(SAMPLER2D(g_mat3_tex), uvs - t * dir.xy, 0.0).xy;
        t += (1.0 - dir.z * t - h.x) / (dir.z + iz / (h.y * h.y));
    }

    return uvs - t * dir.xy;
}

vec2 ConeSteppingLoop32(vec3 dir, vec2 uvs) {
    float iz = sqrt(1.0 - clamp(dir.z * dir.z, 0.0, 1.0));

    const float MinLayers = 32.0;
    const float MaxLayers = 32.0;
    int steps_count = int(mix(MinLayers, MaxLayers, iz));

    float t = 0.0;

    for (int i = 0; i < steps_count; i++) {
        vec2 h = textureLod(SAMPLER2D(g_mat3_tex), uvs - t * dir.xy, 0.0).xy;
        t += (1.0 - dir.z * t - h.x) / (dir.z + iz / (h.y * h.y));
    }

    return uvs - t * dir.xy;
}

vec2 ConeSteppingRelaxed(vec3 dir, vec2 uvs) {
    const int ConeSteps = 15;
    const int BinarySteps = 8;

    dir.xy *= -1.0;
    dir /= dir.z;
    float ray_ratio = length(dir.xy);

    vec3 pos = vec3(uvs, 0.0);
    for (int i = 0; i < ConeSteps; i++) {
        vec2 h = textureLod(SAMPLER2D(g_mat3_tex), pos.xy, 0.0).xy;
        float height = clamp(h.x - pos.z, 0.0, 1.0);
        float d = h.y * height / (ray_ratio + h.y);
        pos += dir * d;
    }

    vec3 bs_range = 0.5 * dir * pos.z;
    vec3 bs_pos = pos - bs_range;

    for (int i = 0; i < BinarySteps; i++) {
        vec2 h = textureLod(SAMPLER2D(g_mat3_tex), bs_pos.xy, 0.0).xy;
        bs_range *= 0.5;
        if (bs_pos.z < h.x) {
            bs_pos += bs_range;
        } else {
            bs_pos -= bs_range;
        }
    }

    if (gl_FragCoord.x < 1920.0 / 2.0) {
        return pos.xy;
    } else {
        return bs_pos.xy;
    }
}

#define DISP_MAX_ITER 256
#define DISP_MAX_MIP 12.0

vec2 QuadTreeDisplacement(vec3 dir, vec2 uvs, out float iterations) {
    // max mip level of texture itself
    float max_level = float(textureQueryLevels(SAMPLER2D(g_mat3_tex))) - 1.0;
    // max mip level that we will access
    float lim_level = min(max_level - textureQueryLod(SAMPLER2D(g_mat3_tex), uvs).x - 1.0, DISP_MAX_MIP);
    float lim_lod = max(textureQueryLod(SAMPLER2D(g_mat3_tex), uvs).x, max(max_level - DISP_MAX_MIP, 0.0));

    vec2 cursor = uvs;
    vec2 start_point = uvs;
    // defines which planes pair of pixel's bounding box will be checked for intersection
    vec2 quadrant = vec2(0.5) + 0.5 * sign(dir.xy);
    vec2 tex_size = vec2(textureSize(SAMPLER2D(g_mat3_tex), int(max_level - lim_level)));
    float delta = 0.5 / tex_size.x;

    // defines forward/backward step for approximate bilinear interpolation of height map
    vec2 temp = abs(delta / dir.xy);
    float adv = min(temp.x, temp.y);

    float lod = max_level;
    float t_cursor = 0.0;

    // keep track of current resolution (it is faster than calling textureSize every iteration)
    vec2 cur_tex_size = vec2(textureSize(SAMPLER2D(g_mat3_tex), int(max_level)));

    int iter = 0;
    while (iter++ < DISP_MAX_ITER) {
        // check if we reached bottom mip level
        if (lod <= lim_lod) {
            // advance forward by a half of a pixel
            vec3 next_ray_pos = vec3(start_point, 0.0) + dir * (t_cursor + adv);
            float next_height = textureLod(SAMPLER2D(g_mat3_tex), next_ray_pos.xy, lim_lod).y - next_ray_pos.z;
            // check if we intersect interpolated height map
            if (next_height <= 0.0) {
                // step backward by a half of a pixel
                vec3 prev_ray_pos = vec3(start_point, 0.0) + dir * (t_cursor - adv);
                float prev_height = textureLod(SAMPLER2D(g_mat3_tex), prev_ray_pos.xy, lim_lod).y - prev_ray_pos.z;
                // compute interpolation factor
                float weight = prev_height / (prev_height - next_height);
                // final cursor position at intersection point
                cursor = mix(prev_ray_pos.xy, next_ray_pos.xy, weight);
                break;
            }
        }

#if 0
        // fetch max bump map height at current level (manually because nearest sampling is required)
        ivec2 icursor = ivec2(fract(vec2(1.0) + fract(cursor)) * cur_tex_size);
        float max_height = texelFetch(SAMPLER2D(g_mat3_tex), icursor, int(lod)).y;
#else
        // snap cursor to pixel's center to emulate nearest sampling
        vec2 snapped_cursor = (vec2(0.5) + floor(cursor * cur_tex_size)) / cur_tex_size;
        float max_height = textureLod(SAMPLER2D(g_mat3_tex), snapped_cursor, lod).y;
#endif
        // intersection of ray with z-plane of pixel's bounding box
        float t = max_height / dir.z;
        vec2 bound = floor(cursor * cur_tex_size + quadrant);
        // intersection of ray with xy-planes of pixel's bounding box
        vec2 t_bound = (bound / cur_tex_size - start_point) / dir.xy;
        float t_min = min(t_bound.x, t_bound.y);
        t_cursor = max(t_cursor + 1e-04, min(t, t_min + delta));
        cursor = start_point + dir.xy * t_cursor;
        // we either jump inside of current tile or skip it
        bool expand_tile = (t < t_min + delta) && (lod > lim_lod);
        // adjust current lod and resolution
        lod += (expand_tile ? -1.0 : +1.0);
        cur_tex_size *= (expand_tile ? 2.0 : 0.5);
    }

    iterations = float(iter);
    return cursor;
}

mat3 CotangentFrame_Fast(vec3 normal, vec3 position, vec2 uv) {
    // get edge vectors of the pixel triangle
    vec3 dp1 = dFdx(position);
    vec3 dp2 = dFdy(position);
    vec2 duv1 = dFdx(uv);
    vec2 duv2 = dFdy(uv);

    // solve the linear system
    vec3 dp2perp = cross(dp2, normal);
    vec3 dp1perp = cross(normal, dp1);
    vec3 T = dp2perp * duv1.x + dp1perp * duv2.x;
    vec3 B = dp2perp * duv1.y + dp1perp * duv2.y;

    // construct a scale-invariant frame
    float invmax = inversesqrt(max(dot(T, T), dot(B, B)));
    return mat3(T * invmax, B * invmax, normal);
}

mat3 CotangentFrame_Precise(vec3 normal, vec3 position, vec2 uv) {
    // get edge vectors of the pixel triangle
    vec3 dp1 = dFdx(position);
    vec3 dp2 = dFdy(position);
    vec2 duv1 = dFdx(uv);
    vec2 duv2 = dFdy(uv);

    vec3 N = normalize(normal);
    vec3 T = normalize(dp2 * duv1.y - dp1 * duv2.y);
    vec3 B = normalize(cross(T, N));

    return mat3(T, B, N);
}

void main() {
    float lin_depth = LinearizeDepth(gl_FragCoord.z, g_shrd_data.clip_info);
    float k = log2(lin_depth / g_shrd_data.clip_info[1]) / g_shrd_data.clip_info[3];
    int slice = clamp(int(k * float(ITEM_GRID_RES_Z)), 0, ITEM_GRID_RES_Z - 1);

    int ix = int(gl_FragCoord.x), iy = int(gl_FragCoord.y);
    int cell_index = GetCellIndex(ix, iy, slice, g_shrd_data.res_and_fres.xy);

    uvec2 cell_data = texelFetch(g_cells_buf, cell_index).xy;
    uvec2 offset_and_lcount = uvec2(bitfieldExtract(cell_data.x, 0, 24),
                                    bitfieldExtract(cell_data.x, 24, 8));
    uvec2 dcount_and_pcount = uvec2(bitfieldExtract(cell_data.y, 0, 8),
                                    bitfieldExtract(cell_data.y, 8, 8));

    vec3 view_ray_ws = normalize(g_shrd_data.cam_pos_and_exp.xyz - g_vtx_pos);

    mat3 basis = mat3(cross(g_vtx_tangent, g_vtx_normal), -g_vtx_tangent, g_vtx_normal);
    /*mat3 basis;
    if (gl_FragCoord.x < 500.0) {
        basis = CotangentFrame_Fast(g_vtx_normal, g_vtx_pos, g_vtx_uvs);
    } else {
        basis = CotangentFrame_Precise(g_vtx_normal, g_vtx_pos, g_vtx_uvs);
    }*/

    vec3 view_ray_ts = view_ray_ws * basis;

    vec2 modified_uvs;

    const float ParallaxDepth = 0.25;//0.008;

    float iterations = 0.0;
    vec3 _view_ray_ts = normalize(vec3(-view_ray_ts.xy, view_ray_ts.z / ParallaxDepth));

    //modified_uvs = ConeSteppingExact(normalize(vec3(view_ray_ts.xy, view_ray_ts.z / ParallaxDepth)), g_vtx_uvs);
    //modified_uvs = ConeSteppingLoop(normalize(vec3(view_ray_ts.xy, view_ray_ts.z / ParallaxDepth)), g_vtx_uvs);
    //modified_uvs = ConeSteppingRelaxed(normalize(vec3(view_ray_ts.xy, view_ray_ts.z / ParallaxDepth)), g_vtx_uvs);
    //modified_uvs = ParallaxOcclusionMapping(_view_ray_ts, g_vtx_uvs, iterations);
    //modified_uvs = ReliefParallaxMapping(view_ray_ts, g_vtx_uvs);
    modified_uvs = QuadTreeDisplacement(_view_ray_ts, g_vtx_uvs, iterations);

    /*if (gl_FragCoord.x < 1.0 * (1920.0 / 4.0)) {
        modified_uvs = ParallaxOcclusionMapping(view_ray_ts, g_vtx_uvs);
    } else if (gl_FragCoord.x < 2.0 * (1920.0 / 4.0)) {
        modified_uvs = ReliefParallaxMapping(view_ray_ts, g_vtx_uvs);
    } else if (gl_FragCoord.x < 3.0 * (1920.0 / 4.0)) {
        modified_uvs = ConeSteppingLoop(normalize(vec3(view_ray_ts.xy, view_ray_ts.z / ParallaxDepth)), g_vtx_uvs);
    } else {
        modified_uvs = ConeSteppingLoop32(normalize(vec3(view_ray_ts.xy, view_ray_ts.z / ParallaxDepth)), g_vtx_uvs);
    }*/

    /*if (gl_FragCoord.x < (1920.0 / 2.0)) {
        modified_uvs = ParallaxOcclusionMapping(view_ray_ts, g_vtx_uvs);
    } else {
        modified_uvs = ReliefParallaxMapping(view_ray_ts, g_vtx_uvs);
    }*/

    //modified_uvs = ConeSteppingFixed(view_ray_ts, g_vtx_uvs);


    vec3 albedo_color = SRGBToLinear(YCoCg_to_RGB(texture(SAMPLER2D(g_diff_tex), modified_uvs)));

    vec2 duv_dx = dFdx(g_vtx_uvs), duv_dy = dFdy(g_vtx_uvs);
    vec3 normal_color = texture(SAMPLER2D(g_norm_tex), modified_uvs).wyz;
    vec4 spec_color = texture(SAMPLER2D(g_spec_tex), g_vtx_uvs);

    vec3 dp_dx = dFdx(g_vtx_pos);
    vec3 dp_dy = dFdy(g_vtx_pos);

    for (uint i = offset_and_lcount.x; i < offset_and_lcount.x + dcount_and_pcount.x; i++) {
        uint item_data = texelFetch(g_items_buf, int(i)).x;
        int di = int(bitfieldExtract(item_data, 12, 12));

        mat4 de_proj;
        de_proj[0] = texelFetch(g_decals_buf, di * DECALS_BUF_STRIDE + 0);
        de_proj[1] = texelFetch(g_decals_buf, di * DECALS_BUF_STRIDE + 1);
        de_proj[2] = texelFetch(g_decals_buf, di * DECALS_BUF_STRIDE + 2);
        de_proj[3] = vec4(0.0, 0.0, 0.0, 1.0);
        de_proj = transpose(de_proj);

        vec4 pp = de_proj * vec4(g_vtx_pos, 1.0);
        pp /= pp[3];

        vec3 app = abs(pp.xyz);
        vec2 uvs = pp.xy * 0.5 + 0.5;

        vec2 duv_dx = 0.5 * (de_proj * vec4(dp_dx, 0.0)).xy;
        vec2 duv_dy = 0.5 * (de_proj * vec4(dp_dy, 0.0)).xy;

        if (app.x < 1.0 && app.y < 1.0 && app.z < 1.0) {
            float decal_influence = 1.0;
            vec4 mask_uvs_tr = texelFetch(g_decals_buf, di * DECALS_BUF_STRIDE + 3);
            if (mask_uvs_tr.z > 0.0) {
                vec2 mask_uvs = mask_uvs_tr.xy + mask_uvs_tr.zw * uvs;
                decal_influence = textureGrad(g_decals_tex, mask_uvs, mask_uvs_tr.zw * duv_dx, mask_uvs_tr.zw * duv_dy).x;
            }

            vec4 diff_uvs_tr = texelFetch(g_decals_buf, di * DECALS_BUF_STRIDE + 4);
            if (diff_uvs_tr.z > 0.0) {
                vec2 diff_uvs = diff_uvs_tr.xy + diff_uvs_tr.zw * uvs;
                vec3 decal_diff = YCoCg_to_RGB(textureGrad(g_decals_tex, diff_uvs, diff_uvs_tr.zw * duv_dx, diff_uvs_tr.zw * duv_dy));
                albedo_color = mix(albedo_color, SRGBToLinear(decal_diff), decal_influence);
            }

            vec4 norm_uvs_tr = texelFetch(g_decals_buf, di * DECALS_BUF_STRIDE + 5);
            if (norm_uvs_tr.z > 0.0) {
                vec2 norm_uvs = norm_uvs_tr.xy + norm_uvs_tr.zw * uvs;
                vec3 decal_norm = textureGrad(g_decals_tex, norm_uvs, norm_uvs_tr.zw * duv_dx, norm_uvs_tr.zw * duv_dy).wyz;
                normal_color = mix(normal_color, decal_norm, decal_influence);
            }

            vec4 spec_uvs_tr = texelFetch(g_decals_buf, di * DECALS_BUF_STRIDE + 6);
            if (spec_uvs_tr.z > 0.0) {
                vec2 spec_uvs = spec_uvs_tr.xy + spec_uvs_tr.zw * uvs;
                vec4 decal_spec = textureGrad(g_decals_tex, spec_uvs, spec_uvs_tr.zw * duv_dx, spec_uvs_tr.zw * duv_dy);
                spec_color = mix(spec_color, decal_spec, decal_influence);
            }
        }
    }

    vec3 normal = normal_color * 2.0 - 1.0;
    normal = normalize(mat3(cross(g_vtx_tangent, g_vtx_normal), g_vtx_tangent,
                            g_vtx_normal) * normal);

    vec3 additional_light = vec3(0.0, 0.0, 0.0);

    for (uint i = offset_and_lcount.x; i < offset_and_lcount.x + offset_and_lcount.y; i++) {
        uint item_data = texelFetch(g_items_buf, int(i)).x;
        int li = int(bitfieldExtract(item_data, 0, 12));

        vec4 pos_and_radius = texelFetch(g_lights_buf, li * LIGHTS_BUF_STRIDE + 0);
        vec4 col_and_index = texelFetch(g_lights_buf, li * LIGHTS_BUF_STRIDE + 1);
        vec4 dir_and_spot = texelFetch(g_lights_buf, li * LIGHTS_BUF_STRIDE + 2);

        vec3 L = pos_and_radius.xyz - g_vtx_pos;
        float dist = length(L);
        float d = max(dist - pos_and_radius.w, 0.0);
        L /= dist;

        float denom = d / pos_and_radius.w + 1.0;
        float atten = 1.0 / (denom * denom);

        float brightness = max(col_and_index.x, max(col_and_index.y, col_and_index.z));

        float factor = LIGHT_ATTEN_CUTOFF / brightness;
        atten = (atten - factor) / (1.0 - LIGHT_ATTEN_CUTOFF);
        atten = max(atten, 0.0);

        float _dot1 = clamp(dot(L, normal), 0.0, 1.0);
        float _dot2 = dot(L, dir_and_spot.xyz);

        atten = _dot1 * atten;
        if (_dot2 > dir_and_spot.w && (brightness * atten) > FLT_EPS) {
            int shadowreg_index = floatBitsToInt(col_and_index.w);
            if (shadowreg_index != -1) {
                vec4 reg_tr = g_shrd_data.shadowmap_regions[shadowreg_index].transform;

                vec4 pp = g_shrd_data.shadowmap_regions[shadowreg_index].clip_from_world * vec4(g_vtx_pos, 1.0);
                pp /= pp.w;

                pp.xy = pp.xy * 0.5 + 0.5;
                pp.xy = reg_tr.xy + pp.xy * reg_tr.zw;
#if defined(VULKAN)
                pp.y = 1.0 - pp.y;
#endif // VULKAN
                atten *= SampleShadowPCF5x5(g_shadow_tex, pp.xyz);
            }

            additional_light += col_and_index.xyz * atten *
                                smoothstep(dir_and_spot.w, dir_and_spot.w + 0.2, _dot2);
        }
    }

    vec3 indirect_col = vec3(0.0);
    float total_fade = 0.0;

    for (uint i = offset_and_lcount.x; i < offset_and_lcount.x + dcount_and_pcount.y; i++) {
        uint item_data = texelFetch(g_items_buf, int(i)).x;
        int pi = int(bitfieldExtract(item_data, 24, 8));

        float dist = distance(g_shrd_data.probes[pi].pos_and_radius.xyz, g_vtx_pos);
        float fade = 1.0 - smoothstep(0.9, 1.0, dist / g_shrd_data.probes[pi].pos_and_radius.w);

        indirect_col += fade * EvalSHIrradiance_NonLinear(normal,
                                                          g_shrd_data.probes[pi].sh_coeffs[0],
                                                          g_shrd_data.probes[pi].sh_coeffs[1],
                                                          g_shrd_data.probes[pi].sh_coeffs[2]);
        total_fade += fade;
    }

    indirect_col /= max(total_fade, 1.0);
    indirect_col = max(indirect_col, vec3(0.0));

    float lambert = clamp(dot(normal, g_shrd_data.sun_dir.xyz), 0.0, 1.0);
    float visibility = 0.0;
    if (lambert > 0.00001) {
        visibility = GetSunVisibility(lin_depth, g_shadow_tex, g_vtx_sh_uvs);
    }

    vec2 ao_uvs = (vec2(ix, iy) + 0.5) / g_shrd_data.res_and_fres.zw;
    float ambient_occlusion = textureLod(g_ao_tex, ao_uvs, 0.0).x;
    vec3 base_color = albedo_color * (g_shrd_data.sun_col.xyz * lambert * visibility +
                                         ambient_occlusion * ambient_occlusion * indirect_col +
                                         additional_light);


    float N_dot_V = clamp(dot(normal, view_ray_ws), 0.0, 1.0);

    vec3 kD = 1.0 - FresnelSchlickRoughness(N_dot_V, spec_color.xyz, spec_color.a);

    g_out_color = vec4(base_color * kD, 1.0);
    g_out_normal = PackNormalAndRoughness(normal, spec_color.w);
    g_out_specular = spec_color;
}
