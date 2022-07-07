#include "RpDownDepth.h"

#include <Ren/Context.h>
#include <Ren/Program.h>
#include <Ren/RastState.h>

#include "../../Utils/ShaderLoader.h"
#include "../PrimDraw.h"
#include "../Renderer_Structs.h"

#include "../assets/shaders/internal/blit_down_depth_interface.glsl"

void RpDownDepth::Execute(RpBuilder &builder) {
    RpAllocBuf &unif_shared_data_buf = builder.GetReadBuffer(shared_data_buf_);
    RpAllocTex &depth_tex = builder.GetReadTexture(depth_tex_);
    RpAllocTex &output_tex = builder.GetWriteTexture(output_tex_);

    LazyInit(builder.ctx(), builder.sh(), output_tex);

    Ren::RastState rast_state;

    rast_state.viewport[2] = view_state_->act_res[0] / 2;
    rast_state.viewport[3] = view_state_->act_res[1] / 2;

    const Ren::ProgramRef &down_depth_prog =
        view_state_->is_multisampled ? blit_down_depth_ms_prog_ : blit_down_depth_prog_;

    const Ren::Binding bindings[] = {
        {view_state_->is_multisampled ? Ren::eBindTarget::Tex2DMs : Ren::eBindTarget::Tex2D, DownDepth::DEPTH_TEX_SLOT,
         *depth_tex.ref}};

    DownDepth::Params uniform_params;
    uniform_params.transform = Ren::Vec4f{0.0f, 0.0f, float(view_state_->act_res[0]), float(view_state_->act_res[1])};
    uniform_params.clip_info = view_state_->clip_info;
    uniform_params.linearize = 1.0f;

    prim_draw_.DrawPrim(PrimDraw::ePrim::Quad, down_depth_prog, depth_down_fb_, render_pass_, rast_state,
                        builder.rast_state(), bindings, 1, &uniform_params, sizeof(DownDepth::Params), 0);
}

void RpDownDepth::LazyInit(Ren::Context &ctx, ShaderLoader &sh, RpAllocTex &down_depth_2x_tex) {
    if (!initialized) {
        blit_down_depth_prog_ = sh.LoadProgram(ctx, "blit_down_depth", "internal/blit_down_depth.vert.glsl",
                                               "internal/blit_down_depth.frag.glsl");
        assert(blit_down_depth_prog_->ready());
        blit_down_depth_ms_prog_ = sh.LoadProgram(ctx, "blit_down_depth_ms", "internal/blit.vert.glsl",
                                                  "internal/blit_down_depth.frag.glsl@MSAA_4");
        assert(blit_down_depth_ms_prog_->ready());

        initialized = true;
    }

    const Ren::RenderTarget render_targets[] = {{down_depth_2x_tex.ref, Ren::eLoadOp::DontCare, Ren::eStoreOp::Store}};

    if (!render_pass_.Setup(ctx.api_ctx(), render_targets, {}, ctx.log())) {
        ctx.log()->Error("RpDownDepth: render_pass_ init failed!");
    }

    if (!depth_down_fb_.Setup(ctx.api_ctx(), render_pass_, down_depth_2x_tex.desc.w, down_depth_2x_tex.desc.h, {}, {},
                              render_targets, ctx.log())) {
        ctx.log()->Error("RpDownDepth: depth_down_fb_ init failed!");
    }
}
