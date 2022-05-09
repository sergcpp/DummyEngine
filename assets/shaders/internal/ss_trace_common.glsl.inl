#ifndef Z_THICKNESS
    #define Z_THICKNESS 0.02
#endif
#ifndef MAX_STEPS
    #define MAX_STEPS 256
#endif
#ifndef MOST_DETAILED_MIP
    #define MOST_DETAILED_MIP 0
#endif
#ifndef LEAST_DETAILED_MIP
    #define LEAST_DETAILED_MIP 6
#endif

#define FLOAT_MAX 3.402823466e+38

//
// https://github.com/GPUOpen-Effects/FidelityFX-SSSR
//
bool IntersectRay(vec3 ray_origin_ss, vec3 ray_origin_vs, vec3 ray_dir_vs, out vec3 out_hit_point) {
    vec4 ray_offsetet_ss = g_shrd_data.proj_matrix * vec4(ray_origin_vs + ray_dir_vs, 1.0);
    ray_offsetet_ss.xyz /= ray_offsetet_ss.w;

#if defined(VULKAN)
    ray_offsetet_ss.y = -ray_offsetet_ss.y;
    ray_offsetet_ss.xy = 0.5 * ray_offsetet_ss.xy + 0.5;
#else // VULKAN
    ray_offsetet_ss.xyz = 0.5 * ray_offsetet_ss.xyz + 0.5;
#endif // VULKAN

    vec3 ray_dir_ss = normalize(ray_offsetet_ss.xyz - ray_origin_ss);
    vec3 ray_dir_ss_inv = mix(1.0 / ray_dir_ss, vec3(FLOAT_MAX), equal(ray_dir_ss, vec3(0.0)));

    int cur_mip = MOST_DETAILED_MIP;

    vec2 cur_mip_res = g_shrd_data.res_and_fres.xy / exp2(MOST_DETAILED_MIP);
    vec2 cur_mip_res_inv = 1.0 / cur_mip_res;

    vec2 uv_offset = 0.005 * exp2(MOST_DETAILED_MIP) / g_shrd_data.res_and_fres.xy;
    uv_offset = mix(uv_offset, -uv_offset, lessThan(ray_dir_ss.xy, vec2(0.0)));

    vec2 floor_offset = mix(vec2(1.0), vec2(0.0), lessThan(ray_dir_ss.xy, vec2(0.0)));

    float cur_t;
    vec3 cur_pos_ss;

    { // advance ray to avoid self intersection
        vec2 cur_mip_pos = cur_mip_res * ray_origin_ss.xy;

        vec2 xy_plane = floor(cur_mip_pos) + floor_offset;
        xy_plane = xy_plane * cur_mip_res_inv + uv_offset;

        vec2 t = (xy_plane - ray_origin_ss.xy) * ray_dir_ss_inv.xy;
        cur_t = min(t.x, t.y);
        cur_pos_ss = ray_origin_ss.xyz + cur_t * ray_dir_ss;
    }

    int iter = 0;
    while (iter++ < MAX_STEPS && cur_mip >= MOST_DETAILED_MIP) {
        vec2 cur_pos_px = cur_mip_res * cur_pos_ss.xy;
        float surf_z = texelFetch(g_depth_texture, clamp(ivec2(cur_pos_px), ivec2(0), ivec2(cur_mip_res - 1)), cur_mip).r;
        bool increment_mip = cur_mip < LEAST_DETAILED_MIP;

        { // advance ray
            vec2 xy_plane = floor(cur_pos_px) + floor_offset;
            xy_plane = xy_plane * cur_mip_res_inv + uv_offset;
            vec3 boundary_planes = vec3(xy_plane, surf_z);
            // o + d * t = p' => t = (p' - o) / d
            vec3 t = (boundary_planes - ray_origin_ss.xyz) * ray_dir_ss_inv;

            t.z = (ray_dir_ss.z > 0.0) ? t.z : FLOAT_MAX;

            // choose nearest intersection
            float t_min = min(min(t.x, t.y), t.z);

            bool is_above_surface = surf_z > cur_pos_ss.z;

            increment_mip = increment_mip && (t_min != t.z) && is_above_surface;

            cur_t = is_above_surface ? t_min : cur_t;
            cur_pos_ss = ray_origin_ss.xyz + cur_t * ray_dir_ss;
        }

        cur_mip += increment_mip ? 1 : -1;
        cur_mip_res *= increment_mip ? 0.5 : 2.0;
        cur_mip_res_inv *= increment_mip ? 2.0 : 0.5;
    }

    if (iter > MAX_STEPS) {
        // Intersection was not found
        return false;
    }

    // Reject out-of-view hits
    if (any(lessThan(cur_pos_ss.xy, vec2(0.0))) || any(greaterThan(cur_pos_ss.xy, vec2(1.0)))) {
        return false;
    }

    // Reject if we hit surface from the back
    vec3 hit_normal_ws = UnpackNormalAndRoughness(textureLod(g_norm_texture, cur_pos_ss.xy, 0.0)).xyz;
    vec3 hit_normal_vs = (g_shrd_data.view_matrix * vec4(hit_normal_ws, 0.0)).xyz;
    if (dot(hit_normal_vs, ray_dir_vs) > 0.0) {
        return false;
    }

    vec3 hit_point_cs = cur_pos_ss;
#if defined(VULKAN)
    hit_point_cs.xy = 2.0 * hit_point_cs.xy - 1.0;
    hit_point_cs.y = -hit_point_cs.y;
#else // VULKAN
    hit_point_cs.xyz = 2.0 * hit_point_cs.xyz - 1.0;
#endif // VULKAN

    out_hit_point = hit_point_cs.xyz;

    vec4 hit_point_vs = g_shrd_data.inv_proj_matrix * vec4(hit_point_cs, 1.0);
    hit_point_vs.xyz /= hit_point_vs.w;

    float hit_depth_fetch = texelFetch(g_depth_texture, ivec2(cur_pos_ss.xy * g_params.resolution.xy), 0).r;
    vec4 hit_surf_cs = vec4(hit_point_cs.xy, hit_depth_fetch, 1.0);
#if !defined(VULKAN)
    hit_surf_cs.z = 2.0 * hit_surf_cs.z - 1.0;
#endif // VULKAN

    vec4 hit_surf_vs = g_shrd_data.inv_proj_matrix * hit_surf_cs;
    hit_surf_vs.xyz /= hit_surf_vs.w;
    float dist_vs = distance(hit_point_vs.xyz, hit_surf_vs.xyz);

    return dist_vs < Z_THICKNESS;
}