#ifndef MAX_TRACE_DIST
    #define MAX_TRACE_DIST 100
#endif
#ifndef MAX_TRACE_STEPS
    #define MAX_TRACE_STEPS 16
#endif
#ifndef Z_THICKNESS
    #define Z_THICKNESS 0.05
#endif
#ifndef STRIDE
    #define STRIDE 0.0125
#endif
#ifndef BSEARCH_STEPS
    #define BSEARCH_STEPS 0
#endif

float distance2(vec2 P0, vec2 P1) {
    vec2 d = P1 - P0;
    return d.x * d.x + d.y * d.y;
}

//
// From "Efficient GPU Screen-Space Ray Tracing"
//
bool IntersectRay(vec3 ray_origin_vs, vec3 ray_dir_vs, float jitter, out vec2 hit_pixel, out vec3 hit_point) {
    // Clip ray length to camera near plane
    float ray_length = (ray_origin_vs.z + ray_dir_vs.z * MAX_TRACE_DIST) > - g_shrd_data.clip_info[1] ?
                       (-ray_origin_vs.z - g_shrd_data.clip_info[1]) / ray_dir_vs.z :
                       MAX_TRACE_DIST;

    vec3 ray_end_vs = ray_origin_vs + ray_length * ray_dir_vs;

    // Project into screen space
    vec4 H0 = g_shrd_data.clip_from_view * vec4(ray_origin_vs, 1.0),
         H1 = g_shrd_data.clip_from_view * vec4(ray_end_vs, 1.0);
    float k0 = 1.0 / H0.w, k1 = 1.0 / H1.w;

#if defined(VULKAN)
    H0.y = -H0.y;
    H1.y = -H1.y;
#endif // VULKAN

    vec3 Q0 = ray_origin_vs * k0, Q1 = ray_end_vs * k1;

    // Screen-space endpoints
    vec2 P0 = H0.xy * k0, P1 = H1.xy * k1;

    //P1 += vec2((distance2(P0, P1) < 0.0001) ? 0.0001 : 0.0);

    P0 = 0.5 * P0 + 0.5;
    P1 = 0.5 * P1 + 0.5;

    P0 *= g_shrd_data.res_and_fres.xy;
    P1 *= g_shrd_data.res_and_fres.xy;

    vec2 delta = P1 - P0;

    bool permute = false;
    if (abs(delta.x) < abs(delta.y)) {
        permute = true;
        delta = delta.yx;
        P0 = P0.yx;
        P1 = P1.yx;
    }

    if (abs(delta.x) < 1.0) {
        return false;
    }

    float step_dir = sign(delta.x);
    float inv_dx = step_dir / delta.x;
    vec2 dP = vec2(step_dir, delta.y * inv_dx);

    vec3 dQ = (Q1 - Q0) * inv_dx;
    float dk = (k1 - k0) * inv_dx;

#ifdef STRIDE_PX
    float stride = STRIDE_PX;
#else
    float stride = STRIDE * g_shrd_data.res_and_fres.x;
#endif
    dP *= stride;
    dQ *= stride;
    dk *= stride;

    P0 += dP * (1.0 + jitter);
    Q0 += dQ * (1.0 + jitter);
    k0 += dk * (1.0 + jitter);

    vec3 Q = Q0;
    float k = k0;
    float step_count = 0.0;
    float end = P1.x * step_dir;
    float prev_zmax_estimate = ray_origin_vs.z + 0.0 * Z_THICKNESS;
    hit_pixel = vec2(-1.0, -1.0);

    for (vec2 P = P0;
        ((P.x * step_dir) <= end) && (step_count < MAX_TRACE_STEPS);
         P += dP, Q.z += dQ.z, k += dk, step_count += 1.0) {

        float ray_zmin = Q.z / k;
        //prev_zmax_estimate;
        // take half of step forward
        float ray_zmax = (dQ.z * 0.5 + Q.z) / (dk * 0.5 + k);
        prev_zmax_estimate = ray_zmax;

        //if (ray_zmin > ray_zmax) {
        //    const float temp = ray_zmin; ray_zmin = ray_zmax; ray_zmax = temp;
        //}

        const vec2 pixel = permute ? P.yx : P;

        const float scene_zmax = -LinearDepthTexelFetch(pixel);
        const float scene_zmin = scene_zmax - Z_THICKNESS;

        if (/*(ray_zmax >= scene_zmin) &&*/ (ray_zmin <= scene_zmax)) {
            hit_pixel = P;
            break;
        }
    }

    const vec2 test_pixel = permute ? hit_pixel.yx : hit_pixel;
    bool res = all(lessThanEqual(abs(test_pixel - (g_shrd_data.res_and_fres.xy * 0.5)), g_shrd_data.res_and_fres.xy * 0.5));

#if BSEARCH_STEPS != 0
    if (res) {
        Q.xy += dQ.xy * step_count;

        // perform binary search to find intersection more accurately
        for (int i = 0; i < BSEARCH_STEPS; i++) {
            vec2 pixel = permute ? hit_pixel.yx : hit_pixel;
            float scene_z = -LinearDepthTexelFetch(ivec2(pixel));
            float ray_z = Q.z / k;

            float depth_diff = ray_z - scene_z;

            dQ *= 0.5;
            dP *= 0.5;
            dk *= 0.5;
            if (depth_diff > 0.0) {
                Q += dQ;
                hit_pixel += dP;
                k += dk;
            } else {
                Q -= dQ;
                hit_pixel -= dP;
                k -= dk;
            }
        }

        hit_pixel = permute ? hit_pixel.yx : hit_pixel;
        hit_point = Q * (1.0 / k);
    }
#else
    Q.xy += dQ.xy * step_count;

    hit_pixel = permute ? hit_pixel.yx : hit_pixel;
    hit_point = Q * (1.0 / k);
#endif

    { // validate hit
        const float scene_zmax = -LinearDepthTexelFetch(hit_pixel);
        const float scene_zmin = scene_zmax - Z_THICKNESS;

        if ((hit_point.z < scene_zmin) || (hit_point.z > scene_zmax)) {
            res = false;
        }
    }

    return res;
}