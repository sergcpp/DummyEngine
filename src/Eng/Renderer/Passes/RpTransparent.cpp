#include "RpTransparent.h"

#include "../../Utils/ShaderLoader.h"
#include "../Renderer_Structs.h"

void RpTransparent::Setup(RpBuilder &builder, const DrawList &list,
                          const int *alpha_blend_start_index, const ViewState *view_state,
                          int orphan_index, Ren::TexHandle color_tex,
                          Ren::TexHandle normal_tex, Ren::TexHandle spec_tex,
                          Ren::TexHandle depth_tex, Ren::TexHandle transparent_tex,
                          Ren::TexHandle moments_b0, Ren::TexHandle moments_z_and_z2,
                          Ren::TexHandle moments_z3_and_z4,
                          Ren::TexHandle shadow_tex,
                          Ren::TexHandle ssao_tex, Ren::Tex2DRef brdf_lut,
                          Ren::Tex2DRef noise_tex, Ren::Tex2DRef cone_rt_lut) {
    orphan_index_ = orphan_index;

    color_tex_ = color_tex;
    normal_tex_ = normal_tex;
    spec_tex_ = spec_tex;
    depth_tex_ = depth_tex;
    transparent_tex_ = transparent_tex;
    moments_b0_ = moments_b0;
    moments_z_and_z2_ = moments_z_and_z2;
    moments_z3_and_z4_ = moments_z3_and_z4;
    view_state_ = view_state;

    brdf_lut_ = std::move(brdf_lut);
    noise_tex_ = std::move(noise_tex);
    cone_rt_lut_ = std::move(cone_rt_lut);

    shadow_tex_ = shadow_tex;
    ssao_tex_ = ssao_tex;
    env_ = &list.env;
    decals_atlas_ = list.decals_atlas;
    probe_storage_ = list.probe_storage;

    render_flags_ = list.render_flags;
    main_batches_ = list.main_batches;
    main_batch_indices_ = list.main_batch_indices;
    alpha_blend_start_index_ = alpha_blend_start_index;

    input_[0] = builder.ReadBuffer(INSTANCES_BUF);
    input_[1] = builder.ReadBuffer(SHARED_DATA_BUF);
    input_[2] = builder.ReadBuffer(CELLS_BUF);
    input_[3] = builder.ReadBuffer(ITEMS_BUF);
    input_[4] = builder.ReadBuffer(LIGHTS_BUF);
    input_[5] = builder.ReadBuffer(DECALS_BUF);
    input_count_ = 6;

    // output_[0] = builder.WriteBuffer(input_[0], *this);
    output_count_ = 0;
}

void RpTransparent::Execute(RpBuilder &builder) {
    LazyInit(builder.ctx(), builder.sh());
    DrawTransparent(builder);
}

void RpTransparent::DrawTransparent(RpBuilder &builder) {
    RpAllocBuf &instances_buf = builder.GetReadBuffer(input_[0]);
    RpAllocBuf &unif_shared_data_buf = builder.GetReadBuffer(input_[1]);
    RpAllocBuf &cells_buf = builder.GetReadBuffer(input_[2]);
    RpAllocBuf &items_buf = builder.GetReadBuffer(input_[3]);
    RpAllocBuf &lights_buf = builder.GetReadBuffer(input_[4]);
    RpAllocBuf &decals_buf = builder.GetReadBuffer(input_[5]);

    if (alpha_blend_start_index_ == nullptr) {
        return;
    }

#if (REN_OIT_MODE == REN_OIT_MOMENT_BASED)
    DrawTransparent_Moments(builder);
#elif (REN_OIT_MODE == REN_OIT_WEIGHTED_BLENDED)
#else
    DrawTransparent_Simple(builder, instances_buf, unif_shared_data_buf, cells_buf,
                           items_buf, lights_buf, decals_buf);
#endif
}

