#ifndef GI_CACHE_COMMON_GLSL
#define GI_CACHE_COMMON_GLSL

// Based on https://github.com/NVIDIAGameWorks/RTXGI-DDGI/

float sign_not_zero(const float v) {
    return (v >= 0.0) ? 1.0 : -1.0;
}

vec2 sign_not_zero(const vec2 v) {
    return vec2(sign_not_zero(v.x), sign_not_zero(v.y));
}

ivec3 get_ray_data_coords(const int ray_index, const int probe_index) {
    ivec3 coords;
    coords.x = ray_index;
    coords.z = probe_index / (PROBE_VOLUME_RES * PROBE_VOLUME_RES);
    coords.y = probe_index - (coords.z * PROBE_VOLUME_RES * PROBE_VOLUME_RES);
    return coords;
}

ivec3 get_probe_coords(const int probe_index) {
    return ivec3(probe_index % PROBE_VOLUME_RES,
                 probe_index / (PROBE_VOLUME_RES * PROBE_VOLUME_RES),
                 (probe_index / PROBE_VOLUME_RES) % PROBE_VOLUME_RES);
}

ivec3 get_probe_texel_coords(const int probe_index) {
    const int plane_index = probe_index / (PROBE_VOLUME_RES * PROBE_VOLUME_RES);

    const int x = (probe_index % PROBE_VOLUME_RES);
    const int y = (probe_index / PROBE_VOLUME_RES) % PROBE_VOLUME_RES;

    return ivec3(x, y, plane_index);
}

ivec3 get_probe_texel_coords(const int probe_index, const int volume_index) {
    const int plane_index = probe_index / (PROBE_VOLUME_RES * PROBE_VOLUME_RES);

    const int x = (probe_index % PROBE_VOLUME_RES);
    const int y = (probe_index / PROBE_VOLUME_RES) % PROBE_VOLUME_RES;

    return ivec3(x, y, plane_index + volume_index * PROBE_VOLUME_RES);
}

int get_probe_index(const ivec3 coords) {
    const int probe_index_in_plane = coords.x + (coords.z * PROBE_VOLUME_RES);
    return (coords.y * PROBE_VOLUME_RES * PROBE_VOLUME_RES) + probe_index_in_plane;
}

int get_probe_index(const ivec3 coords, const int texel_res) {
    const int probe_index_in_plane = (coords.x / texel_res) + (PROBE_VOLUME_RES * (coords.y / texel_res));
    return (coords.z * PROBE_VOLUME_RES * PROBE_VOLUME_RES) + probe_index_in_plane;
}

int get_scrolling_probe_index(const ivec3 coords, const ivec3 offset) {
    return get_probe_index((coords + offset + PROBE_VOLUME_RES) % PROBE_VOLUME_RES);
}

vec3 get_probe_pos_ws(const ivec3 coords, const ivec3 offset, const vec3 grid_origin, const vec3 grid_spacing) {
    const vec3 probe_grid_pos_ws = vec3(coords) * grid_spacing;
    const vec3 probe_grid_shift = (grid_spacing * float(PROBE_VOLUME_RES - 1)) * 0.5;

    vec3 probe_pos_ws = (probe_grid_pos_ws - probe_grid_shift);
    probe_pos_ws += grid_origin + vec3(offset) * grid_spacing;

    return probe_pos_ws;
}

vec3 get_probe_pos_ws(const ivec3 coords, const ivec3 offset, const vec3 grid_origin, const vec3 grid_spacing, sampler2DArray offset_tex) {
    const vec3 pos_ws = get_probe_pos_ws(coords, offset, grid_origin, grid_spacing);

    const int probe_index = get_scrolling_probe_index(coords, offset);
    const ivec3 tex_coords = get_probe_texel_coords(probe_index);

    return pos_ws + texelFetch(offset_tex, tex_coords, 0).xyz;
}

vec3 get_probe_pos_ws(const int volume_index, const ivec3 coords, const ivec3 offset, const vec3 grid_origin, const vec3 grid_spacing, sampler2DArray offset_tex) {
    const vec3 pos_ws = get_probe_pos_ws(coords, offset, grid_origin, grid_spacing);

    const int probe_index = get_scrolling_probe_index(coords, offset);
    const ivec3 tex_coords = get_probe_texel_coords(probe_index, volume_index);

    return pos_ws + texelFetch(offset_tex, tex_coords, 0).xyz;
}

