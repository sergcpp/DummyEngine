#include "Renderer.h"

#include <cstdio>

#include <Ren/Context.h>
#include <Ren/ResizableBuffer.h>
#include <Ren/utils/Utils.h>
#include <Sys/MonoAlloc.h>
#include <Sys/ScopeExit.h>
#include <Sys/ThreadPool.h>
#include <Sys/Time_.h>

#include "../utils/ShaderLoader.h"
#include "CDFUtils.h"
#include "Renderer_Names.h"
#include "executors/ExBuildAccStructures.h"
#include "executors/ExDebugOIT.h"
#include "executors/ExDebugProbes.h"
#include "executors/ExDebugRT.h"
#include "executors/ExDepthFill.h"
#include "executors/ExDepthHierarchy.h"
#include "executors/ExShadowColor.h"
#include "executors/ExShadowDepth.h"
#include "executors/ExSkinning.h"
#include "executors/ExUpdateAccBuffers.h"

#include <vtune/ittnotify.h>
extern __itt_domain *__g_itt_domain;

namespace RendererInternal {
bool bbox_test(const float p[3], const float bbox_min[3], const float bbox_max[3]);

int upper_power_of_two(int v) {
    int res = 1;
    while (res < v) {
        res *= 2;
    }
    return res;
}

float filter_box(float /*v*/, float /*width*/) { return 1.0f; }

float filter_gaussian(float v, float width) {
    v *= 6.0f / width;
    return expf(-2.0f * v * v);
}

float filter_blackman_harris(float v, float width) {
    v = 2.0f * Ren::Pi<float>() * (v / width + 0.5f);
    return 0.35875f - 0.48829f * cosf(v) + 0.14128f * cosf(2.0f * v) - 0.01168f * cosf(3.0f * v);
}

extern const Ren::Vec2f PMJSamples64[64] = {
    Ren::Vec2f{0.958420f, 0.616889f}, Ren::Vec2f{0.103091f, 0.467694f}, Ren::Vec2f{0.615132f, 0.080424f},
    Ren::Vec2f{0.304792f, 0.849100f}, Ren::Vec2f{0.743586f, 0.931359f}, Ren::Vec2f{0.499603f, 0.185085f},
    Ren::Vec2f{0.796234f, 0.362629f}, Ren::Vec2f{0.188610f, 0.646428f}, Ren::Vec2f{0.824160f, 0.782852f},
    Ren::Vec2f{0.162675f, 0.017184f}, Ren::Vec2f{0.644037f, 0.412660f}, Ren::Vec2f{0.389489f, 0.505277f},
    Ren::Vec2f{0.525000f, 0.734366f}, Ren::Vec2f{0.337707f, 0.266659f}, Ren::Vec2f{0.898768f, 0.221771f},
    Ren::Vec2f{0.016718f, 0.940399f}, Ren::Vec2f{0.770391f, 0.658983f}, Ren::Vec2f{0.241033f, 0.313640f},
    Ren::Vec2f{0.703191f, 0.128602f}, Ren::Vec2f{0.462169f, 0.877456f}, Ren::Vec2f{0.572541f, 0.816341f},
    Ren::Vec2f{0.274165f, 0.108939f}, Ren::Vec2f{0.991257f, 0.499376f}, Ren::Vec2f{0.071017f, 0.568060f},
    Ren::Vec2f{0.907936f, 0.978040f}, Ren::Vec2f{0.037836f, 0.216070f}, Ren::Vec2f{0.560729f, 0.302652f},
    Ren::Vec2f{0.364138f, 0.701545f}, Ren::Vec2f{0.685960f, 0.559922f}, Ren::Vec2f{0.415520f, 0.391381f},
    Ren::Vec2f{0.868236f, 0.035305f}, Ren::Vec2f{0.127385f, 0.774920f}, Ren::Vec2f{0.859127f, 0.519883f},
    Ren::Vec2f{0.153271f, 0.426705f}, Ren::Vec2f{0.671710f, 0.004346f}, Ren::Vec2f{0.432280f, 0.811949f},
    Ren::Vec2f{0.539022f, 0.955953f}, Ren::Vec2f{0.348504f, 0.244827f}, Ren::Vec2f{0.933120f, 0.260737f},
    Ren::Vec2f{0.056740f, 0.739383f}, Ren::Vec2f{0.983675f, 0.868506f}, Ren::Vec2f{0.091411f, 0.076221f},
    Ren::Vec2f{0.589910f, 0.437795f}, Ren::Vec2f{0.260761f, 0.609224f}, Ren::Vec2f{0.698111f, 0.636027f},
    Ren::Vec2f{0.438718f, 0.357869f}, Ren::Vec2f{0.761806f, 0.168467f}, Ren::Vec2f{0.229773f, 0.917371f},
    Ren::Vec2f{0.886230f, 0.717436f}, Ren::Vec2f{0.013440f, 0.284208f}, Ren::Vec2f{0.500464f, 0.198053f},
    Ren::Vec2f{0.318128f, 0.990740f}, Ren::Vec2f{0.627941f, 0.754441f}, Ren::Vec2f{0.403971f, 0.050069f},
    Ren::Vec2f{0.839805f, 0.389970f}, Ren::Vec2f{0.186524f, 0.546504f}, Ren::Vec2f{0.806219f, 0.903521f},
    Ren::Vec2f{0.211735f, 0.148988f}, Ren::Vec2f{0.720969f, 0.337133f}, Ren::Vec2f{0.476894f, 0.676505f},
    Ren::Vec2f{0.603472f, 0.588461f}, Ren::Vec2f{0.288023f, 0.473484f}, Ren::Vec2f{0.940450f, 0.121696f},
    Ren::Vec2f{0.113382f, 0.834329f}};

extern const int TaaSampleCountNormal = 16;
extern const int TaaSampleCountStatic = 64;

#include "precomputed/__brdf_lut.inl"
#include "precomputed/__cone_rt_lut.inl"
#include "precomputed/__ltc_clearcoat.inl"
#include "precomputed/__ltc_diffuse.inl"
#include "precomputed/__ltc_sheen.inl"
#include "precomputed/__ltc_specular.inl"
#include "precomputed/__noise.inl"
#include "precomputed/__pmj02_samples.inl"

// 1D blue noise, used for volumetrics
namespace stbn_1D_64spp {
#include "precomputed/__stbn_sampler_1D_64spp.inl"
}

// 2D blue noise, used for GI
namespace bn_2D_64spp_0 {
#include "precomputed/__bn_sampler_2D_64spp_0.inl"
}
namespace bn_2D_64spp_1 {
#include "precomputed/__bn_sampler_2D_64spp_1.inl"
}
namespace bn_2D_64spp_2 {
#include "precomputed/__bn_sampler_2D_64spp_2.inl"
}
namespace bn_2D_64spp_3 {
#include "precomputed/__bn_sampler_2D_64spp_3.inl"
}
namespace bn_2D_64spp_4 {
#include "precomputed/__bn_sampler_2D_64spp_4.inl"
}
namespace bn_2D_64spp_5 {
#include "precomputed/__bn_sampler_2D_64spp_5.inl"
}
namespace bn_2D_64spp_6 {
#include "precomputed/__bn_sampler_2D_64spp_6.inl"
}
namespace bn_2D_64spp_7 {
#include "precomputed/__bn_sampler_2D_64spp_7.inl"
}

__itt_string_handle *itt_exec_dr_str = __itt_string_handle_create("ExecuteDrawList");
} // namespace RendererInternal

// TODO: remove this coupling!!!
namespace SceneManagerInternal {
int WriteImage(const uint8_t *out_data, int w, int h, int channels, bool flip_y, bool is_rgbm, const char *name);
}

#define BBOX_POINTS(min, max)                                                                                          \
    (min)[0], (min)[1], (min)[2], (max)[0], (min)[1], (min)[2], (min)[0], (min)[1], (max)[2], (max)[0], (min)[1],      \
        (max)[2], (min)[0], (max)[1], (min)[2], (max)[0], (max)[1], (min)[2], (min)[0], (max)[1], (max)[2], (max)[0],  \
        (max)[1], (max)[2]

