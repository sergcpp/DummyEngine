#ifndef _PRINCIPLED_GLSL
#define _PRINCIPLED_GLSL

#include "_ltc.glsl"

float fresnel_dielectric_cos(float cosi, float eta) {
    // compute fresnel reflectance without explicitly computing the refracted direction
    float c = abs(cosi);
    float g = eta * eta - 1 + c * c;
    float result;

    if (g > 0) {
        g = sqrt(g);
        float A = (g - c) / (g + c);
        float B = (c * (g + c) - 1) / (c * (g - c) + 1);
        result = 0.5 * A * A * (1 + B * B);
    } else {
        result = 1.0; // TIR (no refracted component)
    }

    return result;
}

struct lobe_weights_t {
    float diffuse, specular, clearcoat, refraction;
    float diffuse_mul;
};

lobe_weights_t get_lobe_weights(float base_color_lum, float spec_color_lum, float specular,
                                float metallic, float transmission, float clearcoat) {
    // taken from Cycles
    lobe_weights_t ret;
    ret.diffuse_mul = (1.0 - metallic) * (1.0 - transmission);
    ret.diffuse = base_color_lum * ret.diffuse_mul;
    float final_transmission = transmission * (1.0 - metallic);
    ret.specular = (specular != 0.0 || metallic != 0.0) ? spec_color_lum * (1.0 - final_transmission) : 0.0;
    ret.clearcoat = 0.25 * clearcoat * (1.0 - metallic);
    ret.refraction = final_transmission * base_color_lum;

    float total_weight = ret.diffuse + ret.specular + ret.clearcoat + ret.refraction;
    if (total_weight != 0.0) {
        ret.diffuse /= total_weight;
        ret.specular /= total_weight;
        ret.clearcoat /= total_weight;
        ret.refraction /= total_weight;
    }

    return ret;
}

///////////////////////////////////////////////////////////////////////

int cubemap_face(vec3 dir) {
    if (abs(dir.x) >= abs(dir.y) && abs(dir.x) >= abs(dir.z)) {
        return dir.x > 0.0 ? 4 : 5;
    } else if (abs(dir.y) >= abs(dir.z)) {
        return dir.y > 0.0 ? 2 : 3;
    }
    return dir.z > 0.0 ? 0 : 1;
}

int cubemap_face(vec3 _dir, vec3 f, vec3 u, vec3 v) {
    vec3 dir = vec3(-dot(_dir, f), dot(_dir, u), dot(_dir, v));
    if (abs(dir.x) >= abs(dir.y) && abs(dir.x) >= abs(dir.z)) {
        return dir.x > 0.0 ? 4 : 5;
    } else if (abs(dir.y) >= abs(dir.z)) {
        return dir.y > 0.0 ? 2 : 3;
    }
    return dir.z > 0.0 ? 0 : 1;
}

struct ltc_params_t {
    vec4 diff_t1;
    vec2 diff_t2;
    vec4 sheen_t1;
    vec2 sheen_t2;
    vec4 spec_t1;
    vec2 spec_t2;
    vec4 coat_t1;
    vec2 coat_t2;
};

ltc_params_t SampleLTC_Params(sampler2D luts, float N_dot_V, float roughness, float clearcoat_roughness2) {
    const vec2 ltc_uv = LTC_Coords(N_dot_V, roughness);
    const vec2 coat_ltc_uv = LTC_Coords(N_dot_V, clearcoat_roughness2);

    ltc_params_t ret;
    ret.diff_t1 = textureLod(luts, ltc_uv + vec2(0.0, 0.0), 0.0);
    ret.diff_t2 = textureLod(luts, ltc_uv + vec2(0.125, 0.0), 0.0).xy;
    ret.sheen_t1 = textureLod(luts, ltc_uv + vec2(0.25, 0.0), 0.0);
    ret.sheen_t2 = textureLod(luts, ltc_uv + vec2(0.375, 0.0), 0.0).xy;
    ret.spec_t1 = textureLod(luts, ltc_uv + vec2(0.5, 0.0), 0.0);
    ret.spec_t2 = textureLod(luts, ltc_uv + vec2(0.625, 0.0), 0.0).xy;
    ret.coat_t1 = textureLod(luts, coat_ltc_uv + vec2(0.75, 0.0), 0.0);
    ret.coat_t2 = textureLod(luts, coat_ltc_uv + vec2(0.875, 0.0), 0.0).xy;

    return ret;
}

struct light_item_t {
    vec4 col_and_type;
    vec4 pos_and_radius;
    vec4 dir_and_spot;
    vec4 u_and_reg;
    vec4 v_and_blend;
};