vec3 spherical_fibonacci(const float sample_index, const float sample_count) {
    const float b = (sqrt(5.0) * 0.5 + 0.5) - 1.0;
    const float phi = 2.0 * M_PI * fract(sample_index * b);
    const float cos_theta = 1.0 - (2.0 * sample_index + 1.0) / sample_count;
    const float sin_theta = sqrt(saturate(1.0 - (cos_theta * cos_theta)));
    return vec3(cos(phi) * sin_theta, sin(phi) * sin_theta, cos_theta);
}

vec3 quat_rotate(const vec3 v, const vec4 q) {
    const vec3 b = q.xyz;
    const float b2 = dot(b, b);
    return (v * (q.w * q.w - b2) + b * (dot(v, b) * 2.0) + cross(b, v) * (q.w * 2.0));
}

vec3 get_probe_ray_dir(const int ray_index, vec4 rot_quat) {
    const bool is_fixed_ray = (ray_index < PROBE_FIXED_RAYS_COUNT);
    const int rays_count = is_fixed_ray ? PROBE_FIXED_RAYS_COUNT : (PROBE_TOTAL_RAYS_COUNT - PROBE_FIXED_RAYS_COUNT);
    const int sample_index = is_fixed_ray ? ray_index : (ray_index - PROBE_FIXED_RAYS_COUNT);

    vec3 dir = spherical_fibonacci(float(sample_index), float(rays_count));
    if (!is_fixed_ray) {
        dir = quat_rotate(dir, rot_quat);
    }
    return normalize(dir);
}

vec2 get_normalized_oct_coords(const ivec2 tex_coords, const int texel_res) {
    // Map 2D texture coordinates to a normalized octahedral space
    vec2 ret = vec2(tex_coords.x % (texel_res - 2), tex_coords.y % (texel_res - 2));

    // Move to the center of a texel
    ret.xy += 0.5;

    // Normalize
    ret.xy /= float(texel_res - 2);

    // Shift to [-1, 1);
    ret *= 2.0;
    ret -= 1.0;

    return ret;
}

vec3 get_oct_dir(const vec2 coords) {
    vec3 direction = vec3(coords.x, coords.y, 1.0 - abs(coords.x) - abs(coords.y));
    if (direction.z < 0.0) {
        direction.xy = (1.0 - abs(direction.yx)) * sign_not_zero(direction.xy);
    }
    return normalize(direction);
}

vec2 get_oct_coords(const vec3 direction) {
    float l1norm = abs(direction.x) + abs(direction.y) + abs(direction.z);
    vec2 uv = direction.xy * (1.0 / l1norm);
    if (direction.z < 0.0) {
        uv = (1.0 - abs(uv.yx)) * sign_not_zero(uv.xy);
    }
    return uv;
}

vec3 get_probe_uv(const int probe_index, const vec2 octant_coords, const int numProbeInteriorTexels) {
    // Get the probe's texel coordinates, assuming one texel per probe
    const ivec3 coords = get_probe_texel_coords(probe_index);

    // Add the border texels to get the total texels per probe
    float numProbeTexels = (numProbeInteriorTexels + 2.0);

    float textureWidth = numProbeTexels * PROBE_VOLUME_RES;
    float textureHeight = numProbeTexels * PROBE_VOLUME_RES;

    // Move to the center of the probe and move to the octant texel before normalizing
    vec2 uv = vec2(coords.x * numProbeTexels, coords.y * numProbeTexels) + (numProbeTexels * 0.5);
    uv += octant_coords.xy * float(numProbeInteriorTexels) * 0.5;
    uv /= vec2(textureWidth, textureHeight);
    return vec3(uv, coords.z);
}

vec3 get_probe_uv(const int probe_index, const int volume_index, const vec2 octant_coords, const int numProbeInteriorTexels) {
    // Get the probe's texel coordinates, assuming one texel per probe
    const ivec3 coords = get_probe_texel_coords(probe_index, volume_index);

    // Add the border texels to get the total texels per probe
    float numProbeTexels = (numProbeInteriorTexels + 2.0);

    float textureWidth = numProbeTexels * PROBE_VOLUME_RES;
    float textureHeight = numProbeTexels * PROBE_VOLUME_RES;

    // Move to the center of the probe and move to the octant texel before normalizing
    vec2 uv = vec2(coords.x * numProbeTexels, coords.y * numProbeTexels) + (numProbeTexels * 0.5);
    uv += octant_coords.xy * float(numProbeInteriorTexels) * 0.5;
    uv /= vec2(textureWidth, textureHeight);
    return vec3(uv, coords.z);
}

