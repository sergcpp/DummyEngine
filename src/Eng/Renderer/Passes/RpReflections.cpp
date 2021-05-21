#include "RpReflections.h"

#include <Ren/Context.h>
#include <Ren/Program.h>
#include <Ren/RastState.h>

#include "../PrimDraw.h"
#include "../Renderer_Structs.h"
#include "../../Scene/ProbeStorage.h"
#include "../../Utils/ShaderLoader.h"

void RpReflections::Setup(RpBuilder &builder, const ViewState *view_state,
                          int orphan_index, const ProbeStorage *probe_storage,
                          Ren::TexHandle down_buf_4x_tex, Ren::Tex2DRef brdf_lut,
                          const char shared_data_buf[], const char cells_buf[],
                          const char items_buf[], const char depth_tex[], const char normal_tex[],
                          const char spec_tex[], const char depth_down_2x[],
                          const char output_tex_name[]) {
    view_state_ = view_state;
    orphan_index_ = orphan_index;

    down_buf_4x_tex_ = down_buf_4x_tex;
    brdf_lut_ = brdf_lut;

    probe_storage_ = probe_storage;

    shared_data_buf_ = builder.ReadBuffer(shared_data_buf, *this);
    cells_buf_ = builder.ReadBuffer(cells_buf, *this);
    items_buf_ = builder.ReadBuffer(items_buf, *this);

    depth_tex_ = builder.ReadTexture(depth_tex, *this);
    normal_tex_ = builder.ReadTexture(normal_tex, *this);
    spec_tex_ = builder.ReadTexture(spec_tex, *this);
    depth_down_2x_tex_ = builder.ReadTexture(depth_down_2x, *this);

    { // Auxilary texture for reflections (rg - uvs, b - influence)
        Ren::Tex2DParams params;
        params.w = view_state->scr_res[0] / 2;
        params.h = view_state->scr_res[1] / 2;
        params.format = Ren::eTexFormat::RawRGB10_A2;
        params.sampling.filter = Ren::eTexFilter::BilinearNoMipmap;
        params.sampling.repeat = Ren::eTexRepeat::ClampToEdge;

        ssr1_tex_ = builder.WriteTexture("SSR Temp 1", params, *this);
        ssr2_tex_ = builder.WriteTexture("SSR Temp 2", params, *this);
    }
    output_tex_ = builder.WriteTexture(output_tex_name, *this);
}

void RpReflections::Execute(RpBuilder &builder) {
    RpAllocBuf &unif_sh_data_buf = builder.GetReadBuffer(shared_data_buf_);
    RpAllocBuf &cells_buf = builder.GetReadBuffer(cells_buf_);
    RpAllocBuf &items_buf = builder.GetReadBuffer(items_buf_);
    RpAllocTex &depth_tex = builder.GetReadTexture(depth_tex_);
    RpAllocTex &normal_tex = builder.GetReadTexture(normal_tex_);
    RpAllocTex &spec_tex = builder.GetReadTexture(spec_tex_);
    RpAllocTex &depth_down_2x_tex = builder.GetReadTexture(depth_down_2x_tex_);
    RpAllocTex &ssr1_tex = builder.GetWriteTexture(ssr1_tex_);
    RpAllocTex &ssr2_tex = builder.GetWriteTexture(ssr2_tex_);
    RpAllocTex &output_tex = builder.GetWriteTexture(output_tex_);

    LazyInit(builder.ctx(), builder.sh(), ssr1_tex, ssr2_tex, output_tex);

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
            {Ren::eBindTarget::Tex2D, REN_REFL_DEPTH_TEX_SLOT,
             depth_down_2x_tex.ref->handle()},
            {clean_buf_bind_target, REN_REFL_NORM_TEX_SLOT, normal_tex.ref->handle()},
            {clean_buf_bind_target, REN_REFL_SPEC_TEX_SLOT, spec_tex.ref->handle()},
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
            {Ren::eBindTarget::Tex2D, REN_BASE0_TEX_SLOT, ssr1_tex.ref->handle()}};

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
            {clean_buf_bind_target, REN_REFL_SPEC_TEX_SLOT, spec_tex.ref->handle()},
            {clean_buf_bind_target, REN_REFL_DEPTH_TEX_SLOT, depth_tex.ref->handle()},
            {clean_buf_bind_target, REN_REFL_NORM_TEX_SLOT, normal_tex.ref->handle()},
            //
            {Ren::eBindTarget::Tex2D, REN_REFL_DEPTH_LOW_TEX_SLOT,
             depth_down_2x_tex.ref->handle()},
            {Ren::eBindTarget::Tex2D, REN_REFL_SSR_TEX_SLOT, ssr2_tex.ref->handle()},
            //
            {Ren::eBindTarget::Tex2D, REN_REFL_PREV_TEX_SLOT, down_buf_4x_tex_},
            {Ren::eBindTarget::Tex2D, REN_REFL_BRDF_TEX_SLOT, brdf_lut_->handle()},
            //
            {Ren::eBindTarget::TexBuf, REN_CELLS_BUF_SLOT,
             cells_buf.tbos[orphan_index_]->handle()},
            {Ren::eBindTarget::TexBuf, REN_ITEMS_BUF_SLOT,
             items_buf.tbos[orphan_index_]->handle()},
            {Ren::eBindTarget::TexCubeArray, REN_ENV_TEX_SLOT,
             probe_storage_ ? probe_storage_->handle() : Ren::TexHandle{}},
            {Ren::eBindTarget::UBuf, REN_UB_SHARED_DATA_LOC,
             orphan_index_ * SharedDataBlockSize, sizeof(SharedDataBlock),
             unif_sh_data_buf.ref->handle()}};

        const PrimDraw::Uniform uniforms[] = {{0, Ren::Vec4f{0.0f, 0.0f, 1.0f, 1.0f}}};

        prim_draw_.DrawPrim(PrimDraw::ePrim::Quad, {output_fb_.id(), 0},
                            blit_ssr_compose_prog, bindings,
                            sizeof(bindings) / sizeof(bindings[0]), uniforms, 1);
    }
}

void RpReflections::LazyInit(Ren::Context &ctx, ShaderLoader &sh, RpAllocTex &ssr1_tex,
                             RpAllocTex &ssr2_tex, RpAllocTex &output_tex) {
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

    if (!ssr_buf1_fb_.Setup(ssr1_tex.ref->handle(), {}, {}, false)) {
        ctx.log()->Error("RpReflections: ssr_buf1_fb_ init failed!");
    }

    if (!ssr_buf2_fb_.Setup(ssr2_tex.ref->handle(), {}, {}, false)) {
        ctx.log()->Error("RpReflections: ssr_buf2_fb_ init failed!");
    }

    if (!output_fb_.Setup(output_tex.ref->handle(), {}, {}, false)) {
        ctx.log()->Error("RpReflections: output_fb_ init failed!");
    }
}
