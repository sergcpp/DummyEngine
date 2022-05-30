#include "RpDownColor.h"

#include <Ren/Context.h>
#include <Ren/Program.h>
#include <Ren/RastState.h>

#include "../../Utils/ShaderLoader.h"
#include "../PrimDraw.h"
#include "../Renderer_Structs.h"

#include "../assets/shaders/internal/blit_down_interface.glsl"

void RpDownColor::Execute(RpBuilder &builder) {
    RpAllocTex &input_tex = builder.GetReadTexture(pass_data_->input_tex);
    RpAllocTex &output_tex = builder.GetWriteTexture(pass_data_->output_tex);

    LazyInit(builder.ctx(), builder.sh(), output_tex);

    Ren::RastState rast_state;

    rast_state.viewport[2] = view_state_->act_res[0] / 4;
    rast_state.viewport[3] = view_state_->act_res[1] / 4;

    const Ren::Binding bindings[] = {{Ren::eBindTarget::Tex2D, DownColor::SRC_TEX_SLOT, *input_tex.ref}};

    DownColor::Params uniform_params;
    uniform_params.transform = Ren::Vec4f{0.0f, 0.0f, float(view_state_->act_res[0]) / float(view_state_->scr_res[0]),
                                          float(view_state_->act_res[1]) / float(view_state_->scr_res[1])};
    uniform_params.resolution = Ren::Vec4f{float(view_state_->act_res[0]), float(view_state_->act_res[1]), 0.0f, 0.0f};

    const Ren::RenderTarget render_targets[] = {{output_tex.ref, Ren::eLoadOp::DontCare, Ren::eStoreOp::Store}};

    prim_draw_.DrawPrim(PrimDraw::ePrim::Quad, blit_down_prog_, render_targets, {}, rast_state, builder.rast_state(),
                        bindings, &uniform_params, sizeof(DownColor::Params), 0);
}

void RpDownColor::LazyInit(Ren::Context &ctx, ShaderLoader &sh, RpAllocTex &output_tex) {
    if (!initialized) {
        blit_down_prog_ =
            sh.LoadProgram(ctx, "blit_down2", "internal/blit_down.vert.glsl", "internal/blit_down.frag.glsl");
        assert(blit_down_prog_->ready());

        initialized = true;
    }
}