ivec3 get_base_probe_grid_coords(const vec3 world_position, const ivec3 offset, const vec3 grid_origin, const vec3 grid_spacing) {
    // Get the vector from the volume origin to the surface point
    vec3 position = world_position - (grid_origin + (vec3(offset) * grid_spacing));

    // Shift from [-n/2, n/2] to [0, n] (grid space)
    position += (grid_spacing * (PROBE_VOLUME_RES - 1)) * 0.5;

    // Quantize the position to grid space
    ivec3 probeCoords = ivec3(position / grid_spacing);

    // Clamp to [0, probeCounts - 1]
    // Snaps positions outside of grid to the grid edge
    probeCoords = clamp(probeCoords, ivec3(0), ivec3(PROBE_VOLUME_RES) - 1);

    return probeCoords;
}

float get_volume_blend_weight(vec3 world_position, const ivec3 offset, const vec3 grid_origin, const vec3 grid_spacing) {
    // Get the volume's origin and extent
    const vec3 origin = grid_origin + vec3(offset) * grid_spacing;
    const vec3 extent = (grid_spacing * float(PROBE_VOLUME_RES - 1)) * 0.5;

    // Get the delta between the (rotated volume) and the world-space position
    const vec3 position = abs(world_position - origin);
    //position = abs(RTXGIQuaternionRotate(position, RTXGIQuaternionConjugate(volume.rotation)));

    const vec3 delta = position - extent;
    if(all(lessThan(delta, vec3(0.0)))) {
        return 1.0;
    }

    // Adjust the blend weight for each axis
    float weight = 1.0;
    weight *= (1.0 - saturate(delta.x / grid_spacing.x));
    weight *= (1.0 - saturate(delta.y / grid_spacing.y));
    weight *= (1.0 - saturate(delta.z / grid_spacing.z));

    return weight;
}

bool IsScrollingPlaneProbe(const int probe_index, const ivec3 grid_scroll, const ivec3 grid_scroll_diff) {
    const ivec3 probe_coords = get_probe_coords(probe_index);

    // TODO: Simplify this!
    ivec3 test_coord = ivec3(-1);
    for (int i = 0; i < 3; ++i) {
        if (grid_scroll_diff[i] > 0) {
            test_coord[i] = (PROBE_VOLUME_RES + (grid_scroll[i] - 1)) % PROBE_VOLUME_RES;
        } else if (grid_scroll_diff[i] < 0) {
            test_coord[i] = (PROBE_VOLUME_RES + (grid_scroll[i] % PROBE_VOLUME_RES)) % PROBE_VOLUME_RES;
        }
    }

    return any(equal(test_coord, probe_coords));
}

