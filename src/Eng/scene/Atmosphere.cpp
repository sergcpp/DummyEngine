#include "Atmosphere.h"

#include <Ren/MMat.h>

namespace Eng {
// Math
float fast_exp(float x) {
    union {
        float f;
        int32_t i;
    } reinterpreter = {};
    reinterpreter.i = (int32_t)(12102203.0f * x) + 127 * (1 << 23);
    int32_t m = (reinterpreter.i >> 7) & 0xFFFF; // copy mantissa
    // empirical values for small maximum relative error (1.21e-5):
    reinterpreter.i += (((((((((((3537 * m) >> 16) + 13668) * m) >> 18) + 15817) * m) >> 14) - 80470) * m) >> 11);
    return reinterpreter.f;
}

Ren::Vec4f fast_exp(Ren::Vec4f x) {
    Ren::Vec4f ret;
    ret[0] = fast_exp(x[0]);
    ret[1] = fast_exp(x[1]);
    ret[2] = fast_exp(x[2]);
    ret[3] = 1.0f;
    return ret;
}

Ren::Vec2f SphereIntersection(Ren::Vec4f ray_start, const Ren::Vec4f &ray_dir, const Ren::Vec4f &sphere_center,
                              const float sphere_radius) {
    ray_start -= sphere_center;
    const float a = Dot(ray_dir, ray_dir);
    const float b = 2.0f * Dot(ray_start, ray_dir);
    const float c = Dot(ray_start, ray_start) - (sphere_radius * sphere_radius);
    float d = b * b - 4 * a * c;
    if (d < 0) {
        return Ren::Vec2f{-1};
    } else {
        d = sqrtf(d);
        return Ren::Vec2f{-b - d, -b + d} / (2 * a);
    }
}

Ren::Vec2f PlanetIntersection(const atmosphere_params_t &params, const Ren::Vec4f &ray_start, const Ren::Vec4f &ray_dir) {
    const Ren::Vec4f planet_center = Ren::Vec4f(0, -params.planet_radius, 0, 0);
    return SphereIntersection(ray_start, ray_dir, planet_center, params.planet_radius);
}

Ren::Vec2f AtmosphereIntersection(const atmosphere_params_t &params, const Ren::Vec4f &ray_start,
                                  const Ren::Vec4f &ray_dir) {
    const Ren::Vec4f planet_center = Ren::Vec4f(0, -params.planet_radius, 0, 0);
    return SphereIntersection(ray_start, ray_dir, planet_center, params.planet_radius + params.atmosphere_height);
}

// Phase functions
float PhaseRayleigh(const float costh) { return 3 * (1 + costh * costh) / (16 * Ren::Pi<float>()); }
float PhaseMie(const float costh, float g = 0.85f) {
    g = fminf(g, 0.9381f);
    float k = 1.55f * g - 0.55f * g * g * g;
    float kcosth = k * costh;
    return (1 - k * k) / ((4 * Ren::Pi<float>()) * (1 - kcosth) * (1 - kcosth));
}

// Atmosphere
float AtmosphereHeight(const atmosphere_params_t &params, const Ren::Vec4f &position_ws, Ren::Vec4f &up_vector) {
    const Ren::Vec4f planet_center = Ren::Vec4f(0, -params.planet_radius, 0, 0);
    up_vector = (position_ws - planet_center);
    const float height = Length(up_vector);
    up_vector /= height;
    return height - params.planet_radius;
}

Ren::Vec4f AtmosphereDensity(const atmosphere_params_t &params, const float h) {
    const float density_rayleigh = fast_exp(-fmaxf(0.0f, h / params.rayleigh_height));
    const float density_mie = fast_exp(-fmaxf(0.0f, h / params.mie_height));
    const float density_ozone = fmaxf(0.0f, 1.0f - fabsf(h - params.ozone_height_center) / params.ozone_half_width);
    return params.atmosphere_density * Ren::Vec4f{density_rayleigh, density_mie, density_ozone, 0.0f};
}

struct atmosphere_medium_t {
    Ren::Vec4f scattering;
    Ren::Vec4f absorption;
    Ren::Vec4f extinction;

    Ren::Vec4f scattering_mie;
    Ren::Vec4f absorption_mie;
    Ren::Vec4f extinction_mie;

    Ren::Vec4f scattering_ray;
    Ren::Vec4f absorption_ray;
    Ren::Vec4f extinction_ray;

