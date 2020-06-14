#include "Renderer.h"

#include <random>

namespace RendererInternal {
float RadicalInverse_VdC(uint32_t bits) {
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);

    return float(bits) * 2.3283064365386963e-10f; // / 0x100000000
}

Ren::Vec2f Hammersley2D(const int i, const int N) {
    return Ren::Vec2f{float(i) / float(N), RadicalInverse_VdC((uint32_t)i)};
}

Ren::Vec3f ImportanceSampleGGX(const Ren::Vec2f &Xi, const float roughness,
                               const Ren::Vec3f &N) {
    const float a = roughness * roughness;

    const float Phi = 2.0f * Ren::Pi<float>() * Xi[0];
    const float CosTheta = std::sqrt((1.0f - Xi[1]) / (1.0f + (a * a - 1.0f) * Xi[1]));
    const float SinTheta = std::sqrt(1.0f - CosTheta * CosTheta);

    const auto H =
        Ren::Vec3f{SinTheta * std::cos(Phi), SinTheta * std::sin(Phi), CosTheta};

    const Ren::Vec3f up =
        std::abs(N[1]) < 0.999f ? Ren::Vec3f{0.0, 1.0, 0.0} : Ren::Vec3f{1.0, 0.0, 0.0};
    const Ren::Vec3f TangentX = Ren::Normalize(Ren::Cross(up, N));
    const Ren::Vec3f TangentY = Ren::Cross(N, TangentX);
    // Tangent to world space
    return TangentX * H[0] + TangentY * H[1] + N * H[2];
}

float GeometrySchlickGGX(const float NdotV, const float k) {
    const float nom = NdotV;
    const float denom = NdotV * (1.0f - k) + k;

    return nom / denom;
}

float GeometrySmith(const Ren::Vec3f &N, const Ren::Vec3f &V, const Ren::Vec3f &L,
                    const float k) {
    const float NdotV = std::max(Ren::Dot(N, V), 0.0f);
    const float NdotL = std::max(Ren::Dot(N, L), 0.0f);
    const float ggx1 = GeometrySchlickGGX(NdotV, k);
    const float ggx2 = GeometrySchlickGGX(NdotL, k);

    return ggx1 * ggx2;
}

float G1V_Epic(const float roughness, const float n_dot_v) {
    const float k = roughness * roughness;
    return n_dot_v / (n_dot_v * (1.0f - k) + k);
}

float G_Smith(const float roughness, const float n_dot_v, const float n_dot_l) {
    return G1V_Epic(roughness, n_dot_v) * G1V_Epic(roughness, n_dot_v);
}

Ren::Vec2f IntegrateBRDF(const float NdotV, const float roughness) {
    const auto V = Ren::Vec3f{std::sqrt(1.0f - NdotV * NdotV), 0.0f, NdotV};

    float A = 0.0f;
    float B = 0.0f;

    const auto N = Ren::Vec3f{0.0f, 0.0f, 1.0f};

    const int SampleCount = 1024;
    for (int i = 0; i < SampleCount; ++i) {
        const Ren::Vec2f Xi = Hammersley2D(i, SampleCount);
        const Ren::Vec3f H = ImportanceSampleGGX(Xi, roughness, N);
        const Ren::Vec3f L = Ren::Normalize(2.0f * Ren::Dot(V, H) * H - V);

        const float NdotL = std::max(L[2], 0.0f);
        const float NdotH = std::max(H[2], 0.0f);
        const float VdotH = std::max(Ren::Dot(V, H), 0.0f);

        if (NdotL > 0.0f) {
            const float G = G1V_Epic(roughness, NdotV);
            const float G_Vis = (G * VdotH) / (NdotH * NdotV);
            const float Fc = std::pow(1.0f - VdotH, 5.0f);

            A += (1.0f - Fc) * G_Vis;
            B += Fc * G_Vis;
        }
    }

    return Ren::Vec2f{A, B} / float(SampleCount);
}

int16_t f32_to_s16(float value);

} // namespace RendererInternal