Eng::Renderer::Renderer(Ren::Context &ctx, ShaderLoader &sh, Random &rand, Sys::ThreadPool &threads)
    : ctx_(ctx), sh_(sh), rand_(rand), threads_(threads), shadow_splitter_(SHADOWMAP_WIDTH, SHADOWMAP_HEIGHT),
      fg_builder_(sh_, prim_draw_) {
    using namespace RendererInternal;

    // Culling is done in lower resolution
    swCullCtxInit(&cull_ctx_, 8 * std::max(ctx.w() / 32, 1), 4 * std::max(ctx.h() / 16, 1), 0.0f);

    /*{ // buffer used to sample probes
        FrameBuf::ColorAttachmentDesc desc;
        desc.format = Ren::eFormat::RGBA32F;
        desc.filter = Ren::eFilter::NoFilter;
        desc.wrap = Ren::eWrap::ClampToEdge;
        probe_sample_buf_ = FrameBuf("Probe sample", ctx_, 24, 8, &desc, 1, {}, 1, ctx.log());
    }*/

    static const uint8_t black[] = {0, 0, 0, 0}, white[] = {255, 255, 255, 255};

    { // dummy 1px textures
        Ren::ImgParams p;
        p.w = p.h = 1;
        p.format = Ren::eFormat::RGBA8;
        p.usage = Ren::Bitmask(Ren::eImgUsage::Transfer) | Ren::eImgUsage::Sampled;
        p.sampling.wrap = Ren::eWrap::ClampToEdge;

        dummy_black_ = ctx_.CreateImage(Ren::String{"dummy_black"}, black, p, ctx_.default_mem_allocs());
        assert(dummy_black_);

        dummy_white_ = ctx_.CreateImage(Ren::String{"dummy_white"}, white, p, ctx_.default_mem_allocs());
        assert(dummy_white_);
    }

    { // cone/sphere intersection LUT
        /*const int resx = 128, resy = 128;

        const float cone_angles[] = {
            16.0f * Ren::Pi<float>() / 180.0f, 32.0f * Ren::Pi<float>() / 180.0f,
            48.0f * Ren::Pi<float>() / 180.0f, 64.0f * Ren::Pi<float>() / 180.0f};

        std::string str;
        const std::unique_ptr<uint8_t[]> occ_data =
            Generate_ConeTraceLUT(resx, resy, cone_angles, str);

        SceneManagerInternal::WriteImage(&occ_data[0], resx, resy, 4,
                                         false, false, "cone_lut.uncompressed.png");
        //std::exit(0);*/

        Ren::ImgParams p;
        p.w = __cone_rt_lut_res;
        p.h = __cone_rt_lut_res;
        p.format = Ren::eFormat::RGBA8;
        p.usage = Ren::Bitmask(Ren::eImgUsage::Transfer) | Ren::eImgUsage::Sampled;
        p.sampling.filter = Ren::eFilter::Bilinear;
        p.sampling.wrap = Ren::eWrap::ClampToEdge;

        cone_rt_lut_ =
            ctx_.CreateImage(Ren::String{"cone_rt_lut"}, {&__cone_rt_lut[0], 4 * __cone_rt_lut_res * __cone_rt_lut_res},
                             p, ctx_.default_mem_allocs());
        assert(cone_rt_lut_);
    }

    {
        // std::string c_header;
        // const std::unique_ptr<uint16_t[]> img_data_rg16 = Generate_BRDF_LUT(256,
        // c_header);

        Ren::ImgParams p;
        p.w = p.h = __brdf_lut_res;
        p.format = Ren::eFormat::RG16;
        p.usage = Ren::Bitmask(Ren::eImgUsage::Transfer) | Ren::eImgUsage::Sampled;
        p.sampling.filter = Ren::eFilter::Bilinear;
        p.sampling.wrap = Ren::eWrap::ClampToEdge;

        brdf_lut_ = ctx_.CreateImage(Ren::String{"brdf_lut"}, {(const uint8_t *)&__brdf_lut[0], sizeof(__brdf_lut)}, p,
                                     ctx_.default_mem_allocs());
        assert(brdf_lut_);
    }

    { // LTC LUTs
        auto combined_data = std::make_unique<float[]>(8 * 4 * 64 * 64);
        float *_combined_data = combined_data.get();
        for (int y = 0; y < 64; ++y) {
            memcpy(_combined_data, &__ltc_diff_tex1[y * 4 * 64], 4 * 64 * sizeof(float));
            _combined_data += 4 * 64;
            memcpy(_combined_data, &__ltc_diff_tex2[y * 4 * 64], 4 * 64 * sizeof(float));
            _combined_data += 4 * 64;
            memcpy(_combined_data, &__ltc_sheen_tex1[y * 4 * 64], 4 * 64 * sizeof(float));
            _combined_data += 4 * 64;
            memcpy(_combined_data, &__ltc_sheen_tex2[y * 4 * 64], 4 * 64 * sizeof(float));
            _combined_data += 4 * 64;
            memcpy(_combined_data, &__ltc_specular_tex1[y * 4 * 64], 4 * 64 * sizeof(float));
            _combined_data += 4 * 64;
            memcpy(_combined_data, &__ltc_specular_tex2[y * 4 * 64], 4 * 64 * sizeof(float));
            _combined_data += 4 * 64;
            memcpy(_combined_data, &__ltc_clearcoat_tex1[y * 4 * 64], 4 * 64 * sizeof(float));
            _combined_data += 4 * 64;
            memcpy(_combined_data, &__ltc_clearcoat_tex2[y * 4 * 64], 4 * 64 * sizeof(float));
            _combined_data += 4 * 64;
        }

        Ren::ImgParams p;
        p.w = 8 * 64;
        p.h = 64;
        p.format = Ren::eFormat::RGBA32F;
        p.usage = Ren::Bitmask(Ren::eImgUsage::Transfer) | Ren::eImgUsage::Sampled;
        p.sampling.filter = Ren::eFilter::Bilinear;
        p.sampling.wrap = Ren::eWrap::ClampToEdge;

        ltc_luts_ = ctx_.CreateImage(Ren::String{"LTC LUTs"},
                                     {(const uint8_t *)combined_data.get(), 8 * 4 * 64 * 64 * sizeof(float)}, p,
                                     ctx_.default_mem_allocs());
        assert(ltc_luts_);
    }

    {
        /*const int res = 128;
        std::string c_header;
        const std::unique_ptr<int8_t[]> img_data = Generate_PeriodicPerlin(res, c_header);
        SceneManagerInternal::WriteImage((const uint8_t*)&img_data[0], res, res, 4,
        false, "test1.png");*/

        Ren::ImgParams p;
        p.w = p.h = __noise_res;
        p.format = Ren::eFormat::RGBA8_snorm;
        p.usage = Ren::Bitmask(Ren::eImgUsage::Transfer) | Ren::eImgUsage::Sampled;
        p.sampling.filter = Ren::eFilter::Bilinear;

        noise_ = ctx_.CreateImage(Ren::String{"noise"}, {(const uint8_t *)&__noise[0], __noise_res * __noise_res * 4},
                                  p, ctx_.default_mem_allocs());
        assert(noise_);
    }

    /*{
        const int res = 128;

        // taken from old GPU-Gems article
        const float gauss_variances[] = {
            0.0064f, 0.0484f, 0.187f, 0.567f, 1.99f, 7.41f
        };

        const Ren::Vec3f diffusion_weights[] = {
            Ren::Vec3f{ 0.233f, 0.455f, 0.649f },
            Ren::Vec3f{ 0.100f, 0.336f, 0.344f },
            Ren::Vec3f{ 0.118f, 0.198f, 0.0f },
            Ren::Vec3f{ 0.113f, 0.007f, 0.007f },
            Ren::Vec3f{ 0.358f, 0.004f, 0.0f },
            Ren::Vec3f{ 0.078f, 0.0f, 0.0f }
        };

        const std::unique_ptr<uint8_t[]> img_data = Generate_SSSProfile_LUT(res, 6,
        gauss_variances, diffusion_weights);
        SceneManagerInternal::WriteImage(&img_data[0], res, res, 4, false,
        "assets/textures/skin_diffusion.uncompressed.png");
    }*/

    { // STBN 1D sampler
        Ren::ImgParams p;
        p.w = stbn_1D_64spp::w;
        p.h = stbn_1D_64spp::h;
        p.d = stbn_1D_64spp::d;
        p.format = Ren::eFormat::R8;
        p.flags = Ren::eImgFlags::Array;
        p.usage = Ren::Bitmask(Ren::eImgUsage::Transfer) | Ren::eImgUsage::Sampled;
        p.sampling.filter = Ren::eFilter::Nearest;

        stbn_1D_64spp_ = ctx_.CreateImage(Ren::String{"STBN 1D 64spp"},
                                          {(const uint8_t *)&stbn_1D_64spp::stbn_samples[0], p.w * p.h * p.d}, p,
                                          ctx_.default_mem_allocs());
        assert(stbn_1D_64spp_);
    }

    { // PMJ 2D blue-noise sampler
        static const int SampleSizePerDimPair = 2 * 64 * sizeof(uint32_t);
        static const int ScramblingSizePerDimPair = 2 * 128 * 128 * sizeof(uint32_t);
        static const int SortingSizePerDimPair = 128 * 128 * sizeof(uint32_t);

        static_assert(SampleSizePerDimPair == sizeof(bn_2D_64spp_0::bn_pmj_samples));
        static_assert(SampleSizePerDimPair == sizeof(bn_2D_64spp_1::bn_pmj_samples));
        static_assert(SampleSizePerDimPair == sizeof(bn_2D_64spp_2::bn_pmj_samples));
        static_assert(SampleSizePerDimPair == sizeof(bn_2D_64spp_3::bn_pmj_samples));
        static_assert(SampleSizePerDimPair == sizeof(bn_2D_64spp_4::bn_pmj_samples));
        static_assert(SampleSizePerDimPair == sizeof(bn_2D_64spp_5::bn_pmj_samples));
        static_assert(SampleSizePerDimPair == sizeof(bn_2D_64spp_6::bn_pmj_samples));
        static_assert(SampleSizePerDimPair == sizeof(bn_2D_64spp_7::bn_pmj_samples));

        static_assert(ScramblingSizePerDimPair == sizeof(bn_2D_64spp_0::bn_pmj_scrambling));
        static_assert(ScramblingSizePerDimPair == sizeof(bn_2D_64spp_1::bn_pmj_scrambling));
        static_assert(ScramblingSizePerDimPair == sizeof(bn_2D_64spp_2::bn_pmj_scrambling));
        static_assert(ScramblingSizePerDimPair == sizeof(bn_2D_64spp_3::bn_pmj_scrambling));
        static_assert(ScramblingSizePerDimPair == sizeof(bn_2D_64spp_4::bn_pmj_scrambling));
        static_assert(ScramblingSizePerDimPair == sizeof(bn_2D_64spp_5::bn_pmj_scrambling));
        static_assert(ScramblingSizePerDimPair == sizeof(bn_2D_64spp_6::bn_pmj_scrambling));
        static_assert(ScramblingSizePerDimPair == sizeof(bn_2D_64spp_7::bn_pmj_scrambling));

        static_assert(SortingSizePerDimPair == sizeof(bn_2D_64spp_0::bn_pmj_sorting));
        static_assert(SortingSizePerDimPair == sizeof(bn_2D_64spp_1::bn_pmj_sorting));
        static_assert(SortingSizePerDimPair == sizeof(bn_2D_64spp_2::bn_pmj_sorting));
        static_assert(SortingSizePerDimPair == sizeof(bn_2D_64spp_3::bn_pmj_sorting));
        static_assert(SortingSizePerDimPair == sizeof(bn_2D_64spp_4::bn_pmj_sorting));
        static_assert(SortingSizePerDimPair == sizeof(bn_2D_64spp_5::bn_pmj_sorting));
        static_assert(SortingSizePerDimPair == sizeof(bn_2D_64spp_6::bn_pmj_sorting));
        static_assert(SortingSizePerDimPair == sizeof(bn_2D_64spp_7::bn_pmj_sorting));

        const uint32_t buf_size = 8 * (SampleSizePerDimPair + ScramblingSizePerDimPair + SortingSizePerDimPair);
        bn_pmj_2D_64spp_seq_buf_ = ctx_.CreateBuffer(Ren::String{"BN_PMJ_2D_64SPP"}, Ren::eBufType::Texture, buf_size);
        { // Add view
            const auto &[buf_main, buf_cold] = ctx_.storages().buffers[bn_pmj_2D_64spp_seq_buf_];
            Ren::Buffer_AddView(ctx_.api(), buf_main, buf_cold, Ren::eFormat::R32UI);
        }

        Ren::BufferMain stage_buf_main = {};
        Ren::BufferCold stage_buf_cold = {};
        if (!Ren::Buffer_Init(ctx_.api(), stage_buf_main, stage_buf_cold, Ren::String{"BN_PMJ_2D_64SPP_Stage"},
                              Ren::eBufType::Upload, buf_size, ctx_.log())) {
            // TODO: Properly handle failure
            assert(false);
        }

        { // init stage buf
            uint8_t *mapped_ptr = Buffer_Map(ctx_.api(), stage_buf_main, stage_buf_cold);

            // sample data
            memcpy(mapped_ptr, bn_2D_64spp_0::bn_pmj_samples, SampleSizePerDimPair);
            mapped_ptr += SampleSizePerDimPair;
            memcpy(mapped_ptr, bn_2D_64spp_1::bn_pmj_samples, SampleSizePerDimPair);
            mapped_ptr += SampleSizePerDimPair;
            memcpy(mapped_ptr, bn_2D_64spp_2::bn_pmj_samples, SampleSizePerDimPair);
            mapped_ptr += SampleSizePerDimPair;
            memcpy(mapped_ptr, bn_2D_64spp_3::bn_pmj_samples, SampleSizePerDimPair);
            mapped_ptr += SampleSizePerDimPair;
            memcpy(mapped_ptr, bn_2D_64spp_4::bn_pmj_samples, SampleSizePerDimPair);
            mapped_ptr += SampleSizePerDimPair;
            memcpy(mapped_ptr, bn_2D_64spp_5::bn_pmj_samples, SampleSizePerDimPair);
            mapped_ptr += SampleSizePerDimPair;
            memcpy(mapped_ptr, bn_2D_64spp_6::bn_pmj_samples, SampleSizePerDimPair);
            mapped_ptr += SampleSizePerDimPair;
            memcpy(mapped_ptr, bn_2D_64spp_7::bn_pmj_samples, SampleSizePerDimPair);
            mapped_ptr += SampleSizePerDimPair;

            // scrambling data
            memcpy(mapped_ptr, bn_2D_64spp_0::bn_pmj_scrambling, ScramblingSizePerDimPair);
            mapped_ptr += ScramblingSizePerDimPair;
            memcpy(mapped_ptr, bn_2D_64spp_1::bn_pmj_scrambling, ScramblingSizePerDimPair);
            mapped_ptr += ScramblingSizePerDimPair;
            memcpy(mapped_ptr, bn_2D_64spp_2::bn_pmj_scrambling, ScramblingSizePerDimPair);
            mapped_ptr += ScramblingSizePerDimPair;
            memcpy(mapped_ptr, bn_2D_64spp_3::bn_pmj_scrambling, ScramblingSizePerDimPair);
            mapped_ptr += ScramblingSizePerDimPair;
            memcpy(mapped_ptr, bn_2D_64spp_4::bn_pmj_scrambling, ScramblingSizePerDimPair);
            mapped_ptr += ScramblingSizePerDimPair;
            memcpy(mapped_ptr, bn_2D_64spp_5::bn_pmj_scrambling, ScramblingSizePerDimPair);
            mapped_ptr += ScramblingSizePerDimPair;
            memcpy(mapped_ptr, bn_2D_64spp_6::bn_pmj_scrambling, ScramblingSizePerDimPair);
            mapped_ptr += ScramblingSizePerDimPair;
            memcpy(mapped_ptr, bn_2D_64spp_7::bn_pmj_scrambling, SortingSizePerDimPair);
            mapped_ptr += SortingSizePerDimPair;

            // sorting data
            memcpy(mapped_ptr, bn_2D_64spp_0::bn_pmj_sorting, SortingSizePerDimPair);
            mapped_ptr += SortingSizePerDimPair;
            memcpy(mapped_ptr, bn_2D_64spp_1::bn_pmj_sorting, SortingSizePerDimPair);
            mapped_ptr += SortingSizePerDimPair;
            memcpy(mapped_ptr, bn_2D_64spp_2::bn_pmj_sorting, SortingSizePerDimPair);
            mapped_ptr += SortingSizePerDimPair;
            memcpy(mapped_ptr, bn_2D_64spp_3::bn_pmj_sorting, SortingSizePerDimPair);
            mapped_ptr += SortingSizePerDimPair;
            memcpy(mapped_ptr, bn_2D_64spp_4::bn_pmj_sorting, SortingSizePerDimPair);
            mapped_ptr += SortingSizePerDimPair;
            memcpy(mapped_ptr, bn_2D_64spp_5::bn_pmj_sorting, SortingSizePerDimPair);
            mapped_ptr += SortingSizePerDimPair;
            memcpy(mapped_ptr, bn_2D_64spp_6::bn_pmj_sorting, SortingSizePerDimPair);
            mapped_ptr += SortingSizePerDimPair;
            memcpy(mapped_ptr, bn_2D_64spp_7::bn_pmj_sorting, SortingSizePerDimPair);
            mapped_ptr += SortingSizePerDimPair;

            Buffer_Unmap(ctx_.api(), stage_buf_main, stage_buf_cold);
        }

        Ren::CommandBuffer cmd_buf = ctx_.BegTempSingleTimeCommands();

        const auto &[dst_buf_main, dst_buf_cold] = ctx_.storages().buffers[bn_pmj_2D_64spp_seq_buf_];
        CopyBufferToBuffer(ctx_.api(), stage_buf_main, 0, dst_buf_main, 0,
                           8 * (SampleSizePerDimPair + ScramblingSizePerDimPair + SortingSizePerDimPair), cmd_buf);

        ctx_.EndTempSingleTimeCommands(cmd_buf);

        Buffer_DestroyImmediately(ctx_.api(), stage_buf_main, stage_buf_cold);
    }

    { // PMJ samples
        const uint32_t buf_size = __pmj02_sample_count * __pmj02_dims_count * sizeof(uint32_t);
        pmj_samples_buf_ = ctx_.CreateBuffer(Ren::String{"PMJSamples"}, Ren::eBufType::Texture, buf_size);
        { // Add view
            const auto &[buf_main, buf_cold] = ctx_.storages().buffers[pmj_samples_buf_];
            Buffer_AddView(ctx_.api(), buf_main, buf_cold, Ren::eFormat::R32UI);
        }

        Ren::BufferMain stage_buf_main = {};
        Ren::BufferCold stage_buf_cold = {};
        if (!Ren::Buffer_Init(ctx_.api(), stage_buf_main, stage_buf_cold, Ren::String{"PMJSamplesStage"},
                              Ren::eBufType::Upload, buf_size, ctx_.log())) {
            // TODO: Properly handle failure
            assert(false);
        }

        { // init stage buf
            uint8_t *mapped_ptr = Buffer_Map(ctx_.api(), stage_buf_main, stage_buf_cold);
            memcpy(mapped_ptr, __pmj02_samples, __pmj02_sample_count * __pmj02_dims_count * sizeof(uint32_t));
            Buffer_Unmap(ctx_.api(), stage_buf_main, stage_buf_cold);
        }

        Ren::CommandBuffer cmd_buf = ctx_.BegTempSingleTimeCommands();

        const auto &[buf_main, buf_cold] = ctx_.storages().buffers[pmj_samples_buf_];
        CopyBufferToBuffer(ctx_.api(), stage_buf_main, 0, buf_main, 0,
                           __pmj02_sample_count * __pmj02_dims_count * sizeof(uint32_t), cmd_buf);

        ctx_.EndTempSingleTimeCommands(cmd_buf);

        Buffer_DestroyImmediately(ctx_.api(), stage_buf_main, stage_buf_cold);
    }

    Ren::ResizableBuffer &vtx_buf1 = ctx_.default_vertex_buf1();
    Ren::ResizableBuffer &vtx_buf2 = ctx_.default_vertex_buf2();
    Ren::ResizableBuffer &ndx_buf = ctx_.default_indices_buf();

    const int buf1_stride = 16;

    { // VertexInput for main drawing (uses all attributes)
        const Ren::VtxAttribDesc attribs[] = {
            {0, VTX_POS_LOC, 3, Ren::eType::Float32, buf1_stride, 0, 0},
            {0, VTX_UV1_LOC, 2, Ren::eType::Float16, buf1_stride, 0, 3 * sizeof(float)},
            {1, VTX_NOR_LOC, 4, Ren::eType::Int16_snorm, buf1_stride, 0, 0},
            {1, VTX_TAN_LOC, 2, Ren::eType::Int16_snorm, buf1_stride, 0, 4 * sizeof(uint16_t)},
            {1, VTX_AUX_LOC, 1, Ren::eType::Uint32, buf1_stride, 0, 6 * sizeof(uint16_t)}};
        draw_pass_vi_ = sh_.FindOrCreateVertexInput(attribs);
    }

    { // RenderPass for main drawing (compatible one)
        Ren::RenderTargetInfo color_rts[] = {
            {Ren::eFormat::RGBA16F, 1 /* samples */, Ren::eImageLayout::ColorAttachmentOptimal, Ren::eLoadOp::Load,
             Ren::eStoreOp::Store},
#if USE_OCT_PACKED_NORMALS == 1
            {Ren::eFormat::RGB10_A2, 1 /* samples */, Ren::eImageLayout::ColorAttachmentOptimal, Ren::eLoadOp::Load,
             Ren::eStoreOp::Store},
#else
            {Ren::eFormat::RGBA8, 1 /* samples */, Ren::eImageLayout::ColorAttachmentOptimal, Ren::eLoadOp::Load,
             Ren::eStoreOp::Store},
#endif
            {Ren::eFormat::RGBA8_srgb, 1 /* samples */, Ren::eImageLayout::ColorAttachmentOptimal, Ren::eLoadOp::Load,
             Ren::eStoreOp::Store}};

        // color_rts[2].flags = Ren::eImgFlags::SRGB;

        // const auto depth_format =
        //     ctx_.capabilities.depth24_stencil8_format ? Ren::eFormat::D24_S8 : Ren::eFormat::D32_S8;
        const auto depth_format = Ren::eFormat::D32_S8;

        const Ren::RenderTargetInfo depth_rt = {depth_format, 1 /* samples */,
                                                Ren::eImageLayout::DepthStencilAttachmentOptimal, Ren::eLoadOp::Load,
                                                Ren::eStoreOp::Store};

        rp_main_draw_ = sh_.FindOrCreateRenderPass(depth_rt, color_rts);
    }

    { // Rasterization states for main drawing
        Ren::RastState rast_state;
        rast_state.poly.cull = uint8_t(Ren::eCullFace::Back);
        rast_state.poly.mode = uint8_t(Ren::ePolygonMode::Fill);
        rast_state.depth.test_enabled = true;
        rast_state.depth.compare_op = unsigned(Ren::eCompareOp::Equal);

        rast_states_[int(eFwdPipeline::FrontfaceDraw)] = rast_state;

        // Rasterization state for shadow drawing
        rast_state.poly.cull = uint8_t(Ren::eCullFace::Front);
        rast_states_[int(eFwdPipeline::BackfaceDraw)] = rast_state;
    }
    { // Rasterization state for wireframe
        Ren::RastState rast_state;
        rast_state.poly.cull = uint8_t(Ren::eCullFace::None);
        rast_state.poly.mode = uint8_t(Ren::ePolygonMode::Line);
        rast_state.depth.test_enabled = false;

        rast_states_[int(eFwdPipeline::Wireframe)] = rast_state;
    }

    { // shadow map texture
        Ren::ImgParams params;
        params.w = SHADOWMAP_WIDTH;
        params.h = SHADOWMAP_HEIGHT;
        params.format = Ren::eFormat::D16;
        params.usage = Ren::Bitmask(Ren::eImgUsage::RenderTarget) | Ren::eImgUsage::Sampled;
        params.sampling.filter = Ren::eFilter::Bilinear;
        params.sampling.wrap = Ren::eWrap::ClampToEdge;
        params.sampling.compare = Ren::eCompareOp::GEqual;

        shadow_depth_ = ctx_.CreateImage(Ren::String{"Shadow Depth Tex"}, {}, params, ctx_.default_mem_allocs());
    }
    { // shadow filter texture
        Ren::ImgParams params;
        params.w = SHADOWMAP_WIDTH;
        params.h = SHADOWMAP_HEIGHT;
        params.format = (ctx_.capabilities.rgb565_render_target ? Ren::eFormat::RGB565 : Ren::eFormat::RGBA8);
        params.usage = Ren::Bitmask(Ren::eImgUsage::RenderTarget) | Ren::eImgUsage::Sampled;
        params.sampling.filter = Ren::eFilter::Bilinear;
        params.sampling.wrap = Ren::eWrap::ClampToEdge;

        shadow_color_ = ctx_.CreateImage(Ren::String{"Shadow Color Tex"}, {}, params, ctx_.default_mem_allocs());
    }

    { // nearest sampler
        Ren::SamplingParams sampler_params;
        sampler_params.wrap = Ren::eWrap::ClampToEdge;

        nearest_sampler_ = ctx_.CreateSampler(sampler_params);

        sampler_params.filter = Ren::eFilter::Bilinear;
        linear_sampler_ = ctx_.CreateSampler(sampler_params);
    }

    { // Allocate buffer for skinned vertices
        // TODO: fix this. do not allocate twice more memory in buf2
        skinned_buf1_vtx_ = vtx_buf1.AllocSubRegion(MAX_SKIN_VERTICES_TOTAL * 16 * 2, 16, "skinned", ctx_.log());
        skinned_buf2_vtx_ = vtx_buf2.AllocSubRegion(MAX_SKIN_VERTICES_TOTAL * 16 * 2, 16, "skinned", ctx_.log());
        assert(skinned_buf1_vtx_.offset == skinned_buf2_vtx_.offset && "Offsets do not match!");
    }

    temp_sub_frustums_.count = ITEM_CELLS_COUNT;
    temp_sub_frustums_.realloc(temp_sub_frustums_.count);

    decals_boxes_.realloc(MAX_DECALS_TOTAL);
    ditem_to_decal_.realloc(MAX_DECALS_TOTAL);
    allocated_shadow_regions_.realloc(MAX_SHADOWMAPS_TOTAL);

#if defined(REN_GL_BACKEND)
    Ren::g_param_buf_binding = BIND_PUSH_CONSTANT_BUF;
#endif
}

