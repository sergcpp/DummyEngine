#pragma once

#include <Ren/MVec.h>
#include <Ren/Span.h>

namespace Eng {
struct atmosphere_params_t {
    float planet_radius = 6371000.0f;
    float viewpoint_height = 700.0f;
    float atmosphere_height = 100000.0f;
    float rayleigh_height = atmosphere_height * 0.08f;
    float mie_height = atmosphere_height * 0.012f;
    float clouds_height_beg = 2000.0f;
    float clouds_height_end = 2500.0f;
    float clouds_variety = 0.5f;
    float clouds_density = 0.5f;
    float clouds_offset_x = 0.0f;
    float clouds_offset_z = 0.0f;
    float clouds_flutter_x = 0.0f;
    float clouds_flutter_z = 0.0f;
    float cirrus_clouds_amount = 0.5f;
    float cirrus_clouds_height = 6000.0f;
    float ozone_height_center = 25000.0f;
    float ozone_half_width = 15000.0f;
    float atmosphere_density = 1.0f;
    float stars_brightness = 1.0f;
    float moon_radius = 1737400.0f;
    float moon_distance = 100000000.0f; // 363100000.0f;
    float _unused0 = 0.0f;
    float _unused1 = 0.0f;
    float _unused2 = 0.0f;
    Ren::Vec4f moon_dir = Ren::Vec4f{0.707f, 0.707f, 0.0f, 0.0f};
    Ren::Vec4f rayleigh_scattering = Ren::Vec4f{5.802f * 1e-6f, 13.558f * 1e-6f, 33.100f * 1e-6f, 0.0f};
    Ren::Vec4f mie_scattering = Ren::Vec4f{3.996f * 1e-6f, 3.996f * 1e-6f, 3.996f * 1e-6f, 0.0f};
    Ren::Vec4f mie_extinction = Ren::Vec4f{4.440f * 1e-6f, 4.440f * 1e-6f, 4.440f * 1e-6f, 0.0f};
    Ren::Vec4f mie_absorption = Ren::Vec4f{0.444f * 1e-6f, 0.444f * 1e-6f, 0.444f * 1e-6f, 0.0f};
    Ren::Vec4f ozone_absorption = Ren::Vec4f{0.650f * 1e-6f, 1.881f * 1e-6f, 0.085f * 1e-6f, 0.0f};
    Ren::Vec4f ground_albedo = Ren::Vec4f{0.05f, 0.05f, 0.05f, 0.0f};
};

const int SKY_TRANSMITTANCE_LUT_W = 256;
const int SKY_TRANSMITTANCE_LUT_H = 64;
const float _SKY_MOON_SUN_RELATION = 0.0000001f;

inline float clamp(const float val, const float min, const float max) {
    return val < min ? min : (val > max ? max : val);
}
inline float saturate(const float val) { return clamp(val, 0.0f, 1.0f); }

inline float from_unit_to_sub_uvs(const float u, const float resolution) {
    return (u + 0.5f / resolution) * (resolution / (resolution + 1.0f));
}
inline float from_sub_uvs_to_unit(const float u, const float resolution) {
    return (u - 0.5f / resolution) * (resolution / (resolution - 1.0f));
}

// Transmittance LUT function parameterisation from Bruneton 2017
// https://github.com/ebruneton/precomputed_atmospheric_scattering
void UvToLutTransmittanceParams(const atmosphere_params_t &params, Ren::Vec2f uv, float &view_height,
                                float &view_zenith_cos_angle);
Ren::Vec2f LutTransmittanceParamsToUv(const atmosphere_params_t &params, float view_height, float view_zenith_cos_angle);

Ren::Vec4f IntegrateOpticalDepth(const atmosphere_params_t &params, const Ren::Vec4f &ray_start,
                                 const Ren::Vec4f &ray_dir);

template <bool UniformPhase = false>
std::pair<Ren::Vec4f, Ren::Vec4f>
IntegrateScatteringMain(const atmosphere_params_t &params, const Ren::Vec4f &ray_start, const Ren::Vec4f &ray_dir,
                        float ray_length, const Ren::Vec4f &light_dir, const Ren::Vec4f &moon_dir,
                        const Ren::Vec4f &light_color, Ren::Span<const Ren::Vec4f> transmittance_lut,
                        Ren::Span<const float> multiscatter_lut, float rand_offset, int sample_count,
                        Ren::Vec4f &inout_transmittance);
} // namespace Eng