std::unique_ptr<uint16_t[]> Renderer::Generate_BRDF_LUT(const int res,
                                                        std::string &out_c_header) {
    std::unique_ptr<uint16_t[]> img_data_rg16(new uint16_t[res * res * 2]);

    out_c_header +=
        "static const uint32_t __brdf_lut_res = " + std::to_string(res) + ";\n";
    out_c_header += "static const uint16_t __brdf_lut[] = {\n";

    for (int j = 0; j < res; j++) {
        out_c_header += '\t';
        for (int i = 0; i < res; i++) {
            const Ren::Vec2f val = RendererInternal::IntegrateBRDF(
                (float(i) + 0.5f) / float(res), (float(j) + 0.5f) / float(res));

            const uint16_t r =
                (uint16_t)std::min(std::max(int(val[0] * 65535), 0), 65535);
            const uint16_t g =
                (uint16_t)std::min(std::max(int(val[1] * 65535), 0), 65535);

            out_c_header += std::to_string(r) + ", " + std::to_string(g) + ", ";

            img_data_rg16[2 * (j * res + i) + 0] = r;
            img_data_rg16[2 * (j * res + i) + 1] = g;
        }
        out_c_header += '\n';
    }

    out_c_header += "};\n";

    return img_data_rg16;
}

std::unique_ptr<int8_t[]> Renderer::Generate_PeriodicPerlin(const int res,
                                                            std::string &out_c_header) {
    std::unique_ptr<int8_t[]> img_data(new int8_t[res * res * 4]);

    for (int y = 0; y < res; y++) {
        for (int x = 0; x < res; x++) {
            const float norm_x = float(x) / float(res), norm_y = float(y) / float(res);

            for (int i = 0; i < 4; i++) {
                const float scale = 8.0f;
                auto coord = Ren::Vec4f{norm_x * scale, float(i) * 1.0f, norm_y * scale, 1.0f};

                float fval = 0.0f;

#if 1
                fval +=
                    0.33f * (0.5f + 0.5f * Ren::PerlinNoise(0.25f * coord,
                                                            Ren::Vec4f{0.25f * scale}));
                fval +=
                    0.66f * (0.5f + 0.5f * Ren::PerlinNoise(0.5f * coord,
                                                            Ren::Vec4f{0.25f * scale}));
                fval += 1.0f * (0.5f + 0.5f * Ren::PerlinNoise(coord, Ren::Vec4f{scale}));
                fval +=
                    0.66f * (0.5f + 0.5f * Ren::PerlinNoise(2.0f * coord,
                                                            Ren::Vec4f{2.0f * scale}));
                fval +=
                    0.33f * (0.5f + 0.5f * Ren::PerlinNoise(4.0f * coord,
                                                            Ren::Vec4f{4.0f * scale}));

                fval /= (1.0f + 2.0f * 0.66f + 2.0f * 0.33f);
#elif 0
                fval +=
                    0.25f * (0.5f + 0.5f * Ren::PerlinNoise(0.125f * coord,
                                                            Ren::Vec4f{0.125f * scale}));
                fval +=
                    0.5f * (0.5f + 0.5f * Ren::PerlinNoise(0.25f * coord,
                                                           Ren::Vec4f{0.25f * scale}));
                fval +=
                    0.75f * (0.5f + 0.5f * Ren::PerlinNoise(0.5f * coord,
                                                            Ren::Vec4f{0.5f * scale}));
                fval += 1.0f * (0.5f + 0.5f * Ren::PerlinNoise(coord, Ren::Vec4f{scale}));
                fval +=
                    0.75f * (0.5f + 0.5f * Ren::PerlinNoise(2.0f * coord,
                                                            Ren::Vec4f{2.0f * scale}));
                fval += 0.5f * (0.5f + 0.5f * Ren::PerlinNoise(4.0f * coord,
                                                               Ren::Vec4f{4.0f * scale}));
                fval +=
                    0.25f * (0.5f + 0.5f * Ren::PerlinNoise(8.0f * coord,
                                                            Ren::Vec4f{8.0f * scale}));

                fval /= (1.0f + 2.0f * 0.75f + 2.0f * 0.5f + 2.0f * 0.25f);
#else
                fval += 0.5f * (0.5f + 0.5f * Ren::PerlinNoise(0.5f * coord,
                                                               Ren::Vec4f{0.5f * scale}));
                fval += 1.0f * (0.5f + 0.5f * Ren::PerlinNoise(coord, Ren::Vec4f{scale}));
                fval += 0.5f * (0.5f + 0.5f * Ren::PerlinNoise(2.0f * coord,
                                                               Ren::Vec4f{2.0f * scale}));

                fval /= (1.0f + 2.0f * 0.5f);
#endif
                img_data[4 * (y * res + x) + i] = (int8_t)std::min(
                    std::max(int((2.0f * fval - 1.0f) * 127.0f), -128), 127);
            }
        }
    }

    std::string str;

    str += "static const uint32_t __noise_res = " + std::to_string(res) + ";\n";
    str += "static const int8_t __noise[] = {\n";

    for (int y = 0; y < res; y++) {
        str += '\t';
        for (int x = 0; x < res; x++) {
            str += std::to_string(img_data[4 * (y * res + x) + 0]) + ", " +
                   std::to_string(img_data[4 * (y * res + x) + 1]) + ", " +
                   std::to_string(img_data[4 * (y * res + x) + 2]) + ", " +
                   std::to_string(img_data[4 * (y * res + x) + 3]) + ", ";
        }
        str += '\n';
    }

    str += "};\n";

    return img_data;
}