    Ren::Vec4f scattering_ozo;
    Ren::Vec4f absorption_ozo;
    Ren::Vec4f extinction_ozo;
};

atmosphere_medium_t SampleAtmosphereMedium(const atmosphere_params_t &params, const float h) {
    const Ren::Vec4f local_density = AtmosphereDensity(params, h);

    atmosphere_medium_t s;

    s.scattering_mie = local_density[1] * params.mie_scattering;
    s.absorption_mie = local_density[1] * params.mie_absorption;
    s.extinction_mie = local_density[1] * params.mie_extinction;

    s.scattering_ray = local_density[0] * params.rayleigh_scattering;
    s.absorption_ray = 0.0f;
    s.extinction_ray = s.scattering_ray + s.absorption_ray;

    s.scattering_ozo = 0.0;
    s.absorption_ozo = local_density[2] * params.ozone_absorption;
    s.extinction_ozo = s.scattering_ozo + s.absorption_ozo;

    s.scattering = s.scattering_mie + s.scattering_ray + s.scattering_ozo;
    s.absorption = s.absorption_mie + s.absorption_ray + s.absorption_ozo;
    s.extinction = s.extinction_mie + s.extinction_ray + s.extinction_ozo;
    s.extinction[3] = 1.0f; // make it safe divisor

    return s;
}

Ren::Vec4f SampleTransmittanceLUT(Ren::Span<const Ren::Vec4f> lut, Ren::Vec2f uv) {
    uv = uv * Ren::Vec2f(float(SKY_TRANSMITTANCE_LUT_W), float(SKY_TRANSMITTANCE_LUT_H));
    auto iuv0 = Ren::Vec2i(uv);
    iuv0 = Clamp(iuv0, Ren::Vec2i{0}, Ren::Vec2i{SKY_TRANSMITTANCE_LUT_W - 1, SKY_TRANSMITTANCE_LUT_H - 1});
    const Ren::Vec2i iuv1 = Min(iuv0 + 1, Ren::Vec2i{SKY_TRANSMITTANCE_LUT_W - 1, SKY_TRANSMITTANCE_LUT_H - 1});

    const auto tr00 = lut[iuv0[1] * SKY_TRANSMITTANCE_LUT_W + iuv0[0]],
               tr01 = lut[iuv0[1] * SKY_TRANSMITTANCE_LUT_W + iuv1[0]],
               tr10 = lut[iuv1[1] * SKY_TRANSMITTANCE_LUT_W + iuv0[0]],
               tr11 = lut[iuv1[1] * SKY_TRANSMITTANCE_LUT_W + iuv1[0]];

    const Ren::Vec2f k = Fract(uv);

    const Ren::Vec4f tr0 = tr01 * k[0] + tr00 * (1.0f - k[0]), tr1 = tr11 * k[0] + tr10 * (1.0f - k[0]);

    return (tr1 * k[1] + tr0 * (1.0f - k[1]));
}

} // namespace Eng

Ren::Vec4f Eng::IntegrateOpticalDepth(const atmosphere_params_t &params, const Ren::Vec4f &ray_start,
                                      const Ren::Vec4f &ray_dir) {
    Ren::Vec2f intersection = AtmosphereIntersection(params, ray_start, ray_dir);
    float ray_length = intersection[1];

    const int SampleCount = 64;
    float step_size = ray_length / SampleCount;

    Ren::Vec4f optical_depth = 0.0f;

    for (int i = 0; i < SampleCount; i++) {
        Ren::Vec4f local_pos = ray_start + ray_dir * (i + 0.5f) * step_size, up_vector;
        const float local_height = AtmosphereHeight(params, local_pos, up_vector);
        const atmosphere_medium_t medium = SampleAtmosphereMedium(params, local_height);
        optical_depth += medium.extinction * step_size;
    }

    return optical_depth;
}

void Eng::UvToLutTransmittanceParams(const atmosphere_params_t &params, const Ren::Vec2f uv, float &view_height,
                                     float &view_zenith_cos_angle) {
    const float top_radius = params.planet_radius + params.atmosphere_height;

    const float x_mu = uv[0], x_r = uv[1];

    const float H = sqrtf(top_radius * top_radius - params.planet_radius * params.planet_radius);
    const float rho = H * x_r;
    view_height = sqrtf(rho * rho + params.planet_radius * params.planet_radius);

    const float d_min = top_radius - view_height;
    const float d_max = rho + H;
    const float d = d_min + x_mu * (d_max - d_min);
    view_zenith_cos_angle = d == 0.0f ? 1.0f : (H * H - rho * rho - d * d) / (2.0f * view_height * d);
    view_zenith_cos_angle = Ren::Clamp(view_zenith_cos_angle, -1.0f, 1.0f);
}