Eng::Renderer::~Renderer() {
    ctx_.ReleaseBuffer(bn_pmj_2D_64spp_seq_buf_);
    ctx_.ReleaseBuffer(pmj_samples_buf_);

    const Ren::ImageHandle images_to_release[] = {shadow_depth_,
                                                  shadow_color_,
                                                  noise_,
                                                  dummy_black_,
                                                  dummy_white_,
                                                  brdf_lut_,
                                                  ltc_luts_,
                                                  stbn_1D_64spp_,
                                                  cone_rt_lut_,
                                                  tonemap_lut_,
                                                  sky_transmittance_lut_,
                                                  sky_multiscatter_lut_,
                                                  sky_moon_,
                                                  sky_weather_,
                                                  sky_cirrus_,
                                                  sky_curl_,
                                                  sky_noise3d_};
    for (const Ren::ImageHandle img : images_to_release) {
        ctx_.ReleaseImage(img);
    }

    ctx_.ReleaseSampler(nearest_sampler_);
    ctx_.ReleaseSampler(linear_sampler_);

    Ren::ResizableBuffer &vtx_buf1 = ctx_.default_vertex_buf1();
    vtx_buf1.FreeSubRegion(skinned_buf1_vtx_);

    Ren::ResizableBuffer &vtx_buf2 = ctx_.default_vertex_buf2();
    vtx_buf2.FreeSubRegion(skinned_buf2_vtx_);

    swCullCtxDestroy(&cull_ctx_);
}

void Eng::Renderer::PrepareDrawList(const SceneData &scene, const Ren::Camera &cam, const Ren::Camera &ext_cam,
                                    DrawList &list) {
    GatherDrawables(scene, cam, ext_cam, list);
}

