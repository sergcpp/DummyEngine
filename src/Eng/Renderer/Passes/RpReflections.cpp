#include "RpReflections.h"

#include "../../Utils/ShaderLoader.h"
#include "../Renderer_Structs.h"

void RpReflections::Setup(RpBuilder &builder, const ViewState *view_state,
                          int orphan_index, const ProbeStorage *probe_storage,
                          Ren::TexHandle depth_tex, Ren::TexHandle norm_tex,
                          Ren::TexHandle spec_tex, Ren::TexHandle down_depth_2x_tex,
                          Ren::TexHandle down_buf_4x_tex, Ren::TexHandle ssr_buf1_tex,
                          Ren::TexHandle ssr_buf2_tex, Ren::Tex2DRef brdf_lut,
                          Ren::TexHandle output_tex) {
    view_state_ = view_state;
    orphan_index_ = orphan_index;
    depth_tex_ = depth_tex;
    norm_tex_ = norm_tex;
    spec_tex_ = spec_tex;
    down_depth_2x_tex_ = down_depth_2x_tex;
    down_buf_4x_tex_ = down_buf_4x_tex;
    ssr_buf1_tex_ = ssr_buf1_tex;
    ssr_buf2_tex_ = ssr_buf2_tex;
    brdf_lut_ = brdf_lut;

    probe_storage_ = probe_storage;

    output_tex_ = output_tex;

    input_[0] = builder.ReadBuffer(SHARED_DATA_BUF);
    input_[1] = builder.ReadBuffer(CELLS_BUF);
    input_[2] = builder.ReadBuffer(ITEMS_BUF);
    input_count_ = 3;

    // output_[0] = builder.WriteBuffer(input_[0], *this);
    output_count_ = 0;
}

void RpReflections::Execute(RpBuilder &builder) {
    LazyInit(builder.ctx(), builder.sh());

    RpAllocBuf &unif_sh_data_buf = builder.GetReadBuffer(input_[0]);
    RpAllocBuf &cells_buf = builder.GetReadBuffer(input_[1]);
    RpAllocBuf &items_buf = builder.GetReadBuffer(input_[2]);

    Ren::RastState rast_state;
    rast_state.depth_test.enabled = false;
    rast_state.depth_mask = false;
    rast_state.cull_face.enabled = true;

    rast_state.viewport[2] = view_state_->scr_res[0] / 2;
    rast_state.viewport[3] = view_state_->scr_res[1] / 2;

    rast_state.Apply();
    Ren::RastState applied_state = rast_state;

    const Ren::eBindTarget clean_buf_bind_target = view_state_->is_multisampled
                                                       ? Ren::eBindTarget::Tex2DMs
                                                       : Ren::eBindTarget::Tex2D;

    { // screen space tracing
        Ren::Program *ssr_program = nullptr;
        if (view_state_->is_multisampled) {
            ssr_program = blit_ssr_ms_prog_.get();
        } else {
            ssr_program = blit_ssr_prog_.get();
        }

        const PrimDraw::Binding bindings[] = {
            {Ren::eBindTarget::Tex2D, REN_REFL_DEPTH_TEX_SLOT, down_depth_2x_tex_},
            {clean_buf_bind_target, REN_REFL_NORM_TEX_SLOT, norm_tex_},
            {clean_buf_bind_target, REN_REFL_SPEC_TEX_SLOT, spec_tex_},
            {Ren::eBindTarget::UBuf, REN_UB_SHARED_DATA_LOC,
             orphan_index_ * SharedDataBlockSize, sizeof(SharedDataBlock),
             unif_sh_data_buf.ref->handle()}};

        const PrimDraw::Uniform uniforms[] = {
            {0, Ren::Vec4f{0.0f, 0.0f, float(view_state_->act_res[0]),
                           float(view_state_->act_res[1])}}};

        prim_draw_.DrawPrim(PrimDraw::ePrim::Quad, {ssr_buf1_fb_.id(), 0}, ssr_program,
                            bindings, 4, uniforms, 1);
    }

    { // dilate ssr buffer
        Ren::Program *dilate_prog = blit_ssr_dilate_prog_.get();

        const PrimDraw::Binding bindings[] = {
            {Ren::eBindTarget::Tex2D, REN_BASE0_TEX_SLOT, ssr_buf1_tex_}};

        const PrimDraw::Uniform uniforms[] = {
            {0, Ren::Vec4f{0.0f, 0.0f, float(view_state_->scr_res[0]) / 2.0f,
                           float(view_state_->scr_res[1]) / 2.0f}}};

        prim_draw_.DrawPrim(PrimDraw::ePrim::Quad, {ssr_buf2_fb_.id(), 0}, dilate_prog,
                            bindings, 1, uniforms, 1);
    }

    rast_state.viewport[2] = view_state_->scr_res[0];
    rast_state.viewport[3] = view_state_->scr_res[1];

    rast_state.ApplyChanged(applied_state);
    applied_state = rast_state;

    { // compose reflections on top of clean buffer
        Ren::Program *blit_ssr_compose_prog = view_state_->is_multisampled
                                                  ? blit_ssr_compose_ms_prog_.get()
                                                  : blit_ssr_compose_prog_.get();

        rast_state.blend.enabled = true;
        rast_state.blend.src = Ren::eBlendFactor::One;
        rast_state.blend.dst = Ren::eBlendFactor::One;
        rast_state.ApplyChanged(applied_state);
        applied_state = rast_state;

        const PrimDraw::Binding bindings[] = {
            {clean_buf_bind_target, REN_REFL_SPEC_TEX_SLOT, spec_tex_},
            {clean_buf_bind_target, REN_REFL_DEPTH_TEX_SLOT, depth_tex_},
            {clean_buf_bind_target, REN_REFL_NORM_TEX_SLOT, norm_tex_},
            //
            {Ren::eBindTarget::Tex2D, REN_REFL_DEPTH_LOW_TEX_SLOT, down_depth_2x_tex_},
            {Ren::eBindTarget::Tex2D, REN_REFL_SSR_TEX_SLOT, ssr_buf2_tex_},
            //
            {Ren::eBindTarget::Tex2D, REN_REFL_PREV_TEX_SLOT, down_buf_4x_tex_},
            {Ren::eBindTarget::Tex2D, REN_REFL_BRDF_TEX_SLOT, brdf_lut_->handle()},
            //
            {Ren::eBindTarget::TexBuf, REN_CELLS_BUF_SLOT,
             cells_buf.tbos[orphan_index_]->handle()},
            {Ren::eBindTarget::TexBuf, REN_ITEMS_BUF_SLOT,
             items_buf.tbos[orphan_index_]->handle()},
            {Ren::eBindTarget::TexCubeArray, REN_ENV_TEX_SLOT, probe_storage_->handle()},
            {Ren::eBindTarget::UBuf, REN_UB_SHARED_DATA_LOC,
             orphan_index_ * SharedDataBlockSize, sizeof(SharedDataBlock),
             unif_sh_data_buf.ref->handle()}};

        const PrimDraw::Uniform uniforms[] = {{0, Ren::Vec4f{0.0f, 0.0f, 1.0f, 1.0f}}};

        prim_draw_.DrawPrim(PrimDraw::ePrim::Quad, {output_fb_.id(), 0},
                            blit_ssr_compose_prog, bindings,
                            sizeof(bindings) / sizeof(bindings[0]), uniforms, 1);
    }
}