Ren::Vec2f Eng::LutTransmittanceParamsToUv(const atmosphere_params_t &params, const float view_height,
                                           const float view_zenith_cos_angle) {
    const float top_radius = params.planet_radius + params.atmosphere_height;

    const float H = sqrtf(fmaxf(0.0f, top_radius * top_radius - params.planet_radius * params.planet_radius));
    const float rho = sqrtf(fmaxf(0.0f, view_height * view_height - params.planet_radius * params.planet_radius));

    const float discriminant =
        view_height * view_height * (view_zenith_cos_angle * view_zenith_cos_angle - 1.0f) + top_radius * top_radius;
    const float d =
        fmaxf(0.0f, (-view_height * view_zenith_cos_angle + sqrtf(discriminant))); // Distance to atmosphere boundary

    const float d_min = top_radius - view_height;
    const float d_max = rho + H;
    const float x_mu = (d - d_min) / (d_max - d_min);
    const float x_r = rho / H;

    return Ren::Vec2f{x_mu, x_r};
}

template <bool UniformPhase>
std::pair<Ren::Vec4f, Ren::Vec4f>
Eng::IntegrateScatteringMain(const atmosphere_params_t &params, const Ren::Vec4f &ray_start, const Ren::Vec4f &ray_dir,
                             float ray_length, const Ren::Vec4f &light_dir, const Ren::Vec4f &moon_dir,
                             const Ren::Vec4f &light_color, Ren::Span<const Ren::Vec4f> transmittance_lut,
                             Ren::Span<const float> multiscatter_lut, const float rand_offset, const int sample_count,
                             Ren::Vec4f &inout_transmittance) {
    const Ren::Vec2f atm_intersection = AtmosphereIntersection(params, ray_start, ray_dir);
    ray_length = fminf(ray_length, atm_intersection[1]);
    const Ren::Vec2f planet_intersection = PlanetIntersection(params, ray_start, ray_dir);
    if (planet_intersection[0] > 0) {
        ray_length = fminf(ray_length, planet_intersection[0]);
    }

    const float costh = Dot(ray_dir, light_dir);
    const float phase_r = PhaseRayleigh(costh), phase_m = PhaseMie(costh);

    const float moon_costh = Dot(ray_dir, moon_dir);
    const float moon_phase_r = PhaseRayleigh(moon_costh), moon_phase_m = PhaseMie(moon_costh);

    const float phase_uniform = 1.0f / (4.0f * Ren::Pi<float>());

    Ren::Vec4f radiance = 0.0f, multiscat_as_1 = 0.0f;

    //
    // Atmosphere
    //
    const float step_size = ray_length / float(sample_count);
    float ray_time = 0.1f * rand_offset * step_size;
    for (int i = 0; i < sample_count; ++i) {
        const Ren::Vec4f local_position = ray_start + ray_dir * ray_time;
        Ren::Vec4f up_vector;
        const float local_height = AtmosphereHeight(params, local_position, up_vector);
        const atmosphere_medium_t medium = SampleAtmosphereMedium(params, local_height);
        const Ren::Vec4f optical_depth = medium.extinction * step_size;
        const Ren::Vec4f local_transmittance = fast_exp(-optical_depth);

        Ren::Vec4f S = 0.0f;

        if (light_dir[1] > -0.025f) {
            // main light contribution
            const float view_zenith_cos_angle = Dot(light_dir, up_vector);
            const Ren::Vec2f uv =
                LutTransmittanceParamsToUv(params, local_height + params.planet_radius, view_zenith_cos_angle);
            const Ren::Vec4f light_transmittance = SampleTransmittanceLUT(transmittance_lut, uv);

            const Ren::Vec2f _planet_intersection = PlanetIntersection(params, local_position, light_dir);
            const float planet_shadow = _planet_intersection[0] > 0 ? 0.0f : 1.0f;

            Ren::Vec4f multiscattered_lum = 0.0f;
            if (!multiscatter_lut.empty()) {
                /*Ren::Vec2f uv = Ren::Saturate(
                    Ren::Vec2f(view_zenith_cos_angle * 0.5f + 0.5f, local_height / params.atmosphere_height));
                uv = Ren::Vec2f(from_unit_to_sub_uvs(uv[0], SKY_MULTISCATTER_LUT_RES),
                                from_unit_to_sub_uvs(uv[1], SKY_MULTISCATTER_LUT_RES));

                multiscattered_lum = SampleMultiscatterLUT(multiscatter_lut, uv);*/
            }

            const Ren::Vec4f phase_times_scattering =
                UniformPhase ? medium.scattering * phase_uniform
                             : medium.scattering_ray * phase_r + medium.scattering_mie * phase_m;
            S += (planet_shadow * light_transmittance * phase_times_scattering +
                  multiscattered_lum * medium.scattering) *
                 light_color;
        } else if (params.moon_radius > 0.0f) {
            // moon reflection contribution  (totally fake)
            const float view_zenith_cos_angle = Dot(moon_dir, up_vector);
            const Ren::Vec2f uv =
                LutTransmittanceParamsToUv(params, local_height + params.planet_radius, view_zenith_cos_angle);
            const Ren::Vec4f light_transmittance = SampleTransmittanceLUT(transmittance_lut, uv);

            Ren::Vec4f multiscattered_lum = 0.0f;
            if (!multiscatter_lut.empty()) {
                /*fvec2 uv =
                    saturate(fvec2(view_zenith_cos_angle * 0.5f + 0.5f, local_height / params.atmosphere_height));
                uv = fvec2(from_unit_to_sub_uvs(uv.get<0>(), SKY_MULTISCATTER_LUT_RES),
                           from_unit_to_sub_uvs(uv.get<1>(), SKY_MULTISCATTER_LUT_RES));

                multiscattered_lum = SampleMultiscatterLUT(multiscatter_lut, uv);*/
            }

            const Ren::Vec4f phase_times_scattering =
                medium.scattering_ray * moon_phase_r + medium.scattering_mie * moon_phase_m;
            S += _SKY_MOON_SUN_RELATION *
                 (light_transmittance * phase_times_scattering + multiscattered_lum * medium.scattering) * light_color;
        }

        // 1 is the integration of luminance over the 4pi of a sphere, and assuming an isotropic phase function
        // of 1.0/(4*PI)
        const Ren::Vec4f MS = medium.scattering * 1.0f;
        const Ren::Vec4f MS_int = (MS - MS * local_transmittance) / medium.extinction;
        multiscat_as_1 += inout_transmittance * MS_int;

        const Ren::Vec4f S_int = (S - S * local_transmittance) / medium.extinction;
        radiance += inout_transmittance * S_int;

        inout_transmittance *= local_transmittance;

        ray_time += step_size;
    }

    //
    // Ground 'floor'
    //
    if (planet_intersection[0] > 0) {
        const Ren::Vec4f local_position = ray_start + ray_dir * planet_intersection[0];
        Ren::Vec4f up_vector;
        const float local_height = AtmosphereHeight(params, local_position, up_vector);

        const float view_zenith_cos_angle = Dot(light_dir, up_vector);
        const Ren::Vec2f uv =
            LutTransmittanceParamsToUv(params, local_height + params.planet_radius, view_zenith_cos_angle);
        const Ren::Vec4f light_transmittance = SampleTransmittanceLUT(transmittance_lut, uv);
        radiance += params.ground_albedo * saturate(Dot(up_vector, light_dir)) * inout_transmittance *
                    light_transmittance * light_color;
    }

    return {radiance, multiscat_as_1};
}

template std::pair<Ren::Vec4f, Ren::Vec4f> Eng::IntegrateScatteringMain<false>(
    const atmosphere_params_t &params, const Ren::Vec4f &ray_start, const Ren::Vec4f &ray_dir, float ray_length,
    const Ren::Vec4f &light_dir, const Ren::Vec4f &moon_dir, const Ren::Vec4f &light_color,
    Ren::Span<const Ren::Vec4f> transmittance_lut, Ren::Span<const float> multiscatter_lut, float rand_offset,
    int sample_count, Ren::Vec4f &inout_transmittance);
template std::pair<Ren::Vec4f, Ren::Vec4f> Eng::IntegrateScatteringMain<true>(
    const atmosphere_params_t &params, const Ren::Vec4f &ray_start, const Ren::Vec4f &ray_dir, float ray_length,
    const Ren::Vec4f &light_dir, const Ren::Vec4f &moon_dir, const Ren::Vec4f &light_color,
    Ren::Span<const Ren::Vec4f> transmittance_lut, Ren::Span<const float> multiscatter_lut, float rand_offset,
    int sample_count, Ren::Vec4f &inout_transmittance);