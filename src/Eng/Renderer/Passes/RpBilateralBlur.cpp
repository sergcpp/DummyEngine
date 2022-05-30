#include "RpBilateralBlur.h"

#include <Ren/Context.h>
#include <Ren/Program.h>
#include <Ren/RastState.h>

#include "../../Utils/ShaderLoader.h"
#include "../PrimDraw.h"
#include "../Renderer_Structs.h"

#include "../assets/shaders/internal/blit_bilateral_interface.glsl"

void RpBilateralBlur::Execute(RpBuilder &builder) {
    RpAllocTex &depth_tex = builder.GetReadTexture(depth_tex_);
    RpAllocTex &input_tex = builder.GetReadTexture(input_tex_);
    RpAllocTex &output_tex = builder.GetWriteTexture(output_tex_);

    LazyInit(builder.ctx(), builder.sh(), output_tex);

    Ren::RastState rast_state;
    rast_state.poly.cull = uint8_t(Ren::eCullFace::Back);

    rast_state.viewport[2] = res_[0];
    rast_state.viewport[3] = res_[1];

    { // blur ao buffer
        const Ren::RenderTarget render_targets[] = {{output_tex.ref, Ren::eLoadOp::DontCare, Ren::eStoreOp::Store}};

        const Ren::Binding bindings[] = {{Ren::eBindTarget::Tex2D, Bilateral::DEPTH_TEX_SLOT, *depth_tex.ref},
                                         {Ren::eBindTarget::Tex2D, Bilateral::INPUT_TEX_SLOT, *input_tex.ref}};

        Bilateral::Params uniform_params;
        uniform_params.transform = Ren::Vec4f{0.0f, 0.0f, 1.0f, 1.0f};
        uniform_params.resolution = Ren::Vec2f{float(res_[0]), float(res_[1])};
        uniform_params.vertical = vertical_ ? 1.0f : 0.0f;

        prim_draw_.DrawPrim(PrimDraw::ePrim::Quad, blit_bilateral_prog_, render_targets, {}, rast_state,
                            builder.rast_state(), bindings, &uniform_params, sizeof(Bilateral::Params), 0);
    }
}

void RpBilateralBlur::LazyInit(Ren::Context &ctx, ShaderLoader &sh, RpAllocTex &output_tex) {
    if (!initialized) {
        blit_bilateral_prog_ = sh.LoadProgram(ctx, "blit_bilateral2", "internal/blit_bilateral.vert.glsl",
                                              "internal/blit_bilateral.frag.glsl");
        assert(blit_bilateral_prog_->ready());

        initialized = true;
    }
}
