#ifndef PRINCIPLED_COMMON_GLSL
#define PRINCIPLED_COMMON_GLSL

#include "ltc_common.glsl"

float schlick_weight(const float u) {
    const float m = saturate(1.0 - u);
    return (m * m) * (m * m) * m;
}

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

// https://github.com/BruceKnowsHow/Ebin-Shaders/blob/master/shaders/lib/Utility/fastMath.glsl
float acos_fast(float inX) {
	float x = abs(inX);
	float res = -0.156583 * x + (0.5 * M_PI);
	res *= sqrt(1.0 - x);
	return (inX >= 0.0) ? res : M_PI - res;
}

float RectangleSolidAngle(const vec3 P, const vec3 points[4]) {
	const vec3 v0 = points[0] - P, v1 = points[1] - P, v2 = points[2] - P, v3 = points[3] - P;
	const vec3 n0 = normalize(cross(v0, v1));
	const vec3 n1 = normalize(cross(v1, v2));
	const vec3 n2 = normalize(cross(v2, v3));
	const vec3 n3 = normalize(cross(v3, v0));
	const float g0 = acos_fast(clamp(dot(-n0, n1), -1.0, 1.0));
	const float g1 = acos_fast(clamp(dot(-n1, n2), -1.0, 1.0));
	const float g2 = acos_fast(clamp(dot(-n2, n3), -1.0, 1.0));
	const float g3 = acos_fast(clamp(dot(-n3, n0), -1.0, 1.0));
	return g0 + g1 + g2 + g3 - 2.0 * M_PI;
}

vec3 SpecularDominantDirection(vec3 N, vec3 R, float roughness) {
	return normalize(mix(N, R, 1.0 - roughness));
}

float SmithG_GGX(const float N_dot_V, const float alpha_g) {
    const float a = alpha_g * alpha_g;
    const float b = N_dot_V * N_dot_V;
    return 1.0 / (N_dot_V + sqrt(a + b - a * b));
}

float D_GTR1(const float N_dot_H, const float a) {
    if (a >= 1.0) {
        return 1.0 / M_PI;
    }
    const float a2 = sqr(a);
    const float t = 1.0 + (a2 - 1.0) * N_dot_H * N_dot_H;
    return (a2 - 1.0) / (M_PI * log(a2) * t);
}

float D_GTR2(const float N_dot_H, const float a) {
    const float a2 = sqr(a);
    const float t = 1.0 + (a2 - 1.0) * N_dot_H * N_dot_H;
    return a2 / max(M_PI * t * t, 1e-12);
    //return a2 / (M_PI * t * t);
}

float PrincipledDiffuse(const float alpha, const float N_dot_V, const float N_dot_L, const float L_dot_H) {
    const float FL = schlick_weight(N_dot_L), FV = schlick_weight(N_dot_V);
    const float roughness = sqrt(alpha);
    const float Fd90 = 0.5 + 2.0 * L_dot_H * L_dot_H * roughness;
    const float Fd = mix(1.0, Fd90, FL) * mix(1.0, Fd90, FV);
    return Fd;
}

float PrincipledSheen(const float N_dot_L, const float L_dot_H) {
    return M_PI * schlick_weight(L_dot_H);
}

float PrincipledSpecular(const float alpha, const float N_dot_V, const float N_dot_L, const float N_dot_H) {
    return D_GTR2(N_dot_H, alpha) * SmithG_GGX(N_dot_V, alpha) * SmithG_GGX(N_dot_L, alpha);
}

float PrincipledClearcoat(const float alpha, const float N_dot_V, const float N_dot_L, const float N_dot_H) {
    return D_GTR1(N_dot_H, alpha) * SmithG_GGX(N_dot_V, alpha) * SmithG_GGX(N_dot_L, alpha);
}

struct lobe_weights_t {
    float diffuse, specular, clearcoat, refraction;
    float diffuse_mul, specular_mul;
};

lobe_weights_t get_lobe_weights(const float base_color_lum, const float spec_color_lum, const float specular,
                                const float metallic, const float transmission, const float clearcoat) {
    // taken from Cycles
    lobe_weights_t ret;
    ret.diffuse_mul = (1.0 - metallic) * (1.0 - transmission);
    ret.diffuse = base_color_lum * ret.diffuse_mul;
    const float final_transmission = transmission * (1.0 - metallic);
    ret.specular = (specular != 0.0 || metallic != 0.0) ? spec_color_lum * (1.0 - final_transmission) : 0.0;
    ret.clearcoat = 0.25 * clearcoat * (1.0 - metallic);
    ret.specular_mul = 1.0;
    ret.refraction = final_transmission * base_color_lum;

    const float total_weight = ret.diffuse + ret.specular + ret.clearcoat + ret.refraction;
    if (total_weight != 0.0) {
        ret.diffuse /= total_weight;
        ret.specular /= total_weight;
        ret.clearcoat /= total_weight;
        ret.refraction /= total_weight;
    }

    return ret;
}

const uint LOBE_DIFFUSE_BIT = (1u << 0);
const uint LOBE_SPECULAR_BIT = (1u << 1);
const uint LOBE_CLEARCOAT_BIT = (1u << 2);
const uint LOBE_REFRACTION_BIT = (1u << 3);

struct lobe_masks_t {
    uint bits;
    float diffuse_mul, specular_mul;
};