std::unique_ptr<uint8_t[]>
Renderer::Generate_SSSProfile_LUT(const int res, const int gauss_count,
                                  const float gauss_variances[],
                                  const Ren::Vec3f diffusion_weights[]) {
    std::unique_ptr<uint8_t[]> img_data(new uint8_t[res * res * 4]);

    auto gauss = [](const float v, const float r) -> float {
        // return (1.0f / (2.0f * Ren::Pi<float>() * v)) * std::exp(-(r * r) / (2.0f *
        // v));
        return std::exp(-(r * r) / (2.0f * v));
    };

    auto eval_gauss_sum = [&](const float r) -> Ren::Vec3f {
        Ren::Vec3f weighted_sum;
        for (int i = 0; i < gauss_count; i++) {
            const float val = gauss(gauss_variances[i], r);
            weighted_sum += val * diffusion_weights[i];
        }
        weighted_sum = Ren::Clamp(weighted_sum, Ren::Vec3f{0.0f}, Ren::Vec3f{1.0f});
        return weighted_sum;
    };

    auto linear_to_srgb = [](float x) -> float {
        if (x <= 0.0f) {
            return 0.0f;
        } else if (x >= 1.0f) {
            return 1.0f;
        } else if (x < 0.0031308f) {
            return x * 12.92f;
        } else {
            return std::pow(x, 1.0f / 2.4f) * 1.055f - 0.055f;
        }
    };

    const int SampleCount = 256;
    const float dx = 2.0f * Ren::Pi<float>() / float(SampleCount);

    for (int y = 0; y < res; y++) {
        const float py = (float(res - y) + 0.5f) / float(res);
        const float sphere_radius = 4.0f / py;

        Ren::Vec3f normalization_factor;
        for (int i = 0; i < SampleCount; i++) {
            const float angle_delta =
                Ren::Pi<float>() * (2.0f * float(i) / float(SampleCount) - 1.0f);
            const float r = 2.0f * sphere_radius * std::sin(angle_delta / 2.0f);
            normalization_factor += eval_gauss_sum(r) * dx;
        }

        for (int x = 0; x < res; x++) {
            const float N_dot_L = 2.0f * (float(x) + 0.5f) / float(res) - 1.0f;
            const float angle = std::acos(N_dot_L);

            Ren::Vec3f result;
            for (int i = 0; i < SampleCount; i++) {
                const float angle_delta =
                    Ren::Pi<float>() * (2.0f * float(i) / float(SampleCount) - 1.0f);
                const float r = 2.0f * sphere_radius * std::sin(angle_delta / 2.0f);
                result += Ren::Clamp(std::cos(angle + angle_delta), 0.0f, 1.0f) *
                          eval_gauss_sum(r) * dx;
            }
            result = Ren::Clamp(result / normalization_factor, Ren::Vec3f{0.0f},
                                Ren::Vec3f{1.0f});

            result[0] = linear_to_srgb(result[0]);
            result[1] = linear_to_srgb(result[1]);
            result[2] = linear_to_srgb(result[2]);

            img_data[4 * (y * res + x) + 0] = (uint8_t)(result[0] * 255);
            img_data[4 * (y * res + x) + 1] = (uint8_t)(result[1] * 255);
            img_data[4 * (y * res + x) + 2] = (uint8_t)(result[2] * 255);
            img_data[4 * (y * res + x) + 3] = 255;
        }
    }

    return img_data;
}

std::unique_ptr<int16_t[]> Renderer::Generate_RandDirs(const int res,
                                                       std::string &out_c_header) {
    using namespace RendererInternal;

    assert(res <= 16);
    float angles[256];

    for (int i = 0; i < res * res; i++) {
        angles[i] = Ren::Pi<float>() * (2.0f * float(i) / float(res * res) - 1.0f);
    }
    std::shuffle(std::begin(angles), std::begin(angles) + res * res,
                 std::default_random_engine(0)); // NOLINT

    std::unique_ptr<int16_t[]> out_data_rg16(new int16_t[2 * res * res]);

    out_c_header +=
        "static const uint32_t __rand_dirs_res = " + std::to_string(res) + ";\n";
    out_c_header += "static const int16_t __rand_dirs[] = {\n";

    for (int i = 0; i < res * res; i++) {
        const float ra = angles[i];

        out_data_rg16[2 * i + 0] = f32_to_s16(std::cos(ra));
        out_data_rg16[2 * i + 1] = f32_to_s16(std::sin(ra));

        out_c_header += std::to_string(out_data_rg16[2 * i + 0]) + ", " +
                        std::to_string(out_data_rg16[2 * i + 1]) + ",\n";
    }

    out_c_header += "};";

    return out_data_rg16;
}