#define ENABLE_SPHERE_LIGHT 1
#define ENABLE_RECT_LIGHT 1
#define ENABLE_DISK_LIGHT 1
#define ENABLE_LINE_LIGHT 1

#define ENABLE_DIFFUSE 1
#define ENABLE_SHEEN 1
#define ENABLE_SPECULAR 1
#define ENABLE_CLEARCOAT 1

vec3 EvaluateLightSource(light_item_t litem, vec3 pos_ws, vec3 I, vec3 N, lobe_weights_t lobe_weights, ltc_params_t ltc,
                         sampler2D ltc_luts, float sheen, vec3 base_color, vec3 sheen_color, vec3 spec_color, vec3 clearcoat_color) {
    const bool TwoSided = false;

    const uint type = floatBitsToUint(litem.col_and_type.w) & LIGHT_TYPE_BITS;
    const vec3 from_light = normalize(pos_ws - litem.pos_and_radius.xyz);
    const float _dot = -dot(from_light, litem.dir_and_spot.xyz);
    const float _angle = approx_acos(_dot);
    if (type != LIGHT_TYPE_LINE && _angle > litem.dir_and_spot.w) {
        return vec3(0.0);
    }

    vec3 ret = vec3(0.0);

    if (type == LIGHT_TYPE_SPHERE && ENABLE_SPHERE_LIGHT != 0) {
        // TODO: simplify this!
        vec3 u = normalize(I - from_light * dot(I, from_light));
        vec3 v = cross(from_light, u);

        u *= litem.pos_and_radius.w;
        v *= litem.pos_and_radius.w;

        vec3 points[4];
        points[0] = litem.pos_and_radius.xyz + u + v;
        points[1] = litem.pos_and_radius.xyz + u - v;
        points[2] = litem.pos_and_radius.xyz - u - v;
        points[3] = litem.pos_and_radius.xyz - u + v;

        if (lobe_weights.diffuse > 0.0 && ENABLE_DIFFUSE != 0) {
            vec3 dcol = base_color;

            vec3 diff = LTC_Evaluate_Disk(ltc_luts, 0.125, N, I, pos_ws.xyz, ltc.diff_t1, points, TwoSided);
            diff *= dcol * ltc.diff_t2.x;// + (1.0 - dcol) * ltc.diff_t2.y;

            if (sheen > 0.0 && ENABLE_SHEEN != 0) {
                vec3 _sheen = LTC_Evaluate_Disk(ltc_luts, 0.375, N, I, pos_ws.xyz, ltc.sheen_t1, points, TwoSided);
                diff += _sheen * (sheen_color * ltc.sheen_t2.x + (1.0 - sheen_color) * ltc.sheen_t2.y);
            }

            ret += lobe_weights.diffuse_mul * litem.col_and_type.xyz * diff / M_PI;
        }
        if (lobe_weights.specular > 0.0 && ENABLE_SPECULAR != 0) {
            vec3 scol = spec_color;

            vec3 spec = LTC_Evaluate_Disk(ltc_luts, 0.625, N, I, pos_ws.xyz, ltc.spec_t1, points, TwoSided);
            spec *= scol * ltc.spec_t2.x + (1.0 - scol) * ltc.spec_t2.y;

            ret += litem.col_and_type.xyz * spec / M_PI;
        }
        if (lobe_weights.clearcoat > 0.0 && ENABLE_CLEARCOAT != 0) {
            vec3 ccol = clearcoat_color;

            vec3 coat = LTC_Evaluate_Disk(ltc_luts, 0.875, N, I, pos_ws.xyz, ltc.coat_t1, points, TwoSided);
            coat *= ccol * ltc.coat_t2.x + (1.0 - ccol) * ltc.coat_t2.y;

            ret += 0.25 * litem.col_and_type.xyz * coat / M_PI;
        }
    } else if (type == LIGHT_TYPE_RECT && ENABLE_RECT_LIGHT != 0) {
        vec3 points[4];
        points[0] = litem.pos_and_radius.xyz + litem.u_and_reg.xyz + litem.v_and_blend.xyz;
        points[1] = litem.pos_and_radius.xyz + litem.u_and_reg.xyz - litem.v_and_blend.xyz;
        points[2] = litem.pos_and_radius.xyz - litem.u_and_reg.xyz - litem.v_and_blend.xyz;
        points[3] = litem.pos_and_radius.xyz - litem.u_and_reg.xyz + litem.v_and_blend.xyz;

        if (lobe_weights.diffuse > 0.0 && ENABLE_DIFFUSE != 0) {
            vec3 dcol = base_color;

            vec3 diff = LTC_Evaluate_Rect(ltc_luts, 0.125, N, I, pos_ws.xyz, ltc.diff_t1, points, TwoSided);
            diff *= dcol * ltc.diff_t2.x;// + (1.0 - dcol) * ltc.diff_t2.y;

            if (sheen > 0.0 && ENABLE_SHEEN != 0) {
                vec3 _sheen = LTC_Evaluate_Rect(ltc_luts, 0.375, N, I, pos_ws.xyz, ltc.sheen_t1, points, TwoSided);
                diff += _sheen * (sheen_color * ltc.sheen_t2.x + (1.0 - sheen_color) * ltc.sheen_t2.y);
            }

            ret += lobe_weights.diffuse_mul * litem.col_and_type.xyz * diff / 4.0;
        }
        if (lobe_weights.specular > 0.0 && ENABLE_SPECULAR != 0) {
            vec3 scol = spec_color;

            vec3 spec = LTC_Evaluate_Rect(ltc_luts, 0.625, N, I, pos_ws.xyz, ltc.spec_t1, points, TwoSided);
            spec *= scol * ltc.spec_t2.x + (1.0 - scol) * ltc.spec_t2.y;

            ret += litem.col_and_type.xyz * spec / 4.0;
        }
        if (lobe_weights.clearcoat > 0.0 && ENABLE_CLEARCOAT != 0) {
            vec3 ccol = clearcoat_color;

            vec3 coat = LTC_Evaluate_Rect(ltc_luts, 0.875, N, I, pos_ws.xyz, ltc.coat_t1, points, TwoSided);
            coat *= ccol * ltc.coat_t2.x + (1.0 - ccol) * ltc.coat_t2.y;

            ret += 0.25 * litem.col_and_type.xyz * coat / 4.0;
        }
    } else if (type == LIGHT_TYPE_DISK && ENABLE_DISK_LIGHT != 0) {
        vec3 points[4];
        points[0] = litem.pos_and_radius.xyz + litem.u_and_reg.xyz + litem.v_and_blend.xyz;
        points[1] = litem.pos_and_radius.xyz + litem.u_and_reg.xyz - litem.v_and_blend.xyz;
        points[2] = litem.pos_and_radius.xyz - litem.u_and_reg.xyz - litem.v_and_blend.xyz;
        points[3] = litem.pos_and_radius.xyz - litem.u_and_reg.xyz + litem.v_and_blend.xyz;

        if (lobe_weights.diffuse > 0.0 && ENABLE_DIFFUSE != 0) {
            vec3 dcol = base_color;

            vec3 diff = LTC_Evaluate_Disk(ltc_luts, 0.125, N, I, pos_ws.xyz, ltc.diff_t1, points, TwoSided);
            diff *= dcol * ltc.diff_t2.x;// + (1.0 - dcol) * ltc.diff_t2.y;

            if (sheen > 0.0 && ENABLE_SHEEN != 0) {
                vec3 _sheen = LTC_Evaluate_Disk(ltc_luts, 0.375, N, I, pos_ws.xyz, ltc.sheen_t1, points, TwoSided);
                diff += _sheen * (sheen_color * ltc.sheen_t2.x + (1.0 - sheen_color) * ltc.sheen_t2.y);
            }

            ret += lobe_weights.diffuse_mul * litem.col_and_type.xyz * diff / 4.0;
        }
        if (lobe_weights.specular > 0.0 && ENABLE_SPECULAR != 0) {
            vec3 scol = spec_color;

            vec3 spec = LTC_Evaluate_Disk(ltc_luts, 0.625, N, I, pos_ws.xyz, ltc.spec_t1, points, TwoSided);
            spec *= scol * ltc.spec_t2.x + (1.0 - scol) * ltc.spec_t2.y;

            ret += litem.col_and_type.xyz * spec / 4.0;
        }
        if (lobe_weights.clearcoat > 0.0 && ENABLE_CLEARCOAT != 0) {
            vec3 ccol = clearcoat_color;

            vec3 coat = LTC_Evaluate_Disk(ltc_luts, 0.875, N, I, pos_ws.xyz, ltc.coat_t1, points, TwoSided);
            coat *= ccol * ltc.coat_t2.x + (1.0 - ccol) * ltc.coat_t2.y;

            ret += 0.25 * litem.col_and_type.xyz * coat / 4.0;
        }
    } else if (type == LIGHT_TYPE_LINE && ENABLE_LINE_LIGHT != 0) {
        vec3 points[2];
        points[0] = litem.pos_and_radius.xyz + litem.v_and_blend.xyz;
        points[1] = litem.pos_and_radius.xyz - litem.v_and_blend.xyz;

        if (lobe_weights.diffuse > 0.0 && ENABLE_DIFFUSE != 0) {
            vec3 dcol = base_color;

            vec3 diff = LTC_Evaluate_Line(N, I, pos_ws.xyz, ltc.diff_t1, points, 0.01);
            diff *= dcol * ltc.diff_t2.x;// + (1.0 - dcol) * ltc.diff_t2.y;

            if (sheen > 0.0 && ENABLE_SHEEN != 0) {
                vec3 _sheen = LTC_Evaluate_Line(N, I, pos_ws.xyz, ltc.sheen_t1, points, 0.01);
                diff += _sheen * (sheen_color * ltc.sheen_t2.x + (1.0 - sheen_color) * ltc.sheen_t2.y);
            }

            ret += lobe_weights.diffuse_mul * litem.col_and_type.xyz * diff;
        }
        if (lobe_weights.specular > 0.0 && ENABLE_SPECULAR != 0) {
            vec3 scol = spec_color;

            vec3 spec = LTC_Evaluate_Line(N, I, pos_ws.xyz, ltc.spec_t1, points, 0.01);
            spec *= scol * ltc.spec_t2.x + (1.0 - scol) * ltc.spec_t2.y;

            ret += litem.col_and_type.xyz * spec;
        }
        if (lobe_weights.clearcoat > 0.0 && ENABLE_CLEARCOAT != 0) {
            vec3 ccol = clearcoat_color;

            vec3 coat = LTC_Evaluate_Line(N, I, pos_ws.xyz, ltc.coat_t1, points, 0.01);
            coat *= ccol * ltc.coat_t2.x + (1.0 - ccol) * ltc.coat_t2.y;

            ret += 0.25 * litem.col_and_type.xyz * coat / 4.0;
        }
    }

    if (type == LIGHT_TYPE_SPHERE && litem.v_and_blend.w > 0.0) {
        ret *= saturate((litem.dir_and_spot.w - _angle) / litem.v_and_blend.w);
    }

    return ret;
}