lobe_masks_t get_lobe_masks(const float base_color_lum, const float spec_color_lum, const float specular,
                            const float metallic, const float transmission, const float clearcoat) {
    // taken from Cycles
    lobe_masks_t ret;
    ret.bits = 0u;
    ret.diffuse_mul = (1.0 - metallic) * (1.0 - transmission);
    if ((base_color_lum * ret.diffuse_mul) > 1e-7) {
        ret.bits |= LOBE_DIFFUSE_BIT;
    }
    float final_transmission = transmission * (1.0 - metallic);
    if ((specular != 0.0 || metallic != 0.0) && (spec_color_lum * (1.0 - final_transmission)) > 1e-7) {
        ret.bits |= LOBE_SPECULAR_BIT;
    }
    if (clearcoat * (1.0 - metallic) > 1e-7) {
        ret.bits |= LOBE_CLEARCOAT_BIT;
    }
    ret.specular_mul = 1.0;
    if (final_transmission * base_color_lum > 1e-7) {
        ret.bits |= LOBE_REFRACTION_BIT;
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

#ifndef MIN_SPEC_ROUGHNESS
    #define MIN_SPEC_ROUGHNESS 0.0
#endif

#ifndef SIMPLIFIED_LTC_DIFFUSE
    #define SIMPLIFIED_LTC_DIFFUSE 1
#endif

ltc_params_t SampleLTC_Params(sampler2D luts, float N_dot_V, float roughness, float clearcoat_roughness2) {
    ltc_params_t ret;

    const vec2 diff_ltc_uv = LTC_Coords(N_dot_V, clamp(roughness, 0.0, 1.0));
#if SIMPLIFIED_LTC_DIFFUSE
    ret.diff_t1 = vec4(1.0, 0.0, 0.0, 1.0);
#else
    ret.diff_t1 = textureLod(luts, diff_ltc_uv + vec2(0.0, 0.0), 0.0);
#endif
    ret.diff_t2 = textureLod(luts, diff_ltc_uv + vec2(0.125, 0.0), 0.0).xy;
    ret.sheen_t1 = textureLod(luts, diff_ltc_uv + vec2(0.25, 0.0), 0.0);
    ret.sheen_t2 = textureLod(luts, diff_ltc_uv + vec2(0.375, 0.0), 0.0).xy;

    const vec2 spec_ltc_uv = LTC_Coords(N_dot_V, clamp(roughness, MIN_SPEC_ROUGHNESS, 1.0));
    ret.spec_t1 = textureLod(luts, spec_ltc_uv + vec2(0.5, 0.0), 0.0);
    ret.spec_t2 = textureLod(luts, spec_ltc_uv + vec2(0.625, 0.0), 0.0).xy;

    const vec2 coat_ltc_uv = LTC_Coords(N_dot_V, clearcoat_roughness2);
    ret.coat_t1 = textureLod(luts, coat_ltc_uv + vec2(0.75, 0.0), 0.0);
    ret.coat_t2 = textureLod(luts, coat_ltc_uv + vec2(0.875, 0.0), 0.0).xy;

    return ret;
}

struct _light_item_t {
    vec4 col_and_type;
    vec4 pos_and_radius;
    vec4 dir_and_spot;
    vec4 u_and_reg;
    vec4 v_and_blend;
    vec4 shadow_pos_and_tri_index;
};

_light_item_t FetchLightItem(usamplerBuffer lights_buf, const int li) {
    _light_item_t ret;
    ret.col_and_type = uintBitsToFloat(texelFetch(lights_buf, li * LIGHTS_BUF_STRIDE + 0));
    ret.pos_and_radius = uintBitsToFloat(texelFetch(lights_buf, li * LIGHTS_BUF_STRIDE + 1));
    ret.dir_and_spot = uintBitsToFloat(texelFetch(lights_buf, li * LIGHTS_BUF_STRIDE + 2));
    ret.u_and_reg = uintBitsToFloat(texelFetch(lights_buf, li * LIGHTS_BUF_STRIDE + 3));
    ret.v_and_blend = uintBitsToFloat(texelFetch(lights_buf, li * LIGHTS_BUF_STRIDE + 4));
    ret.shadow_pos_and_tri_index = uintBitsToFloat(texelFetch(lights_buf, li * LIGHTS_BUF_STRIDE + 5));
    return ret;
}

float spread_attenuation(const vec3 D, const vec3 light_fwd, const float tan_half_spread, const float spread_normalization) {
    const float tan_a = tan_angle(-D, light_fwd);
    return max((tan_half_spread - tan_a) * spread_normalization, 0.0);
}

#ifndef ENABLE_SPHERE_LIGHT
    #define ENABLE_SPHERE_LIGHT 1
#endif
#ifndef ENABLE_RECT_LIGHT
    #define ENABLE_RECT_LIGHT 1
#endif
#ifndef ENABLE_DISK_LIGHT
    #define ENABLE_DISK_LIGHT 1
#endif
#ifndef ENABLE_LINE_LIGHT
    #define ENABLE_LINE_LIGHT 1
#endif

#ifndef ENABLE_DIFFUSE
    #define ENABLE_DIFFUSE 1
#endif
#ifndef ENABLE_SHEEN
    // NOTE: Sheen is disabled as it requires different LTC implementation
    #define ENABLE_SHEEN 0
#endif
#ifndef ENABLE_SPECULAR
    #define ENABLE_SPECULAR 1
#endif
#ifndef ENABLE_CLEARCOAT
    #define ENABLE_CLEARCOAT 1
#endif

vec3 EvaluateLightSource_LTC(const _light_item_t litem, const vec3 P, const vec3 I, const vec3 N, const lobe_masks_t lobe_masks, const ltc_params_t ltc,
                             sampler2D ltc_luts, const float sheen, const vec3 base_color, const vec3 sheen_color, const vec3 spec_color, const vec3 clearcoat_color) {
    const bool TwoSided = false;

    const uint type = floatBitsToUint(litem.col_and_type.w) & LIGHT_TYPE_BITS;
    const vec3 from_light = normalize(P - litem.pos_and_radius.xyz);
    const float _dot = -dot(from_light, litem.dir_and_spot.xyz);
    const float _angle = approx_acos(_dot);
    if (type == LIGHT_TYPE_SPHERE && _angle > litem.dir_and_spot.w) {
        return vec3(0.0);
    }

    vec3 ret = vec3(0.0);

    if (type == LIGHT_TYPE_SPHERE && ENABLE_SPHERE_LIGHT != 0) {
        const vec3 surface_to_center = litem.pos_and_radius.xyz - P;
        const float d = length(surface_to_center);

        const float temp = sqrt(d * d - litem.pos_and_radius.w * litem.pos_and_radius.w);
        const float disk_radius = (temp * litem.pos_and_radius.w) / d;
        const float k = (temp * disk_radius) / (litem.pos_and_radius.w * d);
        const vec3 disk_center = P + k * surface_to_center;

        const vec3 u = normalize(I - from_light * dot(I, from_light)) * disk_radius;
        const vec3 v = cross(from_light, u);

        vec3 points[4];
        points[0] = disk_center + u + v;
        points[1] = disk_center + u - v;
        points[2] = disk_center - u - v;
        points[3] = disk_center - u + v;

        if ((lobe_masks.bits & LOBE_DIFFUSE_BIT) != 0 && ENABLE_DIFFUSE != 0) {
            const vec3 dcol = base_color;

            vec3 diff = LTC_Evaluate_Disk(ltc_luts, 0.125, N, I, P, ltc.diff_t1, points, TwoSided);
            diff *= dcol * ltc.diff_t2.x;// + (1.0 - dcol) * ltc.diff_t2.y;

            if (sheen > 0.0 && ENABLE_SHEEN != 0) {
                const vec3 _sheen = LTC_Evaluate_Disk(ltc_luts, 0.375, N, I, P, ltc.sheen_t1, points, TwoSided);
                diff += _sheen * (sheen_color * ltc.sheen_t2.x + (1.0 - sheen_color) * ltc.sheen_t2.y);
            }

            ret += lobe_masks.diffuse_mul * litem.col_and_type.xyz * diff / M_PI;
        }
        if ((lobe_masks.bits & (LOBE_SPECULAR_BIT | LOBE_REFRACTION_BIT)) != 0 && ENABLE_SPECULAR != 0) {
            const vec3 scol = spec_color;

            vec3 spec = LTC_Evaluate_Disk(ltc_luts, 0.625, N, I, P, ltc.spec_t1, points, TwoSided);
            spec *= scol * ltc.spec_t2.x + (1.0 - scol) * ltc.spec_t2.y;

            ret += lobe_masks.specular_mul * litem.col_and_type.xyz * spec / M_PI;
        }
        if ((lobe_masks.bits & LOBE_CLEARCOAT_BIT) != 0 && ENABLE_CLEARCOAT != 0) {
            const vec3 ccol = clearcoat_color;

            vec3 coat = LTC_Evaluate_Disk(ltc_luts, 0.875, N, I, P, ltc.coat_t1, points, TwoSided);
            coat *= ccol * ltc.coat_t2.x + (1.0 - ccol) * ltc.coat_t2.y;

            ret += 0.25 * lobe_masks.specular_mul * litem.col_and_type.xyz * coat / M_PI;
        }
        if (litem.v_and_blend.w > 0.0) {
            ret *= saturate((litem.dir_and_spot.w - _angle) / sqr(litem.v_and_blend.w));
        }
    } else if (type == LIGHT_TYPE_RECT && ENABLE_RECT_LIGHT != 0) {
        vec3 points[4];
        points[0] = litem.pos_and_radius.xyz + litem.u_and_reg.xyz + litem.v_and_blend.xyz;
        points[1] = litem.pos_and_radius.xyz + litem.u_and_reg.xyz - litem.v_and_blend.xyz;
        points[2] = litem.pos_and_radius.xyz - litem.u_and_reg.xyz - litem.v_and_blend.xyz;
        points[3] = litem.pos_and_radius.xyz - litem.u_and_reg.xyz + litem.v_and_blend.xyz;

        if ((lobe_masks.bits & LOBE_DIFFUSE_BIT) != 0 && ENABLE_DIFFUSE != 0) {
            const vec3 dcol = base_color;

            vec3 diff = vec3(LTC_Evaluate_Rect(ltc_luts, 0.125, N, I, P, ltc.diff_t1, points, TwoSided));
            diff *= dcol * ltc.diff_t2.x;// + (1.0 - dcol) * ltc.diff_t2.y;

            if (sheen > 0.0 && ENABLE_SHEEN != 0) {
                const float _sheen = LTC_Evaluate_Rect(ltc_luts, 0.375, N, I, P, ltc.sheen_t1, points, TwoSided);
                diff += _sheen * (sheen_color * ltc.sheen_t2.x + (1.0 - sheen_color) * ltc.sheen_t2.y);
            }

            ret += lobe_masks.diffuse_mul * litem.col_and_type.xyz * diff / 4.0;
        }
        if ((lobe_masks.bits & (LOBE_SPECULAR_BIT | LOBE_REFRACTION_BIT)) != 0 && ENABLE_SPECULAR != 0) {
            const vec3 scol = spec_color;

            vec3 spec = vec3(LTC_Evaluate_Rect(ltc_luts, 0.625, N, I, P, ltc.spec_t1, points, TwoSided));
            spec *= scol * ltc.spec_t2.x + (1.0 - scol) * ltc.spec_t2.y;

            ret += lobe_masks.specular_mul * litem.col_and_type.xyz * spec / 4.0;
        }
        if ((lobe_masks.bits & LOBE_CLEARCOAT_BIT) != 0 && ENABLE_CLEARCOAT != 0) {
            const vec3 ccol = clearcoat_color;

            vec3 coat = vec3(LTC_Evaluate_Rect(ltc_luts, 0.875, N, I, P, ltc.coat_t1, points, TwoSided));
            coat *= ccol * ltc.coat_t2.x + (1.0 - ccol) * ltc.coat_t2.y;

            ret += 0.25 * lobe_masks.specular_mul * litem.col_and_type.xyz * coat / 4.0;
        }
        const float tan_half_spread = litem.dir_and_spot.w, spread_normalization = litem.v_and_blend.w;
        if (spread_normalization > 0.0) {
            vec2 tuv = vec2(dot(litem.u_and_reg.xyz, P - litem.pos_and_radius.xyz) / length2(litem.u_and_reg.xyz),
                            dot(litem.v_and_blend.xyz, P - litem.pos_and_radius.xyz) / length2(litem.v_and_blend.xyz));
            tuv = clamp(tuv, vec2(-1.0), vec2(1.0));

            const vec3 closest_P = litem.pos_and_radius.xyz + tuv.x * litem.u_and_reg.xyz + tuv.y * litem.v_and_blend.xyz;
            ret *= spread_attenuation(normalize(P - closest_P), litem.dir_and_spot.xyz, tan_half_spread, spread_normalization);
        }
    } else if (type == LIGHT_TYPE_DISK && ENABLE_DISK_LIGHT != 0) {
        vec3 points[4];
        points[0] = litem.pos_and_radius.xyz + litem.u_and_reg.xyz + litem.v_and_blend.xyz;
        points[1] = litem.pos_and_radius.xyz + litem.u_and_reg.xyz - litem.v_and_blend.xyz;
        points[2] = litem.pos_and_radius.xyz - litem.u_and_reg.xyz - litem.v_and_blend.xyz;
        points[3] = litem.pos_and_radius.xyz - litem.u_and_reg.xyz + litem.v_and_blend.xyz;

        if ((lobe_masks.bits & LOBE_DIFFUSE_BIT) != 0 && ENABLE_DIFFUSE != 0) {
            const vec3 dcol = base_color;

            vec3 diff = LTC_Evaluate_Disk(ltc_luts, 0.125, N, I, P, ltc.diff_t1, points, TwoSided);
            diff *= dcol * ltc.diff_t2.x;// + (1.0 - dcol) * ltc.diff_t2.y;

            if (sheen > 0.0 && ENABLE_SHEEN != 0) {
                const vec3 _sheen = LTC_Evaluate_Disk(ltc_luts, 0.375, N, I, P, ltc.sheen_t1, points, TwoSided);
                diff += _sheen * (sheen_color * ltc.sheen_t2.x + (1.0 - sheen_color) * ltc.sheen_t2.y);
            }

            ret += lobe_masks.diffuse_mul * litem.col_and_type.xyz * diff / 4.0;
        }
        if ((lobe_masks.bits & (LOBE_SPECULAR_BIT | LOBE_REFRACTION_BIT)) != 0 && ENABLE_SPECULAR != 0) {
            const vec3 scol = spec_color;

            vec3 spec = LTC_Evaluate_Disk(ltc_luts, 0.625, N, I, P, ltc.spec_t1, points, TwoSided);
            spec *= scol * ltc.spec_t2.x + (1.0 - scol) * ltc.spec_t2.y;

            ret += lobe_masks.specular_mul * litem.col_and_type.xyz * spec / 4.0;
        }
        if ((lobe_masks.bits & LOBE_CLEARCOAT_BIT) != 0 && ENABLE_CLEARCOAT != 0) {
            const vec3 ccol = clearcoat_color;

            vec3 coat = LTC_Evaluate_Disk(ltc_luts, 0.875, N, I, P, ltc.coat_t1, points, TwoSided);
            coat *= ccol * ltc.coat_t2.x + (1.0 - ccol) * ltc.coat_t2.y;

            ret += 0.25 * lobe_masks.specular_mul * litem.col_and_type.xyz * coat / 4.0;
        }
        const float tan_half_spread = litem.dir_and_spot.w, spread_normalization = litem.v_and_blend.w;
        if (spread_normalization > 0.0) {
            vec2 tuv = vec2(dot(litem.u_and_reg.xyz, P - litem.pos_and_radius.xyz) / length2(litem.u_and_reg.xyz),
                            dot(litem.v_and_blend.xyz, P - litem.pos_and_radius.xyz) / length2(litem.v_and_blend.xyz));
            tuv /= max(1.0 , length(tuv));

            const vec3 closest_P = litem.pos_and_radius.xyz + tuv.x * litem.u_and_reg.xyz + tuv.y * litem.v_and_blend.xyz;
            ret *= spread_attenuation(normalize(P - closest_P), litem.dir_and_spot.xyz, tan_half_spread, spread_normalization);
        }
    } else if (type == LIGHT_TYPE_LINE && ENABLE_LINE_LIGHT != 0) {
        vec3 points[2];
        points[0] = litem.pos_and_radius.xyz + litem.v_and_blend.xyz;
        points[1] = litem.pos_and_radius.xyz - litem.v_and_blend.xyz;

        if ((lobe_masks.bits & LOBE_DIFFUSE_BIT) != 0 && ENABLE_DIFFUSE != 0) {
            const vec3 dcol = base_color;

            vec3 diff = LTC_Evaluate_Line(N, I, P, ltc.diff_t1, points, 0.01);
            diff *= dcol * ltc.diff_t2.x;// + (1.0 - dcol) * ltc.diff_t2.y;

            if (sheen > 0.0 && ENABLE_SHEEN != 0) {
                const vec3 _sheen = LTC_Evaluate_Line(N, I, P, ltc.sheen_t1, points, 0.01);
                diff += _sheen * (sheen_color * ltc.sheen_t2.x + (1.0 - sheen_color) * ltc.sheen_t2.y);
            }

            ret += lobe_masks.diffuse_mul * litem.col_and_type.xyz * diff;
        }
        if ((lobe_masks.bits & (LOBE_SPECULAR_BIT | LOBE_REFRACTION_BIT)) != 0 && ENABLE_SPECULAR != 0) {
            const vec3 scol = spec_color;

            vec3 spec = LTC_Evaluate_Line(N, I, P, ltc.spec_t1, points, 0.01);
            spec *= scol * ltc.spec_t2.x + (1.0 - scol) * ltc.spec_t2.y;

            ret += lobe_masks.specular_mul * litem.col_and_type.xyz * spec;
        }
        if ((lobe_masks.bits & LOBE_CLEARCOAT_BIT) != 0 && ENABLE_CLEARCOAT != 0) {
            const vec3 ccol = clearcoat_color;

            vec3 coat = LTC_Evaluate_Line(N, I, P, ltc.coat_t1, points, 0.01);
            coat *= ccol * ltc.coat_t2.x + (1.0 - ccol) * ltc.coat_t2.y;

            ret += 0.25 * lobe_masks.specular_mul * litem.col_and_type.xyz * coat / 4.0;
        }
    }

    return ret;
}

vec3 EvaluateSunLight_LTC(const vec3 light_color, const vec3 light_dir, const float light_radius, const vec3 P,
                          const vec3 I, const vec3 N, const lobe_masks_t lobe_masks, const ltc_params_t ltc,
                          sampler2D ltc_luts, const float sheen, const vec3 base_color, const vec3 sheen_color,
                          const vec3 spec_color, const vec3 clearcoat_color) {
    vec3 u = vec3(1.0, 0.0, 0.0);
    if (abs(light_dir.y) < 0.999) {
        u = vec3(0.0, 1.0, 0.0);
    }

    vec3 v = normalize(cross(u, light_dir));
    u = cross(light_dir, v);

    vec3 points[4];
    points[0] = P + light_dir + light_radius * u + light_radius * v;
    points[1] = P + light_dir + light_radius * u - light_radius * v;
    points[2] = P + light_dir - light_radius * u - light_radius * v;
    points[3] = P + light_dir - light_radius * u + light_radius * v;

    vec3 ret = vec3(0.0);

    if ((lobe_masks.bits & LOBE_DIFFUSE_BIT) != 0 && ENABLE_DIFFUSE != 0) {
        const vec3 dcol = base_color;

        vec3 diff = LTC_Evaluate_Disk(ltc_luts, 0.125, N, I, P, ltc.diff_t1, points, false);
        diff *= dcol * ltc.diff_t2.x;// + (1.0 - dcol) * ltc.diff_t2.y;

        if (sheen > 0.0 && ENABLE_SHEEN != 0) {
            const vec3 _sheen = LTC_Evaluate_Disk(ltc_luts, 0.375, N, I, P, ltc.sheen_t1, points, false);
            diff += _sheen * (sheen_color * ltc.sheen_t2.x + (1.0 - sheen_color) * ltc.sheen_t2.y);
        }

        ret += lobe_masks.diffuse_mul * light_color * diff;
    }
    if ((lobe_masks.bits & LOBE_SPECULAR_BIT) != 0 && ENABLE_SPECULAR != 0) {
        const vec3 scol = spec_color;

        vec3 spec = LTC_Evaluate_Disk(ltc_luts, 0.625, N, I, P, ltc.spec_t1, points, false);
        spec *= scol * ltc.spec_t2.x + (1.0 - scol) * ltc.spec_t2.y;

        ret += lobe_masks.specular_mul * light_color * spec;
    }
    if ((lobe_masks.bits & LOBE_CLEARCOAT_BIT) != 0 && ENABLE_CLEARCOAT != 0) {
        const vec3 ccol = clearcoat_color;

        vec3 coat = LTC_Evaluate_Disk(ltc_luts, 0.875, N, I, P, ltc.coat_t1, points, false);
        coat *= ccol * ltc.coat_t2.x + (1.0 - ccol) * ltc.coat_t2.y;

        ret += 0.25 * lobe_masks.specular_mul * light_color * coat;
    }

    return ret;
}

#ifdef LTC_SHARED_MEM
shared ltc_params_t g_ltc[LTC_SHARED_MEM];

vec3 EvaluateLightSource_LTC(const _light_item_t litem, const vec3 P, const vec3 I, const vec3 N, const lobe_masks_t lobe_masks,
                             sampler2D ltc_luts, const float sheen, const vec3 base_color, const vec3 sheen_color, const vec3 spec_color, const vec3 clearcoat_color) {
    const bool TwoSided = false;

    const uint type = floatBitsToUint(litem.col_and_type.w) & LIGHT_TYPE_BITS;
    const vec3 from_light = normalize(P - litem.pos_and_radius.xyz);
    const float _dot = -dot(from_light, litem.dir_and_spot.xyz);
    const float _angle = approx_acos(_dot);
    if (type == LIGHT_TYPE_SPHERE && _angle > litem.dir_and_spot.w) {
        return vec3(0.0);
    }

    vec3 ret = vec3(0.0);

    if (type == LIGHT_TYPE_SPHERE && ENABLE_SPHERE_LIGHT != 0) {
        const vec3 surface_to_center = litem.pos_and_radius.xyz - P;
        const float d = length(surface_to_center);

        const float temp = sqrt(d * d - litem.pos_and_radius.w * litem.pos_and_radius.w);
        const float disk_radius = (temp * litem.pos_and_radius.w) / d;
        const float k = (temp * disk_radius) / (litem.pos_and_radius.w * d);
        const vec3 disk_center = P + k * surface_to_center;

        const vec3 u = normalize(I - from_light * dot(I, from_light)) * disk_radius;
        const vec3 v = cross(from_light, u);

        vec3 points[4];
        points[0] = disk_center + u + v;
        points[1] = disk_center + u - v;
        points[2] = disk_center - u - v;
        points[3] = disk_center - u + v;

        if ((lobe_masks.bits & LOBE_DIFFUSE_BIT) != 0 && ENABLE_DIFFUSE != 0) {
            const vec3 dcol = base_color;

            vec3 diff = LTC_Evaluate_Disk(ltc_luts, 0.125, N, I, P, g_ltc[gl_LocalInvocationIndex].diff_t1, points, TwoSided);
            diff *= dcol * g_ltc[gl_LocalInvocationIndex].diff_t2.x;// + (1.0 - dcol) * g_ltc[gl_LocalInvocationIndex].diff_t2.y;

            if (sheen > 0.0 && ENABLE_SHEEN != 0) {
                const vec3 _sheen = LTC_Evaluate_Disk(ltc_luts, 0.375, N, I, P, g_ltc[gl_LocalInvocationIndex].sheen_t1, points, TwoSided);
                diff += _sheen * (sheen_color * g_ltc[gl_LocalInvocationIndex].sheen_t2.x + (1.0 - sheen_color) * g_ltc[gl_LocalInvocationIndex].sheen_t2.y);
            }

            ret += lobe_masks.diffuse_mul * litem.col_and_type.xyz * diff / M_PI;
        }
        if ((lobe_masks.bits & (LOBE_SPECULAR_BIT | LOBE_REFRACTION_BIT)) != 0 && ENABLE_SPECULAR != 0) {
            const vec3 scol = spec_color;

            vec3 spec = LTC_Evaluate_Disk(ltc_luts, 0.625, N, I, P, g_ltc[gl_LocalInvocationIndex].spec_t1, points, TwoSided);
            spec *= scol * g_ltc[gl_LocalInvocationIndex].spec_t2.x + (1.0 - scol) * g_ltc[gl_LocalInvocationIndex].spec_t2.y;

            ret += lobe_masks.specular_mul * litem.col_and_type.xyz * spec / M_PI;
        }
        if ((lobe_masks.bits & LOBE_CLEARCOAT_BIT) != 0 && ENABLE_CLEARCOAT != 0) {
            const vec3 ccol = clearcoat_color;

            vec3 coat = LTC_Evaluate_Disk(ltc_luts, 0.875, N, I, P, g_ltc[gl_LocalInvocationIndex].coat_t1, points, TwoSided);
            coat *= ccol * g_ltc[gl_LocalInvocationIndex].coat_t2.x + (1.0 - ccol) * g_ltc[gl_LocalInvocationIndex].coat_t2.y;

            ret += 0.25 * lobe_masks.specular_mul * litem.col_and_type.xyz * coat / M_PI;
        }
        if (litem.v_and_blend.w > 0.0) {
            ret *= saturate((litem.dir_and_spot.w - _angle) / sqr(litem.v_and_blend.w));
        }
    } else if (type == LIGHT_TYPE_RECT && ENABLE_RECT_LIGHT != 0) {
        vec3 points[4];
        points[0] = litem.pos_and_radius.xyz + litem.u_and_reg.xyz + litem.v_and_blend.xyz;
        points[1] = litem.pos_and_radius.xyz + litem.u_and_reg.xyz - litem.v_and_blend.xyz;
        points[2] = litem.pos_and_radius.xyz - litem.u_and_reg.xyz - litem.v_and_blend.xyz;
        points[3] = litem.pos_and_radius.xyz - litem.u_and_reg.xyz + litem.v_and_blend.xyz;

        [[dont_flatten]] if ((lobe_masks.bits & LOBE_DIFFUSE_BIT) != 0 && ENABLE_DIFFUSE != 0) {
            const vec3 dcol = base_color;

            vec3 diff = vec3(LTC_Evaluate_Rect(ltc_luts, 0.125, N, I, P, g_ltc[gl_LocalInvocationIndex].diff_t1, points, TwoSided));
            diff *= dcol * g_ltc[gl_LocalInvocationIndex].diff_t2.x;// + (1.0 - dcol) * g_ltc[gl_LocalInvocationIndex].diff_t2.y;

            if (sheen > 0.0 && ENABLE_SHEEN != 0) {
                const float _sheen = LTC_Evaluate_Rect(ltc_luts, 0.375, N, I, P, g_ltc[gl_LocalInvocationIndex].sheen_t1, points, TwoSided);
                diff += _sheen * (sheen_color * g_ltc[gl_LocalInvocationIndex].sheen_t2.x + (1.0 - sheen_color) * g_ltc[gl_LocalInvocationIndex].sheen_t2.y);
            }

            ret += lobe_masks.diffuse_mul * litem.col_and_type.xyz * diff / 4.0;
        }
        [[dont_flatten]] if ((lobe_masks.bits & (LOBE_SPECULAR_BIT | LOBE_REFRACTION_BIT)) != 0 && ENABLE_SPECULAR != 0) {
            const vec3 scol = spec_color;

            vec3 spec = vec3(LTC_Evaluate_Rect(ltc_luts, 0.625, N, I, P, g_ltc[gl_LocalInvocationIndex].spec_t1, points, TwoSided));
            spec *= scol * g_ltc[gl_LocalInvocationIndex].spec_t2.x + (1.0 - scol) * g_ltc[gl_LocalInvocationIndex].spec_t2.y;

            ret += lobe_masks.specular_mul * litem.col_and_type.xyz * spec / 4.0;
        }
        [[dont_flatten]] if ((lobe_masks.bits & LOBE_CLEARCOAT_BIT) != 0 && ENABLE_CLEARCOAT != 0) {
            const vec3 ccol = clearcoat_color;

            vec3 coat = vec3(LTC_Evaluate_Rect(ltc_luts, 0.875, N, I, P, g_ltc[gl_LocalInvocationIndex].coat_t1, points, TwoSided));
            coat *= ccol * g_ltc[gl_LocalInvocationIndex].coat_t2.x + (1.0 - ccol) * g_ltc[gl_LocalInvocationIndex].coat_t2.y;

            ret += 0.25 * lobe_masks.specular_mul * litem.col_and_type.xyz * coat / 4.0;
        }
        const float tan_half_spread = litem.dir_and_spot.w, spread_normalization = litem.v_and_blend.w;
        if (spread_normalization > 0.0) {
            vec2 tuv = vec2(dot(litem.u_and_reg.xyz, P - litem.pos_and_radius.xyz) / length2(litem.u_and_reg.xyz),
                            dot(litem.v_and_blend.xyz, P - litem.pos_and_radius.xyz) / length2(litem.v_and_blend.xyz));
            tuv = clamp(tuv, vec2(-1.0), vec2(1.0));

            const vec3 closest_P = litem.pos_and_radius.xyz + tuv.x * litem.u_and_reg.xyz + tuv.y * litem.v_and_blend.xyz;
            ret *= spread_attenuation(normalize(P - closest_P), litem.dir_and_spot.xyz, tan_half_spread, spread_normalization);
        }
    } else if (type == LIGHT_TYPE_DISK && ENABLE_DISK_LIGHT != 0) {
        vec3 points[4];
        points[0] = litem.pos_and_radius.xyz + litem.u_and_reg.xyz + litem.v_and_blend.xyz;
        points[1] = litem.pos_and_radius.xyz + litem.u_and_reg.xyz - litem.v_and_blend.xyz;
        points[2] = litem.pos_and_radius.xyz - litem.u_and_reg.xyz - litem.v_and_blend.xyz;
        points[3] = litem.pos_and_radius.xyz - litem.u_and_reg.xyz + litem.v_and_blend.xyz;

        if ((lobe_masks.bits & LOBE_DIFFUSE_BIT) != 0 && ENABLE_DIFFUSE != 0) {
            const vec3 dcol = base_color;

            vec3 diff = LTC_Evaluate_Disk(ltc_luts, 0.125, N, I, P, g_ltc[gl_LocalInvocationIndex].diff_t1, points, TwoSided);
            diff *= dcol * g_ltc[gl_LocalInvocationIndex].diff_t2.x;// + (1.0 - dcol) * g_ltc[gl_LocalInvocationIndex].diff_t2.y;

            if (sheen > 0.0 && ENABLE_SHEEN != 0) {
                const vec3 _sheen = LTC_Evaluate_Disk(ltc_luts, 0.375, N, I, P, g_ltc[gl_LocalInvocationIndex].sheen_t1, points, TwoSided);
                diff += _sheen * (sheen_color * g_ltc[gl_LocalInvocationIndex].sheen_t2.x + (1.0 - sheen_color) * g_ltc[gl_LocalInvocationIndex].sheen_t2.y);
            }

            ret += lobe_masks.diffuse_mul * litem.col_and_type.xyz * diff / 4.0;
        }
        if ((lobe_masks.bits & (LOBE_SPECULAR_BIT | LOBE_REFRACTION_BIT)) != 0 && ENABLE_SPECULAR != 0) {
            const vec3 scol = spec_color;

            vec3 spec = LTC_Evaluate_Disk(ltc_luts, 0.625, N, I, P, g_ltc[gl_LocalInvocationIndex].spec_t1, points, TwoSided);
            spec *= scol * g_ltc[gl_LocalInvocationIndex].spec_t2.x + (1.0 - scol) * g_ltc[gl_LocalInvocationIndex].spec_t2.y;

            ret += lobe_masks.specular_mul * litem.col_and_type.xyz * spec / 4.0;
        }
        if ((lobe_masks.bits & LOBE_CLEARCOAT_BIT) != 0 && ENABLE_CLEARCOAT != 0) {
            const vec3 ccol = clearcoat_color;

            vec3 coat = LTC_Evaluate_Disk(ltc_luts, 0.875, N, I, P, g_ltc[gl_LocalInvocationIndex].coat_t1, points, TwoSided);
            coat *= ccol * g_ltc[gl_LocalInvocationIndex].coat_t2.x + (1.0 - ccol) * g_ltc[gl_LocalInvocationIndex].coat_t2.y;

            ret += 0.25 * lobe_masks.specular_mul * litem.col_and_type.xyz * coat / 4.0;
        }
        const float tan_half_spread = litem.dir_and_spot.w, spread_normalization = litem.v_and_blend.w;
        if (spread_normalization > 0.0) {
            vec2 tuv = vec2(dot(litem.u_and_reg.xyz, P - litem.pos_and_radius.xyz) / length2(litem.u_and_reg.xyz),
                            dot(litem.v_and_blend.xyz, P - litem.pos_and_radius.xyz) / length2(litem.v_and_blend.xyz));
            tuv /= max(1.0 , length(tuv));

            const vec3 closest_P = litem.pos_and_radius.xyz + tuv.x * litem.u_and_reg.xyz + tuv.y * litem.v_and_blend.xyz;
            ret *= spread_attenuation(normalize(P - closest_P), litem.dir_and_spot.xyz, tan_half_spread, spread_normalization);
        }
    } else if (type == LIGHT_TYPE_LINE && ENABLE_LINE_LIGHT != 0) {
        vec3 points[2];
        points[0] = litem.pos_and_radius.xyz + litem.v_and_blend.xyz;
        points[1] = litem.pos_and_radius.xyz - litem.v_and_blend.xyz;

        [[dont_flatten]] if ((lobe_masks.bits & LOBE_DIFFUSE_BIT) != 0 && ENABLE_DIFFUSE != 0) {
            const vec3 dcol = base_color;

            vec3 diff = LTC_Evaluate_Line(N, I, P, g_ltc[gl_LocalInvocationIndex].diff_t1, points, 0.01);
            diff *= dcol * g_ltc[gl_LocalInvocationIndex].diff_t2.x;// + (1.0 - dcol) * g_ltc[gl_LocalInvocationIndex].diff_t2.y;

            if (sheen > 0.0 && ENABLE_SHEEN != 0) {
                const vec3 _sheen = LTC_Evaluate_Line(N, I, P, g_ltc[gl_LocalInvocationIndex].sheen_t1, points, 0.01);
                diff += _sheen * (sheen_color * g_ltc[gl_LocalInvocationIndex].sheen_t2.x + (1.0 - sheen_color) * g_ltc[gl_LocalInvocationIndex].sheen_t2.y);
            }

            ret += lobe_masks.diffuse_mul * litem.col_and_type.xyz * diff;
        }
        [[dont_flatten]] if ((lobe_masks.bits & (LOBE_SPECULAR_BIT | LOBE_REFRACTION_BIT)) != 0 && ENABLE_SPECULAR != 0) {
            const vec3 scol = spec_color;

            vec3 spec = LTC_Evaluate_Line(N, I, P, g_ltc[gl_LocalInvocationIndex].spec_t1, points, 0.01);
            spec *= scol * g_ltc[gl_LocalInvocationIndex].spec_t2.x + (1.0 - scol) * g_ltc[gl_LocalInvocationIndex].spec_t2.y;

            ret += lobe_masks.specular_mul * litem.col_and_type.xyz * spec;
        }
        [[dont_flatten]] if ((lobe_masks.bits & LOBE_CLEARCOAT_BIT) != 0 && ENABLE_CLEARCOAT != 0) {
            const vec3 ccol = clearcoat_color;

            vec3 coat = LTC_Evaluate_Line(N, I, P, g_ltc[gl_LocalInvocationIndex].coat_t1, points, 0.01);
            coat *= ccol * g_ltc[gl_LocalInvocationIndex].coat_t2.x + (1.0 - ccol) * g_ltc[gl_LocalInvocationIndex].coat_t2.y;

            ret += 0.25 * lobe_masks.specular_mul * litem.col_and_type.xyz * coat / 4.0;
        }
    }

    return ret;
}

vec3 EvaluateSunLight_LTC(const vec3 light_color, const vec3 light_dir, const float light_radius, const vec3 P,
                          const vec3 I, const vec3 N, const lobe_masks_t lobe_masks,
                          sampler2D ltc_luts, const float sheen, const vec3 base_color, const vec3 sheen_color,
                          const vec3 spec_color, const vec3 clearcoat_color) {
    vec3 u = vec3(1.0, 0.0, 0.0);
    if (abs(light_dir.y) < 0.999) {
        u = vec3(0.0, 1.0, 0.0);
    }

    vec3 v = normalize(cross(u, light_dir));
    u = cross(light_dir, v);

    vec3 points[4];
    points[0] = P + light_dir + light_radius * u + light_radius * v;
    points[1] = P + light_dir + light_radius * u - light_radius * v;
    points[2] = P + light_dir - light_radius * u - light_radius * v;
    points[3] = P + light_dir - light_radius * u + light_radius * v;

    vec3 ret = vec3(0.0);

    if ((lobe_masks.bits & LOBE_DIFFUSE_BIT) != 0 && ENABLE_DIFFUSE != 0) {
        const vec3 dcol = base_color;

        vec3 diff = LTC_Evaluate_Disk(ltc_luts, 0.125, N, I, P, g_ltc[gl_LocalInvocationIndex].diff_t1, points, false);
        diff *= dcol * g_ltc[gl_LocalInvocationIndex].diff_t2.x;// + (1.0 - dcol) * g_ltc[gl_LocalInvocationIndex].diff_t2.y;

        if (sheen > 0.0 && ENABLE_SHEEN != 0) {
            const vec3 _sheen = LTC_Evaluate_Disk(ltc_luts, 0.375, N, I, P, g_ltc[gl_LocalInvocationIndex].sheen_t1, points, false);
            diff += _sheen * (sheen_color * g_ltc[gl_LocalInvocationIndex].sheen_t2.x + (1.0 - sheen_color) * g_ltc[gl_LocalInvocationIndex].sheen_t2.y);
        }

        ret += lobe_masks.diffuse_mul * light_color * diff;
    }
    if ((lobe_masks.bits & LOBE_SPECULAR_BIT) != 0 && ENABLE_SPECULAR != 0) {
        const vec3 scol = spec_color;

        vec3 spec = LTC_Evaluate_Disk(ltc_luts, 0.625, N, I, P, g_ltc[gl_LocalInvocationIndex].spec_t1, points, false);
        spec *= scol * g_ltc[gl_LocalInvocationIndex].spec_t2.x + (1.0 - scol) * g_ltc[gl_LocalInvocationIndex].spec_t2.y;

        ret += lobe_masks.specular_mul * light_color * spec;
    }
    if ((lobe_masks.bits & LOBE_CLEARCOAT_BIT) != 0 && ENABLE_CLEARCOAT != 0) {
        const vec3 ccol = clearcoat_color;

        vec3 coat = LTC_Evaluate_Disk(ltc_luts, 0.875, N, I, P, g_ltc[gl_LocalInvocationIndex].coat_t1, points, false);
        coat *= ccol * g_ltc[gl_LocalInvocationIndex].coat_t2.x + (1.0 - ccol) * g_ltc[gl_LocalInvocationIndex].coat_t2.y;

        ret += 0.25 * lobe_masks.specular_mul * light_color * coat;
    }

    return ret;
}
#endif

vec3 EvaluateSunLight_Approx(const vec3 light_color, const vec3 light_dir, const float light_radius,
                             const vec3 I, const vec3 N, const lobe_masks_t lobe_masks, const float roughness, const float clearcoat_roughness2,
                             const vec3 base_color, const vec3 sheen_color, const vec3 spec_color, const vec3 clearcoat_color) {
    vec3 ret = vec3(0.0);

    vec3 L = light_dir, H = normalize(I + L);
    float N_dot_V = abs(dot(N, I)) + 1e-5, L_dot_H = saturate(dot(L, H)), N_dot_H = saturate(dot(N, H)), N_dot_L = saturate(dot(N, L));
    if ((lobe_masks.bits & LOBE_DIFFUSE_BIT) != 0 && ENABLE_DIFFUSE != 0) {
        // NOTE: We assume point light as sun disk is small
        const vec3 diff = base_color * PrincipledDiffuse(roughness, N_dot_V, N_dot_L, L_dot_H) +
                          sheen_color * PrincipledSheen(N_dot_L, L_dot_H);
        ret += lobe_masks.diffuse_mul * N_dot_L * diff / M_PI;
    }
#if 0 // Representative point method (unused)
    vec3 R = reflect(-I, N);
    const float RdotN = dot(R, light_dir);
    if (RdotN < 0.0) {
        R = 10.0 * (R - light_dir * RdotN);
    } else {
        R = R / RdotN - light_dir;
    }
    L += R / max(1.0, length(R) / light_radius);
    L = normalize(L);
    // Update dot products
    H = normalize(I + L);
    N_dot_H = saturate(dot(N, H));
    N_dot_L = saturate(dot(N, L));
#endif
    if ((lobe_masks.bits & LOBE_SPECULAR_BIT) != 0 && ENABLE_SPECULAR != 0) {
        const float roughness2 = sqr(max(roughness, MIN_SPEC_ROUGHNESS));
        ret += lobe_masks.specular_mul * spec_color * N_dot_L * PrincipledSpecular(max(roughness2, 0.00001), N_dot_V, N_dot_L, N_dot_H);
    }
    if ((lobe_masks.bits & LOBE_CLEARCOAT_BIT) != 0 && ENABLE_CLEARCOAT != 0) {
        ret += 0.25 * lobe_masks.specular_mul * N_dot_L * clearcoat_color * PrincipledClearcoat(max(clearcoat_roughness2, 0.00001), N_dot_V, N_dot_L, N_dot_H);
    }

    ret *= light_color;

    return ret;
}

float spec_normalization(const float dist, const float light_dim, const float roughness){
	return sqr(roughness / saturate(roughness + 0.5 * saturate(light_dim / dist)));
}

vec3 EvaluateLightSource_Approx(const _light_item_t litem, const vec3 P, const vec3 I, const vec3 N, const lobe_masks_t lobe_masks,
                                const float roughness, const vec3 base_color, const vec3 spec_color) {
    const uint type = floatBitsToUint(litem.col_and_type.w) & LIGHT_TYPE_BITS;
    const vec3 from_light = normalize(P - litem.pos_and_radius.xyz);
    const float _dot = -dot(from_light, litem.dir_and_spot.xyz);
    const float _angle = approx_acos(_dot);
    if (type == LIGHT_TYPE_SPHERE && _angle > litem.dir_and_spot.w) {
        return vec3(0.0);
    } else if (type != LIGHT_TYPE_LINE && type != LIGHT_TYPE_SPHERE && _angle > (0.5 * M_PI)) {
        // Single-sided
        return vec3(0.0);
    }

    vec3 ret = vec3(0.0);

    const float sqr_dist = dot(litem.pos_and_radius.xyz - P, litem.pos_and_radius.xyz - P);
    const vec3 L = normalize(litem.pos_and_radius.xyz - P), H = normalize(I + L);
    const float N_dot_V = abs(dot(N, I)) + 1e-5, L_dot_H = saturate(dot(L, H)), N_dot_H = saturate(dot(N, H)), N_dot_L = saturate(dot(N, L));
    float brightness_mul = 1.0;

    if (type == LIGHT_TYPE_SPHERE && ENABLE_SPHERE_LIGHT != 0) {
        brightness_mul = litem.pos_and_radius.w * litem.pos_and_radius.w;
        if (litem.v_and_blend.w > 0.0) {
            brightness_mul *= saturate((litem.dir_and_spot.w - _angle) / sqr(litem.v_and_blend.w));
        }
        if ((lobe_masks.bits & LOBE_DIFFUSE_BIT) != 0 && ENABLE_DIFFUSE != 0) {
            ret += brightness_mul * lobe_masks.diffuse_mul * N_dot_L * base_color * litem.col_and_type.xyz * PrincipledDiffuse(roughness, N_dot_V, N_dot_L, L_dot_H) / (M_PI * max(0.001, sqr_dist));
        }
    } else if (type == LIGHT_TYPE_RECT && ENABLE_RECT_LIGHT != 0) {
        brightness_mul = length(litem.u_and_reg.xyz) * length(litem.v_and_blend.xyz);
        if ((lobe_masks.bits & LOBE_DIFFUSE_BIT) != 0 && ENABLE_DIFFUSE != 0) {
            vec3 points[4];
            points[0] = litem.pos_and_radius.xyz + litem.u_and_reg.xyz + litem.v_and_blend.xyz;
            points[1] = litem.pos_and_radius.xyz + litem.u_and_reg.xyz - litem.v_and_blend.xyz;
            points[2] = litem.pos_and_radius.xyz - litem.u_and_reg.xyz - litem.v_and_blend.xyz;
            points[3] = litem.pos_and_radius.xyz - litem.u_and_reg.xyz + litem.v_and_blend.xyz;
            const float solid_angle = RectangleSolidAngle(P, points);
            const vec3 diff = base_color * solid_angle * 0.2 * (saturate(dot(normalize(points[0] - P), N)) +
                                                                saturate(dot(normalize(points[1] - P), N)) +
                                                                saturate(dot(normalize(points[2] - P), N)) +
                                                                saturate(dot(normalize(points[3] - P), N)) +
                                                                saturate(dot(normalize(litem.pos_and_radius.xyz - P), N)));
            ret += lobe_masks.diffuse_mul * litem.col_and_type.xyz * diff / (4.0 * M_PI);
        }
        const float tan_half_spread = litem.dir_and_spot.w, spread_normalization = litem.v_and_blend.w;
        if (spread_normalization > 0.0) {
            vec2 tuv = vec2(dot(litem.u_and_reg.xyz, P - litem.pos_and_radius.xyz) / length2(litem.u_and_reg.xyz),
                            dot(litem.v_and_blend.xyz, P - litem.pos_and_radius.xyz) / length2(litem.v_and_blend.xyz));
            tuv = clamp(tuv, vec2(-1.0), vec2(1.0));

            const vec3 closest_P = litem.pos_and_radius.xyz + tuv.x * litem.u_and_reg.xyz + tuv.y * litem.v_and_blend.xyz;
            const float att = spread_attenuation(normalize(P - closest_P), litem.dir_and_spot.xyz, tan_half_spread, spread_normalization);
            brightness_mul *= att;
            ret *= att;
        }
    } else if (type == LIGHT_TYPE_DISK && ENABLE_DISK_LIGHT != 0) {
        brightness_mul = 0.25 * M_PI * length(litem.u_and_reg.xyz) * length(litem.v_and_blend.xyz);
        if ((lobe_masks.bits & LOBE_DIFFUSE_BIT) != 0 && ENABLE_DIFFUSE != 0) {
            const float inv_solid_angle = sqr_dist / (length(litem.u_and_reg.xyz) * length(litem.v_and_blend.xyz));
            const float front_dot_l = saturate(dot(litem.dir_and_spot.xyz, L));
            ret += lobe_masks.diffuse_mul * base_color * litem.col_and_type.xyz * saturate(front_dot_l * N_dot_L / (1.0 + inv_solid_angle)) / 4.0;
        }
        const float tan_half_spread = litem.dir_and_spot.w, spread_normalization = litem.v_and_blend.w;
        if (spread_normalization > 0.0) {
            vec2 tuv = vec2(dot(litem.u_and_reg.xyz, P - litem.pos_and_radius.xyz) / length2(litem.u_and_reg.xyz),
                            dot(litem.v_and_blend.xyz, P - litem.pos_and_radius.xyz) / length2(litem.v_and_blend.xyz));
            tuv /= max(1.0 , length(tuv));

            const vec3 closest_P = litem.pos_and_radius.xyz + tuv.x * litem.u_and_reg.xyz + tuv.y * litem.v_and_blend.xyz;
            const float att = spread_attenuation(normalize(P - closest_P), litem.dir_and_spot.xyz, tan_half_spread, spread_normalization);
            brightness_mul *= att;
            ret *= att;
        }
    } else if (type == LIGHT_TYPE_LINE && ENABLE_LINE_LIGHT != 0) {
        brightness_mul = 0.004 * litem.pos_and_radius.w;
        if ((lobe_masks.bits & LOBE_DIFFUSE_BIT) != 0 && ENABLE_DIFFUSE != 0) {
            const vec3 L0 = litem.pos_and_radius.xyz + litem.v_and_blend.xyz - P;
            const vec3 L1 = litem.pos_and_radius.xyz - litem.v_and_blend.xyz - P;
            const vec2 len_sqr = vec2(dot(L0, L0), dot(L1, L1));
            const vec2 inv_len = inversesqrt(len_sqr);
            const vec2 len = len_sqr * inv_len;
            const float N_dot_L = saturate(0.5 * (dot(N, L0) * inv_len.x + dot(N, L1) * inv_len.y));
            ret += brightness_mul * lobe_masks.diffuse_mul * base_color * litem.col_and_type.xyz * N_dot_L / max(0.001, 0.5 * (len.x * len.y + dot(L0, L1)));
        }
    }

    if ((lobe_masks.bits & (LOBE_SPECULAR_BIT | LOBE_REFRACTION_BIT)) != 0 && ENABLE_SPECULAR != 0) {
        const float dist = sqrt(sqr_dist);
        const float roughness_mod = saturate(roughness + litem.pos_and_radius.w / (3.0 * dist));
        const float roughness2 = sqr(max(roughness_mod, MIN_SPEC_ROUGHNESS));
        const vec3 spec = spec_color * PrincipledSpecular(roughness2, N_dot_V, N_dot_L, N_dot_H);
        ret += lobe_masks.specular_mul * litem.col_and_type.xyz * brightness_mul * N_dot_L * spec / max(0.001, sqr_dist);
    }

    return ret;
}

#endif // PRINCIPLED_COMMON_GLSL
