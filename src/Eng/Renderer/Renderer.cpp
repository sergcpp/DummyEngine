#include "Renderer.h"

#include <cstdio>

#include <Ren/Context.h>
#include <Ren/Utils.h>
#include <Sys/MonoAlloc.h>
#include <Sys/ThreadPool.h>
#include <Sys/Time_.h>

#include "Renderer_Names.h"

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

const Ren::Vec2f HaltonSeq23[64] = {Ren::Vec2f{0.0000000000f, 0.0000000000f}, Ren::Vec2f{0.5000000000f, 0.3333333430f},
                                    Ren::Vec2f{0.2500000000f, 0.6666666870f}, Ren::Vec2f{0.7500000000f, 0.1111111190f},
                                    Ren::Vec2f{0.1250000000f, 0.4444444780f}, Ren::Vec2f{0.6250000000f, 0.7777778510f},
                                    Ren::Vec2f{0.3750000000f, 0.2222222390f}, Ren::Vec2f{0.8750000000f, 0.5555555820f},
                                    Ren::Vec2f{0.0625000000f, 0.8888889550f}, Ren::Vec2f{0.5625000000f, 0.0370370410f},
                                    Ren::Vec2f{0.3125000000f, 0.3703704180f}, Ren::Vec2f{0.8125000000f, 0.7037037610f},
                                    Ren::Vec2f{0.1875000000f, 0.1481481640f}, Ren::Vec2f{0.6875000000f, 0.4814815220f},
                                    Ren::Vec2f{0.4375000000f, 0.8148149250f}, Ren::Vec2f{0.9375000000f, 0.2592592840f},
                                    Ren::Vec2f{0.0312500000f, 0.5925926570f}, Ren::Vec2f{0.5312500000f, 0.9259260300f},
                                    Ren::Vec2f{0.2812500000f, 0.0740740821f}, Ren::Vec2f{0.7812500000f, 0.4074074630f},
                                    Ren::Vec2f{0.1562500000f, 0.7407408360f}, Ren::Vec2f{0.6562500000f, 0.1851852090f},
                                    Ren::Vec2f{0.4062500000f, 0.5185185670f}, Ren::Vec2f{0.9062500000f, 0.8518519400f},
                                    Ren::Vec2f{0.0937500000f, 0.2962963280f}, Ren::Vec2f{0.5937500000f, 0.6296296720f},
                                    Ren::Vec2f{0.3437500000f, 0.9629630450f}, Ren::Vec2f{0.8437500000f, 0.0123456810f},
                                    Ren::Vec2f{0.2187500000f, 0.3456790750f}, Ren::Vec2f{0.7187500000f, 0.6790124770f},
                                    Ren::Vec2f{0.4687500000f, 0.1234568060f}, Ren::Vec2f{0.9687500000f, 0.4567902090f},
                                    Ren::Vec2f{0.0156250000f, 0.7901235820f}, Ren::Vec2f{0.5156250000f, 0.2345679400f},
                                    Ren::Vec2f{0.2656250000f, 0.5679013130f}, Ren::Vec2f{0.7656250000f, 0.9012346860f},
                                    Ren::Vec2f{0.1406250000f, 0.0493827239f}, Ren::Vec2f{0.6406250000f, 0.3827161190f},
                                    Ren::Vec2f{0.3906250000f, 0.7160494920f}, Ren::Vec2f{0.8906250000f, 0.1604938510f},
                                    Ren::Vec2f{0.0781250000f, 0.4938272240f}, Ren::Vec2f{0.5781250000f, 0.8271605970f},
                                    Ren::Vec2f{0.3281250000f, 0.2716049850f}, Ren::Vec2f{0.8281250000f, 0.6049383880f},
                                    Ren::Vec2f{0.2031250000f, 0.9382717610f}, Ren::Vec2f{0.7031250000f, 0.0864197686f},
                                    Ren::Vec2f{0.4531250000f, 0.4197531640f}, Ren::Vec2f{0.9531250000f, 0.7530865670f},
                                    Ren::Vec2f{0.0468750000f, 0.1975308950f}, Ren::Vec2f{0.5468750000f, 0.5308642980f},
                                    Ren::Vec2f{0.2968750000f, 0.8641976710f}, Ren::Vec2f{0.7968750000f, 0.3086420300f},
                                    Ren::Vec2f{0.1718750000f, 0.6419754030f}, Ren::Vec2f{0.6718750000f, 0.9753087760f},
                                    Ren::Vec2f{0.4218750000f, 0.0246913619f}, Ren::Vec2f{0.9218750000f, 0.3580247460f},
                                    Ren::Vec2f{0.1093750000f, 0.6913581490f}, Ren::Vec2f{0.6093750000f, 0.1358024920f},
                                    Ren::Vec2f{0.3593750000f, 0.4691358800f}, Ren::Vec2f{0.8593750000f, 0.8024692540f},
                                    Ren::Vec2f{0.2343750000f, 0.2469136120f}, Ren::Vec2f{0.7343750000f, 0.5802469850f},
                                    Ren::Vec2f{0.4843750000f, 0.9135804180f}, Ren::Vec2f{0.9843750000f, 0.0617284030f}};

const int TaaSampleCount = 8;

#include "__brdf_lut.inl"
#include "__cone_rt_lut.inl"
#include "__noise.inl"

extern const int g_sobol_256spp_256d[];
extern const int g_scrambling_tile_1spp[];
extern const int g_scrambling_tile_32spp[];
extern const int g_scrambling_tile_64spp[];
extern const int g_scrambling_tile_128spp[];
extern const int g_ranking_tile_1spp[];
extern const int g_ranking_tile_32spp[];
extern const int g_ranking_tile_64spp[];
extern const int g_ranking_tile_128spp[];

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