void Eng::Renderer::ExecuteDrawList(const DrawList &list, const PersistentGpuData &persistent_data,
                                    const Ren::ImageHandle target, const bool blit_to_backbuffer) {
    using namespace RendererInternal;

    __itt_task_begin(__g_itt_domain, __itt_null, __itt_null, itt_exec_dr_str);

    const int cur_scr_w = ctx_.w(), cur_scr_h = ctx_.h();
    Ren::ILog *log = ctx_.log();

    if (!cur_scr_w || !cur_scr_h) {
        // view is minimized?
        return;
    }

    if (!prim_draw_.LazyInit(sh_)) {
        log->Error("[Renderer] Failed to initialize primitive drawing!");
    }

    if (view_state_.prev_world_origin != list.world_origin) {
        const Ren::Vec3f origin_diff = Ren::Vec3f(list.world_origin - view_state_.prev_world_origin);

        view_state_.prev_view_from_world = Ren::Translate(view_state_.prev_view_from_world, origin_diff);
        view_state_.prev_clip_from_world = Ren::Translate(view_state_.prev_clip_from_world, origin_diff);

        for (int i = 0; i < PROBE_VOLUMES_COUNT; ++i) {
            const probe_volume_t &volume = persistent_data.probe_volumes[i];

            const Ren::Vec3d new_pivot = view_state_.prev_world_origin - list.world_origin + Ren::Vec3d(volume.pivot);
            volume.pivot = Ren::Vec3f(new_pivot);

            const Ren::Vec3d new_orig = view_state_.prev_world_origin - list.world_origin + Ren::Vec3d(volume.origin);
            volume.origin = Ren::Vec3f(new_orig);

            const Ren::Vec3i test = Ren::Vec3i{Floor((volume.pivot - volume.origin) / volume.spacing)};
            assert(test == volume.scroll);
        }
    }

    for (int i = 0; i < PROBE_VOLUMES_COUNT; ++i) {
        const probe_volume_t &volume = persistent_data.probe_volumes[i];
        if (i == list.volume_to_update) {
            const Ren::Vec3i new_scroll =
                Ren::Vec3i{Floor((list.draw_cam.world_position() - volume.origin) / volume.spacing)};

            volume.pivot = list.draw_cam.world_position();
            volume.scroll_diff = new_scroll - volume.scroll;
            volume.scroll = new_scroll;
            ++volume.updates_count;
        } else {
            volume.scroll_diff = Ren::Vec3i{0};
        }
    }

    if (list.volume_to_update == PROBE_VOLUMES_COUNT - 1) {
        // force the last volume to cover the whole scene
        const probe_volume_t &last_volume = persistent_data.probe_volumes[PROBE_VOLUMES_COUNT - 1];
        last_volume.spacing =
            (list.bbox_max - list.bbox_min) / Ren::Vec3f{PROBE_VOLUME_RES_X, PROBE_VOLUME_RES_Y, PROBE_VOLUME_RES_Z};
        const float scalar_spacing =
            std::max(std::max(last_volume.spacing[0], last_volume.spacing[1]), last_volume.spacing[2]);
        last_volume.spacing = Ren::Vec3f{scalar_spacing};
        const Ren::Vec3i new_scroll =
            Ren::Vec3i{(0.5f * (list.bbox_max + list.bbox_min) - last_volume.origin) / last_volume.spacing};
        last_volume.scroll_diff = new_scroll - last_volume.scroll;
        last_volume.scroll = new_scroll;
        last_volume.pivot = 0.5f * (list.bbox_max + list.bbox_min);
    }

    float probe_volume_spacing = 0.5f;
    for (int i = 0; i < PROBE_VOLUMES_COUNT - 1; ++i) {
        const probe_volume_t &volume = persistent_data.probe_volumes[i],
                             &last_volume = persistent_data.probe_volumes[PROBE_VOLUMES_COUNT - 1];
        if (probe_volume_spacing > last_volume.spacing[0]) {
            volume.spacing = last_volume.spacing[0];
            volume.scroll = Ren::Vec3i{(0.5f * (list.bbox_max + list.bbox_min) - volume.origin) / volume.spacing};
            volume.scroll_diff = Ren::Vec3i(0);
            volume.pivot = 0.5f * (list.bbox_max + list.bbox_min);
        } else {
            volume.spacing = probe_volume_spacing;
        }
        probe_volume_spacing *= 3.0f;
    }

    const bool cur_hq_ssr_enabled = int(list.render_settings.reflections_quality) >= int(eReflectionsQuality::High);
    const bool cur_dof_enabled = list.render_settings.enable_dof;

    const uint64_t cpu_draw_start_us = Sys::GetTimeUs();
    // Write timestamp at the beginning of execution
    backend_gpu_start_ = ctx_.WriteTimestamp(true);

    bool rendertarget_changed = false;

    if (cur_scr_w != view_state_.out_res[0] || cur_scr_h != view_state_.out_res[1] ||
        list.render_settings.taa_mode != taa_mode_ || cur_dof_enabled != dof_enabled_ || cached_rp_index_ != 0) {
        rendertarget_changed = true;

        view_state_.out_res = Ren::Vec2i{cur_scr_w, cur_scr_h};
        taa_mode_ = list.render_settings.taa_mode;
        dof_enabled_ = cur_dof_enabled;
        accumulated_frames_ = 0;
    }

    if (list.render_settings.pixel_filter != px_filter_table_filter_ ||
        list.render_settings.pixel_filter_width != px_filter_table_width_) {
        UpdatePixelFilterTable(list.render_settings.pixel_filter, list.render_settings.pixel_filter_width);
        px_filter_table_filter_ = settings.pixel_filter;
        px_filter_table_width_ = settings.pixel_filter_width;
    }

    if (!target) {
        view_state_.ren_res[0] = int(float(view_state_.out_res[0]) / list.render_settings.resolution_scale);
        view_state_.ren_res[1] = int(float(view_state_.out_res[1]) / list.render_settings.resolution_scale);
    } else {
        const auto &[trg_main, trg_cold] = ctx_.storages().images[target];
        view_state_.ren_res[0] = int(float(trg_cold.params.w) / list.render_settings.resolution_scale);
        view_state_.ren_res[1] = int(float(trg_cold.params.h) / list.render_settings.resolution_scale);
    }
    assert(view_state_.ren_res[0] <= view_state_.out_res[0] && view_state_.ren_res[1] <= view_state_.out_res[1]);

    view_state_.vertical_fov = list.draw_cam.angle();
    view_state_.pixel_spread_angle = std::atan(
        2.0f * std::tan(0.5f * view_state_.vertical_fov * Ren::Pi<float>() / 180.0f) / float(view_state_.ren_res[1]));
    view_state_.frame_index = list.frame_index;
    view_state_.volume_to_update = list.volume_to_update;
    view_state_.stochastic_lights_count = view_state_.stochastic_lights_count_cache = 0;
    if (persistent_data.stoch_lights) {
        const auto &[lights_buf_main, lights_buf_cold] = ctx_.storages().buffers[persistent_data.stoch_lights];
        view_state_.stochastic_lights_count_cache = lights_buf_cold.size / sizeof(light_item_t);
    }
    if (persistent_data.stoch_lights && list.render_settings.gi_quality > eGIQuality::Medium) {
        const auto &[lights_buf_main, lights_buf_cold] = ctx_.storages().buffers[persistent_data.stoch_lights];
        view_state_.stochastic_lights_count = lights_buf_cold.size / sizeof(light_item_t);
    }

    view_state_.env_generation = list.env.generation;
    view_state_.pre_exposure = custom_pre_exposure_.value_or(readback_exposure());
    view_state_.prev_pre_exposure = std::min(std::max(view_state_.prev_pre_exposure, min_exposure_), max_exposure_);

    bool has_global_volume = Ren::Length2(list.env.fog.scatter_color) != 0.0f;
    has_global_volume |= list.env.fog.absorption != 1.0f;
    has_global_volume |= Ren::Length2(list.env.fog.emission_color) != 0.0f;
    has_global_volume &= (list.env.fog.density != 0.0f);

    view_state_.skip_volumetrics = !has_global_volume && list.rt_obj_instances[int(eTLASIndex::Volume)].count == 0;

    if (list.render_settings.taa_mode != eTAAMode::Off) {
        const int samples_to_use =
            (list.render_settings.taa_mode == eTAAMode::Static) ? TaaSampleCountStatic : TaaSampleCountNormal;
        const int sample_index =
            ((list.render_settings.taa_mode == eTAAMode::Static) ? accumulated_frames_ : list.frame_index) %
            samples_to_use;
        Ren::Vec2f jitter = PMJSamples64[sample_index];

        auto lookup_filter_table = [this](float x) {
            x *= (PxFilterTableSize - 1);

            const int index = std::min(int(x), PxFilterTableSize - 1);
            const int nindex = std::min(index + 1, PxFilterTableSize - 1);
            const float t = x - float(index);

            const float data0 = px_filter_table_[index];
            if (t == 0.0f) {
                return data0;
            }

            float data1 = px_filter_table_[nindex];
            return (1.0f - t) * data0 + t * data1;
        };

        if (list.render_settings.taa_mode == eTAAMode::Static &&
            list.render_settings.pixel_filter != ePixelFilter::Box) {
            jitter[0] = lookup_filter_table(jitter[0]);
            jitter[1] = lookup_filter_table(jitter[1]);
        }

        view_state_.jitter = jitter;

        jitter = (2.0f * jitter - Ren::Vec2f{1.0f}) / Ren::Vec2f{view_state_.ren_res};
        // jitter = Ren::Vec2f{0.0f};
        list.draw_cam.SetPxOffset(jitter);
    } else {
        view_state_.jitter = Ren::Vec2f{0.0f};
        list.draw_cam.SetPxOffset(Ren::Vec2f{0.0f, 0.0f});
    }

    BindlessTextureData bindless_tex;
#if defined(REN_VK_BACKEND)
    bindless_tex.textures_descr_sets = persistent_data.textures_descr_sets[ctx_.backend_frame()];
    bindless_tex.rt_textures.descr_set = persistent_data.rt_textures_descr_sets[ctx_.backend_frame()];
    bindless_tex.rt_inline_textures.descr_set = persistent_data.rt_inline_textures_descr_sets[ctx_.backend_frame()];
#elif defined(REN_GL_BACKEND)
    bindless_tex.rt_inline_textures.buf = persistent_data.textures_buf;
#endif

    const bool deferred_shading =
        list.render_settings.render_mode == eRenderMode::Deferred && !list.render_settings.debug_wireframe;
    const bool env_map_changed = false;
    //(env_map_ != list.env.env_map);
    const bool rebuild_renderpasses = !cached_settings_.has_value() ||
                                      (cached_settings_.value() != list.render_settings) || !fg_builder_.ready() ||
                                      rendertarget_changed || env_map_changed;

    cached_settings_ = list.render_settings;
    cached_rp_index_ = 0;
    env_map_ = list.env.env_map;
    p_list_ = &list;
    min_exposure_ = std::pow(2.0f, list.draw_cam.min_exposure);
    max_exposure_ = std::pow(2.0f, list.draw_cam.max_exposure);

    if (rebuild_renderpasses) {
        const uint64_t rp_setup_beg_us = Sys::GetTimeUs();

        fg_builder_.Reset();
        backbuffer_sources_.clear();

        accumulated_frames_ = 0;

        InitSkyResources();

        auto &common_buffers = *fg_builder_.AllocTempData<CommonBuffers>();
        { // Import external buffers
            common_buffers.vertex_buf1 = fg_builder_.ImportResource(persistent_data.vertex_buf1->handle());
            common_buffers.vertex_buf2 = fg_builder_.ImportResource(persistent_data.vertex_buf2->handle());
            common_buffers.indices_buf = fg_builder_.ImportResource(persistent_data.indices_buf->handle());
            common_buffers.skin_vertex_buf = fg_builder_.ImportResource(persistent_data.skin_vertex_buf->handle());
            common_buffers.delta_buf = fg_builder_.ImportResource(persistent_data.delta_buf->handle());

            common_buffers.instances = fg_builder_.ImportResource(persistent_data.instances);
            common_buffers.materials = fg_builder_.ImportResource(persistent_data.materials);

            common_buffers.pmj_samples = fg_builder_.ImportResource(pmj_samples_buf_);
            common_buffers.bn_pmj_2D_64spp_seq = fg_builder_.ImportResource(bn_pmj_2D_64spp_seq_buf_);

            if (persistent_data.stoch_lights) {
                common_buffers.stoch_lights = fg_builder_.ImportResource(persistent_data.stoch_lights);
                common_buffers.stoch_lights_nodes = fg_builder_.ImportResource(persistent_data.stoch_lights_nodes);
            }
        }

        auto &frame_textures = *fg_builder_.AllocTempData<FrameTextures>();
        { // Import external textures
            frame_textures.gi_cache_irradiance = fg_builder_.ImportResource(persistent_data.probe_irradiance);
            frame_textures.gi_cache_distance = fg_builder_.ImportResource(persistent_data.probe_distance);
            frame_textures.gi_cache_offset = fg_builder_.ImportResource(persistent_data.probe_offset);

            frame_textures.envmap = fg_builder_.ImportResource(list.env.env_map);

            frame_textures.shadow_depth = fg_builder_.ImportResource(shadow_depth_);
            frame_textures.shadow_color = fg_builder_.ImportResource(shadow_color_);

            frame_textures.noise = fg_builder_.ImportResource(noise_);
            frame_textures.dummy_white = fg_builder_.ImportResource(dummy_white_);
            frame_textures.dummy_black = fg_builder_.ImportResource(dummy_black_);
            frame_textures.ltc_luts = fg_builder_.ImportResource(ltc_luts_);
            frame_textures.brdf_lut = fg_builder_.ImportResource(brdf_lut_);
            frame_textures.cone_rt_lut = fg_builder_.ImportResource(cone_rt_lut_);
            frame_textures.stbn_1D_64spp = fg_builder_.ImportResource(stbn_1D_64spp_);
            if (tonemap_lut_) {
                frame_textures.tonemap_lut = fg_builder_.ImportResource(tonemap_lut_);
            }

            if (sky_transmittance_lut_) {
                frame_textures.sky_transmittance_lut = fg_builder_.ImportResource(sky_transmittance_lut_);
                frame_textures.sky_multiscatter_lut = fg_builder_.ImportResource(sky_multiscatter_lut_);
                frame_textures.sky_moon = fg_builder_.ImportResource(sky_moon_);
                frame_textures.sky_weather = fg_builder_.ImportResource(sky_weather_);
                frame_textures.sky_cirrus = fg_builder_.ImportResource(sky_cirrus_);
                frame_textures.sky_curl = fg_builder_.ImportResource(sky_curl_);
                frame_textures.sky_noise3d = fg_builder_.ImportResource(sky_noise3d_);
            }
        }

        AddBuffersUpdatePass(common_buffers, persistent_data);
        AddLightBuffersUpdatePass(common_buffers);
        AddSunColorUpdatePass(common_buffers, frame_textures);

        { // Skinning
            auto &skinning = fg_builder_.AddNode("SKINNING");

            const FgBufROHandle skin_vtx_res =
                skinning.AddStorageReadonlyInput(common_buffers.skin_vertex_buf, Ren::eStage::ComputeShader);
            const FgBufROHandle in_skin_transforms_res =
                skinning.AddStorageReadonlyInput(common_buffers.skin_transforms, Ren::eStage::ComputeShader);
            const FgBufROHandle in_shape_keys_res =
                skinning.AddStorageReadonlyInput(common_buffers.shape_keys, Ren::eStage::ComputeShader);
            const FgBufROHandle delta_buf_res =
                skinning.AddStorageReadonlyInput(common_buffers.delta_buf, Ren::eStage::ComputeShader);

            const FgBufRWHandle vtx_buf1_res = common_buffers.vertex_buf1 =
                skinning.AddStorageOutput(common_buffers.vertex_buf1, Ren::eStage::ComputeShader);
            const FgBufRWHandle vtx_buf2_res = common_buffers.vertex_buf2 =
                skinning.AddStorageOutput(common_buffers.vertex_buf2, Ren::eStage::ComputeShader);

            skinning.make_executor<ExSkinning>(p_list_, skin_vtx_res, in_skin_transforms_res, in_shape_keys_res,
                                               delta_buf_res, vtx_buf1_res, vtx_buf2_res);
        }

        //
        // RT acceleration structures
        //
        auto &acc_structs = *fg_builder_.AllocTempData<AccelerationStructures>();
        for (int i = 0; i < int(eTLASIndex::_Count); ++i) {
            acc_structs.rt_tlas_buf[i] = fg_builder_.ImportResource(persistent_data.rt_tlas_buf[i]);
            if (persistent_data.rt_tlases[i]) {
                acc_structs.rt_tlases[i] = persistent_data.rt_tlases[i];
            }
        }
        acc_structs.hwrt.rt_tlas_build_scratch_size = persistent_data.hwrt.rt_tlas_build_scratch_size;
        if (persistent_data.swrt.rt_blas_buf) {
            acc_structs.swrt.rt_blas_buf = fg_builder_.ImportResource(persistent_data.swrt.rt_blas_buf->handle());
        }
        if (persistent_data.swrt.rt_prim_indices_buf) {
            acc_structs.swrt.rt_prim_indices =
                fg_builder_.ImportResource(persistent_data.swrt.rt_prim_indices_buf->handle());
        }
        acc_structs.swrt.rt_root_node = persistent_data.swrt.rt_root_node;

        FgBufRWHandle rt_geo_instances_res[int(eTLASIndex::_Count)], rt_obj_instances_res[int(eTLASIndex::_Count)];

        if (ctx_.capabilities.hwrt) {
            auto &update_rt_bufs = fg_builder_.AddNode("UPDATE ACC BUFS");

            { // geo instances buffer
                FgBufDesc desc;
                desc.type = Ren::eBufType::Storage;
                desc.size = RTGeoInstancesBufChunkSize;

                rt_geo_instances_res[int(eTLASIndex::Main)] =
                    update_rt_bufs.AddTransferOutput("RT Geo Instances", desc);
            }
            { // obj instances buffer
                FgBufDesc desc;
                desc.type = Ren::eBufType::Storage;
                desc.size = HWRTObjInstancesBufChunkSize;
                desc.views.push_back(Ren::eFormat::RGBA32F);

                rt_obj_instances_res[int(eTLASIndex::Main)] =
                    update_rt_bufs.AddTransferOutput("RT Obj Instances", desc);
            }

            update_rt_bufs.make_executor<ExUpdateAccBuffers>(p_list_, int(eTLASIndex::Main),
                                                             rt_geo_instances_res[int(eTLASIndex::Main)],
                                                             rt_obj_instances_res[int(eTLASIndex::Main)]);

            auto &build_acc_structs = fg_builder_.AddNode("BUILD ACC STRUCTS");

            const FgBufROHandle rt_obj_instances_main_res =
                build_acc_structs.AddASBuildReadonlyInput(rt_obj_instances_res[int(eTLASIndex::Main)]);
            const FgBufRWHandle rt_tlas_res = acc_structs.rt_tlas_buf[int(eTLASIndex::Main)] =
                build_acc_structs.AddASBuildOutput(acc_structs.rt_tlas_buf[int(eTLASIndex::Main)]);

            FgBufRWHandle rt_tlas_build_scratch_res;
            { // scratch buffer
                FgBufDesc desc;
                desc.type = Ren::eBufType::Storage;
                desc.size = acc_structs.hwrt.rt_tlas_build_scratch_size;
                rt_tlas_build_scratch_res = build_acc_structs.AddASBuildOutput("TLAS Scratch Buf", desc);
            }

            build_acc_structs.make_executor<ExBuildAccStructures>(
                p_list_, 0, rt_obj_instances_main_res, acc_structs.rt_tlases[int(eTLASIndex::Main)],
                Ren::Span<const mesh_t>{persistent_data.swrt.rt_meshes.data(),
                                        persistent_data.swrt.rt_meshes.capacity()},
                rt_tlas_res, rt_tlas_build_scratch_res);
        } else if (ctx_.capabilities.swrt && acc_structs.rt_tlas_buf[int(eTLASIndex::Main)]) {
            auto &update_rt_bufs = fg_builder_.AddNode("UPDATE ACC BUFS");

            { // geo instances buffer
                FgBufDesc desc;
                desc.type = Ren::eBufType::Storage;
                desc.size = RTGeoInstancesBufChunkSize;

                rt_geo_instances_res[int(eTLASIndex::Main)] =
                    update_rt_bufs.AddTransferOutput("RT Geo Instances", desc);
            }

            update_rt_bufs.make_executor<ExUpdateAccBuffers>(
                p_list_, int(eTLASIndex::Main), rt_geo_instances_res[int(eTLASIndex::Main)], FgBufRWHandle{});

            auto &build_acc_structs = fg_builder_.AddNode("BUILD ACC STRUCTS");

            { // obj instances buffer
                FgBufDesc desc;
                desc.type = Ren::eBufType::Storage;
                desc.size = SWRTObjInstancesBufChunkSize;
                desc.views.push_back(Ren::eFormat::RGBA32F);

                rt_obj_instances_res[int(eTLASIndex::Main)] =
                    build_acc_structs.AddTransferOutput("RT Obj Instances", desc);
            }

            const FgBufRWHandle rt_tlas_res = acc_structs.rt_tlas_buf[int(eTLASIndex::Main)] =
                build_acc_structs.AddTransferOutput(acc_structs.rt_tlas_buf[int(eTLASIndex::Main)]);

            build_acc_structs.make_executor<ExBuildAccStructures>(
                p_list_, 0, rt_obj_instances_res[int(eTLASIndex::Main)], acc_structs.rt_tlases[int(eTLASIndex::Main)],
                Ren::Span<const mesh_t>{persistent_data.swrt.rt_meshes.data(),
                                        persistent_data.swrt.rt_meshes.capacity()},
                rt_tlas_res, FgBufRWHandle{});
        }

        if (deferred_shading && list.render_settings.shadows_quality == eShadowsQuality::Raytraced) {
            if (ctx_.capabilities.hwrt) {
                auto &update_rt_bufs = fg_builder_.AddNode("UPDATE SH ACC BUFS");

                { // geo instances buffer
                    FgBufDesc desc;
                    desc.type = Ren::eBufType::Storage;
                    desc.size = RTGeoInstancesBufChunkSize;

                    rt_geo_instances_res[int(eTLASIndex::Shadow)] =
                        update_rt_bufs.AddTransferOutput("RT SH Geo Instances", desc);
                }
                { // obj instances buffer
                    FgBufDesc desc;
                    desc.type = Ren::eBufType::Storage;
                    desc.size = HWRTObjInstancesBufChunkSize;
                    desc.views.push_back(Ren::eFormat::RGBA32F);

                    rt_obj_instances_res[int(eTLASIndex::Shadow)] =
                        update_rt_bufs.AddTransferOutput("RT SH Obj Instances", desc);
                }

                update_rt_bufs.make_executor<ExUpdateAccBuffers>(p_list_, int(eTLASIndex::Shadow),
                                                                 rt_geo_instances_res[int(eTLASIndex::Shadow)],
                                                                 rt_obj_instances_res[int(eTLASIndex::Shadow)]);

                ////

                auto &build_acc_structs = fg_builder_.AddNode("SH ACC STRUCTS");
                const FgBufROHandle rt_obj_instances_shadow_res =
                    build_acc_structs.AddASBuildReadonlyInput(rt_obj_instances_res[int(eTLASIndex::Shadow)]);
                const FgBufRWHandle rt_sh_tlas_res = acc_structs.rt_tlas_buf[int(eTLASIndex::Shadow)] =
                    build_acc_structs.AddASBuildOutput(acc_structs.rt_tlas_buf[int(eTLASIndex::Shadow)]);

                FgBufRWHandle rt_sh_tlas_build_scratch_res;
                { // scratch buffer
                    FgBufDesc desc;
                    desc.type = Ren::eBufType::Storage;
                    desc.size = acc_structs.hwrt.rt_tlas_build_scratch_size;
                    rt_sh_tlas_build_scratch_res = build_acc_structs.AddASBuildOutput("SH TLAS Scratch Buf", desc);
                }

                build_acc_structs.make_executor<ExBuildAccStructures>(
                    p_list_, 1, rt_obj_instances_shadow_res, acc_structs.rt_tlases[int(eTLASIndex::Shadow)],
                    Ren::Span<const mesh_t>{persistent_data.swrt.rt_meshes.data(),
                                            persistent_data.swrt.rt_meshes.capacity()},
                    rt_sh_tlas_res, rt_sh_tlas_build_scratch_res);
            } else if (ctx_.capabilities.swrt && acc_structs.rt_tlas_buf[int(eTLASIndex::Shadow)]) {
                auto &update_rt_bufs = fg_builder_.AddNode("UPDATE SH ACC BUFS");

                { // geo instances buffer
                    FgBufDesc desc;
                    desc.type = Ren::eBufType::Storage;
                    desc.size = RTGeoInstancesBufChunkSize;

                    rt_geo_instances_res[int(eTLASIndex::Shadow)] =
                        update_rt_bufs.AddTransferOutput("RT SH Geo Instances", desc);
                }

                update_rt_bufs.make_executor<ExUpdateAccBuffers>(
                    p_list_, 1, rt_geo_instances_res[int(eTLASIndex::Shadow)], FgBufRWHandle{});

                auto &build_acc_structs = fg_builder_.AddNode("BUILD SH ACC STRUCTS");

                { // obj instances buffer
                    FgBufDesc desc;
                    desc.type = Ren::eBufType::Storage;
                    desc.size = SWRTObjInstancesBufChunkSize;
                    desc.views.push_back(Ren::eFormat::RGBA32F);

                    rt_obj_instances_res[int(eTLASIndex::Shadow)] =
                        build_acc_structs.AddTransferOutput("RT SH Obj Instances", desc);
                }

                const FgBufRWHandle rt_sh_tlas_res = acc_structs.rt_tlas_buf[int(eTLASIndex::Shadow)] =
                    build_acc_structs.AddTransferOutput(acc_structs.rt_tlas_buf[int(eTLASIndex::Shadow)]);

                build_acc_structs.make_executor<ExBuildAccStructures>(
                    p_list_, int(eTLASIndex::Shadow), rt_obj_instances_res[int(eTLASIndex::Shadow)],
                    acc_structs.rt_tlases[int(eTLASIndex::Shadow)],
                    Ren::Span<const mesh_t>{persistent_data.swrt.rt_meshes.data(),
                                            persistent_data.swrt.rt_meshes.capacity()},
                    rt_sh_tlas_res, FgBufRWHandle{});
            }
        }

        if (list.render_settings.vol_quality != eVolQuality::Off) {
            if (ctx_.capabilities.hwrt) {
                auto &update_rt_bufs = fg_builder_.AddNode("UPDATE VOL ACC BUF");

                { // geo instances buffer
                    FgBufDesc desc;
                    desc.type = Ren::eBufType::Storage;
                    desc.size = RTGeoInstancesBufChunkSize;

                    rt_geo_instances_res[int(eTLASIndex::Volume)] =
                        update_rt_bufs.AddTransferOutput("RT VOL Geo Instances", desc);
                }
                { // obj instances buffer
                    FgBufDesc desc;
                    desc.type = Ren::eBufType::Storage;
                    desc.size = HWRTObjInstancesBufChunkSize;
                    desc.views.push_back(Ren::eFormat::RGBA32F);

                    rt_obj_instances_res[int(eTLASIndex::Volume)] =
                        update_rt_bufs.AddTransferOutput("RT VOL Obj Instances", desc);
                }

                update_rt_bufs.make_executor<ExUpdateAccBuffers>(p_list_, int(eTLASIndex::Volume),
                                                                 rt_geo_instances_res[int(eTLASIndex::Volume)],
                                                                 rt_obj_instances_res[int(eTLASIndex::Volume)]);

                ////

                auto &build_acc_structs = fg_builder_.AddNode("VOL ACC STRUCTS");
                const FgBufROHandle rt_obj_instances_volume_res =
                    build_acc_structs.AddASBuildReadonlyInput(rt_obj_instances_res[int(eTLASIndex::Volume)]);
                const FgBufRWHandle rt_vol_tlas_res = acc_structs.rt_tlas_buf[int(eTLASIndex::Volume)] =
                    build_acc_structs.AddASBuildOutput(acc_structs.rt_tlas_buf[int(eTLASIndex::Volume)]);

                FgBufRWHandle rt_vol_tlas_build_scratch_res;
                { // scratch buffer
                    FgBufDesc desc;
                    desc.type = Ren::eBufType::Storage;
                    desc.size = acc_structs.hwrt.rt_tlas_build_scratch_size;
                    rt_vol_tlas_build_scratch_res = build_acc_structs.AddASBuildOutput("VOL TLAS Scratch Buf", desc);
                }

                build_acc_structs.make_executor<ExBuildAccStructures>(
                    p_list_, int(eTLASIndex::Volume), rt_obj_instances_volume_res,
                    acc_structs.rt_tlases[int(eTLASIndex::Volume)],
                    Ren::Span<const mesh_t>{persistent_data.swrt.rt_meshes.data(),
                                            persistent_data.swrt.rt_meshes.capacity()},
                    rt_vol_tlas_res, rt_vol_tlas_build_scratch_res);
            } else if (ctx_.capabilities.swrt && acc_structs.rt_tlas_buf[int(eTLASIndex::Volume)]) {
                auto &update_rt_bufs = fg_builder_.AddNode("UPDATE VOL ACC BUF");

                { // geo instances buffer
                    FgBufDesc desc;
                    desc.type = Ren::eBufType::Storage;
                    desc.size = RTGeoInstancesBufChunkSize;

                    rt_geo_instances_res[int(eTLASIndex::Volume)] =
                        update_rt_bufs.AddTransferOutput("RT VOL Geo Instances", desc);
                }

                update_rt_bufs.make_executor<ExUpdateAccBuffers>(
                    p_list_, int(eTLASIndex::Volume), rt_geo_instances_res[int(eTLASIndex::Volume)], FgBufRWHandle{});

                auto &build_acc_structs = fg_builder_.AddNode("BUILD VOL ACC STRUCTS");

                { // obj instances buffer
                    FgBufDesc desc;
                    desc.type = Ren::eBufType::Storage;
                    desc.size = SWRTObjInstancesBufChunkSize;
                    desc.views.push_back(Ren::eFormat::RGBA32F);

                    rt_obj_instances_res[int(eTLASIndex::Volume)] =
                        build_acc_structs.AddTransferOutput("RT VOL Obj Instances", desc);
                }

                const FgBufRWHandle rt_vol_tlas_res = acc_structs.rt_tlas_buf[int(eTLASIndex::Volume)] =
                    build_acc_structs.AddTransferOutput(acc_structs.rt_tlas_buf[int(eTLASIndex::Volume)]);

                build_acc_structs.make_executor<ExBuildAccStructures>(
                    p_list_, int(eTLASIndex::Volume), rt_obj_instances_res[int(eTLASIndex::Volume)],
                    acc_structs.rt_tlases[int(eTLASIndex::Volume)],
                    Ren::Span<const mesh_t>{persistent_data.swrt.rt_meshes.data(),
                                            persistent_data.swrt.rt_meshes.capacity()},
                    rt_vol_tlas_res, FgBufRWHandle{});
            }
        }

        { // Shadow depth
            auto &shadow_depth = fg_builder_.AddNode("SHADOW DEPTH");
            const FgBufROHandle vtx_buf1_res = shadow_depth.AddVertexBufferInput(common_buffers.vertex_buf1);
            const FgBufROHandle vtx_buf2_res = shadow_depth.AddVertexBufferInput(common_buffers.vertex_buf2);
            const FgBufROHandle ndx_buf_res = shadow_depth.AddIndexBufferInput(common_buffers.indices_buf);

            const FgBufROHandle shared_data_res = shadow_depth.AddUniformBufferInput(
                common_buffers.shared_data, Ren::Bitmask{Ren::eStage::VertexShader} | Ren::eStage::FragmentShader);
            const FgBufROHandle instances =
                shadow_depth.AddStorageReadonlyInput(common_buffers.instances, Ren::eStage::VertexShader);
            const FgBufROHandle instance_indices =
                shadow_depth.AddStorageReadonlyInput(common_buffers.instance_indices, Ren::eStage::VertexShader);

            const FgBufROHandle materials =
                shadow_depth.AddStorageReadonlyInput(common_buffers.materials, Ren::eStage::VertexShader);
            const FgImgROHandle noise = shadow_depth.AddTextureInput(frame_textures.noise, Ren::eStage::VertexShader);
            const FgImgROHandle dummy_white =
                shadow_depth.AddTextureInput(frame_textures.dummy_white, Ren::eStage::FragmentShader);

            frame_textures.shadow_depth = shadow_depth.AddDepthOutput(frame_textures.shadow_depth);

            shadow_depth.make_executor<ExShadowDepth>(SHADOWMAP_WIDTH, SHADOWMAP_HEIGHT, &p_list_, vtx_buf1_res,
                                                      vtx_buf2_res, ndx_buf_res, materials, &bindless_tex, instances,
                                                      instance_indices, shared_data_res, noise, dummy_white,
                                                      frame_textures.shadow_depth);
        }
        { // Shadow color
            auto &shadow_color = fg_builder_.AddNode("SHADOW COLOR");
            const FgBufROHandle vtx_buf1_res = shadow_color.AddVertexBufferInput(common_buffers.vertex_buf1);
            const FgBufROHandle vtx_buf2_res = shadow_color.AddVertexBufferInput(common_buffers.vertex_buf2);
            const FgBufROHandle ndx_buf_res = shadow_color.AddIndexBufferInput(common_buffers.indices_buf);

            const FgBufROHandle shared_data_res = shadow_color.AddUniformBufferInput(
                common_buffers.shared_data, Ren::Bitmask{Ren::eStage::VertexShader} | Ren::eStage::FragmentShader);
            const FgBufROHandle instances_res =
                shadow_color.AddStorageReadonlyInput(common_buffers.instances, Ren::eStage::VertexShader);
            const FgBufROHandle instance_indices_res =
                shadow_color.AddStorageReadonlyInput(common_buffers.instance_indices, Ren::eStage::VertexShader);

            const FgBufROHandle materials_buf =
                shadow_color.AddStorageReadonlyInput(common_buffers.materials, Ren::eStage::VertexShader);
            const FgImgROHandle noise = shadow_color.AddTextureInput(frame_textures.noise, Ren::eStage::VertexShader);
            const FgImgROHandle dummy_white =
                shadow_color.AddTextureInput(frame_textures.dummy_white, Ren::eStage::FragmentShader);

            frame_textures.shadow_depth = shadow_color.AddDepthOutput(frame_textures.shadow_depth);
            frame_textures.shadow_color = shadow_color.AddColorOutput(frame_textures.shadow_color);

            shadow_color.make_executor<ExShadowColor>(
                SHADOWMAP_WIDTH, SHADOWMAP_HEIGHT, &p_list_, vtx_buf1_res, vtx_buf2_res, ndx_buf_res, materials_buf,
                &bindless_tex, instances_res, instance_indices_res, shared_data_res, noise, dummy_white,
                frame_textures.shadow_depth, frame_textures.shadow_color);
        }

        frame_textures.depth_desc.w = view_state_.ren_res[0];
        frame_textures.depth_desc.h = view_state_.ren_res[1];
        frame_textures.depth_desc.format = Ren::eFormat::D32_S8;
        // ctx_.capabilities.depth24_stencil8_format ? Ren::eFormat::D24_S8 : Ren::eFormat::D32_S8;
        frame_textures.depth_desc.sampling.wrap = Ren::eWrap::ClampToEdge;

        // Main HDR color
        frame_textures.color_desc.w = view_state_.ren_res[0];
        frame_textures.color_desc.h = view_state_.ren_res[1];
        frame_textures.color_desc.format = Ren::eFormat::RGBA16F;
        frame_textures.color_desc.sampling.filter = Ren::eFilter::Bilinear;
        frame_textures.color_desc.sampling.wrap = Ren::eWrap::ClampToBorder;

        if (deferred_shading) {
            // 4-component world-space normal (alpha or z is roughness)
            frame_textures.normal_desc.w = view_state_.ren_res[0];
            frame_textures.normal_desc.h = view_state_.ren_res[1];
            frame_textures.normal_desc.format = Ren::eFormat::R32UI;
            frame_textures.normal_desc.sampling.wrap = Ren::eWrap::ClampToEdge;
            // packed material params
            frame_textures.specular_desc.w = view_state_.ren_res[0];
            frame_textures.specular_desc.h = view_state_.ren_res[1];
            frame_textures.specular_desc.format = Ren::eFormat::R32UI;
            frame_textures.specular_desc.sampling.wrap = Ren::eWrap::ClampToEdge;
        } else {
            // 4-component world-space normal (alpha or z is roughness)
            frame_textures.normal_desc.w = view_state_.ren_res[0];
            frame_textures.normal_desc.h = view_state_.ren_res[1];
#if USE_OCT_PACKED_NORMALS == 1
            frame_textures.normal_desc.format = Ren::eFormat::RGB10_A2;
#else
            frame_textures.normal_desc.format = Ren::eFormat::RGBA8;
#endif
            frame_textures.normal_desc.sampling.wrap = Ren::eWrap::ClampToEdge;
            // 4-component specular (alpha is roughness)
            frame_textures.specular_desc.w = view_state_.ren_res[0];
            frame_textures.specular_desc.h = view_state_.ren_res[1];
            frame_textures.specular_desc.format = Ren::eFormat::RGBA8;
            frame_textures.specular_desc.sampling.wrap = Ren::eWrap::ClampToEdge;
        }

        // 4-component albedo (alpha is unused)
        frame_textures.albedo_desc.w = view_state_.ren_res[0];
        frame_textures.albedo_desc.h = view_state_.ren_res[1];
        frame_textures.albedo_desc.format = Ren::eFormat::RGBA8;
        frame_textures.albedo_desc.sampling.wrap = Ren::eWrap::ClampToEdge;

        //
        // Depth prepass
        //
        if (!list.render_settings.debug_culling) {
            auto &depth_fill = fg_builder_.AddNode("DEPTH FILL");

            const FgBufROHandle vtx_buf1 = depth_fill.AddVertexBufferInput(common_buffers.vertex_buf1);
            const FgBufROHandle vtx_buf2 = depth_fill.AddVertexBufferInput(common_buffers.vertex_buf2);
            const FgBufROHandle ndx_buf = depth_fill.AddIndexBufferInput(common_buffers.indices_buf);

            const FgBufROHandle shared_data_res = depth_fill.AddUniformBufferInput(
                common_buffers.shared_data, Ren::Bitmask{Ren::eStage::VertexShader} | Ren::eStage::FragmentShader);
            const FgBufROHandle instances =
                depth_fill.AddStorageReadonlyInput(common_buffers.instances, Ren::eStage::VertexShader);
            const FgBufROHandle instance_indices =
                depth_fill.AddStorageReadonlyInput(common_buffers.instance_indices, Ren::eStage::VertexShader);

            const FgBufROHandle materials =
                depth_fill.AddStorageReadonlyInput(common_buffers.materials, Ren::eStage::VertexShader);
            const FgImgROHandle noise = depth_fill.AddTextureInput(frame_textures.noise, Ren::eStage::VertexShader);
            const FgImgROHandle dummy_white =
                depth_fill.AddTextureInput(frame_textures.dummy_white, Ren::eStage::FragmentShader);

            frame_textures.depth = depth_fill.AddDepthOutput(MAIN_DEPTH_TEX, frame_textures.depth_desc);

            { // Image that holds 3D motion vectors
                FgImgDesc desc;
                desc.w = view_state_.ren_res[0];
                desc.h = view_state_.ren_res[1];
                desc.format = Ren::eFormat::RGBA16F;
                desc.sampling.wrap = Ren::eWrap::ClampToEdge;

                frame_textures.velocity = depth_fill.AddColorOutput(MAIN_VELOCITY_TEX, desc);
            }

            depth_fill.make_executor<ExDepthFill>(&p_list_, &view_state_, deferred_shading /* clear_depth */, vtx_buf1,
                                                  vtx_buf2, ndx_buf, materials, &bindless_tex, instances,
                                                  instance_indices, shared_data_res, noise, dummy_white,
                                                  frame_textures.depth, frame_textures.velocity);
        }

        //
        // Downsample depth
        //
        FgImgRWHandle depth_down_2x;
        FgImgRWHandle depth_hierarchy;

        if ((list.render_settings.ssao_quality != eSSAOQuality::Off ||
             list.render_settings.reflections_quality != eReflectionsQuality::Off) &&
            !list.render_settings.debug_wireframe) {
            // TODO: get rid of this (or use on low spec only)
            depth_down_2x = AddDownsampleDepthPass(common_buffers, frame_textures.depth);

            auto &depth_h = fg_builder_.AddNode("DEPTH HIERARCHY");
            const FgImgROHandle depth = depth_h.AddTextureInput(frame_textures.depth, Ren::eStage::ComputeShader);
            const FgBufRWHandle atomic_cnt =
                depth_h.AddStorageOutput(common_buffers.atomic_cnt, Ren::eStage::ComputeShader);

            { // 32-bit float depth hierarchy
                FgImgDesc desc;
                desc.w = ((view_state_.ren_res[0] + ExDepthHierarchy::TileSize - 1) / ExDepthHierarchy::TileSize) *
                         ExDepthHierarchy::TileSize;
                desc.h = ((view_state_.ren_res[1] + ExDepthHierarchy::TileSize - 1) / ExDepthHierarchy::TileSize) *
                         ExDepthHierarchy::TileSize;
                desc.format = Ren::eFormat::R32F;
                desc.mip_count = ExDepthHierarchy::MipCount;
                desc.sampling.wrap = Ren::eWrap::ClampToEdge;
                desc.sampling.filter = Ren::eFilter::Nearest;

                // Add view for each mip level
                for (int i = 0; i < desc.mip_count; ++i) {
                    desc.views.push_back({desc.format, i, 1, 0, 1});
                }

                depth_hierarchy = depth_h.AddStorageImageOutput("Depth Hierarchy", desc, Ren::eStage::ComputeShader);
            }

            depth_h.make_executor<ExDepthHierarchy>(sh_, &view_state_, depth, atomic_cnt, depth_hierarchy);
        }

        if (!list.render_settings.debug_wireframe /*&& list.render_settings.taa_mode != eTAAMode::Static*/) {
            AddFillStaticVelocityPass(common_buffers, frame_textures.depth, frame_textures.velocity);
        }

        std::tie(frame_textures.dilated_depth, frame_textures.dilated_velocity, frame_textures.disocclusion_mask) =
            AddDisocclusionPasses(frame_textures.depth, frame_textures.velocity);

        if (deferred_shading) {
            // Skydome drawing
            AddSkydomePass(common_buffers, frame_textures);

            // GBuffer filling pass
            AddGBufferFillPass(common_buffers, bindless_tex, frame_textures);

            // SSAO
            frame_textures.ssao = AddGTAOPasses(list.render_settings.ssao_quality, frame_textures.depth,
                                                frame_textures.velocity, frame_textures.normal);

            if ((ctx_.capabilities.hwrt || ctx_.capabilities.swrt) &&
                list.render_settings.shadows_quality == eShadowsQuality::Raytraced) {
                frame_textures.sun_shadow = AddHQSunShadowsPasses(
                    common_buffers, acc_structs, bindless_tex, rt_geo_instances_res[int(eTLASIndex::Shadow)],
                    rt_obj_instances_res[int(eTLASIndex::Shadow)], frame_textures,
                    list.render_settings.debug_denoise == eDebugDenoise::Shadow);
            } else if (list.render_settings.shadows_quality != eShadowsQuality::Off) {
                frame_textures.sun_shadow = AddLQSunShadowsPass(common_buffers, bindless_tex, frame_textures);
            } else {
                frame_textures.sun_shadow = fg_builder_.ImportResource(dummy_white_);
            }

            // GI cache
            AddGICachePasses(common_buffers, persistent_data, acc_structs, bindless_tex,
                             rt_geo_instances_res[int(eTLASIndex::Main)], rt_obj_instances_res[int(eTLASIndex::Main)],
                             frame_textures);

            // GI
            AddDiffusePasses(list.render_settings.debug_denoise == eDebugDenoise::GI, common_buffers, acc_structs,
                             bindless_tex, depth_hierarchy, rt_geo_instances_res[int(eTLASIndex::Main)],
                             rt_obj_instances_res[int(eTLASIndex::Main)], frame_textures);

            // GBuffer shading pass
            AddDeferredShadingPass(common_buffers, frame_textures, list.render_settings.gi_quality != eGIQuality::Off);

            // Emissives pass
            AddEmissivePass(common_buffers, persistent_data, bindless_tex, frame_textures);

            // Additional forward pass (for custom-shaded objects)
            AddForwardOpaquePass(common_buffers, persistent_data, bindless_tex, frame_textures);
        } else {
            const bool use_ssao =
                list.render_settings.ssao_quality != eSSAOQuality::Off && !list.render_settings.debug_wireframe;
            if (use_ssao) {
                frame_textures.ssao = AddSSAOPasses(depth_down_2x, frame_textures.depth);
            } else {
                frame_textures.ssao = fg_builder_.ImportResource(dummy_white_);
            }

            AddForwardOpaquePass(common_buffers, persistent_data, bindless_tex, frame_textures);
        }

        //
        // Reflections
        //
        if (list.render_settings.reflections_quality != eReflectionsQuality::Off &&
            !list.render_settings.debug_wireframe) {
            if (cur_hq_ssr_enabled) {
                AddHQSpecularPasses(deferred_shading, list.render_settings.debug_denoise == eDebugDenoise::Reflection,
                                    common_buffers, acc_structs, bindless_tex, depth_hierarchy,
                                    rt_geo_instances_res[int(eTLASIndex::Main)],
                                    rt_obj_instances_res[int(eTLASIndex::Main)], frame_textures);
            } else {
                AddLQSpecularPasses(common_buffers, depth_down_2x, frame_textures);
            }
        }

        //
        // Transparency
        //
        if (deferred_shading) {
            AddOITPasses(common_buffers, acc_structs, bindless_tex, depth_hierarchy,
                         rt_geo_instances_res[int(eTLASIndex::Main)], rt_obj_instances_res[int(eTLASIndex::Main)],
                         frame_textures);
        } else {
            // Simple transparent pass
            AddForwardTransparentPass(common_buffers, persistent_data, bindless_tex, frame_textures);
        }

        if (list.render_settings.vol_quality != eVolQuality::Off) {
            AddVolumetricPasses(common_buffers, acc_structs, rt_geo_instances_res[int(eTLASIndex::Volume)],
                                rt_obj_instances_res[int(eTLASIndex::Volume)], frame_textures);
        }

        frame_textures.exposure = AddAutoexposurePasses(frame_textures.color);

        //
        // Debug stuff
        //
        if (list.render_settings.debug_probes != -1) {
            auto &debug_probes = fg_builder_.AddNode("DEBUG PROBES");

            auto *data = fg_builder_.AllocTempData<ExDebugProbes::Args>();
            data->shared_data =
                debug_probes.AddUniformBufferInput(common_buffers.shared_data, Ren::eStage::VertexShader);
            data->offset = debug_probes.AddTextureInput(frame_textures.gi_cache_offset, Ren::eStage::VertexShader);
            data->irradiance =
                debug_probes.AddTextureInput(frame_textures.gi_cache_irradiance, Ren::eStage::FragmentShader);
            data->distance =
                debug_probes.AddTextureInput(frame_textures.gi_cache_distance, Ren::eStage::FragmentShader);

            data->volume_to_debug = list.render_settings.debug_probes;
            data->probe_volumes = persistent_data.probe_volumes;

            frame_textures.depth = data->depth = debug_probes.AddDepthOutput(frame_textures.depth);
            frame_textures.color = data->output = debug_probes.AddColorOutput(frame_textures.color);

            debug_probes.make_executor<ExDebugProbes>(prim_draw_, sh_, &view_state_, data);
        }

        if (list.render_settings.debug_oit_layer != -1) {
            auto &debug_oit = fg_builder_.AddNode("DEBUG OIT");

            auto *data = fg_builder_.AllocTempData<ExDebugOIT::Args>();
            data->layer_index = list.render_settings.debug_oit_layer;
            data->oit_depth = debug_oit.AddStorageReadonlyInput(frame_textures.oit_depth, Ren::eStage::ComputeShader);
            frame_textures.color = data->output =
                debug_oit.AddStorageImageOutput(frame_textures.color, Ren::eStage::ComputeShader);

            debug_oit.make_executor<ExDebugOIT>(sh_, &view_state_, data);
        }

        if (list.render_settings.debug_rt != eDebugRT::Off && (ctx_.capabilities.hwrt || ctx_.capabilities.swrt)) {
            auto &debug_rt = fg_builder_.AddNode("DEBUG RT");

            const Ren::eStage stage =
                ctx_.capabilities.hwrt ? Ren::eStage::RayTracingShader : Ren::eStage::ComputeShader;

            auto *data = fg_builder_.AllocTempData<ExDebugRT::Args>();
            data->shared_data = debug_rt.AddUniformBufferInput(common_buffers.shared_data, stage);
            data->geo_data =
                debug_rt.AddStorageReadonlyInput(rt_geo_instances_res[int(list.render_settings.debug_rt) - 1], stage);
            data->materials = debug_rt.AddStorageReadonlyInput(common_buffers.materials, stage);
            data->vtx_buf1 = debug_rt.AddStorageReadonlyInput(common_buffers.vertex_buf1, stage);
            data->vtx_buf2 = debug_rt.AddStorageReadonlyInput(common_buffers.vertex_buf2, stage);
            data->ndx_buf = debug_rt.AddStorageReadonlyInput(common_buffers.indices_buf, stage);
            data->lights = debug_rt.AddStorageReadonlyInput(common_buffers.lights, stage);
            data->shadow_depth = debug_rt.AddTextureInput(frame_textures.shadow_depth, stage);
            data->shadow_color = debug_rt.AddTextureInput(frame_textures.shadow_color, stage);
            data->ltc_luts = debug_rt.AddTextureInput(frame_textures.ltc_luts, stage);
            data->cells = debug_rt.AddStorageReadonlyInput(common_buffers.rt_cells, stage);
            data->items = debug_rt.AddStorageReadonlyInput(common_buffers.rt_items, stage);

            data->tlas_buf = debug_rt.AddStorageReadonlyInput(
                acc_structs.rt_tlas_buf[int(list.render_settings.debug_rt) - 1], stage);
            data->tlas = acc_structs.rt_tlases[int(list.render_settings.debug_rt) - 1];

            if (!ctx_.capabilities.hwrt) {
                data->swrt.root_node = acc_structs.swrt.rt_root_node;
                data->swrt.rt_blas = debug_rt.AddStorageReadonlyInput(acc_structs.swrt.rt_blas_buf, stage);
                data->swrt.prim_ndx = debug_rt.AddStorageReadonlyInput(acc_structs.swrt.rt_prim_indices, stage);
                data->swrt.mesh_instances = debug_rt.AddStorageReadonlyInput(
                    rt_obj_instances_res[int(list.render_settings.debug_rt) - 1], stage);
            }

            data->env = debug_rt.AddTextureInput(frame_textures.envmap, stage);

            data->irradiance = debug_rt.AddTextureInput(frame_textures.gi_cache_irradiance, stage);
            data->distance = debug_rt.AddTextureInput(frame_textures.gi_cache_distance, stage);
            data->offset = debug_rt.AddTextureInput(frame_textures.gi_cache_offset, stage);

            frame_textures.color = data->output = debug_rt.AddStorageImageOutput(frame_textures.color, stage);

            debug_rt.make_executor<ExDebugRT>(sh_, &view_state_, &bindless_tex, data);
        }

        FgImgRWHandle resolved_color;

        //
        // Temporal resolve
        //
        const bool use_taa = list.render_settings.taa_mode != eTAAMode::Off && !list.render_settings.debug_wireframe;
        if (use_taa) {
            resolved_color = AddTSRPass(frame_textures, list.render_settings.taa_mode);
        } else {
            resolved_color = frame_textures.color;
        }

        //
        // Sharpening
        //
        if (list.render_settings.enable_sharpen) {
            resolved_color = AddSharpenPass(resolved_color, frame_textures.exposure, true);
        }

        //
        // Motion blur
        //
        if (list.render_settings.enable_motion_blur) {
            resolved_color = AddMotionBlurPasses(resolved_color, frame_textures);
        }

#if defined(REN_GL_BACKEND) && 0 // gl-only for now
        const bool apply_dof = (list.render_flags & EnableDOF) && list.draw_cam.focus_near_mul > 0.0f &&
                               list.draw_cam.focus_far_mul > 0.0f && ((list.render_flags & DebugWireframe) == 0);

        if (apply_dof) {
            const int qres_w = cur_scr_w / 4, qres_h = cur_scr_h / 4;

            const char *color_in_name = nullptr;
            const char *dof_out_name = refl_out_name;

            if (view_state_.is_multisampled) {
                // color_tex = resolved_or_transparent_tex_->handle();
                color_in_name = RESOLVED_COLOR_TEX;
            } else {
                if ((list.render_flags & EnableTaa) != 0u) {
                    // color_tex = resolved_or_transparent_tex_->handle();
                    color_in_name = RESOLVED_COLOR_TEX;
                } else {
                    // color_tex = color_tex_->handle();
                    color_in_name = MAIN_COLOR_TEX;
                }
            }

            rp_dof_.Setup(fg_builder_, &list.draw_cam, &view_state_, SHARED_DATA_BUF, color_in_name, MAIN_DEPTH_TEX,
                          DEPTH_DOWN_2X_TEX, DEPTH_DOWN_4X_TEX, down_tex_4x_, dof_out_name);
            rp_tail->p_next = &rp_dof_;
            rp_tail = rp_tail->p_next;
        }
#endif

        //
        // Bloom
        //
        FgImgRWHandle bloom;
        if (list.render_settings.enable_bloom && !list.render_settings.debug_wireframe) {
            bloom = AddBloomPasses(resolved_color, frame_textures.exposure, true);
        }

        //
        // Debugging
        //
        if (list.render_settings.debug_motion) {
            resolved_color = AddDebugVelocityPass(frame_textures.velocity);
            bloom = {};
        }
        if (list.render_settings.debug_disocclusion) {
            resolved_color = frame_textures.disocclusion_mask;
            bloom = {};
        }
        if (list.render_settings.debug_ssao) {
            resolved_color = frame_textures.ssao;
            bloom = {};
        }
        if (list.render_settings.debug_depth) {
            resolved_color = AddDebugGBufferPass(frame_textures, 0);
            bloom = {};
        }
        if (list.render_settings.debug_albedo) {
            resolved_color = frame_textures.albedo;
            bloom = {};
        }
        if (list.render_settings.debug_normals) {
            resolved_color = AddDebugGBufferPass(frame_textures, 1);
            bloom = {};
        }
        if (list.render_settings.debug_roughness) {
            resolved_color = AddDebugGBufferPass(frame_textures, 2);
            bloom = {};
        }
        if (list.render_settings.debug_metallic) {
            resolved_color = AddDebugGBufferPass(frame_textures, 3);
            bloom = {};
        }

        bool apply_dof = false;

        //
        // Combine with blurred and tonemap
        //
        {
            FgImgRWHandle color;
            const char *output_tex = nullptr;

            if ((list.render_settings.taa_mode != eTAAMode::Off && !list.render_settings.debug_wireframe) ||
                apply_dof) {
                if (apply_dof) {
                    if (list.render_settings.taa_mode != eTAAMode::Off) {
                        color = frame_textures.color;
                    } else {
                        // color_tex = DOF_COLOR_TEX;
                    }
                } else {
                    color = resolved_color;
                }
            } else {
                color = frame_textures.color;
            }

            if (/*(list.render_flags & EnableFxaa) && !list.render_settings.debug_wireframe*/ false) {
                output_tex = MAIN_COMBINED_TEX;
            } else {
                if (!target) {
                    // output to back buffer
                    output_tex = nullptr;
                } else {
                    // TODO: fix this!!!
                    // output_tex = target->attachments[0].tex->handle();
                }
            }

            auto &postprocess = fg_builder_.AddNode("POSTPROCESS");

            ex_postprocess_args_ = {};
            ex_postprocess_args_.exposure =
                postprocess.AddTextureInput(frame_textures.exposure, Ren::eStage::FragmentShader);
            ex_postprocess_args_.color = postprocess.AddTextureInput(color, Ren::eStage::FragmentShader);
            if (list.render_settings.enable_bloom && bloom) {
                ex_postprocess_args_.bloom = postprocess.AddTextureInput(bloom, Ren::eStage::FragmentShader);
            } else {
                ex_postprocess_args_.bloom =
                    postprocess.AddTextureInput(frame_textures.dummy_black, Ren::eStage::FragmentShader);
            }
            if (tonemap_lut_) {
                ex_postprocess_args_.lut =
                    postprocess.AddTextureInput(frame_textures.tonemap_lut, Ren::eStage::FragmentShader);
            }
            if (output_tex) {
                FgImgDesc desc;
                desc.w = view_state_.out_res[0];
                desc.h = view_state_.out_res[1];
                desc.format = Ren::eFormat::RGB8;
                desc.sampling.filter = Ren::eFilter::Bilinear;
                desc.sampling.wrap = Ren::eWrap::ClampToEdge;

                ex_postprocess_args_.output = postprocess.AddColorOutput(output_tex, desc);
            } else if (target) {
                const FgImgRWHandle target_ref = fg_builder_.ImportResource(target);
                ex_postprocess_args_.output = postprocess.AddColorOutput(target_ref);
                if (blit_to_backbuffer) {
                    const FgImgRWHandle backbuffer_ref = fg_builder_.ImportResource(ctx_.backbuffer_img());
                    ex_postprocess_args_.output2 = postprocess.AddColorOutput(backbuffer_ref);
                }
            } else {
                const FgImgRWHandle backbuffer_ref = fg_builder_.ImportResource(ctx_.backbuffer_img());
                ex_postprocess_args_.output = postprocess.AddColorOutput(backbuffer_ref);
            }
            ex_postprocess_args_.linear_sampler = linear_sampler_;
            ex_postprocess_args_.tonemap_mode = int(list.render_settings.tonemap_mode);
            ex_postprocess_args_.inv_gamma = 1.0f / list.draw_cam.gamma;
            ex_postprocess_args_.fade = list.draw_cam.fade;
            ex_postprocess_args_.aberration = list.render_settings.enable_aberration ? 1.0f : 0.0f;
            ex_postprocess_args_.purkinje = list.render_settings.enable_purkinje ? 1.0f : 0.0f;

            backbuffer_sources_.push_back(ex_postprocess_args_.output);

            postprocess.make_executor<ExPostprocess>(prim_draw_, sh_, &view_state_, &ex_postprocess_args_);
        }

        { // Readback exposure
            auto &read_exposure = fg_builder_.AddNode("READ EXPOSURE");

            auto *data = fg_builder_.AllocTempData<ExReadExposure::Args>();
            data->input = read_exposure.AddTransferImageInput(frame_textures.exposure);

            FgBufDesc desc;
            desc.type = Ren::eBufType::Readback;
            desc.size = Ren::MaxFramesInFlight * sizeof(float);
            data->output = read_exposure.AddTransferOutput("Exposure Readback", desc);

            backbuffer_sources_.push_back(data->output);

            ex_read_exposure_.Setup(data);
            read_exposure.set_executor(&ex_read_exposure_);
        }

        fg_builder_.Compile(backbuffer_sources_);
        const uint64_t rp_setup_end_us = Sys::GetTimeUs();
        ctx_.log()->Info("Framegraph setup is done in %.2fms", (rp_setup_end_us - rp_setup_beg_us) * 0.001);
    } else {
        // Update handles of external resources
        fg_builder_.ImportResource(persistent_data.vertex_buf1->handle());
        fg_builder_.ImportResource(persistent_data.vertex_buf2->handle());
        fg_builder_.ImportResource(persistent_data.indices_buf->handle());
        fg_builder_.ImportResource(persistent_data.skin_vertex_buf->handle());
        fg_builder_.ImportResource(persistent_data.delta_buf->handle());

        //  Use correct backbuffer image (assume topology is not changed)
        //  TODO: get rid of this
        auto &postprocess = *fg_builder_.FindNode("POSTPROCESS");
        if (target) {
            const FgImgRWHandle target_ref = fg_builder_.ImportResource(target);
            ex_postprocess_args_.output = postprocess.ReplaceColorOutput(0, target_ref);
            ex_postprocess_args_.output2 = {};
            if (blit_to_backbuffer) {
                const FgImgRWHandle backbuffer_ref = fg_builder_.ImportResource(ctx_.backbuffer_img());
                ex_postprocess_args_.output2 = postprocess.ReplaceColorOutput(1, backbuffer_ref);
            }
        } else {
            const FgImgRWHandle backbuffer_ref = fg_builder_.ImportResource(ctx_.backbuffer_img());
            ex_postprocess_args_.output = postprocess.ReplaceColorOutput(0, backbuffer_ref);
            ex_postprocess_args_.output2 = {};
        }

        ex_postprocess_args_.fade = list.draw_cam.fade;

        postprocess.make_executor<ExPostprocess>(prim_draw_, sh_, &view_state_, &ex_postprocess_args_);
    }

    fg_builder_.Execute();

    { // store info to use it in next frame
        view_state_.down_buf_view_from_world = list.draw_cam.view_matrix();
        view_state_.prev_sun_dir = list.env.sun_dir;

        view_state_.prev_clip_from_world = list.draw_cam.proj_matrix() * list.draw_cam.view_matrix();
        view_state_.prev_view_from_world = list.draw_cam.view_matrix();
        view_state_.prev_clip_from_view = list.draw_cam.proj_matrix_offset();

        Ren::Mat4f view_from_world_no_translation = list.draw_cam.view_matrix();
        view_from_world_no_translation[3][0] = view_from_world_no_translation[3][1] =
            view_from_world_no_translation[3][2] = 0;
        view_state_.prev_clip_from_world_no_translation =
            (list.draw_cam.proj_matrix() * view_from_world_no_translation);

        view_state_.prev_pre_exposure = view_state_.pre_exposure;
        view_state_.prev_world_origin = list.world_origin;
    }

    const uint64_t cpu_draw_end_us = Sys::GetTimeUs();

    // store values for current frame
    backend_cpu_start_ = cpu_draw_start_us;
    backend_cpu_end_ = cpu_draw_end_us;

    // Write timestamp at the end of execution
    backend_gpu_end_ = ctx_.WriteTimestamp(false);

    __itt_task_end(__g_itt_domain);
}