std::unique_ptr<uint8_t[]> Renderer::Generate_ConeTraceLUT(const int resx, const int resy,
                                                           const float cone_angles[4],
                                                           std::string &out_c_header) {
    using namespace RendererInternal;

    std::unique_ptr<uint8_t[]> out_data(new uint8_t[4 * resx * resy]);

    const auto B = Ren::Vec3f{1.0f, 0.0f, 0.0f};

    auto intersect_sphere = [](const Ren::Vec3f &sph_pos, const float radius,
                               const Ren::Vec3f &o, const Ren::Vec3f &d) {
        const Ren::Vec3f L = sph_pos - o;
        const float tca = Ren::Dot(L, d);
        // if (tca < 0) return false;
        const float d2 = Ren::Dot(L, L) - tca * tca;
        if (d2 > radius * radius) {
            return false;
        }
        const float thc = std::sqrt(radius * radius - d2);

        float t0 = tca - thc;
        float t1 = tca + thc;

        if (t0 > t1) {
            std::swap(t0, t1);
        }

        if (t0 < 0.0f) {
            t0 = t1; // if t0 is negative, let's use t1 instead
            if (t0 < 0.0f) {
                return false; // both t0 and t1 are negative
            }
        }

        // t = t0;

        return true;
    };

    assert(resx == resy);
    out_c_header +=
        "static const uint32_t __cone_rt_lut_res = " + std::to_string(resx) + ";\n";
    out_c_header += "static const uint8_t __cone_rt_lut[] = {\n";

    for (int y = 0; y < resy; y++) {
        const float sin_omega = (float(y) + 0.5f) / float(resy - 0);
        const float tan_omega = (float)std::tan(std::asin((double)sin_omega));
        const float sph_dist = (y == 0) ? -1.5f : (/*sph_radius*/ 1.0f / tan_omega);

        for (int x = 0; x < resx; x++) {
            const float cos_phi = (float(x) + 0.5f) / float(resx - 0);
            const float phi = (float)std::acos((double)cos_phi);

            const Ren::Mat4f rot_matrix = Ren::Rotate(Ren::Mat4f{}, phi, B);

            auto cone_dir = Ren::Vec3f{0.0f, 1.0f, 0.0f};
            cone_dir = Ren::Normalize(Ren::Vec3f{
                rot_matrix * Ren::Vec4f{cone_dir[0], cone_dir[1], cone_dir[2], 0.0f}});

            const Ren::Vec3f T = Ren::Normalize(Ren::Cross(Ren::Vec3f{cone_dir}, B));

            out_c_header += '\t';

            for (int j = 0; j < 4; j++) {
                const float cone_angle = cone_angles[j];

                int occluded_rays = 0;

                const int SampleCount = 1024;
                for (int i = 0; i < SampleCount; i++) {
                    const Ren::Vec2f rnd = Hammersley2D(i, SampleCount);

                    const float z = rnd[0] * (1.0f - std::cos(cone_angle));
                    const float dir = std::sqrt(z);

                    const float cos_angle = std::cos(2.0f * Ren::Pi<float>() * rnd[1]);
                    const float sin_angle = std::sin(2.0f * Ren::Pi<float>() * rnd[1]);

                    const Ren::Vec3f ray_dir = Ren::Normalize(
                        dir * sin_angle * B + std::sqrt(1.0f - dir) * cone_dir +
                        dir * cos_angle * T);

                    if (intersect_sphere(Ren::Vec3f{0.0f, sph_dist, 0.0f}, 1.0f,
                                         Ren::Vec3f{0.0f, 0.0f, 0.0f}, ray_dir)) {
                        occluded_rays++;
                    }
                }

                const float occlusion =
                    float(SampleCount - occluded_rays) / float(SampleCount);

                out_data[4 * (y * resx + x) + j] = uint8_t(255 * occlusion);

                out_c_header += ' ';
                out_c_header += std::to_string(uint8_t(255 * occlusion));
                out_c_header += ',';
            }
        }

        out_c_header += '\n';
    }

    out_c_header += "};";

    return out_data;
}