Renderer::Renderer(Ren::Context &ctx, ShaderLoader &sh, Random &rand, std::shared_ptr<Sys::ThreadPool> threads)
    : ctx_(ctx), sh_(sh), rand_(rand), threads_(std::move(threads)),
      shadow_splitter_(SHADOWMAP_WIDTH, SHADOWMAP_HEIGHT), rp_builder_(ctx_, sh_) {
    using namespace RendererInternal;

    swCullCtxInit(&cull_ctx_, ctx.w(), ctx.h(), 0.0f);

    { // buffer used to sample probes
        FrameBuf::ColorAttachmentDesc desc;
        desc.format = Ren::eTexFormat::RawRGBA32F;
        desc.filter = Ren::eTexFilter::NoFilter;
        desc.wrap = Ren::eTexWrap::ClampToEdge;
        probe_sample_buf_ = FrameBuf("Probe sample", ctx_, 24, 8, &desc, 1, {}, 1, ctx.log());
    }

    static const uint8_t black[] = {0, 0, 0, 0}, white[] = {255, 255, 255, 255};

    { // dummy 1px textures
        Ren::Tex2DParams p;
        p.w = p.h = 1;
        p.format = Ren::eTexFormat::RawRGBA8888;
        p.usage = (Ren::eTexUsageBits::Transfer | Ren::eTexUsageBits::Sampled);
        p.sampling.wrap = Ren::eTexWrap::ClampToEdge;

        Ren::eTexLoadStatus status;
        dummy_black_ = ctx_.LoadTexture2D("dummy_black", black, sizeof(black), p, ctx_.default_stage_bufs(),
                                          ctx_.default_mem_allocs(), &status);
        assert(status == Ren::eTexLoadStatus::CreatedFromData);

        dummy_white_ = ctx_.LoadTexture2D("dummy_white", white, sizeof(white), p, ctx_.default_stage_bufs(),
                                          ctx_.default_mem_allocs(), &status);
        assert(status == Ren::eTexLoadStatus::CreatedFromData);
    }

    { // random 2d halton 8x8
        Ren::Tex2DParams p;
        p.w = p.h = 8;
        p.format = Ren::eTexFormat::RawRG32F;
        p.usage = (Ren::eTexUsageBits::Transfer | Ren::eTexUsageBits::Sampled);

        Ren::eTexLoadStatus status;
        rand2d_8x8_ = ctx_.LoadTexture2D("rand2d_8x8", &HaltonSeq23[0][0], sizeof(HaltonSeq23), p,
                                         ctx_.default_stage_bufs(), ctx_.default_mem_allocs(), &status);
        assert(status == Ren::eTexLoadStatus::CreatedFromData);
    }

    { // random 2d directions 4x4
        Ren::Tex2DParams p;
        p.w = p.h = 4;
        p.format = Ren::eTexFormat::RawRG16;
        p.usage = (Ren::eTexUsageBits::Transfer | Ren::eTexUsageBits::Sampled);

        Ren::eTexLoadStatus status;
        rand2d_dirs_4x4_ = ctx_.LoadTexture2D("rand2d_dirs_4x4", &__rand_dirs[0], sizeof(__rand_dirs), p,
                                              ctx_.default_stage_bufs(), ctx_.default_mem_allocs(), &status);
        assert(status == Ren::eTexLoadStatus::CreatedFromData);
    }

    { // Brightness readback buffer
        readback_buf_ = ctx.LoadBuffer("Brightness Readback", Ren::eBufType::Stage,
                                       rp_sample_brightness_.res()[0] * rp_sample_brightness_.res()[1] * sizeof(float) *
                                           Ren::MaxFramesInFlight,
                                       16);
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

        Ren::Tex2DParams p;
        p.w = __cone_rt_lut_res;
        p.h = __cone_rt_lut_res;
        p.format = Ren::eTexFormat::RawRGBA8888;
        p.usage = (Ren::eTexUsageBits::Transfer | Ren::eTexUsageBits::Sampled);
        p.sampling.filter = Ren::eTexFilter::BilinearNoMipmap;
        p.sampling.wrap = Ren::eTexWrap::ClampToEdge;

        Ren::eTexLoadStatus status;
        cone_rt_lut_ = ctx_.LoadTexture2D("cone_rt_lut", &__cone_rt_lut[0], 4 * __cone_rt_lut_res * __cone_rt_lut_res,
                                          p, ctx_.default_stage_bufs(), ctx_.default_mem_allocs(), &status);

        // cone_rt_lut_ =
        //    ctx_.LoadTexture2D("cone_rt_lut", &occ_data[0], 4 * resx * resy, p,
        //    &status);
    }

    {
        // std::string c_header;
        // const std::unique_ptr<uint16_t[]> img_data_rg16 = Generate_BRDF_LUT(256,
        // c_header);

        Ren::Tex2DParams p;
        p.w = p.h = RendererInternal::__brdf_lut_res;
        p.format = Ren::eTexFormat::RawRG16;
        p.usage = (Ren::eTexUsageBits::Transfer | Ren::eTexUsageBits::Sampled);
        p.sampling.filter = Ren::eTexFilter::BilinearNoMipmap;
        p.sampling.wrap = Ren::eTexWrap::ClampToEdge;

        Ren::eTexLoadStatus status;
        brdf_lut_ = ctx_.LoadTexture2D("brdf_lut", &RendererInternal::__brdf_lut[0], sizeof(__brdf_lut), p,
                                       ctx_.default_stage_bufs(), ctx_.default_mem_allocs(), &status);
        assert(status == Ren::eTexLoadStatus::CreatedFromData);
    }

    {
        /*const int res = 128;
        std::string c_header;
        const std::unique_ptr<int8_t[]> img_data = Generate_PeriodicPerlin(res, c_header);
        SceneManagerInternal::WriteImage((const uint8_t*)&img_data[0], res, res, 4,
        false, "test1.png");*/

        Ren::Tex2DParams p;
        p.w = p.h = __noise_res;
        p.format = Ren::eTexFormat::RawRGBA8888Snorm;
        p.usage = (Ren::eTexUsageBits::Transfer | Ren::eTexUsageBits::Sampled);
        p.sampling.filter = Ren::eTexFilter::BilinearNoMipmap;

        Ren::eTexLoadStatus status;
        noise_tex_ = ctx_.LoadTexture2D("noise", &__noise[0], __noise_res * __noise_res * 4, p,
                                        ctx_.default_stage_bufs(), ctx_.default_mem_allocs(), &status);
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
        void *cmd_buf = ctx_.BegTempSingleTimeCommands();

        // Sobol sequence
        sobol_seq_buf_ = ctx_.LoadBuffer("SobolSequenceBuf", Ren::eBufType::Texture, 256 * 256 * sizeof(int));
        Ren::Buffer sobol_seq_buf_stage("SobolSequenceBufStage", ctx_.api_ctx(), Ren::eBufType::Stage,
                                        sobol_seq_buf_->size());

        { // init stage buf
            uint8_t *mapped_ptr = sobol_seq_buf_stage.Map(Ren::BufMapWrite);
            memcpy(mapped_ptr, g_sobol_256spp_256d, 256 * 256 * sizeof(int));
            sobol_seq_buf_stage.Unmap();
        }

        Ren::CopyBufferToBuffer(sobol_seq_buf_stage, 0, *sobol_seq_buf_, 0, 256 * 256 * sizeof(int), cmd_buf);

        // Scrambling tile
        scrambling_tile_1spp_buf_ =
            ctx_.LoadBuffer("ScramblingTile32SppBuf", Ren::eBufType::Texture, 128 * 128 * 8 * sizeof(int));
        Ren::Buffer scrambling_tile_buf_stage("ScramblingTile1SppBufStage", ctx_.api_ctx(), Ren::eBufType::Stage,
                                              scrambling_tile_1spp_buf_->size());

        { // init stage buf
            uint8_t *mapped_ptr = scrambling_tile_buf_stage.Map(Ren::BufMapWrite);
            memcpy(mapped_ptr, g_scrambling_tile_1spp, 128 * 128 * 8 * sizeof(int));
            scrambling_tile_buf_stage.Unmap();
        }

        Ren::CopyBufferToBuffer(scrambling_tile_buf_stage, 0, *scrambling_tile_1spp_buf_, 0,
                                128 * 128 * 8 * sizeof(int), cmd_buf);

        // Ranking tile
        ranking_tile_1spp_buf_ =
            ctx_.LoadBuffer("RankingTile32SppBuf", Ren::eBufType::Texture, 128 * 128 * 8 * sizeof(int));
        Ren::Buffer ranking_tile_buf_stage("RankingTile1SppBufStage", ctx_.api_ctx(), Ren::eBufType::Stage,
                                           ranking_tile_1spp_buf_->size());

        { // init stage buf
            uint8_t *mapped_ptr = ranking_tile_buf_stage.Map(Ren::BufMapWrite);
            memcpy(mapped_ptr, g_ranking_tile_1spp, 128 * 128 * 8 * sizeof(int));
            ranking_tile_buf_stage.Unmap();
        }

        Ren::CopyBufferToBuffer(ranking_tile_buf_stage, 0, *ranking_tile_1spp_buf_, 0, 128 * 128 * 8 * sizeof(int),
                                cmd_buf);

        ctx_.EndTempSingleTimeCommands(cmd_buf);
    }

    const auto vtx_buf1 = ctx_.default_vertex_buf1();
    const auto vtx_buf2 = ctx_.default_vertex_buf2();
    const auto ndx_buf = ctx_.default_indices_buf();

    const int buf1_stride = 16;

    { // VertexInput for main drawing (uses all attributes)
        const Ren::VtxAttribDesc attribs[] = {
            // Attributes from buffer 1
            {vtx_buf1, REN_VTX_POS_LOC, 3, Ren::eType::Float32, buf1_stride, 0},
            {vtx_buf1, REN_VTX_UV1_LOC, 2, Ren::eType::Float16, buf1_stride, 3 * sizeof(float)},
            // Attributes from buffer 2
            {vtx_buf2, REN_VTX_NOR_LOC, 4, Ren::eType::Int16SNorm, buf1_stride, 0},
            {vtx_buf2, REN_VTX_TAN_LOC, 2, Ren::eType::Int16SNorm, buf1_stride, 4 * sizeof(uint16_t)},
            {vtx_buf2, REN_VTX_AUX_LOC, 1, Ren::eType::Uint32, buf1_stride, 6 * sizeof(uint16_t)}};

        draw_pass_vi_.Setup(attribs, ndx_buf);
    }

    { // RenderPass for main drawing (compatible one)
        Ren::RenderTargetInfo color_rts[] = {
            {Ren::eTexFormat::RawRG11F_B10F, 1 /* samples */, Ren::eImageLayout::ColorAttachmentOptimal,
             Ren::eLoadOp::Load, Ren::eStoreOp::Store},
#if REN_USE_OCT_PACKED_NORMALS == 1
            {Ren::eTexFormat::RawRGB10_A2, 1 /* samples */, Ren::eImageLayout::ColorAttachmentOptimal,
             Ren::eLoadOp::Load, Ren::eStoreOp::Store},
#else
            {Ren::eTexFormat::RawRGBA8888, 1 /* samples */, Ren::eImageLayout::ColorAttachmentOptimal,
             Ren::eLoadOp::Load, Ren::eStoreOp::Store},
#endif
            {Ren::eTexFormat::RawRGBA8888, 1 /* samples */, Ren::eImageLayout::ColorAttachmentOptimal,
             Ren::eLoadOp::Load, Ren::eStoreOp::Store}
        };

        color_rts[2].flags = Ren::eTexFlagBits::SRGB;

        const auto depth_format = ctx_.capabilities.depth24_stencil8_format ? Ren::eTexFormat::Depth24Stencil8
                                                                            : Ren::eTexFormat::Depth32Stencil8;

        const Ren::RenderTargetInfo depth_rt = {depth_format, 1 /* samples */,
                                                Ren::eImageLayout::DepthStencilAttachmentOptimal, Ren::eLoadOp::Load,
                                                Ren::eStoreOp::Store};

        const bool res = rp_main_draw_.Setup(ctx_.api_ctx(), color_rts, depth_rt, ctx_.log());
        if (!res) {
            ctx_.log()->Error("Failed to initialize render pass!");
        }
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

    // Compile built-in shaders etc.
    InitPipelines();
    InitRendererInternal();

    { // shadow map texture
        Ren::Tex2DParams params;
        params.w = SHADOWMAP_WIDTH;
        params.h = SHADOWMAP_HEIGHT;
        params.format = Ren::eTexFormat::Depth16;
        params.usage = Ren::eTexUsage::RenderTarget | Ren::eTexUsage::Sampled;
        params.sampling.min_lod.from_float(0.0f);
        params.sampling.max_lod.from_float(0.0f);
        params.sampling.filter = Ren::eTexFilter::BilinearNoMipmap;
        params.sampling.wrap = Ren::eTexWrap::ClampToEdge;
        params.sampling.compare = Ren::eTexCompare::LEqual;

        Ren::eTexLoadStatus status;
        shadow_map_tex_ = ctx_.LoadTexture2D("Shadow Map", params, ctx_.default_mem_allocs(), &status);
    }

    {
        const int TEMP_BUF_SIZE = 256;

        Ren::BufferRef vtx_buf1 = ctx_.default_vertex_buf1(), vtx_buf2 = ctx_.default_vertex_buf2(),
                       ndx_buf = ctx_.default_indices_buf();

        // Allocate temporary buffer
        temp_buf1_vtx_offset_ = vtx_buf1->AllocSubRegion(TEMP_BUF_SIZE, "temp buf");
        temp_buf2_vtx_offset_ = vtx_buf2->AllocSubRegion(TEMP_BUF_SIZE, "temp buf");
        assert(temp_buf1_vtx_offset_ == temp_buf2_vtx_offset_ && "Offsets do not match!");
        temp_buf_ndx_offset_ = ndx_buf->AllocSubRegion(TEMP_BUF_SIZE, "temp buf");

        // Allocate buffer for skinned vertices
        // TODO: fix this. do not allocate twice more memory in buf2
        skinned_buf1_vtx_offset_ = vtx_buf1->AllocSubRegion(REN_MAX_SKIN_VERTICES_TOTAL * 16 * 2, "skinned");
        skinned_buf2_vtx_offset_ = vtx_buf2->AllocSubRegion(REN_MAX_SKIN_VERTICES_TOTAL * 16 * 2, "skinned");
        assert(skinned_buf1_vtx_offset_ == skinned_buf2_vtx_offset_ && "Offsets do not match!");
    }

    temp_sub_frustums_.count = REN_CELLS_COUNT;
    temp_sub_frustums_.realloc(temp_sub_frustums_.count);

    decals_boxes_.realloc(REN_MAX_DECALS_TOTAL);
    litem_to_lsource_.realloc(REN_MAX_LIGHTS_TOTAL);
    ditem_to_decal_.realloc(REN_MAX_DECALS_TOTAL);
    allocated_shadow_regions_.realloc(REN_MAX_SHADOWMAPS_TOTAL);

    for (int i = 0; i < 2; i++) {
        temp_sort_spans_32_[i].realloc(std::max(REN_MAX_SHADOW_BATCHES, REN_MAX_TEX_COUNT));
        temp_sort_spans_64_[i].realloc(REN_MAX_MAIN_BATCHES);
    }

    proc_objects_.reset(new ProcessedObjData[REN_MAX_OBJ_COUNT]);
    temp_visible_objects_.realloc(REN_MAX_OBJ_COUNT);
    temp_rt_visible_objects_.realloc(REN_MAX_OBJ_COUNT);

#if defined(USE_GL_RENDER)
    Ren::g_param_buf_binding = REN_UB_UNIF_PARAM_LOC;
#endif
}

Renderer::~Renderer() {
    prim_draw_.CleanUp();
    DestroyRendererInternal();
    swCullCtxDestroy(&cull_ctx_);
}

void Renderer::PrepareDrawList(const SceneData &scene, const Ren::Camera &cam, DrawList &list) {
    GatherDrawables(scene, cam, list);
}

void Renderer::ExecuteDrawList(const DrawList &list, const PersistentGpuData &persistent_data, const FrameBuf *target) {
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

    const bool cur_msaa_enabled = (list.render_flags & EnableMsaa) != 0;
    const bool cur_taa_enabled = (list.render_flags & EnableTaa) != 0;
    const bool cur_hq_ssr_enabled = (list.render_flags & EnableSSR_HQ) != 0;
    const bool cur_dof_enabled = (list.render_flags & EnableDOF) != 0;

    uint64_t gpu_draw_start = 0;
    if (list.render_flags & DebugTimings) {
        gpu_draw_start = GetGpuTimeBlockingUs();
    }
    const uint64_t cpu_draw_start_us = Sys::GetTimeUs();
    // Write timestamp at the beginning of execution
    backend_gpu_start_ = ctx_.WriteTimestamp(true);

    bool rendertarget_changed = false;

    if (cur_scr_w != view_state_.scr_res[0] || cur_scr_h != view_state_.scr_res[1] ||
        cur_msaa_enabled != view_state_.is_multisampled || cur_taa_enabled != taa_enabled_ ||
        cur_dof_enabled != dof_enabled_) {
        rendertarget_changed = true;

        if (cur_taa_enabled) {
            log->Info("Setting texture lod bias to -1.0");

            // TODO: Replace this with usage of sampler objects
            int counter = 0;
            ctx_.VisitTextures(Ren::eTexFlagBits::UsageScene, [&counter, this](Ren::Texture2D &tex) {
                Ren::Tex2DParams p = tex.params;
                if (p.sampling.lod_bias.to_float() > -1.0f) {
                    p.sampling.lod_bias.from_float(-1.0f);
                    tex.ApplySampling(p.sampling, ctx_.log());
                    ++counter;
                }
            });

            log->Info("Textures processed: %i", counter);
        } else {
            log->Info("Setting texture lod bias to 0.0");

            int counter = 0;
            ctx_.VisitTextures(Ren::eTexFlagBits::UsageScene, [&counter, this](Ren::Texture2D &tex) {
                Ren::Tex2DParams p = tex.params;
                if (p.sampling.lod_bias.to_float() < 0.0f) {
                    p.sampling.lod_bias.from_float(0.0f);
                    tex.ApplySampling(p.sampling, ctx_.log());
                    ++counter;
                }
            });

            log->Info("Textures processed: %i", counter);
        }
        { // Texture that holds previous frame (used for SSR)
            Ren::Tex2DParams params;
            params.w = cur_scr_w / 4;
            params.h = cur_scr_h / 4;
            params.format = Ren::eTexFormat::RawRG11F_B10F;
            params.usage = (Ren::eTexUsage::Sampled | Ren::eTexUsage::RenderTarget);
            params.sampling.filter = Ren::eTexFilter::BilinearNoMipmap;
            params.sampling.wrap = Ren::eTexWrap::ClampToEdge;

            Ren::eTexLoadStatus status;
            down_tex_4x_ = ctx_.LoadTexture2D("DOWN 4x", params, ctx_.default_mem_allocs(), &status);
            assert(status == Ren::eTexLoadStatus::CreatedDefault || status == Ren::eTexLoadStatus::Found ||
                   status == Ren::eTexLoadStatus::Reinitialized);
        }

        view_state_.scr_res = Ren::Vec2i{cur_scr_w, cur_scr_h};
        view_state_.is_multisampled = cur_msaa_enabled;
        taa_enabled_ = cur_taa_enabled;
        dof_enabled_ = cur_dof_enabled;
        log->Info("Successfully initialized framebuffers %ix%i", view_state_.scr_res[0], view_state_.scr_res[1]);
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
        view_state_.act_res[0] = target->w;
        view_state_.act_res[1] = target->h;
    }
    assert(view_state_.act_res[0] <= view_state_.scr_res[0] && view_state_.act_res[1] <= view_state_.scr_res[1]);

    view_state_.vertical_fov = list.draw_cam.angle();
    view_state_.frame_index = list.frame_index;

    if ((list.render_flags & EnableTaa) != 0) {
        Ren::Vec2f jitter = HaltonSeq23[list.frame_index % TaaSampleCount];
        jitter = (jitter * 2.0f - Ren::Vec2f{1.0f}) / Ren::Vec2f{view_state_.act_res};

        list.draw_cam.SetPxOffset(jitter);
    } else {
        list.draw_cam.SetPxOffset(Ren::Vec2f{0.0f, 0.0f});
    }

    BindlessTextureData bindless_tex;
#if defined(USE_VK_RENDER)
    bindless_tex.textures_descr_sets = &persistent_data.textures_descr_sets[ctx_.backend_frame()];
    bindless_tex.rt_textures_descr_set = persistent_data.rt_textures_descr_sets[ctx_.backend_frame()];
    bindless_tex.rt_inline_textures_descr_set = persistent_data.rt_inline_textures_descr_sets[ctx_.backend_frame()];
#elif defined(USE_GL_RENDER)
    bindless_tex.textures_buf = persistent_data.textures_buf;
#endif

    const bool deferred_shading =
        (list.render_flags & EnableDeferred) != 0 && (list.render_flags & DebugWireframe) == 0;
    const bool env_map_changed = (env_map_ != list.env.env_map);
    const bool lm_tex_changed =
        lm_direct_ != list.env.lm_direct || lm_indir_ != list.env.lm_indir ||
        !std::equal(std::begin(list.env.lm_indir_sh), std::end(list.env.lm_indir_sh), std::begin(lm_indir_sh_));
    const bool probe_storage_changed = (list.probe_storage != probe_storage_);
    const bool rebuild_renderpasses = (list.render_flags != cached_render_flags_) || probe_storage_changed ||
                                      !rp_builder_.ready() || rendertarget_changed || env_map_changed || lm_tex_changed;

    cached_render_flags_ = list.render_flags;
    env_map_ = list.env.env_map;
    lm_direct_ = list.env.lm_direct;
    lm_indir_ = list.env.lm_indir;
    std::copy(std::begin(list.env.lm_indir_sh), std::end(list.env.lm_indir_sh), std::begin(lm_indir_sh_));
    p_list_ = &list;
    probe_storage_ = list.probe_storage;

    if (rebuild_renderpasses) {
        const uint64_t rp_setup_beg_us = Sys::GetTimeUs();

        rp_builder_.Reset();
        backbuffer_sources_.clear();

        auto &common_buffers = *rp_builder_.AllocPassData<CommonBuffers>();
        AddBuffersUpdatePass(common_buffers);
        AddLightBuffersUpdatePass(common_buffers);

        {
            auto &skinning = rp_builder_.AddPass("SKINNING");

            RpResRef skin_vtx_res =
                skinning.AddStorageReadonlyInput(ctx_.default_skin_vertex_buf(), Ren::eStageBits::ComputeShader);
            RpResRef in_skin_transforms_res =
                skinning.AddStorageReadonlyInput(common_buffers.skin_transforms_res, Ren::eStageBits::ComputeShader);
            RpResRef in_shape_keys_res =
                skinning.AddStorageReadonlyInput(common_buffers.shape_keys_res, Ren::eStageBits::ComputeShader);
            RpResRef delta_buf_res =
                skinning.AddStorageReadonlyInput(ctx_.default_delta_buf(), Ren::eStageBits::ComputeShader);

            RpResRef vtx_buf1_res =
                skinning.AddStorageOutput(ctx_.default_vertex_buf1(), Ren::eStageBits::ComputeShader);
            RpResRef vtx_buf2_res =
                skinning.AddStorageOutput(ctx_.default_vertex_buf2(), Ren::eStageBits::ComputeShader);

            skinning.make_executor<RpSkinningExecutor>(pi_skinning_, p_list_, skin_vtx_res, in_skin_transforms_res,
                                                       in_shape_keys_res, delta_buf_res, vtx_buf1_res, vtx_buf2_res);
        }

        //
        // RT acceleration structures
        //
        auto &acc_struct_data = *rp_builder_.AllocPassData<AccelerationStructureData>();
        acc_struct_data.rt_instance_buf = persistent_data.rt_instance_buf;
        acc_struct_data.rt_geo_data_buf = persistent_data.rt_geo_data_buf;
        acc_struct_data.rt_tlas_buf = persistent_data.rt_tlas_buf;
        acc_struct_data.rt_sh_tlas_buf = persistent_data.rt_sh_tlas_buf;
        acc_struct_data.hwrt.rt_tlas_build_scratch_size = persistent_data.rt_tlas_build_scratch_size;
        if (persistent_data.rt_tlas) {
            acc_struct_data.rt_tlases[int(eTLASIndex::Main)] = persistent_data.rt_tlas.get();
        }
        if (persistent_data.rt_sh_tlas) {
            acc_struct_data.rt_tlases[int(eTLASIndex::Shadow)] = persistent_data.rt_sh_tlas.get();
        }

        RpResRef rt_obj_instances_res, rt_sh_obj_instances_res;

        if (ctx_.capabilities.raytracing) {
            auto &update_rt_bufs = rp_builder_.AddPass("UPDATE ACC BUFS");

            { // create obj instances buffer
                RpBufDesc desc;
                desc.type = Ren::eBufType::Storage;
                desc.size = HWRTObjInstancesBufChunkSize;

                rt_obj_instances_res = update_rt_bufs.AddTransferOutput("RT Obj Instances", desc);
            }

            update_rt_bufs.make_executor<RpUpdateAccBuffersExecutor>(p_list_, 0, rt_obj_instances_res);

            auto &build_acc_structs = rp_builder_.AddPass("BUILD ACC STRCTS");

            rt_obj_instances_res = build_acc_structs.AddASBuildReadonlyInput(rt_obj_instances_res);
            RpResRef rt_tlas_res = build_acc_structs.AddASBuildOutput(acc_struct_data.rt_tlas_buf);

            RpResRef rt_tlas_build_scratch_res;

            { // create scratch buffer
                RpBufDesc desc;
                desc.type = Ren::eBufType::Storage;
                desc.size = acc_struct_data.hwrt.rt_tlas_build_scratch_size;
                rt_tlas_build_scratch_res = build_acc_structs.AddASBuildOutput("TLAS Scratch Buf", desc);
            }

            build_acc_structs.make_executor<RpBuildAccStructuresExecutor>(
                p_list_, 0, rt_obj_instances_res, &acc_struct_data, rt_tlas_res, rt_tlas_build_scratch_res);
        } else if (ctx_.capabilities.swrt && acc_struct_data.rt_tlas_buf) {
            auto &build_acc_structs = rp_builder_.AddPass("BUILD ACC STRCTS");

            { // create obj instances buffer
                RpBufDesc desc;
                desc.type = Ren::eBufType::Storage;
                desc.size = SWRTObjInstancesBufChunkSize;

                rt_obj_instances_res = build_acc_structs.AddTransferOutput("RT Obj Instances", desc);
            }

            RpResRef rt_tlas_res = build_acc_structs.AddTransferOutput(acc_struct_data.rt_tlas_buf);

            build_acc_structs.make_executor<RpBuildAccStructuresExecutor>(p_list_, 0, rt_obj_instances_res,
                                                                          &acc_struct_data, rt_tlas_res, RpResRef{});
        }

        if (deferred_shading && (list.render_flags & EnableRTShadows)) {
            if (ctx_.capabilities.raytracing) {
                auto &update_rt_bufs = rp_builder_.AddPass("UPDATE SH ACC BUFS");

                RpResRef rt_sh_obj_instances_res;

                { // create obj instances buffer
                    RpBufDesc desc;
                    desc.type = Ren::eBufType::Storage;
                    desc.size = HWRTObjInstancesBufChunkSize;

                    rt_sh_obj_instances_res = update_rt_bufs.AddTransferOutput("RT SH Obj Instances", desc);
                }

                update_rt_bufs.make_executor<RpUpdateAccBuffersExecutor>(p_list_, 1, rt_sh_obj_instances_res);

                ////

                auto &build_acc_structs = rp_builder_.AddPass("SH ACC STRCTS");
                rt_sh_obj_instances_res = build_acc_structs.AddASBuildReadonlyInput(rt_sh_obj_instances_res);
                RpResRef rt_sh_tlas_res = build_acc_structs.AddASBuildOutput(acc_struct_data.rt_sh_tlas_buf);

                RpResRef rt_sh_tlas_build_scratch_res;

                { // create scratch buffer
                    RpBufDesc desc;
                    desc.type = Ren::eBufType::Storage;
                    desc.size = acc_struct_data.hwrt.rt_tlas_build_scratch_size;
                    rt_sh_tlas_build_scratch_res = build_acc_structs.AddASBuildOutput("SH TLAS Scratch Buf", desc);
                }

                build_acc_structs.make_executor<RpBuildAccStructuresExecutor>(p_list_, 1, rt_sh_obj_instances_res,
                                                                              &acc_struct_data, rt_sh_tlas_res,
                                                                              rt_sh_tlas_build_scratch_res);
            } else if (ctx_.capabilities.swrt && acc_struct_data.rt_sh_tlas_buf) {
                auto &build_acc_structs = rp_builder_.AddPass("BUILD ACC STRCTS");

                { // create obj instances buffer
                    RpBufDesc desc;
                    desc.type = Ren::eBufType::Storage;
                    desc.size = SWRTObjInstancesBufChunkSize;

                    rt_sh_obj_instances_res = build_acc_structs.AddTransferOutput("RT SH Obj Instances", desc);
                }

                RpResRef rt_sh_tlas_res = build_acc_structs.AddTransferOutput(acc_struct_data.rt_sh_tlas_buf);

                build_acc_structs.make_executor<RpBuildAccStructuresExecutor>(
                    p_list_, 1, rt_sh_obj_instances_res, &acc_struct_data, rt_sh_tlas_res, RpResRef{});
            }
        }

        auto &frame_textures = *rp_builder_.AllocPassData<FrameTextures>();

        { // Shadow maps
            auto &shadow_maps = rp_builder_.AddPass("SHADOW MAPS");
            RpResRef vtx_buf1_res = shadow_maps.AddVertexBufferInput(ctx_.default_vertex_buf1());
            RpResRef vtx_buf2_res = shadow_maps.AddVertexBufferInput(ctx_.default_vertex_buf2());
            RpResRef ndx_buf_res = shadow_maps.AddIndexBufferInput(ctx_.default_indices_buf());

            RpResRef shared_data_res = shadow_maps.AddUniformBufferInput(
                common_buffers.shared_data_res, Ren::eStageBits::VertexShader | Ren::eStageBits::FragmentShader);
            RpResRef instances_res = shadow_maps.AddStorageReadonlyInput(
                persistent_data.instance_buf, persistent_data.instance_buf_tbo, Ren::eStageBits::VertexShader);
            RpResRef instance_indices_res =
                shadow_maps.AddStorageReadonlyInput(common_buffers.instance_indices_res, Ren::eStageBits::VertexShader);

            RpResRef materials_buf_res =
                shadow_maps.AddStorageReadonlyInput(persistent_data.materials_buf, Ren::eStageBits::VertexShader);
#if defined(USE_GL_RENDER)
            RpResRef textures_buf_res =
                shadow_maps.AddStorageReadonlyInput(bindless_tex.textures_buf, Ren::eStageBits::VertexShader);
#else
            RpResRef textures_buf_res;
#endif
            RpResRef noise_tex_res = shadow_maps.AddTextureInput(noise_tex_, Ren::eStageBits::VertexShader);

            frame_textures.shadowmap = shadow_maps.AddDepthOutput(shadow_map_tex_);

            rp_shadow_maps_.Setup(&p_list_, vtx_buf1_res, vtx_buf2_res, ndx_buf_res, materials_buf_res, &bindless_tex,
                                  textures_buf_res, instances_res, instance_indices_res, shared_data_res, noise_tex_res,
                                  frame_textures.shadowmap);
            shadow_maps.set_executor(&rp_shadow_maps_);
        }

        frame_textures.depth_params.w = view_state_.scr_res[0];
        frame_textures.depth_params.h = view_state_.scr_res[1];
        frame_textures.depth_params.format = ctx_.capabilities.depth24_stencil8_format
                                                 ? Ren::eTexFormat::Depth24Stencil8
                                                 : Ren::eTexFormat::Depth32Stencil8;
        frame_textures.depth_params.sampling.wrap = Ren::eTexWrap::ClampToEdge;
        frame_textures.depth_params.samples = view_state_.is_multisampled ? 4 : 1;

        // Main HDR color
        frame_textures.color_params.w = view_state_.scr_res[0];
        frame_textures.color_params.h = view_state_.scr_res[1];
#if (REN_OIT_MODE == REN_OIT_WEIGHTED_BLENDED) || (REN_OIT_MODE == REN_OIT_MOMENT_BASED && REN_OIT_MOMENT_RENORMALIZE)
        // renormalization requires buffer with alpha channel
        frame_textures.color_params.format = Ren::eTexFormat::RawRGBA16F;
#else
        frame_textures.color_params.format = Ren::eTexFormat::RawRG11F_B10F;
#endif
        frame_textures.color_params.sampling.wrap = Ren::eTexWrap::ClampToEdge;
        frame_textures.color_params.samples = view_state_.is_multisampled ? 4 : 1;

        // 4-component specular (alpha is roughness)
        frame_textures.specular_params.w = view_state_.scr_res[0];
        frame_textures.specular_params.h = view_state_.scr_res[1];
        frame_textures.specular_params.format = Ren::eTexFormat::RawRGBA8888;
        frame_textures.specular_params.flags = Ren::eTexFlagBits::SRGB;
        frame_textures.specular_params.sampling.wrap = Ren::eTexWrap::ClampToEdge;
        frame_textures.specular_params.samples = view_state_.is_multisampled ? 4 : 1;

        // 4-component world-space normal (alpha or z is roughness)
        frame_textures.normal_params.w = view_state_.scr_res[0];
        frame_textures.normal_params.h = view_state_.scr_res[1];
#if REN_USE_OCT_PACKED_NORMALS == 1
        frame_textures.normal_params.format = Ren::eTexFormat::RawRGB10_A2;
#else
        frame_textures.normal_params.format = Ren::eTexFormat::RawRGBA8888;
#endif
        frame_textures.normal_params.sampling.wrap = Ren::eTexWrap::ClampToEdge;
        frame_textures.normal_params.samples = view_state_.is_multisampled ? 4 : 1;

        // 4-component albedo (alpha is unused)
        frame_textures.albedo_params.w = view_state_.scr_res[0];
        frame_textures.albedo_params.h = view_state_.scr_res[1];
        frame_textures.albedo_params.format = Ren::eTexFormat::RawRGBA8888;
        frame_textures.albedo_params.flags = Ren::eTexFlagBits::SRGB;
        frame_textures.albedo_params.sampling.wrap = Ren::eTexWrap::ClampToEdge;

        if (!deferred_shading) {
            // Skydome drawing
            AddSkydomePass(common_buffers, true /* clear */, frame_textures);
        }

        //
        // Depth prepass
        //
        if ((list.render_flags & (EnableZFill | DebugWireframe)) == EnableZFill) {
            auto &depth_fill = rp_builder_.AddPass("DEPTH FILL");

            RpResRef vtx_buf1 = depth_fill.AddVertexBufferInput(ctx_.default_vertex_buf1());
            RpResRef vtx_buf2 = depth_fill.AddVertexBufferInput(ctx_.default_vertex_buf2());
            RpResRef ndx_buf = depth_fill.AddIndexBufferInput(ctx_.default_indices_buf());

            RpResRef shared_data_res = depth_fill.AddUniformBufferInput(
                common_buffers.shared_data_res, Ren::eStageBits::VertexShader | Ren::eStageBits::FragmentShader);
            RpResRef instances_res = depth_fill.AddStorageReadonlyInput(
                persistent_data.instance_buf, persistent_data.instance_buf_tbo, Ren::eStageBits::VertexShader);
            RpResRef instance_indices_res =
                depth_fill.AddStorageReadonlyInput(common_buffers.instance_indices_res, Ren::eStageBits::VertexShader);

            RpResRef materials_buf_res =
                depth_fill.AddStorageReadonlyInput(persistent_data.materials_buf, Ren::eStageBits::VertexShader);
#if defined(USE_GL_RENDER)
            RpResRef textures_buf_res =
                depth_fill.AddStorageReadonlyInput(bindless_tex.textures_buf, Ren::eStageBits::VertexShader);
#else
            RpResRef textures_buf_res;
#endif
            RpResRef noise_tex_res = depth_fill.AddTextureInput(noise_tex_, Ren::eStageBits::VertexShader);

            frame_textures.depth = depth_fill.AddDepthOutput(MAIN_DEPTH_TEX, frame_textures.depth_params);

            { // Texture that holds 2D velocity
                Ren::Tex2DParams params;
                params.w = view_state_.scr_res[0];
                params.h = view_state_.scr_res[1];
                params.format = Ren::eTexFormat::RawRG16Snorm;
                params.sampling.wrap = Ren::eTexWrap::ClampToEdge;
                params.samples = view_state_.is_multisampled ? 4 : 1;

                frame_textures.velocity = depth_fill.AddColorOutput(MAIN_VELOCITY_TEX, params);
            }

            rp_depth_fill_.Setup(&p_list_, &view_state_, deferred_shading /* clear_depth */, vtx_buf1, vtx_buf2,
                                 ndx_buf, materials_buf_res, textures_buf_res, &bindless_tex, instances_res,
                                 instance_indices_res, shared_data_res, noise_tex_res, frame_textures.depth,
                                 frame_textures.velocity);
            depth_fill.set_executor(&rp_depth_fill_);
        }

        //
        // Downsample depth
        //
        RpResRef depth_down_2x, depth_hierarchy_tex;

        if ((list.render_flags & EnableZFill) && (list.render_flags & (EnableSSAO | EnableSSR)) &&
            ((list.render_flags & DebugWireframe) == 0)) {
            // TODO: get rid of this (or use on low spec only)
            AddDownsampleDepthPass(common_buffers, frame_textures.depth, depth_down_2x);

            auto &depth_hierarchy = rp_builder_.AddPass("DEPTH HIERARCHY");
            const RpResRef depth_tex =
                depth_hierarchy.AddTextureInput(frame_textures.depth, Ren::eStageBits::ComputeShader);
            const RpResRef atomic_buf =
                depth_hierarchy.AddStorageOutput(common_buffers.atomic_cnt_res, Ren::eStageBits::ComputeShader);

            { // 32-bit float depth hierarchy
                Ren::Tex2DParams params;
                params.w = ((view_state_.scr_res[0] + RpDepthHierarchy::TileSize - 1) / RpDepthHierarchy::TileSize) *
                           RpDepthHierarchy::TileSize;
                params.h = ((view_state_.scr_res[1] + RpDepthHierarchy::TileSize - 1) / RpDepthHierarchy::TileSize) *
                           RpDepthHierarchy::TileSize;
                params.format = Ren::eTexFormat::RawR32F;
                params.mip_count = RpDepthHierarchy::MipCount;
                params.sampling.wrap = Ren::eTexWrap::ClampToEdge;
                params.sampling.filter = Ren::eTexFilter::NearestMipmap;

                depth_hierarchy_tex =
                    depth_hierarchy.AddStorageImageOutput(DEPTH_HIERARCHY_TEX, params, Ren::eStageBits::ComputeShader);
            }

            rp_depth_hierarchy_.Setup(rp_builder_, &view_state_, depth_tex, atomic_buf, depth_hierarchy_tex);
            depth_hierarchy.set_executor(&rp_depth_hierarchy_);
        }

        //
        // Ambient occlusion
        //
        const uint64_t use_ssao_mask = (EnableZFill | EnableSSAO | DebugWireframe);
        const uint64_t use_ssao = (EnableZFill | EnableSSAO);
        if ((list.render_flags & use_ssao_mask) == use_ssao) {
            AddSSAOPasses(depth_down_2x, frame_textures.depth, frame_textures.ssao);
        } else {
            frame_textures.ssao = rp_builder_.MakeTextureResource(dummy_white_);
        }

        const uint64_t fill_velocity_mask = (EnableTaa | EnableSSR_HQ | DebugWireframe);
        const uint64_t fill_velocity =
            (EnableTaa | EnableSSR_HQ); // Temporal reprojection is used for reflections and TAA
        if ((list.render_flags & fill_velocity_mask) == fill_velocity) {
            AddFillStaticVelocityPass(common_buffers, frame_textures.depth, frame_textures.velocity);
        }

        if (deferred_shading) {
            // GBuffer filling pass
            AddGBufferFillPass(common_buffers, persistent_data, bindless_tex, frame_textures);

            if ((ctx_.capabilities.raytracing || ctx_.capabilities.swrt) && (list.render_flags & EnableRTShadows)) {
                // RT Sun shadows
                AddHQSunShadowsPasses(common_buffers, persistent_data, acc_struct_data, bindless_tex,
                                      rt_sh_obj_instances_res, frame_textures,
                                      (list.render_flags & DebugShadowDenoise) != 0);
            } else {
                AddLQSunShadowsPasses(common_buffers, persistent_data, acc_struct_data, bindless_tex, frame_textures);
            }

            // GI
            AddDiffusePasses(list.env.env_map, lm_direct_, lm_indir_sh_, (list.render_flags & DebugGIDenoise) != 0,
                             list.probe_storage, common_buffers, persistent_data, acc_struct_data, bindless_tex,
                             depth_hierarchy_tex, frame_textures);

            // GBuffer shading pass
            AddDeferredShadingPass(common_buffers, frame_textures, (list.render_flags & EnableGI));

            // Additional forward pass (for custom-shaded objects)
            AddForwardOpaquePass(common_buffers, persistent_data, bindless_tex, frame_textures);

            // Skydome drawing
            AddSkydomePass(common_buffers, false /* clear */, frame_textures);
        } else {
            AddForwardOpaquePass(common_buffers, persistent_data, bindless_tex, frame_textures);
        }

        if (deferred_shading) {
            // TODO: OIT
        } else {
            // Simple transparent pass
            AddForwardTransparentPass(common_buffers, persistent_data, bindless_tex, frame_textures);
        }

        //
        // Reflections pass
        //
        if ((list.render_flags & EnableSSR) != 0 && (list.render_flags & DebugWireframe) == 0) {
            const char *refl_out_name = view_state_.is_multisampled ? RESOLVED_COLOR_TEX : MAIN_COLOR_TEX;
            if (cur_hq_ssr_enabled) {
                AddHQSpecularPasses(list.env.env_map, lm_direct_, lm_indir_sh_,
                                    (list.render_flags & DebugReflDenoise) != 0, list.probe_storage, common_buffers,
                                    persistent_data, acc_struct_data, bindless_tex, depth_hierarchy_tex,
                                    rt_obj_instances_res, frame_textures);
            } else {
                AddLQSpecularPasses(list.probe_storage, common_buffers, depth_down_2x, frame_textures);
            }
        }

        //
        // Debug geometry
        //
        // if (list.render_flags & DebugProbes) {
        //    rp_debug_probes_.Setup(rp_builder_, list, &view_state_, SHARED_DATA_BUF, refl_out_name);
        //    rp_tail->p_next = &rp_debug_probes_;
        //    rp_tail = rp_tail->p_next;
        //}

        // if (list.render_flags & DebugEllipsoids) {
        //     rp_debug_ellipsoids_.Setup(rp_builder_, list, &view_state_, SHARED_DATA_BUF, refl_out_name);
        //     rp_tail->p_next = &rp_debug_ellipsoids_;
        //     rp_tail = rp_tail->p_next;
        // }

        if ((list.render_flags & (DebugRT | DebugRTShadow)) && list.env.env_map &&
            (ctx_.capabilities.raytracing || ctx_.capabilities.swrt)) {
            auto &debug_rt = rp_builder_.AddPass("DEBUG RT");

            const Ren::eStageBits stages =
                ctx_.capabilities.raytracing ? Ren::eStageBits::RayTracingShader : Ren::eStageBits::ComputeShader;

            auto *data = debug_rt.AllocPassData<RpDebugRTData>();
            data->shared_data = debug_rt.AddUniformBufferInput(common_buffers.shared_data_res, stages);
            data->geo_data_buf = debug_rt.AddStorageReadonlyInput(acc_struct_data.rt_geo_data_buf, stages);
            data->materials_buf = debug_rt.AddStorageReadonlyInput(persistent_data.materials_buf, stages);
            data->vtx_buf1 = debug_rt.AddStorageReadonlyInput(ctx_.default_vertex_buf1(), stages);
            data->vtx_buf2 = debug_rt.AddStorageReadonlyInput(ctx_.default_vertex_buf2(), stages);
            data->ndx_buf = debug_rt.AddStorageReadonlyInput(ctx_.default_indices_buf(), stages);

            if (!ctx_.capabilities.raytracing) {
                data->swrt.root_node = persistent_data.swrt.rt_root_node;
                data->swrt.rt_blas_buf = debug_rt.AddStorageReadonlyInput(persistent_data.rt_blas_buf, stages);
                data->swrt.prim_ndx_buf =
                    debug_rt.AddStorageReadonlyInput(persistent_data.swrt.rt_prim_indices_buf, stages);
                data->swrt.meshes_buf = debug_rt.AddStorageReadonlyInput(persistent_data.swrt.rt_meshes_buf, stages);
                data->swrt.mesh_instances_buf = debug_rt.AddStorageReadonlyInput(rt_obj_instances_res, stages);
                data->swrt.rt_tlas_buf = debug_rt.AddStorageReadonlyInput(persistent_data.rt_tlas_buf, stages);

#if defined(USE_GL_RENDER)
                data->swrt.textures_buf = debug_rt.AddStorageReadonlyInput(bindless_tex.textures_buf, stages);
#endif
            }

            data->env_tex = debug_rt.AddTextureInput(list.env.env_map, stages);

            if (list.env.lm_direct) {
                data->lm_tex[0] = debug_rt.AddTextureInput(list.env.lm_direct, stages);
            }
            for (int i = 0; i < 4; ++i) {
                if (list.env.lm_indir_sh[i]) {
                    data->lm_tex[i + 1] = debug_rt.AddTextureInput(list.env.lm_indir_sh[i], stages);
                }
            }

            data->dummy_black = debug_rt.AddTextureInput(dummy_black_, stages);

            frame_textures.color = data->output_tex = debug_rt.AddStorageImageOutput(frame_textures.color, stages);

            const Ren::IAccStructure *tlas_to_debug = acc_struct_data.rt_tlases[int(eTLASIndex::Main)];
            if (list.render_flags & DebugRTShadow) {
                tlas_to_debug = acc_struct_data.rt_tlases[int(eTLASIndex::Shadow)];
            }

            rp_debug_rt_.Setup(rp_builder_, &view_state_, tlas_to_debug, &bindless_tex, data);
            debug_rt.set_executor(&rp_debug_rt_);
        }

        RpResRef resolved_color;

        //
        // Temporal resolve
        //
        const uint64_t use_taa_mask = (EnableTaa | DebugWireframe);
        const uint64_t use_taa = EnableTaa;
        if ((list.render_flags & use_taa_mask) == use_taa) {
            AddTaaPass(common_buffers, frame_textures, list.draw_cam.max_exposure, resolved_color);
        } else {
            resolved_color = frame_textures.color;
        }

        //
        // Color downsampling
        //
        if ((list.render_flags & (EnableSSR | EnableBloom | EnableTonemap | EnableDOF)) &&
            ((list.render_flags & DebugWireframe) == 0)) {
            RpResRef downsampled_res = rp_builder_.MakeTextureResource(down_tex_4x_);
            AddDownsampleColorPass(resolved_color, downsampled_res);
        }

#if defined(USE_GL_RENDER) && 0 // gl-only for now
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

            rp_dof_.Setup(rp_builder_, &list.draw_cam, &view_state_, SHARED_DATA_BUF, color_in_name, MAIN_DEPTH_TEX,
                          DEPTH_DOWN_2X_TEX, DEPTH_DOWN_4X_TEX, down_tex_4x_, dof_out_name);
            rp_tail->p_next = &rp_dof_;
            rp_tail = rp_tail->p_next;
        }
#endif

        //
        // Blur
        //
        RpResRef blur_tex;
        if ((list.render_flags & (EnableSSR | EnableBloom | EnableTonemap)) &&
            ((list.render_flags & DebugWireframe) == 0)) {
            AddFrameBlurPasses(down_tex_4x_, blur_tex);
        }

        //
        // Sample brightness
        //
        RpResRef exposure_tex; // fake for now
        if (list.render_flags & EnableTonemap) {
            RpResRef lum_tex;
            { // Sample brightness
                auto &lum_sample = rp_builder_.AddPass("LUM SAMPLE");

                auto *data = lum_sample.AllocPassData<RpSampleBrightnessData>();
                data->input_tex = lum_sample.AddTextureInput(down_tex_4x_, Ren::eStageBits::FragmentShader);
                { // aux buffer which gathers frame luminance
                    Ren::Tex2DParams params;
                    params.w = 16;
                    params.h = 8;
                    params.format = Ren::eTexFormat::RawR32F;
                    params.sampling.filter = Ren::eTexFilter::BilinearNoMipmap;
                    params.sampling.wrap = Ren::eTexWrap::ClampToEdge;

                    lum_tex = data->reduced_tex = lum_sample.AddColorOutput(REDUCED_TEX, params);
                }

                rp_sample_brightness_.Setup(data);
                lum_sample.set_executor(&rp_sample_brightness_);
            }
            { // Readback brightness
                auto &lum_read = rp_builder_.AddPass("LUM READBACK");

                auto *data = lum_read.AllocPassData<RpReadBrightnessData>();
                data->input_tex = lum_read.AddTransferImageInput(lum_tex);
                data->output_buf = lum_read.AddTransferOutput(readback_buf_);

                { // 1px exposure texture (fake for now)
                    Ren::Tex2DParams params;
                    params.w = 1;
                    params.h = 1;
                    params.format = Ren::eTexFormat::RawR32F;
                    params.sampling.filter = Ren::eTexFilter::BilinearNoMipmap;
                    params.sampling.wrap = Ren::eTexWrap::ClampToEdge;

                    exposure_tex = data->exposure_tex = lum_read.AddColorOutput("Exposure Tex", params);
                }

                rp_read_brightness_.Setup(data);
                lum_read.set_executor(&rp_read_brightness_);
            }
        }

        //
        // Debugging
        //
        if (list.render_flags & DebugMotionVectors) {
            AddDebugVelocityPass(frame_textures.velocity, resolved_color);
            blur_tex = {};
        }

        bool apply_dof = false;

        //
        // Combine with blurred and tonemap
        //
        {
            RpResRef color_tex;
            const char *output_tex = nullptr;

            if (cur_msaa_enabled || ((list.render_flags & EnableTaa) != 0 && !(list.render_flags & DebugWireframe)) ||
                apply_dof) {
                if (apply_dof) {
                    if ((list.render_flags & EnableTaa) != 0) {
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

            if ((list.render_flags & EnableFxaa) && !(list.render_flags & DebugWireframe)) {
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

            float gamma = 1.0f;
            if ((list.render_flags & EnableTonemap) && !(list.render_flags & DebugLights)) {
                gamma = 2.2f;
            }

            const bool tonemap = (list.render_flags & EnableTonemap);
            const float reduced_average = rp_read_brightness_.reduced_average();

            float exposure = list.draw_cam.max_exposure;
            if (list.draw_cam.autoexposure) {
                exposure = reduced_average > std::numeric_limits<float>::epsilon() ? (1.0f / reduced_average) : 1.0f;
                exposure = std::min(exposure, list.draw_cam.max_exposure);
            }

            // TODO: Remove this condition
            if (list.env.env_map) {
                auto &combine = rp_builder_.AddPass("COMBINE");

                rp_combine_data_.color_tex = combine.AddTextureInput(color_tex, Ren::eStageBits::FragmentShader);
                if (list.render_flags & EnableBloom) {
                    rp_combine_data_.blur_tex = combine.AddTextureInput(blur_tex, Ren::eStageBits::FragmentShader);
                } else {
                    rp_combine_data_.blur_tex = combine.AddTextureInput(dummy_black_, Ren::eStageBits::FragmentShader);
                }
                rp_combine_data_.exposure_tex = combine.AddTextureInput(exposure_tex, Ren::eStageBits::FragmentShader);
                if (output_tex) {
                    Ren::Tex2DParams params;
                    params.w = view_state_.scr_res[0];
                    params.h = view_state_.scr_res[1];
                    params.format = Ren::eTexFormat::RawRGB888;
                    params.sampling.filter = Ren::eTexFilter::BilinearNoMipmap;
                    params.sampling.wrap = Ren::eTexWrap::ClampToEdge;

                    rp_combine_data_.output_tex = combine.AddColorOutput(output_tex, params);
                } else {
                    rp_combine_data_.output_tex = combine.AddColorOutput(ctx_.backbuffer_ref());
                }

                rp_combine_data_.tonemap = tonemap;
                rp_combine_data_.gamma = gamma;
                rp_combine_data_.exposure = exposure;
                rp_combine_data_.fade = list.draw_cam.fade;

                backbuffer_sources_.push_back(rp_combine_data_.output_tex);

                rp_combine_.Setup(&view_state_, &rp_combine_data_);
                combine.set_executor(&rp_combine_);
            }
        }

#if defined(USE_GL_RENDER) && 0 // gl-only for now
        //
        // FXAA
        //
        if ((list.render_flags & EnableFxaa) && !(list.render_flags & DebugWireframe)) {
            // Ren::TexHandle output_tex =
            //    target ? target->attachments[0].tex->handle() : Ren::TexHandle{};

            rp_fxaa_.Setup(rp_builder_, &view_state_, SHARED_DATA_BUF, MAIN_COMBINED_TEX, nullptr);
            rp_tail->p_next = &rp_fxaa_;
            rp_tail = rp_tail->p_next;
        }

        //
        // Debugging (draw auxiliary surfaces)
        //
        {
            const Ren::WeakTex2DRef output_tex =
                target ? Ren::WeakTex2DRef{target->attachments[0].tex} : Ren::WeakTex2DRef{};
            rp_debug_textures_.Setup(rp_builder_, &view_state_, list, down_tex_4x_, SHARED_DATA_BUF, CELLS_BUF,
                                     ITEMS_BUF, SHADOWMAP_TEX, deferred_shading ? MAIN_ALBEDO_TEX : MAIN_COLOR_TEX,
                                     MAIN_NORMAL_TEX, MAIN_SPEC_TEX, MAIN_DEPTH_TEX, SSAO_RES, BLUR_RES_TEX,
                                     REDUCED_TEX, output_tex);
            rp_tail->p_next = &rp_debug_textures_;
            rp_tail = rp_tail->p_next;
        }
#endif

        rp_builder_.Compile(backbuffer_sources_.data(), int(backbuffer_sources_.size()));
        const uint64_t rp_setup_end_us = Sys::GetTimeUs();
        ctx_.log()->Info("Renderpass setup is done in %.2fms", (rp_setup_end_us - rp_setup_beg_us) * 0.001);
    } else {
        assert(!(list.render_flags & EnableFxaa));
        // Use correct backbuffer image (assume topology is not changed)
        // TODO: get rid of this
        auto &combine = *rp_builder_.FindPass("COMBINE");
        rp_combine_data_.output_tex = combine.ReplaceColorOutput(0, ctx_.backbuffer_ref());

        const float reduced_average = rp_read_brightness_.reduced_average();

        float exposure = list.draw_cam.max_exposure;
        if (list.draw_cam.autoexposure) {
            exposure = reduced_average > std::numeric_limits<float>::epsilon() ? (1.0f / reduced_average) : 1.0f;
            exposure = std::min(exposure, list.draw_cam.max_exposure);
        }

        rp_combine_data_.exposure = exposure;
        rp_combine_data_.fade = list.draw_cam.fade;

        rp_combine_.Setup(&view_state_, &rp_combine_data_);
        combine.set_executor(&rp_combine_);
    }

    rp_builder_.Execute();

    { // store matrix to use it in next frame
        view_state_.down_buf_view_from_world = list.draw_cam.view_matrix();
        view_state_.prev_cam_pos = list.draw_cam.world_position();

        Ren::Mat4f view_matrix_no_translation = list.draw_cam.view_matrix();
        view_matrix_no_translation[3][0] = view_matrix_no_translation[3][1] = view_matrix_no_translation[3][2] = 0;

        view_state_.prev_clip_from_world_no_translation = list.draw_cam.proj_matrix() * view_matrix_no_translation;
        view_state_.prev_clip_from_view = list.draw_cam.proj_matrix_offset();
    }

    const uint64_t cpu_draw_end_us = Sys::GetTimeUs();

    // store values for current frame
    backend_cpu_start_ = cpu_draw_start_us;
    backend_cpu_end_ = cpu_draw_end_us;
    backend_time_diff_ = int64_t(gpu_draw_start) - int64_t(backend_cpu_start_);

    // Write timestamp at the end of execution
    backend_gpu_end_ = ctx_.WriteTimestamp(false);

    __itt_task_end(__g_itt_domain);
}

void Renderer::InitBackendInfo() {
    if (frame_index_ < 10) {
        // Skip a few initial frames
        return;
    }

    backend_info_.pass_timings.clear();
    for (auto &t : rp_builder_.pass_timings_[ctx_.backend_frame()]) {
        PassTiming &new_t = backend_info_.pass_timings.emplace_back();
        strncpy(new_t.name, t.name.c_str(), sizeof(new_t.name));
        new_t.duration = ctx_.GetTimestampIntervalDuration(t.query_beg, t.query_end);
    }

    backend_info_.cpu_start_timepoint_us = backend_cpu_start_;
    backend_info_.cpu_end_timepoint_us = backend_cpu_end_;

    backend_info_.gpu_total_duration = 0;
    if (backend_gpu_start_ != -1 && backend_gpu_end_ != -1) {
        backend_info_.gpu_total_duration = ctx_.GetTimestampIntervalDuration(backend_gpu_start_, backend_gpu_end_);
    }
}

void Renderer::InitPipelinesForProgram(const Ren::ProgramRef &prog, const uint32_t mat_flags,
                                       Ren::PipelineStorage &storage,
                                       Ren::SmallVectorImpl<Ren::PipelineRef> &out_pipelines) const {
    for (int i = 0; i < int(eFwdPipeline::_Count); ++i) {
        Ren::RastState rast_state = rast_states_[i];

        if (uint32_t(Ren::eMatFlags::TwoSided)) {
            rast_state.poly.cull = uint8_t(Ren::eCullFace::None);
        }

        if (mat_flags & uint32_t(Ren::eMatFlags::AlphaBlend) && i != int(eFwdPipeline::Wireframe)) {
            rast_state.depth.compare_op = unsigned(Ren::eCompareOp::Less);

            rast_state.blend.enabled = true;
            rast_state.blend.src = unsigned(Ren::eBlendFactor::SrcAlpha);
            rast_state.blend.dst = unsigned(Ren::eBlendFactor::OneMinusSrcAlpha);
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
                new_pipeline.Init(ctx_.api_ctx(), rast_state, prog, &draw_pass_vi_, &rp_main_draw_, 0, ctx_.log());
            if (!res) {
                ctx_.log()->Error("Failed to initialize pipeline!");
            }
        }

        out_pipelines.emplace_back(&storage, new_index);
    }
}

#undef BBOX_POINTS