void Eng::Renderer::SetTonemapLUT(const int res, const Ren::eFormat format, Ren::Span<const uint8_t> data) {
    assert(format == Ren::eFormat::RGB10_A2);

    if (data.empty()) {
        ctx_.ReleaseImage(tonemap_lut_);
        tonemap_lut_ = {};
        return;
    }

    if (!tonemap_lut_) {
        Ren::ImgParams params = {};
        params.w = params.h = params.d = res;
        params.usage = Ren::Bitmask(Ren::eImgUsage::Sampled) | Ren::eImgUsage::Transfer;
        params.format = Ren::eFormat::RGB10_A2;
        params.sampling.filter = Ren::eFilter::Bilinear;
        params.sampling.wrap = Ren::eWrap::ClampToEdge;

        tonemap_lut_ = ctx_.CreateImage(Ren::String{"Tonemap LUT"}, {}, params, ctx_.default_mem_allocs());
    }

    const uint32_t data_len = res * res * res * sizeof(uint32_t);

    Ren::BufferMain temp_upload_buf_main = {};
    Ren::BufferCold temp_upload_buf_cold = {};
    if (!Buffer_Init(ctx_.api(), temp_upload_buf_main, temp_upload_buf_cold, Ren::String{"Temp tonemap LUT upload"},
                     Ren::eBufType::Upload, data_len, ctx_.log())) {
        ctx_.log()->Error("Failed to initialize upload buffer for tonemap LUT");
        return;
    }

    { // update stage buffer
        uint32_t *mapped_ptr =
            reinterpret_cast<uint32_t *>(Buffer_Map(ctx_.api(), temp_upload_buf_main, temp_upload_buf_cold));
        memcpy(mapped_ptr, data.data(), data_len);
        Buffer_Unmap(ctx_.api(), temp_upload_buf_main, temp_upload_buf_cold);
    }

    { // update texture
        Ren::CommandBuffer cmd_buf = ctx_.BegTempSingleTimeCommands();

        const Ren::TransitionInfo res_transitions1[] = {{tonemap_lut_, Ren::eResState::CopyDst}};
        TransitionResourceStates(ctx_.api(), ctx_.storages(), cmd_buf, Ren::AllStages, Ren::AllStages,
                                 res_transitions1);

        const auto &[img_main, img_cold] = ctx_.storages().images[tonemap_lut_];

        Image_SetSubImage(ctx_.api(), img_main, img_cold, 0, 0, Ren::Vec3i{0}, Ren::Vec3i{res}, Ren::eFormat::RGB10_A2,
                          temp_upload_buf_main, cmd_buf, 0, data_len);

        const Ren::TransitionInfo res_transitions2[] = {{tonemap_lut_, Ren::eResState::ShaderResource}};
        TransitionResourceStates(ctx_.api(), ctx_.storages(), cmd_buf, Ren::AllStages, Ren::AllStages,
                                 res_transitions2);

        ctx_.EndTempSingleTimeCommands(cmd_buf);
    }

    Buffer_DestroyImmediately(ctx_.api(), temp_upload_buf_main, temp_upload_buf_cold);
}

