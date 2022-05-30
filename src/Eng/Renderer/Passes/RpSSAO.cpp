#include "RpSSAO.h"

#include <Ren/Context.h>
#include <Ren/Program.h>
#include <Ren/RastState.h>

#include "../../Utils/ShaderLoader.h"
#include "../PrimDraw.h"
#include "../Renderer_Structs.h"

#include "../assets/shaders/internal/blit_ssao_interface.glsl"

void RpSSAO::Execute(RpBuilder &builder) {
    RpAllocTex &down_depth_2x_tex = builder.GetReadTexture(depth_down_2x_tex_);
    RpAllocTex &rand_tex = builder.GetReadTexture(rand_tex_);
    RpAllocTex &output_tex = builder.GetWriteTexture(output_tex_);

    LazyInit(builder.ctx(), builder.sh(), output_tex);

    Ren::RastState rast_state;
    rast_state.poly.cull = uint8_t(Ren::eCullFace::Back);

    rast_state.viewport[2] = view_state_->act_res[0] / 2;
    rast_state.viewport[3] = view_state_->act_res[1] / 2;

    { // prepare ao buffer
        const Ren::Binding bindings[] = {{Ren::eBindTarget::Tex2D, SSAO::DEPTH_TEX_SLOT, *down_depth_2x_tex.ref},
                                         {Ren::eBindTarget::Tex2D, SSAO::RAND_TEX_SLOT, *rand_tex.ref}};

        SSAO::Params uniform_params;
        uniform_params.transform = Ren::Vec4f{0.0f, 0.0f, view_state_->act_res[0] / 2, view_state_->act_res[1] / 2};
        uniform_params.resolution = Ren::Vec2f{float(view_state_->act_res[0]), float(view_state_->act_res[1])};

        const Ren::RenderTarget render_targets[] = {{output_tex.ref, Ren::eLoadOp::DontCare, Ren::eStoreOp::Store}};

        prim_draw_.DrawPrim(PrimDraw::ePrim::Quad, blit_ao_prog_, render_targets, {}, rast_state, builder.rast_state(),
                            bindings, &uniform_params, sizeof(SSAO::Params), 0);
    }
}

void RpSSAO::LazyInit(Ren::Context &ctx, ShaderLoader &sh, RpAllocTex &output_tex) {
    if (!initialized) {
        blit_ao_prog_ = sh.LoadProgram(ctx, "blit_ao", "internal/blit_ssao.vert.glsl", "internal/blit_ssao.frag.glsl");
        assert(blit_ao_prog_->ready());

        initialized = true;
    }
}
