#include "RpSSAO.h"

#include <Ren/Context.h>
#include <Ren/Program.h>
#include <Ren/RastState.h>

#include "../../Utils/ShaderLoader.h"
#include "../PrimDraw.h"
#include "../Renderer_Structs.h"

void RpSSAO::Setup(RpBuilder &builder, const ViewState *view_state,
                   const int orphan_index, Ren::TexHandle rand2d_dirs_4x4_tex,
                   const char shared_data_buf[], const char depth_down_2x[],
                   const char depth_tex[], const char output_tex[]) {
    view_state_ = view_state;
    orphan_index_ = orphan_index;
    rand2d_dirs_4x4_tex_ = rand2d_dirs_4x4_tex;

    shared_data_buf_ = builder.ReadBuffer(shared_data_buf, *this);

    depth_down_2x_tex_ = builder.ReadTexture(depth_down_2x, *this);
    depth_tex_ = builder.ReadTexture(depth_tex, *this);

    { // Allocate temporary textures
        Ren::Tex2DParams params;
        params.w = view_state->scr_res[0] / 2;
        params.h = view_state->scr_res[1] / 2;
        params.format = Ren::eTexFormat::RawR8;
        params.filter = Ren::eTexFilter::BilinearNoMipmap;
        params.repeat = Ren::eTexRepeat::ClampToEdge;

        ssao1_tex_ = builder.WriteTexture("SSAO Temp 1", params, *this);
        ssao2_tex_ = builder.WriteTexture("SSAO Temp 2", params, *this);
    }
    { // Allocate output texture
        Ren::Tex2DParams params;
        params.w = view_state->scr_res[0];
        params.h = view_state->scr_res[1];
        params.format = Ren::eTexFormat::RawR8;
        params.filter = Ren::eTexFilter::BilinearNoMipmap;
        params.repeat = Ren::eTexRepeat::ClampToEdge;

        output_tex_ = builder.WriteTexture(output_tex, params, *this);
    }
}

void RpSSAO::Execute(RpBuilder &builder) {
    RpAllocBuf &unif_shared_data_buf = builder.GetReadBuffer(shared_data_buf_);
    RpAllocTex &down_depth_2x_tex = builder.GetReadTexture(depth_down_2x_tex_);
    RpAllocTex &depth_tex = builder.GetReadTexture(depth_tex_);
    RpAllocTex &ssao1_tex = builder.GetWriteTexture(ssao1_tex_);
    RpAllocTex &ssao2_tex = builder.GetWriteTexture(ssao2_tex_);
    RpAllocTex &output_tex = builder.GetWriteTexture(output_tex_);

    LazyInit(builder.ctx(), builder.sh(), ssao1_tex, ssao2_tex, output_tex);

    Ren::RastState rast_state;
    rast_state.cull_face.enabled = true;

    rast_state.viewport[2] = view_state_->act_res[0] / 2;
    rast_state.viewport[3] = view_state_->act_res[1] / 2;

    rast_state.Apply();
    Ren::RastState applied_state = rast_state;

    { // prepare ao buffer
        const PrimDraw::Binding bindings[] = {
            {Ren::eBindTarget::Tex2D, REN_BASE0_TEX_SLOT,
             down_depth_2x_tex.ref->handle()},
            {Ren::eBindTarget::Tex2D, REN_BASE1_TEX_SLOT, rand2d_dirs_4x4_tex_},
            {Ren::eBindTarget::UBuf, REN_UB_SHARED_DATA_LOC,
             orphan_index_ * SharedDataBlockSize, sizeof(SharedDataBlock),
             unif_shared_data_buf.ref->handle()}};

        const PrimDraw::Uniform uniforms[] = {{0, Ren::Vec4f{applied_state.viewport}}};

        prim_draw_.DrawPrim(PrimDraw::ePrim::Quad, {ssao_buf1_fb_.id(), 0},
                            blit_ao_prog_.get(), bindings, 3, uniforms, 1);
    }

    { // blur ao buffer
        PrimDraw::Binding bindings[] = {
            {Ren::eBindTarget::Tex2D, 0, down_depth_2x_tex.ref->handle()},
            {Ren::eBindTarget::Tex2D, 1, ssao1_tex.ref->handle()}};

        PrimDraw::Uniform uniforms[] = {{0, Ren::Vec4f{applied_state.viewport}},
                                        {3, 0.0f}};

        prim_draw_.DrawPrim(PrimDraw::ePrim::Quad, {ssao_buf2_fb_.id(), 0},
                            blit_bilateral_prog_.get(), bindings, 2, uniforms, 2);

        bindings[1] = {Ren::eBindTarget::Tex2D, 1, ssao2_tex.ref->handle()};

        uniforms[0] = {0, Ren::Vec4f{applied_state.viewport}};
        uniforms[1] = {3, 1.0f};

        prim_draw_.DrawPrim(PrimDraw::ePrim::Quad, {ssao_buf1_fb_.id(), 0},
                            blit_bilateral_prog_.get(), bindings, 2, uniforms, 2);
    }

    rast_state.viewport[2] = view_state_->act_res[0];
    rast_state.viewport[3] = view_state_->act_res[1];

    rast_state.ApplyChanged(applied_state);
    applied_state = rast_state;

    { // upsample ao
        PrimDraw::Binding bindings[] = {
            {Ren::eBindTarget::Tex2D, 0, depth_tex.ref->handle()},
            {Ren::eBindTarget::Tex2D, 1, down_depth_2x_tex.ref->handle()},
            {Ren::eBindTarget::Tex2D, 2, ssao1_tex.ref->handle()},
            {Ren::eBindTarget::UBuf, REN_UB_SHARED_DATA_LOC,
             orphan_index_ * SharedDataBlockSize, sizeof(SharedDataBlock),
             unif_shared_data_buf.ref->handle()}};

        Ren::Program *blit_upscale_prog = nullptr;
        if (view_state_->is_multisampled) {
            blit_upscale_prog = blit_upscale_ms_prog_.get();
            bindings[0].trg = Ren::eBindTarget::Tex2DMs;
        } else {
            blit_upscale_prog = blit_upscale_prog_.get();
        }

        const PrimDraw::Uniform uniforms[] = {{0, Ren::Vec4f{0.0f, 0.0f, 1.0f, 1.0f}}};

        prim_draw_.DrawPrim(PrimDraw::ePrim::Quad, {output_fb_.id(), 0},
                            blit_upscale_prog, bindings, 4, uniforms, 1);
    }
}