void Eng::Renderer::UpdatePixelFilterTable(const ePixelFilter filter, float filter_width) {
    float (*filter_func)(float v, float width);

    switch (filter) {
    case ePixelFilter::Box:
        filter_func = RendererInternal::filter_box;
        filter_width = 1.0f;
        break;
    case ePixelFilter::Gaussian:
        filter_func = RendererInternal::filter_gaussian;
        filter_width *= 3.0f;
        break;
    case ePixelFilter::BlackmanHarris:
        filter_func = RendererInternal::filter_blackman_harris;
        filter_width *= 2.0f;
        break;
    default:
        assert(false && "Unknown filter!");
    }

    px_filter_table_ =
        CDFInverted(PxFilterTableSize, 0.0f, filter_width * 0.5f,
                    std::bind(filter_func, std::placeholders::_1, filter_width), true /* make_symmetric */);
}

void Eng::Renderer::InitBackendInfo() {
    /*if (frame_index_ < 10) {
        // Skip a few initial frames
        return;
    }*/

    backend_info_.passes_info.clear();
    backend_info_.resources_info.clear();

    for (int i = 0; i < int(fg_builder_.node_timings_[ctx_.backend_frame()].size()) - 1 &&
                    settings.debug_frame != eDebugFrame::Off;
         ++i) {
        const auto &t = fg_builder_.node_timings_[ctx_.backend_frame()][i];

        pass_info_t &info = backend_info_.passes_info.emplace_back();
        info.name = t.name;
        info.duration_us = ctx_.GetTimestampIntervalDurationUs(t.query_beg, t.query_end);

        if (settings.debug_frame == eDebugFrame::Full) {
            const FgNode *node = fg_builder_.GetReorderedNode(i);
            for (const FgResource &res : node->input()) {
                info.input.push_back(fg_builder_.GetResourceDebugInfo(res));
            }
            std::sort(begin(info.input), end(info.input));
            for (const FgResource &res : node->output()) {
                info.output.push_back(fg_builder_.GetResourceDebugInfo(res));
            }
            std::sort(begin(info.output), end(info.output));
        }
    }

    if (settings.debug_frame == eDebugFrame::Full) {
        const auto &fg_buffers = fg_builder_.buffers();
        Ren::SmallVector<int, 256> indices(fg_buffers.capacity(), -1);
        uint32_t heap_size = 0; // dummy for the case when memory heaps are not available
        for (auto it = std::cbegin(fg_builder_.name_to_buffer()); it != std::cend(fg_builder_.name_to_buffer()); ++it) {
            const auto &[fgbuf_main, fgbuf_cold] = fg_buffers.GetUnsafe(it->val);
            if (fgbuf_cold.external || !fgbuf_main.handle || fgbuf_cold.alias_of != -1 ||
                !fgbuf_cold.lifetime.is_used() || fgbuf_cold.desc.type == Ren::eBufType::Upload ||
                fgbuf_cold.desc.type == Ren::eBufType::Readback) {
                continue;
            }

            resource_info_t &info = backend_info_.resources_info.emplace_back();
            info.name = std::string("[Buf] ") + fgbuf_cold.name.c_str();
            fg_builder_.GetResourceFrameLifetime(fgbuf_cold, info.lifetime);

            const auto &[buf_main, buf_cold] = ctx_.storages().buffers[fgbuf_main.handle];

            const Ren::MemAllocation &alloc = buf_cold.alloc;
            if (alloc.pool != 0xffff && alloc.owner == nullptr) {
                info.heap = alloc.pool;
                info.offset = alloc.offset;
                info.size = alloc.block;
            } else {
                info.heap = 0;
                info.offset = heap_size;
                info.size = buf_cold.size;
                heap_size += buf_cold.size;
            }

            indices[it->val] = int(backend_info_.resources_info.size() - 1);
        }
        for (auto it = std::cbegin(fg_builder_.name_to_buffer()); it != std::cend(fg_builder_.name_to_buffer()); ++it) {
            const auto &[fgbuf_main, fgbuf_cold] = fg_buffers.GetUnsafe(it->val);
            if (fgbuf_cold.external || !fgbuf_main.handle || fgbuf_cold.alias_of == -1 ||
                !fgbuf_cold.lifetime.is_used() || fgbuf_cold.desc.type == Ren::eBufType::Upload ||
                fgbuf_cold.desc.type == Ren::eBufType::Readback) {
                continue;
            }

            resource_info_t &info = backend_info_.resources_info.emplace_back();
            info.name = std::string("[Buf] ") + fgbuf_cold.name.c_str();
            fg_builder_.GetResourceFrameLifetime(fgbuf_cold, info.lifetime);

            const auto &[buf_main, buf_cold] = ctx_.storages().buffers[fgbuf_main.handle];

            const Ren::MemAllocation &alloc = buf_cold.alloc;
            if (alloc.pool != 0xffff && alloc.owner == nullptr) {
                info.heap = alloc.pool;
                info.offset = alloc.offset;
                info.size = alloc.block;
            } else {
                info.heap = 0;
                info.offset = backend_info_.resources_info[indices[fgbuf_cold.alias_of]].offset;
                info.size = buf_cold.size;
            }
        }

        heap_size = 0; // dummy for the case when memory heaps are not available

        const auto &fg_images = fg_builder_.images();
        indices.clear();
        indices.resize(fg_images.capacity(), -1);
        for (auto it = std::cbegin(fg_builder_.name_to_image()); it != std::cend(fg_builder_.name_to_image()); ++it) {
            const auto &[fgimg_main, fgimg_cold] = fg_images.GetUnsafe(it->val);
            if (fgimg_cold.external || !fgimg_main.handle_to_own || fgimg_cold.alias_of != -1 ||
                !fgimg_cold.lifetime.is_used()) {
                continue;
            }

            resource_info_t &info = backend_info_.resources_info.emplace_back();
            info.name = std::string("[Img] ") + fgimg_cold.name.c_str();
            fg_builder_.GetResourceFrameLifetime(fgimg_cold, info.lifetime);

            const auto &[img_main, img_cold] = ctx_.storages().images[fgimg_main.handle_to_own];

            const Ren::MemAllocation &alloc = img_cold.alloc;
            if (alloc.pool != 0xffff && alloc.owner == nullptr) {
                info.heap = alloc.pool;
                info.offset = alloc.offset;
                info.size = alloc.block;
            } else {
                info.heap = 1;
                info.offset = heap_size;
                info.size = GetDataLenBytes(img_cold.params);
                heap_size += info.size;
            }

            indices[it->val] = int(backend_info_.resources_info.size() - 1);
        }
        for (auto it = std::cbegin(fg_builder_.name_to_image()); it != std::cend(fg_builder_.name_to_image()); ++it) {
            const auto &[fgimg_main, fgimg_cold] = fg_images.GetUnsafe(it->val);
            if (fgimg_cold.external || !fgimg_main.handle_to_own || fgimg_cold.alias_of == -1 ||
                !fgimg_cold.lifetime.is_used()) {
                continue;
            }

            resource_info_t &info = backend_info_.resources_info.emplace_back();
            info.name = std::string("[Img] ") + fgimg_cold.name.c_str();
            fg_builder_.GetResourceFrameLifetime(fgimg_cold, info.lifetime);

            const auto &[img_main, img_cold] = ctx_.storages().images[fgimg_main.handle_to_own];

            const Ren::MemAllocation &alloc = img_cold.alloc;
            if (alloc.pool != 0xffff && alloc.owner == nullptr) {
                info.heap = alloc.pool;
                info.offset = alloc.offset;
                info.size = alloc.block;
            } else {
                info.heap = 1;
                info.offset = backend_info_.resources_info[indices[fgimg_cold.alias_of]].offset;
                info.size = GetDataLenBytes(img_cold.params);
                heap_size += info.size;
            }
        }
    }

    backend_info_.cpu_start_timepoint_us = backend_cpu_start_;
    backend_info_.cpu_end_timepoint_us = backend_cpu_end_;

    backend_info_.gpu_total_duration = 0;
    if (backend_gpu_start_ != -1 && backend_gpu_end_ != -1) {
        backend_info_.gpu_total_duration = ctx_.GetTimestampIntervalDurationUs(backend_gpu_start_, backend_gpu_end_);
    }
}

