#include "RpTransparent.h"

#include <Ren/Context.h>

#include "../../Utils/ShaderLoader.h"
#include "../Renderer_Structs.h"

void RpTransparent::Setup(RpBuilder &builder, const DrawList &list,
                          const int *alpha_blend_start_index, const ViewState *view_state,
                          const PersistentBuffers *bufs, int orphan_index,
                          Ren::Tex2DRef brdf_lut, Ren::Tex2DRef noise_tex,
                          Ren::Tex2DRef cone_rt_lut, const char instances_buf[],
                          const char shared_data_buf[], const char cells_buf[],
                          const char items_buf[], const char lights_buf[],
                          const char decals_buf[], const char shadowmap_tex[],
                          const char ssao_tex[], const char color_tex[],
                          const char normal_tex[], const char spec_tex[],
                          const char depth_tex[], const char transparent_tex_name[]) {
    orphan_index_ = orphan_index;
    view_state_ = view_state;

    brdf_lut_ = std::move(brdf_lut);
    noise_tex_ = std::move(noise_tex);
    cone_rt_lut_ = std::move(cone_rt_lut);

    env_ = &list.env;
    materials_ = list.materials;
    decals_atlas_ = list.decals_atlas;
    probe_storage_ = list.probe_storage;

    render_flags_ = list.render_flags;
    main_batches_ = list.main_batches;
    main_batch_indices_ = list.main_batch_indices;
    alpha_blend_start_index_ = alpha_blend_start_index;
    bufs_ = bufs;

    instances_buf_ = builder.ReadBuffer(instances_buf, *this);
    shared_data_buf_ = builder.ReadBuffer(shared_data_buf, *this);
    cells_buf_ = builder.ReadBuffer(cells_buf, *this);
    items_buf_ = builder.ReadBuffer(items_buf, *this);
    lights_buf_ = builder.ReadBuffer(lights_buf, *this);
    decals_buf_ = builder.ReadBuffer(decals_buf, *this);

    shadowmap_tex_ = builder.ReadTexture(shadowmap_tex, *this);
    ssao_tex_ = builder.ReadTexture(ssao_tex, *this);
    color_tex_ = builder.WriteTexture(color_tex, *this);
    normal_tex_ = builder.WriteTexture(normal_tex, *this);
    spec_tex_ = builder.WriteTexture(spec_tex, *this);
    depth_tex_ = builder.WriteTexture(depth_tex, *this);

    {
        Ren::Tex2DParams params;
        params.w = view_state->scr_res[0];
        params.h = view_state->scr_res[1];
        params.format = Ren::eTexFormat::RawRG11F_B10F;
        params.sampling.filter = Ren::eTexFilter::BilinearNoMipmap;
        params.sampling.repeat = Ren::eTexRepeat::ClampToEdge;

        transparent_tex_ = builder.WriteTexture(transparent_tex_name, params, *this);
    }
}

void RpTransparent::Execute(RpBuilder &builder) {
    RpAllocTex &color_tex = builder.GetWriteTexture(color_tex_);
    RpAllocTex &normal_tex = builder.GetWriteTexture(normal_tex_);
    RpAllocTex &spec_tex = builder.GetWriteTexture(spec_tex_);
    RpAllocTex &depth_tex = builder.GetWriteTexture(depth_tex_);
    RpAllocTex &transparent_tex = builder.GetWriteTexture(transparent_tex_);

    LazyInit(builder.ctx(), builder.sh(), color_tex, normal_tex, spec_tex, depth_tex,
             transparent_tex);
    DrawTransparent(builder, color_tex);
}

void RpTransparent::DrawTransparent(RpBuilder &builder, RpAllocTex &color_tex) {
    RpAllocBuf &instances_buf = builder.GetReadBuffer(instances_buf_);
    RpAllocBuf &unif_shared_data_buf = builder.GetReadBuffer(shared_data_buf_);
    RpAllocBuf &cells_buf = builder.GetReadBuffer(cells_buf_);
    RpAllocBuf &items_buf = builder.GetReadBuffer(items_buf_);
    RpAllocBuf &lights_buf = builder.GetReadBuffer(lights_buf_);
    RpAllocBuf &decals_buf = builder.GetReadBuffer(decals_buf_);

    RpAllocTex &shadowmap_tex = builder.GetReadTexture(shadowmap_tex_);
    RpAllocTex &ssao_tex = builder.GetReadTexture(ssao_tex_);

    if (alpha_blend_start_index_ == nullptr) {
        return;
    }

#if (REN_OIT_MODE == REN_OIT_MOMENT_BASED)
    DrawTransparent_Moments(builder);
#elif (REN_OIT_MODE == REN_OIT_WEIGHTED_BLENDED)
#else
    DrawTransparent_Simple(builder, instances_buf, unif_shared_data_buf, cells_buf,
                           items_buf, lights_buf, decals_buf, shadowmap_tex, color_tex,
                           ssao_tex);
#endif
}

void RpTransparent::LazyInit(Ren::Context &ctx, ShaderLoader &sh, RpAllocTex &color_tex,
                             RpAllocTex &normal_tex, RpAllocTex &spec_tex,
                             RpAllocTex &depth_tex, RpAllocTex &transparent_tex) {
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
        p.sampling.repeat = Ren::eTexRepeat::ClampToEdge;

        Ren::eTexLoadStatus status;
        dummy_black_ = ctx.LoadTexture2D("dummy_black", black, sizeof(black), p, &status);
        assert(status == Ren::eTexLoadStatus::CreatedFromData ||
               status == Ren::eTexLoadStatus::Found);

        dummy_white_ = ctx.LoadTexture2D("dummy_white", white, sizeof(white), p, &status);
        assert(status == Ren::eTexLoadStatus::CreatedFromData ||
               status == Ren::eTexLoadStatus::Found);

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
        const Ren::TexHandle attachments[] = {
            color_tex.ref->handle(), normal_tex.ref->handle(), spec_tex.ref->handle()};
        if (!transparent_draw_fb_.Setup(attachments, 3, depth_tex.ref->handle(),
                                        depth_tex.ref->handle(),
                                        view_state_->is_multisampled)) {
            ctx.log()->Error("RpTransparent: transparent_draw_fb_ init failed!");
        }
    }

    if (!color_only_fb_.Setup(color_tex.ref->handle(), depth_tex.ref->handle(),
                              depth_tex.ref->handle(), view_state_->is_multisampled)) {
        ctx.log()->Error("RpTransparent: color_only_fb_ init failed!");
    }

    if (!resolved_fb_.Setup(transparent_tex.ref->handle(), {}, {}, false)) {
        ctx.log()->Error("RpTransparent: resolved_fb_ init failed!");
    }

    /*if (moments_b0_.id && moments_z_and_z2_.id && moments_z3_and_z4_.id) {
        const Ren::TexHandle attachments[] = {moments_b0_, moments_z_and_z2_,
                                              moments_z3_and_z4_};
        if (!moments_fb_.Setup(attachments, 3, depth_tex.ref->handle(), {},
                               view_state_->is_multisampled)) {
            ctx.log()->Error("RpTransparent: moments_fb_ init failed!");
        }
    }*/
}