#include "RpBlur.h"

#include <Ren/Context.h>
#include <Ren/RastState.h>
#include <Ren/Texture.h>

#include "../../Utils/ShaderLoader.h"
#include "../PrimDraw.h"
#include "../Renderer_Structs.h"

#include "../assets/shaders/internal/blit_gauss_interface.glsl"

void RpBlur::Execute(RpBuilder &builder) {
    RpAllocTex &intput_tex = builder.GetReadTexture(pass_data_->input_tex);
    RpAllocTex &output_tex = builder.GetWriteTexture(pass_data_->output_tex);

    LazyInit(builder.ctx(), builder.sh(), output_tex);

    Ren::RastState rast_state;
    rast_state.poly.cull = uint8_t(Ren::eCullFace::Back);

    rast_state.viewport[2] = view_state_->act_res[0] / 4;
    rast_state.viewport[3] = view_state_->act_res[1] / 4;

    const Ren::RenderTarget render_targets[] = {{output_tex.ref, Ren::eLoadOp::DontCare, Ren::eStoreOp::Store}};

    const Ren::Binding bindings[] = {{Ren::eBindTarget::Tex2D, Gauss::SRC_TEX_SLOT, *intput_tex.ref}};

    Gauss::Params uniform_params;
    uniform_params.transform = Ren::Vec4f{0.0f, 0.0f, float(rast_state.viewport[2]), float(rast_state.viewport[3])};
    uniform_params.vertical[0] = vertical_ ? 1.0f : 0.0f;

    prim_draw_.DrawPrim(PrimDraw::ePrim::Quad, blit_gauss_prog_, render_targets, {}, rast_state, builder.rast_state(),
                        bindings, &uniform_params, sizeof(Gauss::Params), 0);
}

void RpBlur::LazyInit(Ren::Context &ctx, ShaderLoader &sh, RpAllocTex &output_tex) {
    if (!initialized) {
        blit_gauss_prog_ =
            sh.LoadProgram(ctx, "blit_gauss2", "internal/blit_gauss.vert.glsl", "internal/blit_gauss.frag.glsl");
        assert(blit_gauss_prog_->ready());

        initialized = true;
    }
}