void Eng::Renderer::InitPipelinesForProgram(const Ren::ProgramHandle prog, const Ren::Bitmask<Ren::eMatFlags> mat_flags,
                                            Ren::SmallVectorImpl<Ren::PipelineHandle> &out_pipelines) const {
    /*for (int i = 0; i < int(eFwdPipeline::_Count); ++i) {
        Ren::RastState rast_state = rast_states_[i];

        if (mat_flags & Ren::eMatFlags::TwoSided) {
            rast_state.poly.cull = uint8_t(Ren::eCullFace::None);
        }

        if (mat_flags & uint32_t(Ren::eMatFlags::AlphaBlend) && i != int(eFwdPipeline::Wireframe)) {
            rast_state.depth.compare_op = unsigned(Ren::eCompareOp::Less);

            rast_state.blend.enabled = true;
            rast_state.blend.src_color = rast_state.blend.src_alpha = unsigned(Ren::eBlendFactor::SrcAlpha);
            rast_state.blend.dst_color = rast_state.blend.dst_alpha = unsigned(Ren::eBlendFactor::OneMinusSrcAlpha);
        }

        // find of create pipeline (linear search is good enough)
        uint32_t new_index = 0xffffffff;
        for (auto it = std::begin(storage); it != std::end(storage); ++it) {
            if (it->prog() == prog && it->rast_state() == rast_state) {
                new_index = it.index();
                break;
            }
        }

        if (new_index == 0xffffffff) {
            new_index = storage.emplace();
            Ren::Pipeline &new_pipeline = storage.at(new_index);

            const bool res =
                new_pipeline.Init(ctx_.api(), rast_state, prog, draw_pass_vi_, rp_main_draw_, 0, ctx_.log());
            if (!res) {
                ctx_.log()->Error("Failed to initialize pipeline!");
            }
        }

        out_pipelines.emplace_back(&storage, new_index);
    }*/
}