void RpTransparent::LazyInit(Ren::Context &ctx, ShaderLoader &sh) {
    if (!initialized) {
        blit_ms_resolve_prog_ =
            sh.LoadProgram(ctx, "blit_ms_resolve", "internal/blit.vert.glsl",
                           "internal/blit_ms_resolve.frag.glsl");
        assert(blit_ms_resolve_prog_->ready());

        ////////////////////////////////////////

        static const uint8_t black[] = {0, 0, 0, 0}, white[] = {255, 255, 255, 255};

        Ren::Tex2DParams p;
        p.w = p.h = 1;
        p.format = Ren::eTexFormat::RawRGBA8888;
        p.filter = Ren::eTexFilter::NoFilter;
        p.repeat = Ren::eTexRepeat::ClampToEdge;

        Ren::eTexLoadStatus status;
        dummy_black_ = ctx.LoadTexture2D("dummy_black", black, sizeof(black), p, &status);
        assert(status == Ren::eTexLoadStatus::TexCreatedFromData ||
               status == Ren::eTexLoadStatus::TexFound);

        dummy_white_ = ctx.LoadTexture2D("dummy_white", white, sizeof(white), p, &status);
        assert(status == Ren::eTexLoadStatus::TexCreatedFromData ||
               status == Ren::eTexLoadStatus::TexFound);

        initialized = true;
    }

    Ren::BufHandle vtx_buf1 = ctx.default_vertex_buf1()->handle(),
                   vtx_buf2 = ctx.default_vertex_buf2()->handle(),
                   ndx_buf = ctx.default_indices_buf()->handle();

    const int buf1_stride = 16, buf2_stride = 16;

    { // VAO for main drawing (uses all attributes)
        const Ren::VtxAttribDesc attribs[] = {
            // Attributes from buffer 1
            {vtx_buf1, REN_VTX_POS_LOC, 3, Ren::eType::Float32, buf1_stride, 0},
            {vtx_buf1, REN_VTX_UV1_LOC, 2, Ren::eType::Float16, buf1_stride,
             uintptr_t(3 * sizeof(float))},
            // Attributes from buffer 2
            {vtx_buf2, REN_VTX_NOR_LOC, 4, Ren::eType::Int16SNorm, buf1_stride, 0},
            {vtx_buf2, REN_VTX_TAN_LOC, 2, Ren::eType::Int16SNorm, buf1_stride,
             uintptr_t(4 * sizeof(uint16_t))},
            {vtx_buf2, REN_VTX_AUX_LOC, 1, Ren::eType::Uint32, buf1_stride,
             uintptr_t(6 * sizeof(uint16_t))}};

        draw_pass_vao_.Setup(attribs, 5, ndx_buf);
    }

    {
        const Ren::TexHandle attachments[] = {color_tex_, normal_tex_, spec_tex_};
        if (!transparent_draw_fb_.Setup(attachments, 3, depth_tex_, depth_tex_,
                                        view_state_->is_multisampled)) {
            ctx.log()->Error("RpTransparent: transparent_draw_fb_ init failed!");
        }
    }

    if (!color_only_fb_.Setup(&color_tex_, 1, depth_tex_, depth_tex_,
                              view_state_->is_multisampled)) {
        ctx.log()->Error("RpTransparent: color_only_fb_ init failed!");
    }

    if (!resolved_fb_.Setup(&transparent_tex_, 1, {}, {}, false)) {
        ctx.log()->Error("RpTransparent: resolved_fb_ init failed!");
    }

    if (moments_b0_.id && moments_z_and_z2_.id && moments_z3_and_z4_.id) {
        const Ren::TexHandle attachments[] = {moments_b0_, moments_z_and_z2_,
                                              moments_z3_and_z4_};
        if (!moments_fb_.Setup(attachments, 3, depth_tex_, {},
                               view_state_->is_multisampled)) {
            ctx.log()->Error("RpTransparent: moments_fb_ init failed!");
        }
    }
}