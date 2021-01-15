#include "RpBlur.h"

#include <Ren/Context.h>
#include <Ren/RastState.h>
#include <Ren/Texture.h>

#include "../../Utils/ShaderLoader.h"
#include "../PrimDraw.h"
#include "../Renderer_Structs.h"

void RpBlur::Setup(RpBuilder &builder, const ViewState *view_state,
                   Ren::TexHandle down_buf_4x, Ren::TexHandle blur_temp_4x,
                   Ren::TexHandle output_tex) {
    view_state_ = view_state;
    down_buf_4x_ = down_buf_4x;
    blur_temp_4x_ = blur_temp_4x;
    output_tex_ = output_tex;

    // input_[0] = builder.ReadBuffer(in_shared_data_buf);
    input_count_ = 0;

    // output_[0] = builder.WriteBuffer(input_[0], *this);
    output_count_ = 0;
}

void RpBlur::Execute(RpBuilder &builder) {
    LazyInit(builder.ctx(), builder.sh());

    Ren::RastState rast_state;
    rast_state.cull_face.enabled = true;

    rast_state.viewport[2] = view_state_->act_res[0] / 4;
    rast_state.viewport[3] = view_state_->act_res[1] / 4;

    rast_state.Apply();
    Ren::RastState applied_state = rast_state;

    { // horizontal
        const PrimDraw::Binding binding = {Ren::eBindTarget::Tex2D, REN_BASE0_TEX_SLOT,
                                           down_buf_4x_};

        const PrimDraw::Uniform uniforms[] = {
            {0, Ren::Vec4f{0.0f, 0.0f, float(applied_state.viewport[2]),
                           float(applied_state.viewport[3])}},
            {1, 0.0f}};

        prim_draw_.DrawPrim(PrimDraw::ePrim::Quad, {blur_fb_[0].id(), 0},
                            blit_gauss_prog_.get(), &binding, 1, uniforms, 2);
    }

    { // vertical
        const PrimDraw::Binding binding = {Ren::eBindTarget::Tex2D, REN_BASE0_TEX_SLOT,
                                           blur_temp_4x_};

        const PrimDraw::Uniform uniforms[] = {
            {0, Ren::Vec4f{0.0f, 0.0f, float(applied_state.viewport[2]),
                           float(applied_state.viewport[3])}},
            {1, 1.0f}};

        prim_draw_.DrawPrim(PrimDraw::ePrim::Quad, {blur_fb_[1].id(), 0},
                            blit_gauss_prog_.get(), &binding, 1, uniforms, 2);
    }
}

void RpBlur::LazyInit(Ren::Context &ctx, ShaderLoader &sh) {
    if (!initialized) {
        blit_gauss_prog_ = sh.LoadProgram(ctx, "blit_gauss", "internal/blit.vert.glsl",
                                          "internal/blit_gauss.frag.glsl");
        assert(blit_gauss_prog_->ready());

        initialized = true;
    }

    if (!blur_fb_[0].Setup(&blur_temp_4x_, 1, {}, {}, false)) {
        ctx.log()->Error("RpBlur: blur_fb_[0] init failed!");
    }

    if (!blur_fb_[1].Setup(&output_tex_, 1, {}, {}, false)) {
        ctx.log()->Error("RpBlur: blur_fb_[1] init failed!");
    }
}
