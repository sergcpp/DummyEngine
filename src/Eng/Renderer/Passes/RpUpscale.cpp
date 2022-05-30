#include "RpUpscale.h"

#include <Ren/Context.h>
#include <Ren/Program.h>
#include <Ren/RastState.h>

#include "../../Utils/ShaderLoader.h"
#include "../PrimDraw.h"
#include "../Renderer_Structs.h"

#include "../assets/shaders/internal/blit_upscale_interface.glsl"

void RpUpscale::Execute(RpBuilder &builder) {
    RpAllocTex &down_depth_2x_tex = builder.GetReadTexture(depth_down_2x_tex_);
    RpAllocTex &depth_tex = builder.GetReadTexture(depth_tex_);
    RpAllocTex &input_tex = builder.GetReadTexture(input_tex_);
    RpAllocTex &output_tex = builder.GetWriteTexture(output_tex_);

    LazyInit(builder.ctx(), builder.sh(), output_tex);

    Ren::RastState rast_state;
    rast_state.poly.cull = uint8_t(Ren::eCullFace::Back);

    rast_state.viewport[2] = res_[0];
    rast_state.viewport[3] = res_[1];

    { // upsample ao
        const Ren::RenderTarget render_targets[] = {{output_tex.ref, Ren::eLoadOp::DontCare, Ren::eStoreOp::Store}};

        const Ren::Binding bindings[] = {{Ren::eBindTarget::Tex2D, Upscale::DEPTH_TEX_SLOT, *depth_tex.ref},
                                         {Ren::eBindTarget::Tex2D, Upscale::DEPTH_LOW_TEX_SLOT, *down_depth_2x_tex.ref},
                                         {Ren::eBindTarget::Tex2D, Upscale::INPUT_TEX_SLOT, *input_tex.ref}};

        Upscale::Params uniform_params;
        uniform_params.transform = Ren::Vec4f{0.0f, 0.0f, 1.0f, 1.0f};
        uniform_params.resolution = Ren::Vec4f{float(res_[0]), float(res_[1]), float(res_[2]), float(res_[3])};
        uniform_params.clip_info = clip_info_;

        prim_draw_.DrawPrim(PrimDraw::ePrim::Quad, blit_upscale_prog_, render_targets, {}, rast_state,
                            builder.rast_state(), bindings, &uniform_params, sizeof(Upscale::Params), 0);
    }
}

void RpUpscale::LazyInit(Ren::Context &ctx, ShaderLoader &sh, RpAllocTex &output_tex) {
    if (!initialized) {
        blit_upscale_prog_ =
            sh.LoadProgram(ctx, "blit_upscale", "internal/blit_upscale.vert.glsl", "internal/blit_upscale.frag.glsl");
        assert(blit_upscale_prog_->ready());

        initialized = true;
    }
}
