#include "Renderer.h"

#include <cstdio>

#include <Ren/Context.h>
#include <Ren/Utils.h>
#include <Sys/MonoAlloc.h>
#include <Sys/ThreadPool.h>
#include <Sys/Time_.h>

#include "../utils/ShaderLoader.h"
#include "CDFUtils.h"
#include "Renderer_Names.h"
#include "executors/ExDebugOIT.h"
#include "executors/ExDebugProbes.h"
#include "executors/ExDebugRT.h"
#include "executors/ExDepthFill.h"
#include "executors/ExDepthHierarchy.h"
#include "executors/ExShadowColor.h"
#include "executors/ExShadowDepth.h"

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

#include "precomputed/__blue_noise.inl"
#include "precomputed/__brdf_lut.inl"
#include "precomputed/__cone_rt_lut.inl"
#include "precomputed/__ltc_clearcoat.inl"
#include "precomputed/__ltc_diffuse.inl"
#include "precomputed/__ltc_sheen.inl"
#include "precomputed/__ltc_specular.inl"
#include "precomputed/__noise.inl"
#include "precomputed/__pmj02_samples.inl"

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
      fg_builder_(ctx_, sh_, prim_draw_) {
    using namespace RendererInternal;

    // Culling is done in lower resolution
    swCullCtxInit(&cull_ctx_, 8 * std::max(ctx.w() / 32, 1), 4 * std::max(ctx.h() / 16, 1), 0.0f);

    /*{ // buffer used to sample probes
        FrameBuf::ColorAttachmentDesc desc;
        desc.format = Ren::eTexFormat::RGBA32F;
        desc.filter = Ren::eTexFilter::NoFilter;
        desc.wrap = Ren::eTexWrap::ClampToEdge;
        probe_sample_buf_ = FrameBuf("Probe sample", ctx_, 24, 8, &desc, 1, {}, 1, ctx.log());
    }*/

    static const uint8_t black[] = {0, 0, 0, 0}, white[] = {255, 255, 255, 255};

    { // dummy 1px textures
        Ren::TexParams p;
        p.w = p.h = 1;
        p.format = Ren::eTexFormat::RGBA8;
        p.usage = Ren::Bitmask(Ren::eTexUsage::Transfer) | Ren::eTexUsage::Sampled;
        p.sampling.wrap = Ren::eTexWrap::ClampToEdge;

        Ren::eTexLoadStatus status;
        dummy_black_ = ctx_.LoadTexture("dummy_black", black, p, ctx_.default_mem_allocs(), &status);
        assert(status == Ren::eTexLoadStatus::CreatedFromData);

        dummy_white_ = ctx_.LoadTexture("dummy_white", white, p, ctx_.default_mem_allocs(), &status);
        assert(status == Ren::eTexLoadStatus::CreatedFromData);
    }

    { // random 2d halton 8x8
        Ren::TexParams p;
        p.w = p.h = 8;
        p.format = Ren::eTexFormat::RG32F;
        p.usage = Ren::Bitmask(Ren::eTexUsage::Transfer) | Ren::eTexUsage::Sampled;

        Ren::eTexLoadStatus status;
        rand2d_8x8_ = ctx_.LoadTexture("rand2d_8x8", {(const uint8_t *)&PMJSamples64[0][0], sizeof(PMJSamples64)}, p,
                                       ctx_.default_mem_allocs(), &status);
        assert(status == Ren::eTexLoadStatus::CreatedFromData);
    }

    { // random 2d directions 4x4
        Ren::TexParams p;
        p.w = p.h = 4;
        p.format = Ren::eTexFormat::RG16;
        p.usage = Ren::Bitmask(Ren::eTexUsage::Transfer) | Ren::eTexUsage::Sampled;

        Ren::eTexLoadStatus status;
        rand2d_dirs_4x4_ = ctx_.LoadTexture("rand2d_dirs_4x4", {(const uint8_t *)&__rand_dirs[0], sizeof(__rand_dirs)},
                                            p, ctx_.default_mem_allocs(), &status);
        assert(status == Ren::eTexLoadStatus::CreatedFromData);
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

        Ren::TexParams p;
        p.w = __cone_rt_lut_res;
        p.h = __cone_rt_lut_res;
        p.format = Ren::eTexFormat::RGBA8;
        p.usage = Ren::Bitmask(Ren::eTexUsage::Transfer) | Ren::eTexUsage::Sampled;
        p.sampling.filter = Ren::eTexFilter::Bilinear;
        p.sampling.wrap = Ren::eTexWrap::ClampToEdge;

        Ren::eTexLoadStatus status;
        cone_rt_lut_ = ctx_.LoadTexture("cone_rt_lut", {&__cone_rt_lut[0], 4 * __cone_rt_lut_res * __cone_rt_lut_res},
                                        p, ctx_.default_mem_allocs(), &status);

        // cone_rt_lut_ =
        //    ctx_.LoadTexture("cone_rt_lut", &occ_data[0], 4 * resx * resy, p,
        //    &status);
    }

    {
        // std::string c_header;
        // const std::unique_ptr<uint16_t[]> img_data_rg16 = Generate_BRDF_LUT(256,
        // c_header);

        Ren::TexParams p;
        p.w = p.h = __brdf_lut_res;
        p.format = Ren::eTexFormat::RG16;
        p.usage = Ren::Bitmask(Ren::eTexUsage::Transfer) | Ren::eTexUsage::Sampled;
        p.sampling.filter = Ren::eTexFilter::Bilinear;
        p.sampling.wrap = Ren::eTexWrap::ClampToEdge;

        Ren::eTexLoadStatus status;
        brdf_lut_ = ctx_.LoadTexture("brdf_lut", {(const uint8_t *)&__brdf_lut[0], sizeof(__brdf_lut)}, p,
                                     ctx_.default_mem_allocs(), &status);
        assert(status == Ren::eTexLoadStatus::CreatedFromData);
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

        Ren::TexParams p;
        p.w = 8 * 64;
        p.h = 64;
        p.format = Ren::eTexFormat::RGBA32F;
        p.usage = Ren::Bitmask(Ren::eTexUsage::Transfer) | Ren::eTexUsage::Sampled;
        p.sampling.filter = Ren::eTexFilter::Bilinear;
        p.sampling.wrap = Ren::eTexWrap::ClampToEdge;

        Ren::eTexLoadStatus status;
        ltc_luts_ =
            ctx_.LoadTexture("LTC LUTs", {(const uint8_t *)combined_data.get(), 8 * 4 * 64 * 64 * sizeof(float)}, p,
                             ctx_.default_mem_allocs(), &status);
        assert(status == Ren::eTexLoadStatus::CreatedFromData);
    }

    {
        /*const int res = 128;
        std::string c_header;
        const std::unique_ptr<int8_t[]> img_data = Generate_PeriodicPerlin(res, c_header);
        SceneManagerInternal::WriteImage((const uint8_t*)&img_data[0], res, res, 4,
        false, "test1.png");*/

        Ren::TexParams p;
        p.w = p.h = __noise_res;
        p.format = Ren::eTexFormat::RGBA8_snorm;
        p.usage = Ren::Bitmask(Ren::eTexUsage::Transfer) | Ren::eTexUsage::Sampled;
        p.sampling.filter = Ren::eTexFilter::Bilinear;

        Ren::eTexLoadStatus status;
        noise_tex_ = ctx_.LoadTexture("noise", {(const uint8_t *)&__noise[0], __noise_res * __noise_res * 4}, p,
                                      ctx_.default_mem_allocs(), &status);
        assert(status == Ren::eTexLoadStatus::CreatedFromData);
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

    { // blue noise sampling
        Ren::CommandBuffer cmd_buf = ctx_.BegTempSingleTimeCommands();

        // Sobol sequence
        sobol_seq_buf_ = ctx_.LoadBuffer("SobolSequenceBuf", Ren::eBufType::Texture, 256 * 256 * sizeof(int));
        sobol_seq_buf_->AddBufferView(Ren::eTexFormat::R32UI);
        Ren::Buffer sobol_seq_buf_stage("SobolSequenceBufStage", ctx_.api_ctx(), Ren::eBufType::Upload,
                                        sobol_seq_buf_->size());

        { // init stage buf
            uint8_t *mapped_ptr = sobol_seq_buf_stage.Map();
            memcpy(mapped_ptr, g_sobol_256spp_256d, 256 * 256 * sizeof(int));
            sobol_seq_buf_stage.Unmap();
        }

        CopyBufferToBuffer(sobol_seq_buf_stage, 0, *sobol_seq_buf_, 0, 256 * 256 * sizeof(int), cmd_buf);

        // Scrambling tile
        scrambling_tile_buf_ =
            ctx_.LoadBuffer("ScramblingTile32SppBuf", Ren::eBufType::Texture, 128 * 128 * 8 * sizeof(int));
        scrambling_tile_buf_->AddBufferView(Ren::eTexFormat::R32UI);
        Ren::Buffer scrambling_tile_buf_stage("ScramblingTileBufStage", ctx_.api_ctx(), Ren::eBufType::Upload,
                                              scrambling_tile_buf_->size());

        { // init stage buf
            uint8_t *mapped_ptr = scrambling_tile_buf_stage.Map();
            memcpy(mapped_ptr, g_scrambling_tile_1spp, 128 * 128 * 8 * sizeof(int));
            scrambling_tile_buf_stage.Unmap();
        }

        CopyBufferToBuffer(scrambling_tile_buf_stage, 0, *scrambling_tile_buf_, 0, 128 * 128 * 8 * sizeof(int),
                           cmd_buf);

        // Ranking tile
        ranking_tile_buf_ = ctx_.LoadBuffer("RankingTile32SppBuf", Ren::eBufType::Texture, 128 * 128 * 8 * sizeof(int));
        ranking_tile_buf_->AddBufferView(Ren::eTexFormat::R32UI);
        Ren::Buffer ranking_tile_buf_stage("RankingTileBufStage", ctx_.api_ctx(), Ren::eBufType::Upload,
                                           ranking_tile_buf_->size());

        { // init stage buf
            uint8_t *mapped_ptr = ranking_tile_buf_stage.Map();
            memcpy(mapped_ptr, g_ranking_tile_1spp, 128 * 128 * 8 * sizeof(int));
            ranking_tile_buf_stage.Unmap();
        }

        CopyBufferToBuffer(ranking_tile_buf_stage, 0, *ranking_tile_buf_, 0, 128 * 128 * 8 * sizeof(int), cmd_buf);

        ctx_.EndTempSingleTimeCommands(cmd_buf);

        sobol_seq_buf_stage.FreeImmediate();
        scrambling_tile_buf_stage.FreeImmediate();
        ranking_tile_buf_stage.FreeImmediate();
    }

    { // PMJ samples
        pmj_samples_buf_ = ctx_.LoadBuffer("PMJSamples", Ren::eBufType::Texture,
                                           __pmj02_sample_count * __pmj02_dims_count * sizeof(uint32_t));
        pmj_samples_buf_->AddBufferView(Ren::eTexFormat::R32UI);

        Ren::Buffer pmj_samples_stage("PMJSamplesStage", ctx_.api_ctx(), Ren::eBufType::Upload,
                                      pmj_samples_buf_->size());
        { // init stage buf
            uint8_t *mapped_ptr = pmj_samples_stage.Map();
            memcpy(mapped_ptr, __pmj02_samples, __pmj02_sample_count * __pmj02_dims_count * sizeof(uint32_t));
            pmj_samples_stage.Unmap();
        }

        Ren::CommandBuffer cmd_buf = ctx_.BegTempSingleTimeCommands();
        CopyBufferToBuffer(pmj_samples_stage, 0, *pmj_samples_buf_, 0,
                           __pmj02_sample_count * __pmj02_dims_count * sizeof(uint32_t), cmd_buf);
        ctx_.EndTempSingleTimeCommands(cmd_buf);

        pmj_samples_stage.FreeImmediate();
    }

    const auto vtx_buf1 = ctx_.default_vertex_buf1();
    const auto vtx_buf2 = ctx_.default_vertex_buf2();
    const auto ndx_buf = ctx_.default_indices_buf();

    const int buf1_stride = 16;

    { // VertexInput for main drawing (uses all attributes)
        const Ren::VtxAttribDesc attribs[] = {
            // Attributes from buffer 1
            {vtx_buf1, VTX_POS_LOC, 3, Ren::eType::Float32, buf1_stride, 0},
            {vtx_buf1, VTX_UV1_LOC, 2, Ren::eType::Float16, buf1_stride, 3 * sizeof(float)},
            // Attributes from buffer 2
            {vtx_buf2, VTX_NOR_LOC, 4, Ren::eType::Int16_snorm, buf1_stride, 0},
            {vtx_buf2, VTX_TAN_LOC, 2, Ren::eType::Int16_snorm, buf1_stride, 4 * sizeof(uint16_t)},
            {vtx_buf2, VTX_AUX_LOC, 1, Ren::eType::Uint32, buf1_stride, 6 * sizeof(uint16_t)}};
        draw_pass_vi_ = ctx_.LoadVertexInput(attribs, ndx_buf);
    }

    { // RenderPass for main drawing (compatible one)
        Ren::RenderTargetInfo color_rts[] = {
            {Ren::eTexFormat::RGBA16F, 1 /* samples */, Ren::eImageLayout::ColorAttachmentOptimal, Ren::eLoadOp::Load,
             Ren::eStoreOp::Store},
#if USE_OCT_PACKED_NORMALS == 1
            {Ren::eTexFormat::RGB10_A2, 1 /* samples */, Ren::eImageLayout::ColorAttachmentOptimal, Ren::eLoadOp::Load,
             Ren::eStoreOp::Store},
#else
            {Ren::eTexFormat::RGBA8, 1 /* samples */, Ren::eImageLayout::ColorAttachmentOptimal, Ren::eLoadOp::Load,
             Ren::eStoreOp::Store},
#endif
            {Ren::eTexFormat::RGBA8_srgb, 1 /* samples */, Ren::eImageLayout::ColorAttachmentOptimal,
             Ren::eLoadOp::Load, Ren::eStoreOp::Store}};

        // color_rts[2].flags = Ren::eTexFlags::SRGB;

        // const auto depth_format =
        //     ctx_.capabilities.depth24_stencil8_format ? Ren::eTexFormat::D24_S8 : Ren::eTexFormat::D32_S8;
        const auto depth_format = Ren::eTexFormat::D32_S8;

        const Ren::RenderTargetInfo depth_rt = {depth_format, 1 /* samples */,
                                                Ren::eImageLayout::DepthStencilAttachmentOptimal, Ren::eLoadOp::Load,
                                                Ren::eStoreOp::Store};

        rp_main_draw_ = sh_.LoadRenderPass(depth_rt, color_rts);
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
        Ren::TexParams params;
        params.w = SHADOWMAP_WIDTH;
        params.h = SHADOWMAP_HEIGHT;
        params.format = Ren::eTexFormat::D16;
        params.usage = Ren::Bitmask(Ren::eTexUsage::RenderTarget) | Ren::eTexUsage::Sampled;
        params.sampling.filter = Ren::eTexFilter::Bilinear;
        params.sampling.wrap = Ren::eTexWrap::ClampToEdge;
        params.sampling.compare = Ren::eTexCompare::GEqual;

        Ren::eTexLoadStatus status;
        shadow_depth_tex_ = ctx_.LoadTexture("Shadow Depth Tex", params, ctx_.default_mem_allocs(), &status);
    }
    { // shadow filter texture
        Ren::TexParams params;
        params.w = SHADOWMAP_WIDTH;
        params.h = SHADOWMAP_HEIGHT;
        params.format = Ren::eTexFormat::RGBA8;
        params.usage = Ren::Bitmask(Ren::eTexUsage::RenderTarget) | Ren::eTexUsage::Sampled;
        params.sampling.filter = Ren::eTexFilter::Bilinear;
        params.sampling.wrap = Ren::eTexWrap::ClampToEdge;

        Ren::eTexLoadStatus status;
        shadow_color_tex_ = ctx_.LoadTexture("Shadow Color Tex", params, ctx_.default_mem_allocs(), &status);
    }

    { // nearest sampler
        Ren::SamplingParams sampler_params;
        sampler_params.wrap = Ren::eTexWrap::ClampToEdge;

        Ren::eSamplerLoadStatus status;
        nearest_sampler_ = ctx_.LoadSampler(sampler_params, &status);

        sampler_params.filter = Ren::eTexFilter::Bilinear;
        linear_sampler_ = ctx_.LoadSampler(sampler_params, &status);
    }

    {
        Ren::BufRef vtx_buf1 = ctx_.default_vertex_buf1(), vtx_buf2 = ctx_.default_vertex_buf2(),
                    ndx_buf = ctx_.default_indices_buf();

        // Allocate buffer for skinned vertices
        // TODO: fix this. do not allocate twice more memory in buf2
        skinned_buf1_vtx_ = vtx_buf1->AllocSubRegion(MAX_SKIN_VERTICES_TOTAL * 16 * 2, 16, "skinned");
        skinned_buf2_vtx_ = vtx_buf2->AllocSubRegion(MAX_SKIN_VERTICES_TOTAL * 16 * 2, 16, "skinned");
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
    prim_draw_.CleanUp();
    swCullCtxDestroy(&cull_ctx_);
}

void Eng::Renderer::PrepareDrawList(const SceneData &scene, const Ren::Camera &cam, const Ren::Camera &ext_cam,
                                    DrawList &list) {
    GatherDrawables(scene, cam, ext_cam, list);
}

void Eng::Renderer::ExecuteDrawList(const DrawList &list, const PersistentGpuData &persistent_data,
                                    const Ren::TexRef target, const bool blit_to_backbuffer) {
    using namespace RendererInternal;

    __itt_task_begin(__g_itt_domain, __itt_null, __itt_null, itt_exec_dr_str);

    const int cur_scr_w = ctx_.w(), cur_scr_h = ctx_.h();
    Ren::ILog *log = ctx_.log();

    if (!cur_scr_w || !cur_scr_h) {
        // view is minimized?
        return;
    }

    if (!prim_draw_.LazyInit(ctx_)) {
        log->Error("[Renderer] Failed to initialize primitive drawing!");
    }

    if (frame_index_ == 0 && list.render_settings.taa_mode != eTAAMode::Static) {
        Ren::CommandBuffer cmd_buf = ctx_.current_cmd_buf();

        const Ren::TransitionInfo transitions[] = {{persistent_data.probe_irradiance.get(), Ren::eResState::CopyDst},
                                                   {persistent_data.probe_distance.get(), Ren::eResState::CopyDst},
                                                   {persistent_data.probe_offset.get(), Ren::eResState::CopyDst}};
        TransitionResourceStates(ctx_.api_ctx(), cmd_buf, Ren::AllStages, Ren::AllStages, transitions);

        static const float rgba[4] = {0.0f, 0.0f, 0.0f, 0.0f};
        Ren::ClearImage(*persistent_data.probe_irradiance, rgba, cmd_buf);
        Ren::ClearImage(*persistent_data.probe_distance, rgba, cmd_buf);
        Ren::ClearImage(*persistent_data.probe_offset, rgba, cmd_buf);

        for (int i = 0; i < PROBE_VOLUMES_COUNT; ++i) {
            persistent_data.probe_volumes[i].updates_count = 0;
            persistent_data.probe_volumes[i].reset_relocation = true;
            persistent_data.probe_volumes[i].reset_classification = true;
        }
    }

    for (int i = 0; i < PROBE_VOLUMES_COUNT; ++i) {
        const probe_volume_t &volume = persistent_data.probe_volumes[i];
        if (i == list.volume_to_update) {
            Ren::Vec3i new_scroll = Ren::Vec3i{(list.draw_cam.world_position() - volume.origin) / volume.spacing};
            // Ren::Vec3i new_scroll = Ren::Vec3i{list.draw_cam.world_position() / volume.spacing};

            /*volume.origin[0] = float(new_scroll[0] / PROBE_VOLUME_RES_X) * volume.spacing[0] * PROBE_VOLUME_RES_X;
            volume.origin[1] = float(new_scroll[1] / PROBE_VOLUME_RES_Y) * volume.spacing[1] * PROBE_VOLUME_RES_Y;
            volume.origin[2] = float(new_scroll[2] / PROBE_VOLUME_RES_Z) * volume.spacing[2] * PROBE_VOLUME_RES_Z;

            new_scroll[0] = new_scroll[0] % PROBE_VOLUME_RES_X;
            new_scroll[1] = new_scroll[1] % PROBE_VOLUME_RES_Y;
            new_scroll[2] = new_scroll[2] % PROBE_VOLUME_RES_Z;*/

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
        last_volume.spacing =
            Ren::Vec3f{std::max(std::max(last_volume.spacing[0], last_volume.spacing[1]), last_volume.spacing[2])};
        const Ren::Vec3i new_scroll =
            Ren::Vec3i{(0.5f * (list.bbox_max + list.bbox_min) - last_volume.origin) / last_volume.spacing};
        last_volume.scroll_diff = new_scroll - last_volume.scroll;
        last_volume.scroll = new_scroll;
    }

    float probe_volume_spacing = 0.5f;
    for (int i = 0; i < PROBE_VOLUMES_COUNT - 1; ++i) {
        const probe_volume_t &volume = persistent_data.probe_volumes[i],
                             &last_volume = persistent_data.probe_volumes[PROBE_VOLUMES_COUNT - 1];
        if (probe_volume_spacing > last_volume.spacing[0]) {
            volume.spacing = last_volume.spacing[0];
            volume.scroll = Ren::Vec3i{(0.5f * (list.bbox_max + list.bbox_min) - volume.origin) / volume.spacing};
            volume.scroll_diff = Ren::Vec3i(0);
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

    if (cur_scr_w != view_state_.scr_res[0] || cur_scr_h != view_state_.scr_res[1] ||
        list.render_settings.taa_mode != taa_mode_ || cur_dof_enabled != dof_enabled_ || cached_rp_index_ != 0) {
        rendertarget_changed = true;

        view_state_.scr_res = Ren::Vec2i{cur_scr_w, cur_scr_h};
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
        // TODO: Change actual resolution dynamically
#if defined(__ANDROID__)
        view_state_.act_res[0] = int(float(view_state_.scr_res[0]) * 0.4f);
        view_state_.act_res[1] = int(float(view_state_.scr_res[1]) * 0.6f);
#else
        view_state_.act_res[0] = int(float(view_state_.scr_res[0]) * 1.0f);
        view_state_.act_res[1] = int(float(view_state_.scr_res[1]) * 1.0f);
#endif
    } else {
        view_state_.act_res[0] = target->params.w;
        view_state_.act_res[1] = target->params.h;
    }
    assert(view_state_.act_res[0] <= view_state_.scr_res[0] && view_state_.act_res[1] <= view_state_.scr_res[1]);

    view_state_.vertical_fov = list.draw_cam.angle();
    view_state_.pixel_spread_angle = std::atan(
        2.0f * std::tan(0.5f * view_state_.vertical_fov * Ren::Pi<float>() / 180.0f) / float(view_state_.scr_res[1]));
    view_state_.frame_index = list.frame_index;
    view_state_.volume_to_update = list.volume_to_update;
    view_state_.stochastic_lights_count = view_state_.stochastic_lights_count_cache = 0;
    if (persistent_data.stoch_lights_buf) {
        view_state_.stochastic_lights_count_cache = persistent_data.stoch_lights_buf->size() / sizeof(light_item_t);
    }
    if (persistent_data.stoch_lights_buf && list.render_settings.gi_quality > eGIQuality::Medium) {
        view_state_.stochastic_lights_count = persistent_data.stoch_lights_buf->size() / sizeof(light_item_t);
    }

    view_state_.env_generation = list.env.generation;
    view_state_.pre_exposure = readback_exposure();
    view_state_.prev_pre_exposure = std::min(std::max(view_state_.prev_pre_exposure, min_exposure_), max_exposure_);

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

        jitter = (2.0f * jitter - Ren::Vec2f{1.0f}) / Ren::Vec2f{view_state_.act_res};
        list.draw_cam.SetPxOffset(jitter);
    } else {
        list.draw_cam.SetPxOffset(Ren::Vec2f{0.0f, 0.0f});
    }

    BindlessTextureData bindless_tex;
#if defined(REN_VK_BACKEND)
    bindless_tex.textures_descr_sets = persistent_data.textures_descr_sets[ctx_.backend_frame()];
    bindless_tex.rt_textures_descr_set = persistent_data.rt_textures_descr_sets[ctx_.backend_frame()];
    bindless_tex.rt_inline_textures_descr_set = persistent_data.rt_inline_textures_descr_sets[ctx_.backend_frame()];
#elif defined(REN_GL_BACKEND)
    bindless_tex.textures_buf = persistent_data.textures_buf;
#endif

    const bool deferred_shading =
        list.render_settings.render_mode == eRenderMode::Deferred && !list.render_settings.debug_wireframe;
    const bool env_map_changed = false;
    //(env_map_ != list.env.env_map);
    const bool lm_tex_changed =
        lm_direct_ != list.env.lm_direct || lm_indir_ != list.env.lm_indir ||
        !std::equal(std::begin(list.env.lm_indir_sh), std::end(list.env.lm_indir_sh), std::begin(lm_indir_sh_));
    const bool rebuild_renderpasses = !cached_settings_.has_value() ||
                                      (cached_settings_.value() != list.render_settings) || !fg_builder_.ready() ||
                                      rendertarget_changed || env_map_changed || lm_tex_changed;

    cached_settings_ = list.render_settings;
    cached_rp_index_ = 0;
    env_map_ = list.env.env_map;
    lm_direct_ = list.env.lm_direct;
    lm_indir_ = list.env.lm_indir;
    std::copy(std::begin(list.env.lm_indir_sh), std::end(list.env.lm_indir_sh), std::begin(lm_indir_sh_));
    p_list_ = &list;
    min_exposure_ = std::pow(2.0f, list.draw_cam.min_exposure);
    max_exposure_ = std::pow(2.0f, list.draw_cam.max_exposure);

    if (rebuild_renderpasses) {
        const uint64_t rp_setup_beg_us = Sys::GetTimeUs();

        fg_builder_.Reset();
        backbuffer_sources_.clear();

        accumulated_frames_ = 0;

        InitSkyResources();

        auto &common_buffers = *fg_builder_.AllocNodeData<CommonBuffers>();
        AddBuffersUpdatePass(common_buffers, persistent_data);
        AddLightBuffersUpdatePass(common_buffers);
        AddSunColorUpdatePass(common_buffers);

        { // Skinning
            auto &skinning = fg_builder_.AddNode("SKINNING");

            FgResRef skin_vtx_res =
                skinning.AddStorageReadonlyInput(persistent_data.skin_vertex_buf, Ren::eStage::ComputeShader);
            FgResRef in_skin_transforms_res =
                skinning.AddStorageReadonlyInput(common_buffers.skin_transforms, Ren::eStage::ComputeShader);
            FgResRef in_shape_keys_res =
                skinning.AddStorageReadonlyInput(common_buffers.shape_keys, Ren::eStage::ComputeShader);
            FgResRef delta_buf_res =
                skinning.AddStorageReadonlyInput(persistent_data.delta_buf, Ren::eStage::ComputeShader);

            FgResRef vtx_buf1_res = skinning.AddStorageOutput(persistent_data.vertex_buf1, Ren::eStage::ComputeShader);
            FgResRef vtx_buf2_res = skinning.AddStorageOutput(persistent_data.vertex_buf2, Ren::eStage::ComputeShader);

            skinning.make_executor<ExSkinning>(p_list_, skin_vtx_res, in_skin_transforms_res, in_shape_keys_res,
                                               delta_buf_res, vtx_buf1_res, vtx_buf2_res);
        }

        //
        // RT acceleration structures
        //
        auto &acc_struct_data = *fg_builder_.AllocNodeData<AccelerationStructureData>();
        acc_struct_data.rt_tlas_buf = persistent_data.rt_tlas_buf;
        acc_struct_data.rt_sh_tlas_buf = persistent_data.rt_sh_tlas_buf;
        acc_struct_data.hwrt.rt_tlas_build_scratch_size = persistent_data.rt_tlas_build_scratch_size;
        if (persistent_data.rt_tlas) {
            acc_struct_data.rt_tlases[int(eTLASIndex::Main)] = persistent_data.rt_tlas.get();
        }
        if (persistent_data.rt_sh_tlas) {
            acc_struct_data.rt_tlases[int(eTLASIndex::Shadow)] = persistent_data.rt_sh_tlas.get();
        }

        FgResRef rt_geo_instances_res, rt_obj_instances_res, rt_sh_geo_instances_res, rt_sh_obj_instances_res;

        if (ctx_.capabilities.hwrt) {
            auto &update_rt_bufs = fg_builder_.AddNode("UPDATE ACC BUFS");

            { // create geo instances buffer
                FgBufDesc desc;
                desc.type = Ren::eBufType::Storage;
                desc.size = RTGeoInstancesBufChunkSize;

                rt_geo_instances_res = update_rt_bufs.AddTransferOutput("RT Geo Instances", desc);
            }

            { // create obj instances buffer
                FgBufDesc desc;
                desc.type = Ren::eBufType::Storage;
                desc.size = HWRTObjInstancesBufChunkSize;
                desc.views.push_back(Ren::eTexFormat::RGBA32F);

                rt_obj_instances_res = update_rt_bufs.AddTransferOutput("RT Obj Instances", desc);
            }

            update_rt_bufs.make_executor<ExUpdateAccBuffers>(p_list_, 0, rt_geo_instances_res, rt_obj_instances_res);

            auto &build_acc_structs = fg_builder_.AddNode("BUILD ACC STRCTS");

            rt_obj_instances_res = build_acc_structs.AddASBuildReadonlyInput(rt_obj_instances_res);
            FgResRef rt_tlas_res = build_acc_structs.AddASBuildOutput(acc_struct_data.rt_tlas_buf);

            FgResRef rt_tlas_build_scratch_res;

            { // create scratch buffer
                FgBufDesc desc;
                desc.type = Ren::eBufType::Storage;
                desc.size = acc_struct_data.hwrt.rt_tlas_build_scratch_size;
                rt_tlas_build_scratch_res = build_acc_structs.AddASBuildOutput("TLAS Scratch Buf", desc);
            }

            build_acc_structs.make_executor<ExBuildAccStructures>(
                p_list_, 0, rt_obj_instances_res, &acc_struct_data,
                Ren::Span<const mesh_t>{persistent_data.swrt.rt_meshes.data(),
                                        persistent_data.swrt.rt_meshes.capacity()},
                rt_tlas_res, rt_tlas_build_scratch_res);
        } else if (ctx_.capabilities.swrt && acc_struct_data.rt_tlas_buf) {
            auto &update_rt_bufs = fg_builder_.AddNode("UPDATE ACC BUFS");

            { // create geo instances buffer
                FgBufDesc desc;
                desc.type = Ren::eBufType::Storage;
                desc.size = RTGeoInstancesBufChunkSize;

                rt_geo_instances_res = update_rt_bufs.AddTransferOutput("RT Geo Instances", desc);
            }

            update_rt_bufs.make_executor<ExUpdateAccBuffers>(p_list_, 0, rt_geo_instances_res, FgResRef{});

            auto &build_acc_structs = fg_builder_.AddNode("BUILD ACC STRCTS");

            { // create obj instances buffer
                FgBufDesc desc;
                desc.type = Ren::eBufType::Storage;
                desc.size = SWRTObjInstancesBufChunkSize;
                desc.views.push_back(Ren::eTexFormat::RGBA32F);

                rt_obj_instances_res = build_acc_structs.AddTransferOutput("RT Obj Instances", desc);
            }

            FgResRef rt_tlas_res = build_acc_structs.AddTransferOutput(acc_struct_data.rt_tlas_buf);

            build_acc_structs.make_executor<ExBuildAccStructures>(
                p_list_, 0, rt_obj_instances_res, &acc_struct_data,
                Ren::Span<const mesh_t>{persistent_data.swrt.rt_meshes.data(),
                                        persistent_data.swrt.rt_meshes.capacity()},
                rt_tlas_res, FgResRef{});
        }

        if (deferred_shading && list.render_settings.shadows_quality == eShadowsQuality::Raytraced) {
            if (ctx_.capabilities.hwrt) {
                auto &update_rt_bufs = fg_builder_.AddNode("UPDATE SH ACC BUFS");

                { // create geo instances buffer
                    FgBufDesc desc;
                    desc.type = Ren::eBufType::Storage;
                    desc.size = RTGeoInstancesBufChunkSize;

                    rt_sh_geo_instances_res = update_rt_bufs.AddTransferOutput("RT SH Geo Instances", desc);
                }

                { // create obj instances buffer
                    FgBufDesc desc;
                    desc.type = Ren::eBufType::Storage;
                    desc.size = HWRTObjInstancesBufChunkSize;
                    desc.views.push_back(Ren::eTexFormat::RGBA32F);

                    rt_sh_obj_instances_res = update_rt_bufs.AddTransferOutput("RT SH Obj Instances", desc);
                }

                update_rt_bufs.make_executor<ExUpdateAccBuffers>(p_list_, 1, rt_sh_geo_instances_res,
                                                                 rt_sh_obj_instances_res);

                ////

                auto &build_acc_structs = fg_builder_.AddNode("SH ACC STRCTS");
                rt_sh_obj_instances_res = build_acc_structs.AddASBuildReadonlyInput(rt_sh_obj_instances_res);
                FgResRef rt_sh_tlas_res = build_acc_structs.AddASBuildOutput(acc_struct_data.rt_sh_tlas_buf);

                FgResRef rt_sh_tlas_build_scratch_res;

                { // create scratch buffer
                    FgBufDesc desc;
                    desc.type = Ren::eBufType::Storage;
                    desc.size = acc_struct_data.hwrt.rt_tlas_build_scratch_size;
                    rt_sh_tlas_build_scratch_res = build_acc_structs.AddASBuildOutput("SH TLAS Scratch Buf", desc);
                }

                build_acc_structs.make_executor<ExBuildAccStructures>(
                    p_list_, 1, rt_sh_obj_instances_res, &acc_struct_data,
                    Ren::Span<const mesh_t>{persistent_data.swrt.rt_meshes.data(),
                                            persistent_data.swrt.rt_meshes.capacity()},
                    rt_sh_tlas_res, rt_sh_tlas_build_scratch_res);
            } else if (ctx_.capabilities.swrt && acc_struct_data.rt_sh_tlas_buf) {
                auto &update_rt_bufs = fg_builder_.AddNode("UPDATE ACC BUFS");

                { // create geo instances buffer
                    FgBufDesc desc;
                    desc.type = Ren::eBufType::Storage;
                    desc.size = RTGeoInstancesBufChunkSize;

                    rt_sh_geo_instances_res = update_rt_bufs.AddTransferOutput("RT SH Geo Instances", desc);
                }

                update_rt_bufs.make_executor<ExUpdateAccBuffers>(p_list_, 1, rt_sh_geo_instances_res, FgResRef{});

                auto &build_acc_structs = fg_builder_.AddNode("BUILD ACC STRCTS");

                { // create obj instances buffer
                    FgBufDesc desc;
                    desc.type = Ren::eBufType::Storage;
                    desc.size = SWRTObjInstancesBufChunkSize;
                    desc.views.push_back(Ren::eTexFormat::RGBA32F);

                    rt_sh_obj_instances_res = build_acc_structs.AddTransferOutput("RT SH Obj Instances", desc);
                }

                FgResRef rt_sh_tlas_res = build_acc_structs.AddTransferOutput(acc_struct_data.rt_sh_tlas_buf);

                build_acc_structs.make_executor<ExBuildAccStructures>(
                    p_list_, 1, rt_sh_obj_instances_res, &acc_struct_data,
                    Ren::Span<const mesh_t>{persistent_data.swrt.rt_meshes.data(),
                                            persistent_data.swrt.rt_meshes.capacity()},
                    rt_sh_tlas_res, FgResRef{});
            }
        }

        auto &frame_textures = *fg_builder_.AllocNodeData<FrameTextures>();

        { // Shadow depth
            auto &shadow_depth = fg_builder_.AddNode("SHADOW DEPTH");
            FgResRef vtx_buf1_res = shadow_depth.AddVertexBufferInput(persistent_data.vertex_buf1);
            FgResRef vtx_buf2_res = shadow_depth.AddVertexBufferInput(persistent_data.vertex_buf2);
            FgResRef ndx_buf_res = shadow_depth.AddIndexBufferInput(persistent_data.indices_buf);

            FgResRef shared_data_res = shadow_depth.AddUniformBufferInput(
                common_buffers.shared_data, Ren::Bitmask{Ren::eStage::VertexShader} | Ren::eStage::FragmentShader);
            FgResRef instances_res =
                shadow_depth.AddStorageReadonlyInput(persistent_data.instance_buf, Ren::eStage::VertexShader);
            FgResRef instance_indices_res =
                shadow_depth.AddStorageReadonlyInput(common_buffers.instance_indices, Ren::eStage::VertexShader);

            FgResRef materials_buf_res =
                shadow_depth.AddStorageReadonlyInput(persistent_data.materials_buf, Ren::eStage::VertexShader);
#if defined(REN_GL_BACKEND)
            FgResRef textures_buf_res =
                shadow_depth.AddStorageReadonlyInput(bindless_tex.textures_buf, Ren::eStage::VertexShader);
#else
            FgResRef textures_buf_res;
#endif
            FgResRef noise_tex_res = shadow_depth.AddTextureInput(noise_tex_, Ren::eStage::VertexShader);

            frame_textures.shadow_depth = shadow_depth.AddDepthOutput(shadow_depth_tex_);

            shadow_depth.make_executor<ExShadowDepth>(SHADOWMAP_WIDTH, SHADOWMAP_HEIGHT, &p_list_, vtx_buf1_res,
                                                      vtx_buf2_res, ndx_buf_res, materials_buf_res, &bindless_tex,
                                                      textures_buf_res, instances_res, instance_indices_res,
                                                      shared_data_res, noise_tex_res, frame_textures.shadow_depth);
        }
        { // Shadow color
            auto &shadow_color = fg_builder_.AddNode("SHADOW COLOR");
            FgResRef vtx_buf1_res = shadow_color.AddVertexBufferInput(persistent_data.vertex_buf1);
            FgResRef vtx_buf2_res = shadow_color.AddVertexBufferInput(persistent_data.vertex_buf2);
            FgResRef ndx_buf_res = shadow_color.AddIndexBufferInput(persistent_data.indices_buf);

            FgResRef shared_data_res = shadow_color.AddUniformBufferInput(
                common_buffers.shared_data, Ren::Bitmask{Ren::eStage::VertexShader} | Ren::eStage::FragmentShader);
            FgResRef instances_res =
                shadow_color.AddStorageReadonlyInput(persistent_data.instance_buf, Ren::eStage::VertexShader);
            FgResRef instance_indices_res =
                shadow_color.AddStorageReadonlyInput(common_buffers.instance_indices, Ren::eStage::VertexShader);

            FgResRef materials_buf_res =
                shadow_color.AddStorageReadonlyInput(persistent_data.materials_buf, Ren::eStage::VertexShader);
#if defined(REN_GL_BACKEND)
            FgResRef textures_buf_res =
                shadow_color.AddStorageReadonlyInput(bindless_tex.textures_buf, Ren::eStage::VertexShader);
#else
            FgResRef textures_buf_res;
#endif
            FgResRef noise_tex_res = shadow_color.AddTextureInput(noise_tex_, Ren::eStage::VertexShader);

            frame_textures.shadow_depth = shadow_color.AddDepthOutput(shadow_depth_tex_);
            frame_textures.shadow_color = shadow_color.AddColorOutput(shadow_color_tex_);

            shadow_color.make_executor<ExShadowColor>(
                SHADOWMAP_WIDTH, SHADOWMAP_HEIGHT, &p_list_, vtx_buf1_res, vtx_buf2_res, ndx_buf_res, materials_buf_res,
                &bindless_tex, textures_buf_res, instances_res, instance_indices_res, shared_data_res, noise_tex_res,
                frame_textures.shadow_depth, frame_textures.shadow_color);
        }

        frame_textures.depth_params.w = view_state_.scr_res[0];
        frame_textures.depth_params.h = view_state_.scr_res[1];
        frame_textures.depth_params.format = Ren::eTexFormat::D32_S8;
        // ctx_.capabilities.depth24_stencil8_format ? Ren::eTexFormat::D24_S8 : Ren::eTexFormat::D32_S8;
        frame_textures.depth_params.sampling.wrap = Ren::eTexWrap::ClampToEdge;

        // Main HDR color
        frame_textures.color_params.w = view_state_.scr_res[0];
        frame_textures.color_params.h = view_state_.scr_res[1];
        frame_textures.color_params.format = Ren::eTexFormat::RGBA16F;
        frame_textures.color_params.sampling.filter = Ren::eTexFilter::Bilinear;
        frame_textures.color_params.sampling.wrap = Ren::eTexWrap::ClampToBorder;

        if (deferred_shading) {
            // 4-component world-space normal (alpha or z is roughness)
            frame_textures.normal_params.w = view_state_.scr_res[0];
            frame_textures.normal_params.h = view_state_.scr_res[1];
            frame_textures.normal_params.format = Ren::eTexFormat::R32UI;
            frame_textures.normal_params.sampling.wrap = Ren::eTexWrap::ClampToEdge;
            // packed material params
            frame_textures.specular_params.w = view_state_.scr_res[0];
            frame_textures.specular_params.h = view_state_.scr_res[1];
            frame_textures.specular_params.format = Ren::eTexFormat::R32UI;
            frame_textures.specular_params.flags = {};
            frame_textures.specular_params.sampling.wrap = Ren::eTexWrap::ClampToEdge;
        } else {
            // 4-component world-space normal (alpha or z is roughness)
            frame_textures.normal_params.w = view_state_.scr_res[0];
            frame_textures.normal_params.h = view_state_.scr_res[1];
#if USE_OCT_PACKED_NORMALS == 1
            frame_textures.normal_params.format = Ren::eTexFormat::RGB10_A2;
#else
            frame_textures.normal_params.format = Ren::eTexFormat::RGBA8;
#endif
            frame_textures.normal_params.sampling.wrap = Ren::eTexWrap::ClampToEdge;
            // 4-component specular (alpha is roughness)
            frame_textures.specular_params.w = view_state_.scr_res[0];
            frame_textures.specular_params.h = view_state_.scr_res[1];
            frame_textures.specular_params.format = Ren::eTexFormat::RGBA8;
            frame_textures.specular_params.sampling.wrap = Ren::eTexWrap::ClampToEdge;
        }

        // 4-component albedo (alpha is unused)
        frame_textures.albedo_params.w = view_state_.scr_res[0];
        frame_textures.albedo_params.h = view_state_.scr_res[1];
        frame_textures.albedo_params.format = Ren::eTexFormat::RGBA8;
        frame_textures.albedo_params.sampling.wrap = Ren::eTexWrap::ClampToEdge;

        //
        // Depth prepass
        //
        if (!list.render_settings.debug_culling) {
            auto &depth_fill = fg_builder_.AddNode("DEPTH FILL");

            FgResRef vtx_buf1 = depth_fill.AddVertexBufferInput(persistent_data.vertex_buf1);
            FgResRef vtx_buf2 = depth_fill.AddVertexBufferInput(persistent_data.vertex_buf2);
            FgResRef ndx_buf = depth_fill.AddIndexBufferInput(persistent_data.indices_buf);

            FgResRef shared_data_res = depth_fill.AddUniformBufferInput(
                common_buffers.shared_data, Ren::Bitmask{Ren::eStage::VertexShader} | Ren::eStage::FragmentShader);
            FgResRef instances_res =
                depth_fill.AddStorageReadonlyInput(persistent_data.instance_buf, Ren::eStage::VertexShader);
            FgResRef instance_indices_res =
                depth_fill.AddStorageReadonlyInput(common_buffers.instance_indices, Ren::eStage::VertexShader);

            FgResRef materials_buf_res =
                depth_fill.AddStorageReadonlyInput(persistent_data.materials_buf, Ren::eStage::VertexShader);
#if defined(REN_GL_BACKEND)
            FgResRef textures_buf_res =
                depth_fill.AddStorageReadonlyInput(bindless_tex.textures_buf, Ren::eStage::VertexShader);
#else
            FgResRef textures_buf_res;
#endif
            FgResRef noise_tex_res = depth_fill.AddTextureInput(noise_tex_, Ren::eStage::VertexShader);

            frame_textures.depth = depth_fill.AddDepthOutput(MAIN_DEPTH_TEX, frame_textures.depth_params);

            { // Texture that holds 2D velocity
                Ren::TexParams params;
                params.w = view_state_.scr_res[0];
                params.h = view_state_.scr_res[1];
                params.format = Ren::eTexFormat::RGBA16F;
                params.sampling.wrap = Ren::eTexWrap::ClampToEdge;

                frame_textures.velocity = depth_fill.AddColorOutput(MAIN_VELOCITY_TEX, params);
            }

            depth_fill.make_executor<ExDepthFill>(&p_list_, &view_state_, deferred_shading /* clear_depth */, vtx_buf1,
                                                  vtx_buf2, ndx_buf, materials_buf_res, textures_buf_res, &bindless_tex,
                                                  instances_res, instance_indices_res, shared_data_res, noise_tex_res,
                                                  frame_textures.depth, frame_textures.velocity);
        }

        //
        // Downsample depth
        //
        FgResRef depth_down_2x, depth_hierarchy_tex;

        if ((list.render_settings.ssao_quality != eSSAOQuality::Off ||
             list.render_settings.reflections_quality != eReflectionsQuality::Off) &&
            !list.render_settings.debug_wireframe) {
            // TODO: get rid of this (or use on low spec only)
            AddDownsampleDepthPass(common_buffers, frame_textures.depth, depth_down_2x);

            auto &depth_hierarchy = fg_builder_.AddNode("DEPTH HIERARCHY");
            const FgResRef depth_tex =
                depth_hierarchy.AddTextureInput(frame_textures.depth, Ren::eStage::ComputeShader);
            const FgResRef atomic_buf =
                depth_hierarchy.AddStorageOutput(common_buffers.atomic_cnt, Ren::eStage::ComputeShader);

            { // 32-bit float depth hierarchy
                Ren::TexParams params;
                params.w = ((view_state_.scr_res[0] + ExDepthHierarchy::TileSize - 1) / ExDepthHierarchy::TileSize) *
                           ExDepthHierarchy::TileSize;
                params.h = ((view_state_.scr_res[1] + ExDepthHierarchy::TileSize - 1) / ExDepthHierarchy::TileSize) *
                           ExDepthHierarchy::TileSize;
                params.format = Ren::eTexFormat::R32F;
                params.mip_count = ExDepthHierarchy::MipCount;
                params.sampling.wrap = Ren::eTexWrap::ClampToEdge;
                params.sampling.filter = Ren::eTexFilter::Nearest;

                depth_hierarchy_tex =
                    depth_hierarchy.AddStorageImageOutput("Depth Hierarchy", params, Ren::eStage::ComputeShader);
            }

            depth_hierarchy.make_executor<ExDepthHierarchy>(fg_builder_, &view_state_, depth_tex, atomic_buf,
                                                            depth_hierarchy_tex);
        }

        if (!list.render_settings.debug_wireframe /*&& list.render_settings.taa_mode != eTAAMode::Static*/) {
            AddFillStaticVelocityPass(common_buffers, frame_textures.depth, frame_textures.velocity);
        }

        if (deferred_shading) {
            // Skydome drawing
            AddSkydomePass(common_buffers, frame_textures);

            // GBuffer filling pass
            AddGBufferFillPass(common_buffers, persistent_data, bindless_tex, frame_textures);

            // SSAO
            frame_textures.ssao = AddGTAOPasses(list.render_settings.ssao_quality, frame_textures.depth,
                                                frame_textures.velocity, frame_textures.normal);

            if ((ctx_.capabilities.hwrt || ctx_.capabilities.swrt) &&
                list.render_settings.shadows_quality == eShadowsQuality::Raytraced) {
                frame_textures.sun_shadow =
                    AddHQSunShadowsPasses(common_buffers, persistent_data, acc_struct_data, bindless_tex,
                                          rt_sh_geo_instances_res, rt_sh_obj_instances_res, frame_textures,
                                          list.render_settings.debug_denoise == eDebugDenoise::Shadow);
            } else if (list.render_settings.shadows_quality != eShadowsQuality::Off) {
                frame_textures.sun_shadow =
                    AddLQSunShadowsPass(common_buffers, persistent_data, acc_struct_data, bindless_tex, frame_textures);
            } else {
                frame_textures.sun_shadow = fg_builder_.MakeTextureResource(dummy_white_);
            }

            // GI cache
            AddGICachePasses(list.env.env_map, common_buffers, persistent_data, acc_struct_data, bindless_tex,
                             rt_geo_instances_res, rt_obj_instances_res, frame_textures);

            // GI
            AddDiffusePasses(list.env.env_map, lm_direct_, lm_indir_sh_,
                             list.render_settings.debug_denoise == eDebugDenoise::GI, common_buffers, persistent_data,
                             acc_struct_data, bindless_tex, depth_hierarchy_tex, rt_geo_instances_res,
                             rt_obj_instances_res, frame_textures);

            // GBuffer shading pass
            AddDeferredShadingPass(common_buffers, frame_textures, list.render_settings.gi_quality != eGIQuality::Off);

            // Emissives pass
            AddEmissivesPass(common_buffers, persistent_data, bindless_tex, frame_textures);

            // Additional forward pass (for custom-shaded objects)
            AddForwardOpaquePass(common_buffers, persistent_data, bindless_tex, frame_textures);
        } else {
            const bool use_ssao =
                list.render_settings.ssao_quality != eSSAOQuality::Off && !list.render_settings.debug_wireframe;
            if (use_ssao) {
                AddSSAOPasses(depth_down_2x, frame_textures.depth, frame_textures.ssao);
            } else {
                frame_textures.ssao = fg_builder_.MakeTextureResource(dummy_white_);
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
                                    common_buffers, persistent_data, acc_struct_data, bindless_tex, depth_hierarchy_tex,
                                    rt_geo_instances_res, rt_obj_instances_res, frame_textures);
            } else {
                AddLQSpecularPasses(common_buffers, depth_down_2x, frame_textures);
            }
        }

        //
        // Transparency
        //
        if (deferred_shading) {
            AddOITPasses(common_buffers, persistent_data, acc_struct_data, bindless_tex, depth_hierarchy_tex,
                         rt_geo_instances_res, rt_obj_instances_res, frame_textures);
        } else {
            // Simple transparent pass
            AddForwardTransparentPass(common_buffers, persistent_data, bindless_tex, frame_textures);
        }

        if (list.render_settings.vol_quality != eVolQuality::Off) {
            AddFogPasses(common_buffers, frame_textures);
        }

        frame_textures.exposure = AddAutoexposurePasses(frame_textures.color);

        //
        // Debug stuff
        //
        if (list.render_settings.debug_probes != -1) {
            auto &debug_probes = fg_builder_.AddNode("DEBUG PROBES");

            auto *data = debug_probes.AllocNodeData<ExDebugProbes::Args>();
            data->shared_data =
                debug_probes.AddUniformBufferInput(common_buffers.shared_data, Ren::eStage::VertexShader);
            data->offset_tex = debug_probes.AddTextureInput(frame_textures.gi_cache_offset, Ren::eStage::VertexShader);
            data->irradiance_tex =
                debug_probes.AddTextureInput(frame_textures.gi_cache_irradiance, Ren::eStage::FragmentShader);
            data->distance_tex =
                debug_probes.AddTextureInput(frame_textures.gi_cache_distance, Ren::eStage::FragmentShader);

            data->volume_to_debug = list.render_settings.debug_probes;
            data->probe_volumes = persistent_data.probe_volumes;

            frame_textures.depth = data->depth_tex = debug_probes.AddDepthOutput(frame_textures.depth);
            frame_textures.color = data->output_tex = debug_probes.AddColorOutput(frame_textures.color);

            debug_probes.make_executor<ExDebugProbes>(prim_draw_, fg_builder_, list, &view_state_, data);
        }

        if (list.render_settings.debug_oit_layer != -1) {
            auto &debug_oit = fg_builder_.AddNode("DEBUG OIT");

            auto *data = debug_oit.AllocNodeData<ExDebugOIT::Args>();
            data->layer_index = list.render_settings.debug_oit_layer;
            frame_textures.oit_depth_buf = data->oit_depth_buf =
                debug_oit.AddStorageReadonlyInput(frame_textures.oit_depth_buf, Ren::eStage::ComputeShader);
            frame_textures.color = data->output_tex =
                debug_oit.AddStorageImageOutput(frame_textures.color, Ren::eStage::ComputeShader);

            debug_oit.make_executor<ExDebugOIT>(fg_builder_, &view_state_, data);
        }

        if (list.render_settings.debug_rt != eDebugRT::Off && list.env.env_map &&
            (ctx_.capabilities.hwrt || ctx_.capabilities.swrt)) {
            auto &debug_rt = fg_builder_.AddNode("DEBUG RT");

            const Ren::eStage stage = Ren::eStage::ComputeShader;

            auto *data = debug_rt.AllocNodeData<ExDebugRT::Args>();
            data->shared_data = debug_rt.AddUniformBufferInput(common_buffers.shared_data, stage);
            if (list.render_settings.debug_rt == eDebugRT::Main) {
                data->geo_data_buf = debug_rt.AddStorageReadonlyInput(rt_geo_instances_res, stage);
            } else if (list.render_settings.debug_rt == eDebugRT::Shadow) {
                data->geo_data_buf = debug_rt.AddStorageReadonlyInput(rt_sh_geo_instances_res, stage);
            }
            data->materials_buf = debug_rt.AddStorageReadonlyInput(persistent_data.materials_buf, stage);
            data->vtx_buf1 = debug_rt.AddStorageReadonlyInput(persistent_data.vertex_buf1, stage);
            data->vtx_buf2 = debug_rt.AddStorageReadonlyInput(persistent_data.vertex_buf2, stage);
            data->ndx_buf = debug_rt.AddStorageReadonlyInput(persistent_data.indices_buf, stage);
            data->lights_buf = debug_rt.AddStorageReadonlyInput(common_buffers.lights, stage);
            data->shadow_depth_tex = debug_rt.AddTextureInput(shadow_depth_tex_, stage);
            data->shadow_color_tex = debug_rt.AddTextureInput(shadow_color_tex_, stage);
            data->ltc_luts_tex = debug_rt.AddTextureInput(ltc_luts_, stage);
            data->cells_buf = debug_rt.AddStorageReadonlyInput(common_buffers.rt_cells, stage);
            data->items_buf = debug_rt.AddStorageReadonlyInput(common_buffers.rt_items, stage);

            if (!ctx_.capabilities.hwrt) {
                data->swrt.root_node = persistent_data.swrt.rt_root_node;
                data->swrt.rt_blas_buf = debug_rt.AddStorageReadonlyInput(persistent_data.swrt.rt_blas_buf, stage);
                data->swrt.prim_ndx_buf =
                    debug_rt.AddStorageReadonlyInput(persistent_data.swrt.rt_prim_indices_buf, stage);
                data->swrt.mesh_instances_buf = debug_rt.AddStorageReadonlyInput(rt_obj_instances_res, stage);
                data->swrt.rt_tlas_buf = debug_rt.AddStorageReadonlyInput(persistent_data.rt_tlas_buf, stage);

#if defined(REN_GL_BACKEND)
                data->swrt.textures_buf = debug_rt.AddStorageReadonlyInput(bindless_tex.textures_buf, stage);
#endif
            }

            data->env_tex = debug_rt.AddTextureInput(list.env.env_map, stage);

            data->irradiance_tex = debug_rt.AddTextureInput(frame_textures.gi_cache_irradiance, stage);
            data->distance_tex = debug_rt.AddTextureInput(frame_textures.gi_cache_distance, stage);
            data->offset_tex = debug_rt.AddTextureInput(frame_textures.gi_cache_offset, stage);

            frame_textures.color = data->output_tex = debug_rt.AddStorageImageOutput(frame_textures.color, stage);

            const Ren::IAccStructure *tlas_to_debug = acc_struct_data.rt_tlases[int(list.render_settings.debug_rt) - 1];
            debug_rt.make_executor<ExDebugRT>(fg_builder_, &view_state_, tlas_to_debug, &bindless_tex, data);
        }

        FgResRef resolved_color;

        //
        // Temporal resolve
        //
        const bool use_taa = list.render_settings.taa_mode != eTAAMode::Off && !list.render_settings.debug_wireframe;
        if (use_taa) {
            AddTaaPass(common_buffers, frame_textures, list.render_settings.taa_mode == eTAAMode::Static,
                       resolved_color);
        } else {
            resolved_color = frame_textures.color;
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
        FgResRef bloom_tex;
        if (list.render_settings.enable_bloom && !list.render_settings.debug_wireframe) {
            bloom_tex = AddBloomPasses(resolved_color, frame_textures.exposure, true);
        }

        //
        // Debugging
        //
        if (list.render_settings.debug_motion) {
            AddDebugVelocityPass(frame_textures.velocity, resolved_color);
            bloom_tex = {};
        }

        bool apply_dof = false;

        //
        // Combine with blurred and tonemap
        //
        {
            FgResRef color_tex;
            const char *output_tex = nullptr;

            if ((list.render_settings.taa_mode != eTAAMode::Off && !list.render_settings.debug_wireframe) ||
                apply_dof) {
                if (apply_dof) {
                    if (list.render_settings.taa_mode != eTAAMode::Off) {
                        color_tex = frame_textures.color;
                    } else {
                        // color_tex = DOF_COLOR_TEX;
                    }
                } else {
                    color_tex = resolved_color;
                }
            } else {
                color_tex = frame_textures.color;
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
            ex_postprocess_args_.exposure_tex =
                postprocess.AddTextureInput(frame_textures.exposure, Ren::eStage::FragmentShader);
            ex_postprocess_args_.color_tex = postprocess.AddTextureInput(color_tex, Ren::eStage::FragmentShader);
            if (list.render_settings.enable_bloom && bloom_tex) {
                ex_postprocess_args_.bloom_tex = postprocess.AddTextureInput(bloom_tex, Ren::eStage::FragmentShader);
            } else {
                ex_postprocess_args_.bloom_tex = postprocess.AddTextureInput(dummy_black_, Ren::eStage::FragmentShader);
            }
            if (tonemap_lut_) {
                ex_postprocess_args_.lut_tex = postprocess.AddTextureInput(tonemap_lut_, Ren::eStage::FragmentShader);
            }
            if (output_tex) {
                Ren::TexParams params;
                params.w = view_state_.scr_res[0];
                params.h = view_state_.scr_res[1];
                params.format = Ren::eTexFormat::RGB8;
                params.sampling.filter = Ren::eTexFilter::Bilinear;
                params.sampling.wrap = Ren::eTexWrap::ClampToEdge;

                ex_postprocess_args_.output_tex = postprocess.AddColorOutput(output_tex, params);
            } else if (target) {
                ex_postprocess_args_.output_tex = postprocess.AddColorOutput(target);
            } else {
                ex_postprocess_args_.output_tex = postprocess.AddColorOutput(ctx_.backbuffer_ref());
            }
            ex_postprocess_args_.linear_sampler = linear_sampler_;
            ex_postprocess_args_.tonemap_mode = int(list.render_settings.tonemap_mode);
            ex_postprocess_args_.inv_gamma = 1.0f / list.draw_cam.gamma;
            ex_postprocess_args_.fade = list.draw_cam.fade;
            ex_postprocess_args_.aberration = list.render_settings.enable_aberration ? 1.0f : 0.0f;
            ex_postprocess_args_.purkinje = list.render_settings.enable_purkinje ? 1.0f : 0.0f;

            backbuffer_sources_.push_back(ex_postprocess_args_.output_tex);

            postprocess.make_executor<ExPostprocess>(prim_draw_, sh_, &view_state_, &ex_postprocess_args_);
        }

        { // Readback exposure
            auto &read_exposure = fg_builder_.AddNode("READ EXPOSURE");

            auto *data = read_exposure.AllocNodeData<ExReadExposure::Args>();
            data->input_tex = read_exposure.AddTransferImageInput(frame_textures.exposure);

            FgBufDesc desc;
            desc.type = Ren::eBufType::Readback;
            desc.size = Ren::MaxFramesInFlight * sizeof(float);
            data->output_buf = read_exposure.AddTransferOutput("Exposure Readback", desc);

            backbuffer_sources_.push_back(data->output_buf);

            ex_read_exposure_.Setup(data);
            read_exposure.set_executor(&ex_read_exposure_);
        }

        fg_builder_.Compile(backbuffer_sources_);
        const uint64_t rp_setup_end_us = Sys::GetTimeUs();
        ctx_.log()->Info("Renderpass setup is done in %.2fms", (rp_setup_end_us - rp_setup_beg_us) * 0.001);
    } else {
        //  Use correct backbuffer image (assume topology is not changed)
        //  TODO: get rid of this
        auto &postprocess = *fg_builder_.FindNode("POSTPROCESS");
        if (target) {
            ex_postprocess_args_.output_tex = postprocess.ReplaceColorOutput(0, target);
            if (blit_to_backbuffer) {
                ex_postprocess_args_.output_tex2 = postprocess.ReplaceColorOutput(1, ctx_.backbuffer_ref());
            }
        } else {
            ex_postprocess_args_.output_tex = postprocess.ReplaceColorOutput(0, ctx_.backbuffer_ref());
        }

        ex_postprocess_args_.fade = list.draw_cam.fade;

        postprocess.make_executor<ExPostprocess>(prim_draw_, sh_, &view_state_, &ex_postprocess_args_);
    }

    fg_builder_.Execute();

    { // store info to use it in next frame
        view_state_.down_buf_view_from_world = list.draw_cam.view_matrix();
        view_state_.prev_cam_pos = list.draw_cam.world_position();
        view_state_.prev_sun_dir = list.env.sun_dir;

        view_state_.prev_clip_from_world = list.draw_cam.proj_matrix() * list.draw_cam.view_matrix();
        view_state_.prev_view_from_world = list.draw_cam.view_matrix();
        view_state_.prev_clip_from_view = list.draw_cam.proj_matrix_offset();

        view_state_.prev_pre_exposure = view_state_.pre_exposure;
    }

    const uint64_t cpu_draw_end_us = Sys::GetTimeUs();

    // store values for current frame
    backend_cpu_start_ = cpu_draw_start_us;
    backend_cpu_end_ = cpu_draw_end_us;

    // Write timestamp at the end of execution
    backend_gpu_end_ = ctx_.WriteTimestamp(false);

    __itt_task_end(__g_itt_domain);
}

void Eng::Renderer::SetTonemapLUT(const int res, const Ren::eTexFormat format, Ren::Span<const uint8_t> data) {
    assert(format == Ren::eTexFormat::RGB10_A2);

    if (data.empty()) {
        // free texture;
        tonemap_lut_ = {};
    }

    if (!tonemap_lut_) {
        Ren::TexParams params = {};
        params.w = params.h = params.d = res;
        params.usage = Ren::Bitmask(Ren::eTexUsage::Sampled) | Ren::eTexUsage::Transfer;
        params.format = Ren::eTexFormat::RGB10_A2;
        params.sampling.filter = Ren::eTexFilter::Bilinear;
        params.sampling.wrap = Ren::eTexWrap::ClampToEdge;

        Ren::eTexLoadStatus status;
        tonemap_lut_ = ctx_.LoadTexture("Tonemap LUT", params, ctx_.default_mem_allocs(), &status);
    }

    Ren::CommandBuffer cmd_buf = ctx_.BegTempSingleTimeCommands();

    const uint32_t data_len = res * res * res * sizeof(uint32_t);
    Ren::Buffer temp_upload_buf{"Temp tonemap LUT upload", ctx_.api_ctx(), Ren::eBufType::Upload, data_len};
    { // update stage buffer
        uint32_t *mapped_ptr = reinterpret_cast<uint32_t *>(temp_upload_buf.Map());
        memcpy(mapped_ptr, data.data(), data_len);
        temp_upload_buf.Unmap();
    }

    { // update texture
        const Ren::TransitionInfo res_transitions1[] = {{&temp_upload_buf, Ren::eResState::CopySrc},
                                                        {tonemap_lut_.get(), Ren::eResState::CopyDst}};
        TransitionResourceStates(ctx_.api_ctx(), cmd_buf, Ren::AllStages, Ren::AllStages, res_transitions1);

        tonemap_lut_->SetSubImage(0, 0, 0, res, res, res, Ren::eTexFormat::RGB10_A2, temp_upload_buf, cmd_buf, 0,
                                  data_len);

        const Ren::TransitionInfo res_transitions2[] = {{tonemap_lut_.get(), Ren::eResState::ShaderResource}};
        TransitionResourceStates(ctx_.api_ctx(), cmd_buf, Ren::AllStages, Ren::AllStages, res_transitions2);

        ctx_.EndTempSingleTimeCommands(cmd_buf);
    }

    temp_upload_buf.FreeImmediate();
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
    if (frame_index_ < 10) {
        // Skip a few initial frames
        return;
    }

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
        for (auto it = fg_buffers.cbegin(); it != fg_buffers.cend(); ++it) {
            if (it->external || !it->strong_ref || it->alias_of != -1 || !it->lifetime.is_used() ||
                it->strong_ref->type() == Ren::eBufType::Upload || it->strong_ref->type() == Ren::eBufType::Readback) {
                continue;
            }

            resource_info_t &info = backend_info_.resources_info.emplace_back();
            info.name = "[Buf] " + it->name;
            fg_builder_.GetResourceFrameLifetime(*it, info.lifetime);

            const Ren::MemAllocation &alloc = it->strong_ref->mem_alloc();
            if (alloc.pool != 0xffff && alloc.owner == nullptr) {
                info.heap = alloc.pool;
                info.offset = alloc.offset;
                info.size = alloc.block;
            } else {
                info.heap = 0;
                info.offset = heap_size;
                info.size = it->desc.size;
                heap_size += it->desc.size;
            }

            indices[it.index()] = int(backend_info_.resources_info.size() - 1);
        }
        for (auto it = fg_buffers.cbegin(); it != fg_buffers.cend(); ++it) {
            if (it->external || !it->strong_ref || it->alias_of == -1 || !it->lifetime.is_used() ||
                it->strong_ref->type() == Ren::eBufType::Upload || it->strong_ref->type() == Ren::eBufType::Readback) {
                continue;
            }

            resource_info_t &info = backend_info_.resources_info.emplace_back();
            info.name = "[Buf] " + it->name;
            fg_builder_.GetResourceFrameLifetime(*it, info.lifetime);

            const Ren::MemAllocation &alloc = it->strong_ref->mem_alloc();
            if (alloc.pool != 0xffff && alloc.owner == nullptr) {
                info.heap = alloc.pool;
                info.offset = alloc.offset;
                info.size = alloc.block;
            } else {
                info.heap = 0;
                info.offset = backend_info_.resources_info[indices[it->alias_of]].offset;
                info.size = it->desc.size;
            }
        }

        heap_size = 0; // dummy for the case when memory heaps are not available

        const auto &fg_textures = fg_builder_.textures();
        indices.clear();
        indices.resize(fg_textures.capacity(), -1);
        for (auto it = fg_textures.cbegin(); it != fg_textures.cend(); ++it) {
            if (it->external || !it->strong_ref || it->alias_of != -1) {
                continue;
            }

            resource_info_t &info = backend_info_.resources_info.emplace_back();
            info.name = "[Tex] " + it->name;
            fg_builder_.GetResourceFrameLifetime(*it, info.lifetime);

            const Ren::MemAllocation &alloc = it->strong_ref->mem_alloc();
            if (alloc.pool != 0xffff && alloc.owner == nullptr) {
                info.heap = alloc.pool;
                info.offset = alloc.offset;
                info.size = alloc.block;
            } else {
                info.heap = 1;
                info.offset = heap_size;
                info.size = Ren::GetDataLenBytes(it->desc);
                heap_size += info.size;
            }

            indices[it.index()] = int(backend_info_.resources_info.size() - 1);
        }
        for (auto it = fg_textures.cbegin(); it != fg_textures.cend(); ++it) {
            if (it->external || !it->strong_ref || it->alias_of == -1) {
                continue;
            }

            resource_info_t &info = backend_info_.resources_info.emplace_back();
            info.name = "[Tex] " + it->name;
            fg_builder_.GetResourceFrameLifetime(*it, info.lifetime);

            const Ren::MemAllocation &alloc = it->strong_ref->mem_alloc();
            if (alloc.pool != 0xffff && alloc.owner == nullptr) {
                info.heap = alloc.pool;
                info.offset = alloc.offset;
                info.size = alloc.block;
            } else {
                info.heap = 1;
                info.offset = backend_info_.resources_info[indices[it->alias_of]].offset;
                info.size = Ren::GetDataLenBytes(it->desc);
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

void Eng::Renderer::InitPipelinesForProgram(const Ren::ProgramRef &prog, const Ren::Bitmask<Ren::eMatFlags> mat_flags,
                                            Ren::PipelineStorage &storage,
                                            Ren::SmallVectorImpl<Ren::PipelineRef> &out_pipelines) const {
    for (int i = 0; i < int(eFwdPipeline::_Count); ++i) {
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
                new_pipeline.Init(ctx_.api_ctx(), rast_state, prog, draw_pass_vi_, rp_main_draw_, 0, ctx_.log());
            if (!res) {
                ctx_.log()->Error("Failed to initialize pipeline!");
            }
        }

        out_pipelines.emplace_back(&storage, new_index);
    }
}

void Eng::Renderer::BlitPixelsTonemap(const uint8_t *data, const int w, const int h, const int stride,
                                      const Ren::eTexFormat format, const float gamma, const float min_exposure,
                                      const float max_exposure, const Ren::TexRef &target, const bool compressed,
                                      const bool blit_to_backbuffer) {
    const int cur_scr_w = ctx_.w(), cur_scr_h = ctx_.h();
    Ren::ILog *log = ctx_.log();

    if (!cur_scr_w || !cur_scr_h) {
        // view is minimized?
        return;
    }

    if (!prim_draw_.LazyInit(ctx_)) {
        log->Error("[Renderer] Failed to initialize primitive drawing!");
    }

    min_exposure_ = std::pow(2.0f, min_exposure);
    max_exposure_ = std::pow(2.0f, max_exposure);

    bool resolution_changed = false;
    if (cur_scr_w != view_state_.scr_res[0] || cur_scr_h != view_state_.scr_res[1]) {
        resolution_changed = true;
        view_state_.scr_res = view_state_.act_res = Ren::Vec2i{cur_scr_w, cur_scr_h};
    }
    view_state_.pre_exposure = readback_exposure();

    const bool rebuild_renderpasses = !cached_settings_.has_value() || (cached_settings_.value() != settings) ||
                                      !fg_builder_.ready() || cached_rp_index_ != 1 || resolution_changed;
    cached_settings_ = settings;
    cached_rp_index_ = 1;

    assert(format == Ren::eTexFormat::RGBA32F);

    Ren::BufRef temp_upload_buf = ctx_.LoadBuffer("Image upload buf", Ren::eBufType::Upload, 4 * w * h * sizeof(float));
    uint8_t *stage_data = temp_upload_buf->Map();
    for (int y = 0; y < h; ++y) {
#if defined(REN_GL_BACKEND)
        memcpy(&stage_data[(h - y - 1) * 4 * w * sizeof(float)], &data[y * 4 * stride * sizeof(float)],
               4 * w * sizeof(float));
#else
        memcpy(&stage_data[y * 4 * w * sizeof(float)], &data[y * 4 * stride * sizeof(float)], 4 * w * sizeof(float));
#endif
    }
    temp_upload_buf->Unmap();

    if (rebuild_renderpasses) {
        const uint64_t rp_setup_beg_us = Sys::GetTimeUs();

        fg_builder_.Reset();
        backbuffer_sources_.clear();

        auto &update_image = fg_builder_.AddNode("UPDATE IMAGE");

        FgResRef stage_buf_res = update_image.AddTransferInput(temp_upload_buf);

        FgResRef output_tex_res;
        { // output image
            Ren::TexParams params;
            params.w = cur_scr_w;
            params.h = cur_scr_h;
            params.format = format;
            params.sampling.filter = Ren::eTexFilter::Bilinear;
            params.sampling.wrap = Ren::eTexWrap::ClampToEdge;
            output_tex_res = update_image.AddTransferImageOutput("Temp Image", params);
        }

        update_image.set_execute_cb([stage_buf_res, output_tex_res](FgBuilder &builder) {
            FgAllocBuf &stage_buf = builder.GetReadBuffer(stage_buf_res);
            FgAllocTex &output_image = builder.GetWriteTexture(output_tex_res);

            const int w = output_image.ref->params.w;
            const int h = output_image.ref->params.h;

            output_image.ref->SetSubImage(0, 0, w, h, Ren::eTexFormat::RGBA32F, *stage_buf.ref,
                                          builder.ctx().current_cmd_buf(), 0, stage_buf.ref->size());
        });

        FgResRef exposure_tex = AddAutoexposurePasses(output_tex_res);

        FgResRef bloom_tex;
        if (settings.enable_bloom) {
            bloom_tex = AddBloomPasses(output_tex_res, exposure_tex, compressed);
        }

        auto &postprocess = fg_builder_.AddNode("POSTPROCESS");

        ex_postprocess_args_ = {};
        ex_postprocess_args_.exposure_tex = postprocess.AddTextureInput(exposure_tex, Ren::eStage::FragmentShader);
        ex_postprocess_args_.color_tex = postprocess.AddTextureInput(output_tex_res, Ren::eStage::FragmentShader);
        if (bloom_tex) {
            ex_postprocess_args_.bloom_tex = postprocess.AddTextureInput(bloom_tex, Ren::eStage::FragmentShader);
        } else {
            ex_postprocess_args_.bloom_tex = postprocess.AddTextureInput(dummy_black_, Ren::eStage::FragmentShader);
        }
        if (tonemap_lut_) {
            ex_postprocess_args_.lut_tex = postprocess.AddTextureInput(tonemap_lut_, Ren::eStage::FragmentShader);
        }
        if (target) {
            ex_postprocess_args_.output_tex = postprocess.AddColorOutput(target);
            if (blit_to_backbuffer) {
                ex_postprocess_args_.output_tex2 = postprocess.AddColorOutput(ctx_.backbuffer_ref());
            }
        } else {
            ex_postprocess_args_.output_tex = postprocess.AddColorOutput(ctx_.backbuffer_ref());
        }
        ex_postprocess_args_.linear_sampler = linear_sampler_;
        ex_postprocess_args_.tonemap_mode = int(settings.tonemap_mode);
        ex_postprocess_args_.inv_gamma = 1.0f / gamma;
        ex_postprocess_args_.fade = 0.0f;
        ex_postprocess_args_.aberration = settings.enable_aberration ? 1.0f : 0.0f;

        backbuffer_sources_.push_back(ex_postprocess_args_.output_tex);

        postprocess.make_executor<ExPostprocess>(prim_draw_, sh_, &view_state_, &ex_postprocess_args_);

        { // Readback exposure
            auto &read_exposure = fg_builder_.AddNode("READ EXPOSURE");

            auto *data = read_exposure.AllocNodeData<ExReadExposure::Args>();
            data->input_tex = read_exposure.AddTransferImageInput(exposure_tex);

            FgBufDesc desc;
            desc.type = Ren::eBufType::Readback;
            desc.size = Ren::MaxFramesInFlight * sizeof(float);
            data->output_buf = read_exposure.AddTransferOutput("Exposure Readback", desc);

            backbuffer_sources_.push_back(data->output_buf);

            ex_read_exposure_.Setup(data);
            read_exposure.set_executor(&ex_read_exposure_);
        }

        fg_builder_.Compile();

        const uint64_t rp_setup_end_us = Sys::GetTimeUs();
        ctx_.log()->Info("Renderpass setup is done in %.2fms", (rp_setup_end_us - rp_setup_beg_us) * 0.001);
    } else {
        auto *update_image = fg_builder_.FindNode("UPDATE IMAGE");
        update_image->ReplaceTransferInput(0, temp_upload_buf);

        auto *postprocess = fg_builder_.FindNode("POSTPROCESS");
        if (target) {
            ex_postprocess_args_.output_tex = postprocess->ReplaceColorOutput(0, target);
            if (blit_to_backbuffer) {
                ex_postprocess_args_.output_tex2 = postprocess->ReplaceColorOutput(1, ctx_.backbuffer_ref());
            }
        } else {
            ex_postprocess_args_.output_tex = postprocess->ReplaceColorOutput(0, ctx_.backbuffer_ref());
        }
    }

    fg_builder_.Execute();
}

void Eng::Renderer::BlitImageTonemap(const Ren::TexRef &result, const int w, const int h, Ren::eTexFormat format,
                                     const float gamma, const float min_exposure, const float max_exposure,
                                     const Ren::TexRef &target, const bool compressed, const bool blit_to_backbuffer) {
    const int cur_scr_w = ctx_.w(), cur_scr_h = ctx_.h();
    Ren::ILog *log = ctx_.log();

    if (!cur_scr_w || !cur_scr_h) {
        // view is minimized?
        return;
    }

    if (!prim_draw_.LazyInit(ctx_)) {
        log->Error("[Renderer] Failed to initialize primitive drawing!");
    }

    min_exposure_ = std::pow(2.0f, min_exposure);
    max_exposure_ = std::pow(2.0f, max_exposure);

    bool resolution_changed = false;
    if (cur_scr_w != view_state_.scr_res[0] || cur_scr_h != view_state_.scr_res[1]) {
        resolution_changed = true;
        view_state_.scr_res = view_state_.act_res = Ren::Vec2i{cur_scr_w, cur_scr_h};
    }
    view_state_.pre_exposure = readback_exposure();

    const bool rebuild_renderpasses = !cached_settings_.has_value() || (cached_settings_.value() != settings) ||
                                      !fg_builder_.ready() || cached_rp_index_ != 1 || resolution_changed;
    cached_settings_ = settings;
    cached_rp_index_ = 1;

    assert(format == Ren::eTexFormat::RGBA32F);

    if (rebuild_renderpasses) {
        const uint64_t rp_setup_beg_us = Sys::GetTimeUs();

        fg_builder_.Reset();
        backbuffer_sources_.clear();

        FgResRef output_tex_res = fg_builder_.MakeTextureResource(result);
        FgResRef exposure_tex = AddAutoexposurePasses(output_tex_res);

        FgResRef bloom_tex;
        if (settings.enable_bloom) {
            bloom_tex = AddBloomPasses(output_tex_res, exposure_tex, compressed);
        }

        auto &postprocess = fg_builder_.AddNode("POSTPROCESS");

        ex_postprocess_args_ = {};
        ex_postprocess_args_.exposure_tex = postprocess.AddTextureInput(exposure_tex, Ren::eStage::FragmentShader);
        ex_postprocess_args_.color_tex = postprocess.AddTextureInput(output_tex_res, Ren::eStage::FragmentShader);
        if (bloom_tex) {
            ex_postprocess_args_.bloom_tex = postprocess.AddTextureInput(bloom_tex, Ren::eStage::FragmentShader);
        } else {
            ex_postprocess_args_.bloom_tex = postprocess.AddTextureInput(dummy_black_, Ren::eStage::FragmentShader);
        }
        if (tonemap_lut_) {
            ex_postprocess_args_.lut_tex = postprocess.AddTextureInput(tonemap_lut_, Ren::eStage::FragmentShader);
        }
        if (target) {
            ex_postprocess_args_.output_tex = postprocess.AddColorOutput(target);
            if (blit_to_backbuffer) {
                ex_postprocess_args_.output_tex2 = postprocess.AddColorOutput(ctx_.backbuffer_ref());
            }
        } else {
            ex_postprocess_args_.output_tex = postprocess.AddColorOutput(ctx_.backbuffer_ref());
        }
        ex_postprocess_args_.linear_sampler = linear_sampler_;
        ex_postprocess_args_.tonemap_mode = int(settings.tonemap_mode);
        ex_postprocess_args_.inv_gamma = 1.0f / gamma;
        ex_postprocess_args_.fade = 0.0f;
        ex_postprocess_args_.aberration = settings.enable_aberration ? 1.0f : 0.0f;

        backbuffer_sources_.push_back(ex_postprocess_args_.output_tex);

        postprocess.make_executor<ExPostprocess>(prim_draw_, sh_, &view_state_, &ex_postprocess_args_);

        { // Readback exposure
            auto &read_exposure = fg_builder_.AddNode("READ EXPOSURE");

            auto *data = read_exposure.AllocNodeData<ExReadExposure::Args>();
            data->input_tex = read_exposure.AddTransferImageInput(exposure_tex);

            FgBufDesc desc;
            desc.type = Ren::eBufType::Readback;
            desc.size = Ren::MaxFramesInFlight * sizeof(float);
            data->output_buf = read_exposure.AddTransferOutput("Exposure Readback", desc);

            backbuffer_sources_.push_back(data->output_buf);

            ex_read_exposure_.Setup(data);
            read_exposure.set_executor(&ex_read_exposure_);
        }

        fg_builder_.Compile();

        const uint64_t rp_setup_end_us = Sys::GetTimeUs();
        ctx_.log()->Info("Renderpass setup is done in %.2fms", (rp_setup_end_us - rp_setup_beg_us) * 0.001);
    } else {
        auto *postprocess = fg_builder_.FindNode("POSTPROCESS");
        if (target) {
            ex_postprocess_args_.output_tex = postprocess->ReplaceColorOutput(0, target);
            if (blit_to_backbuffer) {
                ex_postprocess_args_.output_tex2 = postprocess->ReplaceColorOutput(1, ctx_.backbuffer_ref());
            }
        } else {
            ex_postprocess_args_.output_tex = postprocess->ReplaceColorOutput(0, ctx_.backbuffer_ref());
        }
    }

    fg_builder_.Execute();
}

#undef BBOX_POINTS