void RpReflections::LazyInit(Ren::Context &ctx, ShaderLoader &sh) {
    if (!initialized) {
        blit_ssr_prog_ = sh.LoadProgram(ctx, "blit_ssr", "internal/blit.vert.glsl",
                                        "internal/blit_ssr.frag.glsl");
        assert(blit_ssr_prog_->ready());
        blit_ssr_ms_prog_ = sh.LoadProgram(ctx, "blit_ssr_ms", "internal/blit.vert.glsl",
                                           "internal/blit_ssr.frag.glsl@MSAA_4");
        assert(blit_ssr_ms_prog_->ready());
        blit_ssr_compose_prog_ =
            sh.LoadProgram(ctx, "blit_ssr_compose", "internal/blit.vert.glsl",
                           "internal/blit_ssr_compose.frag.glsl");
        assert(blit_ssr_compose_prog_->ready());
        blit_ssr_compose_ms_prog_ =
            sh.LoadProgram(ctx, "blit_ssr_compose_ms", "internal/blit.vert.glsl",
                           "internal/blit_ssr_compose.frag.glsl@MSAA_4");
        assert(blit_ssr_compose_ms_prog_->ready());
        blit_ssr_dilate_prog_ =
            sh.LoadProgram(ctx, "blit_ssr_dilate", "internal/blit.vert.glsl",
                           "internal/blit_ssr_dilate.frag.glsl");
        assert(blit_ssr_dilate_prog_->ready());

        initialized = true;
    }

    if (!ssr_buf1_fb_.Setup(&ssr_buf1_tex_, 1, {}, {}, false)) {
        ctx.log()->Error("RpReflections: ssr_buf1_fb_ init failed!");
    }

    if (!ssr_buf2_fb_.Setup(&ssr_buf2_tex_, 1, {}, {}, false)) {
        ctx.log()->Error("RpReflections: ssr_buf2_fb_ init failed!");
    }

    if (!output_fb_.Setup(&output_tex_, 1, {}, {}, false)) {
        ctx.log()->Error("RpReflections: output_fb_ init failed!");
    }
}