vec3 EvaluateSunLight(vec3 light_color, vec3 light_dir, float light_radius, vec3 pos_ws, vec3 I, vec3 N, lobe_weights_t lobe_weights, ltc_params_t ltc,
                      sampler2D ltc_luts, float sheen, vec3 base_color, vec3 sheen_color, vec3 spec_color, vec3 clearcoat_color) {
    //if (dot(N, light_dir) < 0.0) {
    //    return vec3(0.0);
    //}

    vec3 ret = vec3(0.0);

    vec3 u = vec3(1.0, 0.0, 0.0);
    if (abs(light_dir.y) < 0.999) {
        u = vec3(0.0, 1.0, 0.0);
    }

    vec3 v = normalize(cross(u, light_dir));
    u = cross(N, v);

    vec3 points[4];
    points[0] = pos_ws + light_dir + light_radius * u + light_radius * v;
    points[1] = pos_ws + light_dir + light_radius * u - light_radius * v;
    points[2] = pos_ws + light_dir - light_radius * u - light_radius * v;
    points[3] = pos_ws + light_dir - light_radius * u + light_radius * v;

    if (lobe_weights.diffuse > 0.0 && ENABLE_DIFFUSE != 0) {
        vec3 dcol = base_color;

        vec3 diff = LTC_Evaluate_Disk(ltc_luts, 0.125, N, I, pos_ws.xyz, ltc.diff_t1, points, false);
        diff *= dcol * ltc.diff_t2.x;// + (1.0 - dcol) * ltc.diff_t2.y;

        if (sheen > 0.0 && ENABLE_SHEEN != 0) {
            vec3 _sheen = LTC_Evaluate_Disk(ltc_luts, 0.375, N, I, pos_ws.xyz, ltc.sheen_t1, points, false);
            diff += _sheen * (sheen_color * ltc.sheen_t2.x + (1.0 - sheen_color) * ltc.sheen_t2.y);
        }

        ret += lobe_weights.diffuse_mul * light_color * diff;
    }
    if (lobe_weights.specular > 0.0 && ENABLE_SPECULAR != 0) {
        vec3 scol = spec_color;

        vec3 spec = LTC_Evaluate_Disk(ltc_luts, 0.625, N, I, pos_ws.xyz, ltc.spec_t1, points, false);
        spec *= scol * ltc.spec_t2.x + (1.0 - scol) * ltc.spec_t2.y;

        ret += light_color * spec;
    }
    if (lobe_weights.clearcoat > 0.0 && ENABLE_CLEARCOAT != 0) {
        vec3 ccol = clearcoat_color;

        vec3 coat = LTC_Evaluate_Disk(ltc_luts, 0.875, N, I, pos_ws.xyz, ltc.coat_t1, points, false);
        coat *= ccol * ltc.coat_t2.x + (1.0 - ccol) * ltc.coat_t2.y;

        ret += 0.25 * light_color * coat;
    }

    return ret;
}

#endif // _PRINCIPLED_GLSL