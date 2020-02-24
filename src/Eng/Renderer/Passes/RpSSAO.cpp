#include "RpSSAO.h"

#include "../../Utils/ShaderLoader.h"
#include "../Renderer_Structs.h"

void RpSSAO::Setup(Graph::RpBuilder &builder, const ViewState *view_state,
                   Ren::TexHandle depth_tex, Ren::TexHandle down_depth_2x_tex,
                   Ren::TexHandle rand2d_dirs_4x4_tex, Ren::TexHandle ssao_buf1_tex,
                   Ren::TexHandle ssao_buf2_tex, Graph::ResourceHandle in_shared_data_buf,
                   Ren::TexHandle output_tex) {
    view_state_ = view_state;
    depth_tex_ = depth_tex;
    down_depth_2x_tex_ = down_depth_2x_tex;
    rand2d_dirs_4x4_tex_ = rand2d_dirs_4x4_tex;
    ssao_buf1_tex_ = ssao_buf1_tex;
    ssao_buf2_tex_ = ssao_buf2_tex;
    output_tex_ = output_tex;

    input_[0] = builder.ReadBuffer(in_shared_data_buf);
    input_count_ = 1;

    // output_[0] = builder.WriteBuffer(input_[0], *this);
    output_count_ = 0;
}

void RpSSAO::Execute(Graph::RpBuilder &builder) {
    LazyInit(builder.ctx(), builder.sh());

    Ren::RastState rast_state;
    rast_state.cull_face.enabled = true;

    rast_state.viewport[2] = view_state_->act_res[0] / 2;
    rast_state.viewport[3] = view_state_->act_res[1] / 2;

    rast_state.Apply();
    Ren::RastState applied_state = rast_state;

    Graph::AllocatedBuffer &unif_shared_data_buf = builder.GetReadBuffer(input_[0]);

    { // prepare ao buffer
        const PrimDraw::Binding bindings[] = {
            {Ren::eBindTarget::Tex2D, REN_BASE0_TEX_SLOT, down_depth_2x_tex_},
            {Ren::eBindTarget::Tex2D, REN_BASE1_TEX_SLOT, rand2d_dirs_4x4_tex_},
            {Ren::eBindTarget::UBuf, REN_UB_SHARED_DATA_LOC,
             unif_shared_data_buf.ref->handle()}};

        const PrimDraw::Uniform uniforms[] = {{0, Ren::Vec4f{applied_state.viewport}}};

        prim_draw_.DrawPrim(PrimDraw::ePrim::Quad, {ssao_buf1_fb_.id(), 0},
                            blit_ao_prog_.get(), bindings, 3, uniforms, 1);
    }

    { // blur ao buffer
        PrimDraw::Binding bindings[] = {{Ren::eBindTarget::Tex2D, 0, down_depth_2x_tex_},
                                        {Ren::eBindTarget::Tex2D, 1, ssao_buf1_tex_}};

        PrimDraw::Uniform uniforms[] = {{0, Ren::Vec4f{applied_state.viewport}},
                                        {3, 0.0f}};

        prim_draw_.DrawPrim(PrimDraw::ePrim::Quad, {ssao_buf2_fb_.id(), 0},
                            blit_bilateral_prog_.get(), bindings, 2, uniforms, 2);

        bindings[1] = {Ren::eBindTarget::Tex2D, 1, ssao_buf2_tex_};

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
        PrimDraw::Binding bindings[] = {{Ren::eBindTarget::Tex2D, 0, depth_tex_},
                                        {Ren::eBindTarget::Tex2D, 1, down_depth_2x_tex_},
                                        {Ren::eBindTarget::Tex2D, 2, ssao_buf1_tex_},
                                        {Ren::eBindTarget::UBuf, REN_UB_SHARED_DATA_LOC,
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

void RpSSAO::LazyInit(Ren::Context &ctx, ShaderLoader &sh) {
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
            Ren::Texture2DParams p;
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

    if (!ssao_buf1_fb_.Setup(&ssao_buf1_tex_, 1, {}, {}, false)) {
        ctx.log()->Error("RpSSAO: ssao_buf1_fb_ init failed!");
    }

    if (!ssao_buf2_fb_.Setup(&ssao_buf2_tex_, 1, {}, {}, false)) {
        ctx.log()->Error("RpSSAO: ssao_buf2_fb_ init failed!");
    }

    if (!output_fb_.Setup(&output_tex_, 1, {}, {}, false)) {
        ctx.log()->Error("RpSSAO: output_fb_ init failed!");
    }
}
