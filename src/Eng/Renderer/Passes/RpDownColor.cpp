#include "RpDownColor.h"

#include <Ren/Context.h>
#include <Ren/Program.h>
#include <Ren/RastState.h>

#include "../../Utils/ShaderLoader.h"
#include "../PrimDraw.h"
#include "../Renderer_Structs.h"

#include "../assets/shaders/internal/blit_down_interface.glsl"

void RpDownColor::Setup(RpBuilder &builder, const ViewState *view_state, const char color_tex_name[],
                        Ren::WeakTex2DRef output_tex) {
    view_state_ = view_state;

    input_tex_ =
        builder.ReadTexture(color_tex_name, Ren::eResState::ShaderResource, Ren::eStageBits::FragmentShader, *this);
    output_tex_ =
        builder.WriteTexture(output_tex, Ren::eResState::RenderTarget, Ren::eStageBits::ColorAttachment, *this);
}

void RpDownColor::Execute(RpBuilder &builder) {
    RpAllocTex &input_tex = builder.GetReadTexture(input_tex_);
    RpAllocTex &output_tex = builder.GetWriteTexture(output_tex_);

    LazyInit(builder.ctx(), builder.sh(), output_tex);

    Ren::RastState rast_state;

    rast_state.viewport[2] = view_state_->act_res[0] / 4;
    rast_state.viewport[3] = view_state_->act_res[1] / 4;

    const PrimDraw::Binding bindings[] = {{Ren::eBindTarget::Tex2D, DownColor::SRC_TEX_SLOT, *input_tex.ref}};

    DownColor::Params uniform_params;
    uniform_params.transform = Ren::Vec4f{0.0f, 0.0f, float(view_state_->act_res[0]) / float(view_state_->scr_res[0]),
                                          float(view_state_->act_res[1]) / float(view_state_->scr_res[1])};
    uniform_params.resolution = Ren::Vec4f{float(view_state_->act_res[0]), float(view_state_->act_res[1]), 0.0f, 0.0f};

    prim_draw_.DrawPrim(PrimDraw::ePrim::Quad, blit_down_prog_, output_fb_, render_pass_, rast_state,
                        builder.rast_state(), bindings, COUNT_OF(bindings), &uniform_params, sizeof(DownColor::Params),
                        0);
}

void RpDownColor::LazyInit(Ren::Context &ctx, ShaderLoader &sh, RpAllocTex &output_tex) {
    if (!initialized) {
        blit_down_prog_ =
            sh.LoadProgram(ctx, "blit_down2", "internal/blit_down.vert.glsl", "internal/blit_down.frag.glsl");
        assert(blit_down_prog_->ready());

        initialized = true;
    }

    const Ren::RenderTarget render_targets[] = {{output_tex.ref, Ren::eLoadOp::DontCare, Ren::eStoreOp::Store}};

    if (!render_pass_.Setup(ctx.api_ctx(), render_targets, 1, {}, ctx.log())) {
        ctx.log()->Error("RpDownColor: render_pass_ init failed!");
    }

    if (!output_fb_.Setup(ctx.api_ctx(), render_pass_, output_tex.desc.w, output_tex.desc.h, render_targets, 1, {},
                          {})) {
        ctx.log()->Error("RpDownColor: output_fb_ init failed!");
    }
}