vec3 get_volume_irradiance(const int volume_index, sampler2DArray irradiance_tex, sampler2DArray distance_tex, sampler2DArray offset_tex,
                           const vec3 world_position, const vec3 surface_bias, const vec3 direction,
                           const ivec3 grid_scroll, const vec3 grid_origin, const vec3 grid_spacing) {
    // Bias the world space position
    const vec3 biased_world_position = (world_position + surface_bias);

    // Get the 3D grid coordinates of the probe nearest the biased world position (i.e. the "base" probe)
    const ivec3 base_probe_coords = get_base_probe_grid_coords(biased_world_position, grid_scroll, grid_origin, grid_spacing);

    // Get the world-space position of the base probe (ignore relocation)
    const vec3 base_probe_world_position = get_probe_pos_ws(base_probe_coords, grid_scroll, grid_origin, grid_spacing);

    // Clamp the distance (in grid space) between the given point and the base probe's world position (on each axis) to [0, 1]
    const vec3 grid_space_distance = (biased_world_position - base_probe_world_position);

    const vec3 alpha = clamp(grid_space_distance / grid_spacing, vec3(0.0), vec3(1.0));

    vec3 irradiance = vec3(0.0);
    float total_weight = 0.0;

    // Iterate over the 8 closest probes and accumulate their contributions
    for (int i = 0; i < 8; ++i) {
        // Compute the offset to the adjacent probe in grid coordinates by
        // sourcing the offsets from the bits of the loop index: x = bit 0, y = bit 1, z = bit 2
        const ivec3 adjacent_probe_offset = ivec3(i, i >> 1, i >> 2) & 1;

        // Get the 3D grid coordinates of the adjacent probe by adding the offset to
        // the base probe and clamping to the grid boundaries
        const ivec3 adjacent_probe_coords = clamp(base_probe_coords + adjacent_probe_offset, ivec3(0), ivec3(PROBE_VOLUME_RES) - 1);

        // Get the adjacent probe's index, adjusting the adjacent probe index for scrolling offsets (if present)
        const int adjacent_probe_index = get_scrolling_probe_index(adjacent_probe_coords, grid_scroll);

        // Early Out: don 't allow inactive probes to contribute to irradiance
        const ivec3 adj_texel_coords = get_probe_texel_coords(adjacent_probe_index, volume_index);
        if (texelFetch(offset_tex, adj_texel_coords, 0).w < 0.5) {
            continue;
        }

        // Get the adjacent probe's world position
        const vec3 adjacent_probe_world_position = get_probe_pos_ws(volume_index, adjacent_probe_coords, grid_scroll, grid_origin, grid_spacing, offset_tex);

        // Compute the distance and direction from the (biased and non-biased) shading point and the adjacent probe
        const vec3 world_pos_to_adj_probe = normalize(adjacent_probe_world_position - world_position);
        const vec3 biased_pos_to_adj_probe = normalize(adjacent_probe_world_position - biased_world_position);
        const float biased_pos_to_adj_probe_dist = length(adjacent_probe_world_position - biased_world_position);

        // Compute trilinear weights based on the distance to each adjacent probe
        // to smoothly transition between probes. adjacent_probe_offset is binary, so we're
        // using a 1-alpha when adjacent_probe_offset = 0 and alpha when adjacent_probe_offset = 1.
        const vec3 trilinear = max(vec3(0.001), mix(1.0 - alpha, alpha, vec3(adjacent_probe_offset)));
        const float trilinear_weight = (trilinear.x * trilinear.y * trilinear.z);
        float weight = 1.0;

        // A naive soft backface weight would ignore a probe when
        // it is behind the surface. That's good for walls, but for
        // small details inside of a room, the normals on the details
        // might rule out all of the probes that have mutual visibility
        // to the point. We instead use a "wrap shading" test. The small
        // offset at the end reduces the "going to zero" impact.
        float wrap_shading = (dot(world_pos_to_adj_probe, direction) + 1.0) * 0.5;
        weight *= (wrap_shading * wrap_shading) + 0.2;

        // Compute the octahedral coordinates of the adjacent probe
        vec2 octant_coords = get_oct_coords(-biased_pos_to_adj_probe);

        // Get the texture array coordinates for the octant of the probe
        vec3 probe_texture_uv = get_probe_uv(adjacent_probe_index, volume_index, octant_coords, PROBE_DISTANCE_RES - 2);

        // Sample the probe's distance texture to get the mean distance to nearby surfaces
        const vec2 filtered_distance = 2.0 * textureLod(distance_tex, probe_texture_uv, 0.0).rg;

        float chebyshev_weight = 1.0;

        // Occlusion test
        if(biased_pos_to_adj_probe_dist > filtered_distance.x) {
            // Find the variance of the mean distance
            const float variance = abs(filtered_distance.x * filtered_distance.x - filtered_distance.y);

            // v must be greater than 0, which is guaranteed by the if condition above.
            const float v = biased_pos_to_adj_probe_dist - filtered_distance.x;
            chebyshev_weight = variance / (variance + v * v);

            // Increase the contrast in the weight
            chebyshev_weight = clamp(chebyshev_weight * chebyshev_weight * chebyshev_weight, 0.0, 1.0);
        }

        // Avoid visibility weights ever going all the way to zero because
        // when *no* probe has visibility we need a fallback value
        weight *= max(0.05, chebyshev_weight);

        // Avoid a weight of zero
        weight = max(0.000001, weight);

        // A small amount of light is visible due to logarithmic perception, so
        // crush tiny weights but keep the curve continuous
        const float CrushThreshold = 0.2;
        if (weight < CrushThreshold) {
            weight *= (weight * weight) / (CrushThreshold * CrushThreshold);
        }

        // Apply the trilinear weights
        weight *= trilinear_weight;

        // Get the octahedral coordinates for the sample direction
        octant_coords = get_oct_coords(direction);

        // Get the probe's texture coordinates
        probe_texture_uv = get_probe_uv(adjacent_probe_index, volume_index, octant_coords, PROBE_IRRADIANCE_RES - 2);

        // Sample the probe's irradiance
        const vec3 probe_irradiance = textureLod(irradiance_tex, probe_texture_uv, 0.0).rgb;

        // Decode the tone curve, but leave a gamma = 2 curve to approximate sRGB blending
        //vec3 exponent = volume.probeIrradianceEncodingGamma * 0.5;
        //probe_irradiance = pow(probe_irradiance, exponent);

        // Accumulate the weighted irradiance
        irradiance += (weight * probe_irradiance);
        total_weight += weight;
    }

    if (total_weight == 0.0) {
        return vec3(0.0);
    }

    irradiance *= (1.0 / total_weight);   // Normalize by the accumulated weights
    //irradiance *= irradiance;                   // Go back to linear irradiance
    irradiance *= 2.0 * M_PI;                   // Multiply by the area of the integration domain (hemisphere) to complete the Monte Carlo Estimator equation

    // Adjust for energy loss due to reduced precision in the R10G10B10A2 irradiance texture format
    //if (volume.probeIrradianceFormat == RTXGI_DDGI_VOLUME_TEXTURE_FORMAT_U32) {
    //    irradiance *= 1.0989f;
    //}

    return irradiance;
}