void RpSSAO::LazyInit(Ren::Context &ctx, ShaderLoader &sh, RpAllocTex &ssao1_tex,
                      RpAllocTex &ssao2_tex, RpAllocTex &output_tex) {
    if (!initialized) {
        blit_ao_prog_ = sh.LoadProgram(ctx, "blit_ao", "internal/blit.vert.glsl",
                                       "internal/blit_ssao.frag.glsl");
        assert(blit_ao_prog_->ready());
        blit_bilateral_prog_ =
            sh.LoadProgram(ctx, "blit_bilateral", "internal/blit.vert.glsl",
                           "internal/blit_bilateral.frag.glsl");
        assert(blit_bilateral_prog_->ready());
        blit_upscale_prog_ =
            sh.LoadProgram(ctx, "blit_upscale", "internal/blit.vert.glsl",
                           "internal/blit_upscale.frag.glsl");
        assert(blit_upscale_prog_->ready());
        blit_upscale_ms_prog_ =
            sh.LoadProgram(ctx, "blit_upscale_ms", "internal/blit.vert.glsl",
                           "internal/blit_upscale.frag.glsl@MSAA_4");
        assert(blit_upscale_ms_prog_->ready());

        { // dummy 1px texture
            Ren::Tex2DParams p;
            p.w = p.h = 1;
            p.format = Ren::eTexFormat::RawRGBA8888;
            p.filter = Ren::eTexFilter::Bilinear;
            p.repeat = Ren::eTexRepeat::ClampToEdge;

            static const uint8_t white[] = {255, 255, 255, 255};

            Ren::eTexLoadStatus status;
            dummy_white_ =
                ctx.LoadTexture2D("dummy_white", white, sizeof(white), p, &status);
            assert(status == Ren::eTexLoadStatus::TexCreatedFromData ||
                   status == Ren::eTexLoadStatus::TexFound);
        }

        initialized = true;
    }

    if (!ssao_buf1_fb_.Setup(&ssao1_tex.ref->handle(), 1, {}, {}, false)) {
        ctx.log()->Error("RpSSAO: ssao_buf1_fb_ init failed!");
    }

    if (!ssao_buf2_fb_.Setup(&ssao2_tex.ref->handle(), 1, {}, {}, false)) {
        ctx.log()->Error("RpSSAO: ssao_buf2_fb_ init failed!");
    }

    if (!output_fb_.Setup(&output_tex.ref->handle(), 1, {}, {}, false)) {
        ctx.log()->Error("RpSSAO: output_fb_ init failed!");
    }
}