void Eng::Renderer::BlitPixelsTonemap(const uint8_t *px_data, const int w, const int h, const int stride,
                                      const Ren::eFormat format, const float gamma, const float min_exposure,
                                      const float max_exposure, const Ren::ImageHandle target, const bool compressed,
                                      const bool blit_to_backbuffer) {
    const int cur_scr_w = ctx_.w(), cur_scr_h = ctx_.h();
    Ren::ILog *log = ctx_.log();

    if (!cur_scr_w || !cur_scr_h) {
        // view is minimized?
        return;
    }

    if (!prim_draw_.LazyInit(sh_)) {
        log->Error("[Renderer] Failed to initialize primitive drawing!");
    }

    min_exposure_ = std::pow(2.0f, min_exposure);
    max_exposure_ = std::pow(2.0f, max_exposure);

    bool resolution_changed = false;
    if (cur_scr_w != view_state_.out_res[0] || cur_scr_h != view_state_.out_res[1]) {
        resolution_changed = true;
        view_state_.out_res = view_state_.ren_res = Ren::Vec2i{cur_scr_w, cur_scr_h};
    }
    view_state_.pre_exposure = custom_pre_exposure_.value_or(readback_exposure());

    const bool rebuild_renderpasses = !cached_settings_.has_value() || (cached_settings_.value() != settings) ||
                                      !fg_builder_.ready() || cached_rp_index_ != 1 || resolution_changed;
    cached_settings_ = settings;
    cached_rp_index_ = 1;

    assert(format == Ren::eFormat::RGBA32F);

    const Ren::BufferHandle temp_upload_buf =
        ctx_.CreateBuffer(Ren::String{"Image upload buf"}, Ren::eBufType::Upload, 4 * w * h * sizeof(float));

    const auto &[temp_upload_buf_main, temp_upload_buf_cold] = ctx_.storages().buffers[temp_upload_buf];
    uint8_t *stage_data = Buffer_Map(ctx_.api(), temp_upload_buf_main, temp_upload_buf_cold);
    for (int y = 0; y < h; ++y) {
#if defined(REN_GL_BACKEND)
        memcpy(&stage_data[(h - y - 1) * 4 * w * sizeof(float)], &px_data[y * 4 * stride * sizeof(float)],
               4 * w * sizeof(float));
#else
        memcpy(&stage_data[y * 4 * w * sizeof(float)], &px_data[y * 4 * stride * sizeof(float)], 4 * w * sizeof(float));
#endif
    }
    Buffer_Unmap(ctx_.api(), temp_upload_buf_main, temp_upload_buf_cold);

    SCOPE_EXIT({ ctx_.ReleaseBuffer(temp_upload_buf); })

    if (rebuild_renderpasses) {
        const uint64_t rp_setup_beg_us = Sys::GetTimeUs();

        fg_builder_.Reset();
        backbuffer_sources_.clear();

        const FgBufROHandle temp_upload_res = fg_builder_.ImportResource(temp_upload_buf);

        FgImgRWHandle output;
        { // Upload image data
            auto &update_image = fg_builder_.AddNode("UPDATE IMAGE");

            const FgBufROHandle stage_buf_res = update_image.AddTransferInput(temp_upload_res);

            { // output image
                FgImgDesc desc;
                desc.w = cur_scr_w;
                desc.h = cur_scr_h;
                desc.format = format;
                desc.sampling.filter = Ren::eFilter::Bilinear;
                desc.sampling.wrap = Ren::eWrap::ClampToEdge;
                output = update_image.AddTransferImageOutput("Temp Image", desc);
            }

            update_image.set_execute_cb([stage_buf_res, output](const FgContext &fg) {
                const Ren::BufferROHandle stage_buf = fg.AccessROBuffer(stage_buf_res);
                const Ren::ImageRWHandle output_image = fg.AccessRWImage(output);

                const auto &[img_main, img_cold] = fg.storages().images[output_image];
                const int w = img_cold.params.w;
                const int h = img_cold.params.h;

                const auto &[stage_buf_main, stage_buf_cold] = fg.storages().buffers[stage_buf];
                Image_SetSubImage(fg.ren_ctx().api(), img_main, img_cold, 0, 0, Ren::Vec3i{0}, Ren::Vec3i{w, h, 1},
                                  Ren::eFormat::RGBA32F, stage_buf_main, fg.cmd_buf(), 0, stage_buf_cold.size);
            });
        }

        FgImgROHandle exposure = AddAutoexposurePasses(output, Ren::Vec2f{1.0f, 1.0f});

        if (settings.enable_sharpen) {
            output = AddSharpenPass(output, exposure, compressed);
        }

        FgImgROHandle bloom;
        if (settings.enable_bloom) {
            bloom = AddBloomPasses(output, exposure, compressed);
        }

        const FgImgROHandle dummy_black = fg_builder_.ImportResource(dummy_black_);

        auto &postprocess = fg_builder_.AddNode("POSTPROCESS");

        ex_postprocess_args_ = {};
        ex_postprocess_args_.exposure = postprocess.AddTextureInput(exposure, Ren::eStage::FragmentShader);
        ex_postprocess_args_.color = postprocess.AddTextureInput(output, Ren::eStage::FragmentShader);
        if (bloom) {
            ex_postprocess_args_.bloom = postprocess.AddTextureInput(bloom, Ren::eStage::FragmentShader);
        } else {
            ex_postprocess_args_.bloom = postprocess.AddTextureInput(dummy_black, Ren::eStage::FragmentShader);
        }
        if (tonemap_lut_) {
            const FgImgROHandle tonemap_lut = fg_builder_.ImportResource(tonemap_lut_);
            ex_postprocess_args_.lut = postprocess.AddTextureInput(tonemap_lut, Ren::eStage::FragmentShader);
        }
        if (target) {
            const FgImgRWHandle target_ref = fg_builder_.ImportResource(target);
            ex_postprocess_args_.output = postprocess.AddColorOutput(target_ref);
            if (blit_to_backbuffer) {
                const FgImgRWHandle backbuffer_ref = fg_builder_.ImportResource(ctx_.backbuffer_img());
                ex_postprocess_args_.output2 = postprocess.AddColorOutput(backbuffer_ref);
            }
        } else {
            const FgImgRWHandle backbuffer_ref = fg_builder_.ImportResource(ctx_.backbuffer_img());
            ex_postprocess_args_.output = postprocess.AddColorOutput(backbuffer_ref);
        }
        ex_postprocess_args_.linear_sampler = linear_sampler_;
        ex_postprocess_args_.tonemap_mode = int(settings.tonemap_mode);
        ex_postprocess_args_.inv_gamma = 1.0f / gamma;
        ex_postprocess_args_.fade = 0.0f;
        ex_postprocess_args_.aberration = settings.enable_aberration ? 1.0f : 0.0f;

        backbuffer_sources_.push_back(ex_postprocess_args_.output);

        postprocess.make_executor<ExPostprocess>(prim_draw_, sh_, &view_state_, &ex_postprocess_args_);

        { // Readback exposure
            auto &read_exposure = fg_builder_.AddNode("READ EXPOSURE");

            auto *data = fg_builder_.AllocTempData<ExReadExposure::Args>();
            data->input = read_exposure.AddTransferImageInput(exposure);

            FgBufDesc desc;
            desc.type = Ren::eBufType::Readback;
            desc.size = Ren::MaxFramesInFlight * sizeof(float);
            data->output = read_exposure.AddTransferOutput("Exposure Readback", desc);

            backbuffer_sources_.push_back(data->output);

            ex_read_exposure_.Setup(data);
            read_exposure.set_executor(&ex_read_exposure_);
        }

        fg_builder_.Compile();

        const uint64_t rp_setup_end_us = Sys::GetTimeUs();
        ctx_.log()->Info("Framegraph setup is done in %.2fms", (rp_setup_end_us - rp_setup_beg_us) * 0.001);
    } else {
        // Replace previous temp buffer
        const FgBufROHandle temp_upload_res = fg_builder_.ImportResource(temp_upload_buf);

        auto *update_image = fg_builder_.FindNode("UPDATE IMAGE");
        update_image->ReplaceTransferInput(0, temp_upload_res);

        auto *postprocess = fg_builder_.FindNode("POSTPROCESS");
        if (target) {
            const FgImgRWHandle target_ref = fg_builder_.ImportResource(target);
            ex_postprocess_args_.output = postprocess->ReplaceColorOutput(0, target_ref);
            if (blit_to_backbuffer) {
                const FgImgRWHandle backbuffer_ref = fg_builder_.ImportResource(ctx_.backbuffer_img());
                ex_postprocess_args_.output2 = postprocess->ReplaceColorOutput(1, backbuffer_ref);
            }
        } else {
            const FgImgRWHandle backbuffer_ref = fg_builder_.ImportResource(ctx_.backbuffer_img());
            ex_postprocess_args_.output = postprocess->ReplaceColorOutput(0, backbuffer_ref);
        }
    }

    fg_builder_.Execute();
}

void Eng::Renderer::BlitImageTonemap(const Ren::ImageHandle result, const int w, const int h, Ren::eFormat format,
                                     const float gamma, const float min_exposure, const float max_exposure,
                                     const Ren::ImageHandle target, const bool compressed,
                                     const bool blit_to_backbuffer) {
    const int cur_scr_w = ctx_.w(), cur_scr_h = ctx_.h();
    Ren::ILog *log = ctx_.log();

    if (!cur_scr_w || !cur_scr_h) {
        // view is minimized?
        return;
    }

    if (!prim_draw_.LazyInit(sh_)) {
        log->Error("[Renderer] Failed to initialize primitive drawing!");
    }

    min_exposure_ = std::pow(2.0f, min_exposure);
    max_exposure_ = std::pow(2.0f, max_exposure);

    bool resolution_changed = false;
    if (cur_scr_w != view_state_.out_res[0] || cur_scr_h != view_state_.out_res[1]) {
        resolution_changed = true;
        view_state_.out_res = view_state_.ren_res = Ren::Vec2i{cur_scr_w, cur_scr_h};
    }
    view_state_.pre_exposure = custom_pre_exposure_.value_or(readback_exposure());

    const bool rebuild_renderpasses = !cached_settings_.has_value() || (cached_settings_.value() != settings) ||
                                      !fg_builder_.ready() || cached_rp_index_ != 1 || resolution_changed;
    cached_settings_ = settings;
    cached_rp_index_ = 1;

    assert(format == Ren::eFormat::RGBA32F);

    if (rebuild_renderpasses) {
        const uint64_t rp_setup_beg_us = Sys::GetTimeUs();

        fg_builder_.Reset();
        backbuffer_sources_.clear();

        FgImgRWHandle output = fg_builder_.ImportResource(result);
        FgImgROHandle exposure = AddAutoexposurePasses(output, Ren::Vec2f{1.0f, 1.0f});

        if (settings.enable_sharpen) {
            output = AddSharpenPass(output, exposure, compressed);
        }

        FgImgROHandle bloom;
        if (settings.enable_bloom) {
            bloom = AddBloomPasses(output, exposure, compressed);
        }

        const FgImgROHandle dummy_black = fg_builder_.ImportResource(dummy_black_);

        auto &postprocess = fg_builder_.AddNode("POSTPROCESS");

        ex_postprocess_args_ = {};
        ex_postprocess_args_.exposure = postprocess.AddTextureInput(exposure, Ren::eStage::FragmentShader);
        ex_postprocess_args_.color = postprocess.AddTextureInput(output, Ren::eStage::FragmentShader);
        if (bloom) {
            ex_postprocess_args_.bloom = postprocess.AddTextureInput(bloom, Ren::eStage::FragmentShader);
        } else {
            ex_postprocess_args_.bloom = postprocess.AddTextureInput(dummy_black, Ren::eStage::FragmentShader);
        }
        if (tonemap_lut_) {
            const FgImgROHandle tonemap_lut = fg_builder_.ImportResource(tonemap_lut_);
            ex_postprocess_args_.lut = postprocess.AddTextureInput(tonemap_lut, Ren::eStage::FragmentShader);
        }
        if (target) {
            const FgImgRWHandle target_ref = fg_builder_.ImportResource(target);
            ex_postprocess_args_.output = postprocess.AddColorOutput(target_ref);
            if (blit_to_backbuffer) {
                const FgImgRWHandle backbuffer_ref = fg_builder_.ImportResource(ctx_.backbuffer_img());
                ex_postprocess_args_.output2 = postprocess.AddColorOutput(backbuffer_ref);
            }
        } else {
            const FgImgRWHandle backbuffer_ref = fg_builder_.ImportResource(ctx_.backbuffer_img());
            ex_postprocess_args_.output = postprocess.AddColorOutput(backbuffer_ref);
        }
        ex_postprocess_args_.linear_sampler = linear_sampler_;
        ex_postprocess_args_.tonemap_mode = int(settings.tonemap_mode);
        ex_postprocess_args_.inv_gamma = 1.0f / gamma;
        ex_postprocess_args_.fade = 0.0f;
        ex_postprocess_args_.aberration = settings.enable_aberration ? 1.0f : 0.0f;

        backbuffer_sources_.push_back(ex_postprocess_args_.output);

        postprocess.make_executor<ExPostprocess>(prim_draw_, sh_, &view_state_, &ex_postprocess_args_);

        { // Readback exposure
            auto &read_exposure = fg_builder_.AddNode("READ EXPOSURE");

            auto *data = fg_builder_.AllocTempData<ExReadExposure::Args>();
            data->input = read_exposure.AddTransferImageInput(exposure);

            FgBufDesc desc;
            desc.type = Ren::eBufType::Readback;
            desc.size = Ren::MaxFramesInFlight * sizeof(float);
            data->output = read_exposure.AddTransferOutput("Exposure Readback", desc);

            backbuffer_sources_.push_back(data->output);

            ex_read_exposure_.Setup(data);
            read_exposure.set_executor(&ex_read_exposure_);
        }

        fg_builder_.Compile();

        const uint64_t rp_setup_end_us = Sys::GetTimeUs();
        ctx_.log()->Info("Framegraph setup is done in %.2fms", (rp_setup_end_us - rp_setup_beg_us) * 0.001);
    } else {
        auto *postprocess = fg_builder_.FindNode("POSTPROCESS");
        if (target) {
            const FgImgRWHandle target_ref = fg_builder_.ImportResource(target);
            ex_postprocess_args_.output = postprocess->ReplaceColorOutput(0, target_ref);
            if (blit_to_backbuffer) {
                const FgImgRWHandle backbuffer_ref = fg_builder_.ImportResource(ctx_.backbuffer_img());
                ex_postprocess_args_.output2 = postprocess->ReplaceColorOutput(1, backbuffer_ref);
            }
        } else {
            const FgImgRWHandle backbuffer_ref = fg_builder_.ImportResource(ctx_.backbuffer_img());
            ex_postprocess_args_.output = postprocess->ReplaceColorOutput(0, backbuffer_ref);
        }
    }

    fg_builder_.Execute();
}

#undef BBOX_POINTS