vec3 get_volume_irradiance_sep(const int volume_index, sampler2DArray irradiance_tex, sampler2DArray distance_tex, sampler2DArray offset_tex,
                               const vec3 world_position, const vec3 surface_bias, const vec3 direction,
                               const ivec3 grid_scroll, const vec3 grid_origin, const vec3 grid_spacing) {
    // Bias the world space position
    const vec3 biased_world_position = (world_position + surface_bias);

    // Get the 3D grid coordinates of the probe nearest the biased world position (i.e. the "base" probe)
    const ivec3 base_probe_coords = get_base_probe_grid_coords(biased_world_position, grid_scroll, grid_origin, grid_spacing);

    // Get the world-space position of the base probe (ignore relocation)
    const vec3 base_probe_world_position = get_probe_pos_ws(base_probe_coords, grid_scroll, grid_origin, grid_spacing);

    // Clamp the distance (in grid space) between the given point and the base probe's world position (on each axis) to [0, 1]
    const vec3 grid_space_distance = (biased_world_position - base_probe_world_position);

    const vec3 alpha = clamp(grid_space_distance / grid_spacing, vec3(0.0), vec3(1.0));

    vec3 indoor_irradiance = vec3(0.0), outdoor_irradiance = vec3(0.0);
    float indoor_weight = 0.0, outdoor_weight = 0.0;

    // Iterate over the 8 closest probes and accumulate their contributions
    for (int i = 0; i < 8; ++i) {
        // Compute the offset to the adjacent probe in grid coordinates by
        // sourcing the offsets from the bits of the loop index: x = bit 0, y = bit 1, z = bit 2
        const ivec3 adjacent_probe_offset = ivec3(i, i >> 1, i >> 2) & 1;

        // Get the 3D grid coordinates of the adjacent probe by adding the offset to
        // the base probe and clamping to the grid boundaries
        const ivec3 adjacent_probe_coords = clamp(base_probe_coords + adjacent_probe_offset, ivec3(0), ivec3(PROBE_VOLUME_RES) - 1);

        // Get the adjacent probe's index, adjusting the adjacent probe index for scrolling offsets (if present)
        const int adjacent_probe_index = get_scrolling_probe_index(adjacent_probe_coords, grid_scroll);

        // Early Out: don 't allow inactive probes to contribute to irradiance
        const ivec3 adj_texel_coords = get_probe_texel_coords(adjacent_probe_index, volume_index);
        const float probe_state = texelFetch(offset_tex, adj_texel_coords, 0).w;
        if (probe_state < 0.5) {
            continue;
        }

        // Get the adjacent probe's world position
        const vec3 adjacent_probe_world_position = get_probe_pos_ws(volume_index, adjacent_probe_coords, grid_scroll, grid_origin, grid_spacing, offset_tex);

        // Compute the distance and direction from the (biased and non-biased) shading point and the adjacent probe
        const vec3 world_pos_to_adj_probe = normalize(adjacent_probe_world_position - world_position);
        const vec3 biased_pos_to_adj_probe = normalize(adjacent_probe_world_position - biased_world_position);
        const float biased_pos_to_adj_probe_dist = length(adjacent_probe_world_position - biased_world_position);

        // Compute trilinear weights based on the distance to each adjacent probe
        // to smoothly transition between probes. adjacent_probe_offset is binary, so we're
        // using a 1-alpha when adjacent_probe_offset = 0 and alpha when adjacent_probe_offset = 1.
        const vec3 trilinear = max(vec3(0.001), mix(1.0 - alpha, alpha, vec3(adjacent_probe_offset)));
        const float trilinear_weight = (trilinear.x * trilinear.y * trilinear.z);
        float weight = 1.0;

        // A naive soft backface weight would ignore a probe when
        // it is behind the surface. That's good for walls, but for
        // small details inside of a room, the normals on the details
        // might rule out all of the probes that have mutual visibility
        // to the point. We instead use a "wrap shading" test. The small
        // offset at the end reduces the "going to zero" impact.
        float wrap_shading = (dot(world_pos_to_adj_probe, direction) + 1.0) * 0.5;
        weight *= (wrap_shading * wrap_shading) + 0.2;

        // Compute the octahedral coordinates of the adjacent probe
        vec2 octant_coords = get_oct_coords(-biased_pos_to_adj_probe);

        // Get the texture array coordinates for the octant of the probe
        vec3 probe_texture_uv = get_probe_uv(adjacent_probe_index, volume_index, octant_coords, PROBE_DISTANCE_RES - 2);

        // Sample the probe's distance texture to get the mean distance to nearby surfaces
        const vec2 filtered_distance = 2.0 * textureLod(distance_tex, probe_texture_uv, 0.0).rg;

        float chebyshev_weight = 1.0;

        // Occlusion test
        if(biased_pos_to_adj_probe_dist > filtered_distance.x) {
            // Find the variance of the mean distance
            const float variance = abs(filtered_distance.x * filtered_distance.x - filtered_distance.y);

            // v must be greater than 0, which is guaranteed by the if condition above.
            const float v = biased_pos_to_adj_probe_dist - filtered_distance.x;
            chebyshev_weight = variance / (variance + v * v);

            // Increase the contrast in the weight
            chebyshev_weight = clamp(chebyshev_weight * chebyshev_weight * chebyshev_weight, 0.0, 1.0);
        }

        // Avoid visibility weights ever going all the way to zero because
        // when *no* probe has visibility we need a fallback value
        weight *= max(0.05, chebyshev_weight);

        // Avoid a weight of zero
        weight = max(0.000001, weight);

        // A small amount of light is visible due to logarithmic perception, so
        // crush tiny weights but keep the curve continuous
        const float CrushThreshold = 0.2;
        if (weight < CrushThreshold) {
            weight *= (weight * weight) / (CrushThreshold * CrushThreshold);
        }

        // Apply the trilinear weights
        weight *= trilinear_weight;

        // Get the octahedral coordinates for the sample direction
        octant_coords = get_oct_coords(direction);

        // Get the probe's texture coordinates
        probe_texture_uv = get_probe_uv(adjacent_probe_index, volume_index, octant_coords, PROBE_IRRADIANCE_RES - 2);

        // Sample the probe's irradiance
        const vec3 probe_irradiance = textureLod(irradiance_tex, probe_texture_uv, 0.0).rgb;

        // Decode the tone curve, but leave a gamma = 2 curve to approximate sRGB blending
        //vec3 exponent = volume.probeIrradianceEncodingGamma * 0.5;
        //probe_irradiance = pow(probe_irradiance, exponent);

        // Accumulate the weighted irradiance
        if (probe_state < 1.5) {
            indoor_irradiance += (weight * probe_irradiance);
            indoor_weight += weight;
        } else {
            outdoor_irradiance += (weight * probe_irradiance);
            outdoor_weight += weight;
        }
    }

    if (max(indoor_weight, outdoor_weight) == 0.0) {
        return vec3(0.0);
    }

    vec3 irradiance = (outdoor_weight > indoor_weight) ? (outdoor_irradiance / outdoor_weight) : (indoor_irradiance / indoor_weight);

    irradiance *= 2.0 * M_PI;   // Multiply by the area of the integration domain (hemisphere) to complete the Monte Carlo Estimator equation

    return irradiance;
}

vec3 get_surface_bias(const vec3 normal, const vec3 view, const vec3 grid_spacing) {
    return ((normal * 0.1) + (-view * 0.2)) * grid_spacing;
}

vec3 get_surface_bias(const vec3 view, const vec3 grid_spacing) {
    return (-view * 0.3) * grid_spacing;
}

#endif // GI_CACHE_COMMON_GLSL