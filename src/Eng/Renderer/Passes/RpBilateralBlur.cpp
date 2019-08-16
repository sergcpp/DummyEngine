#include "RpBilateralBlur.h"

#include <Ren/Context.h>
#include <Ren/Program.h>
#include <Ren/RastState.h>

#include "../../Utils/ShaderLoader.h"
#include "../PrimDraw.h"
#include "../Renderer_Structs.h"

#include "../assets/shaders/internal/blit_bilateral_interface.glsl"

void RpBilateralBlur::Setup(RpBuilder &builder, const Ren::Vec2i res, const bool vertical, const char depth_tex[],
                            const char input_tex[], const char output_tex[]) {
    res_ = res;
    vertical_ = vertical;

    depth_tex_ = builder.ReadTexture(depth_tex, Ren::eResState::ShaderResource, Ren::eStageBits::FragmentShader, *this);
    input_tex_ = builder.ReadTexture(input_tex, Ren::eResState::ShaderResource, Ren::eStageBits::FragmentShader, *this);

    { // Allocate output texture
        Ren::Tex2DParams params;
        params.w = res_[0];
        params.h = res_[1];
        params.format = Ren::eTexFormat::RawR8;
        params.sampling.filter = Ren::eTexFilter::BilinearNoMipmap;
        params.sampling.wrap = Ren::eTexWrap::ClampToEdge;

        output_tex_ = builder.WriteTexture(output_tex, params, Ren::eResState::RenderTarget,
                                           Ren::eStageBits::ColorAttachment, *this);
    }
}

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
        PrimDraw::Binding bindings[] = {{Ren::eBindTarget::Tex2D, Bilateral::DEPTH_TEX_SLOT, *depth_tex.ref},
                                        {Ren::eBindTarget::Tex2D, Bilateral::INPUT_TEX_SLOT, *input_tex.ref}};

        Bilateral::Params uniform_params;
        uniform_params.transform = Ren::Vec4f{0.0f, 0.0f, 1.0f, 1.0f};
        uniform_params.resolution = Ren::Vec2f{float(res_[0]), float(res_[1])};
        uniform_params.vertical = vertical_ ? 1.0f : 0.0f;

        prim_draw_.DrawPrim(PrimDraw::ePrim::Quad, blit_bilateral_prog_, output_fb_, render_pass_, rast_state,
                            builder.rast_state(), bindings, COUNT_OF(bindings), &uniform_params,
                            sizeof(Bilateral::Params), 0);
    }
}

void RpBilateralBlur::LazyInit(Ren::Context &ctx, ShaderLoader &sh, RpAllocTex &output_tex) {
    if (!initialized) {
        blit_bilateral_prog_ = sh.LoadProgram(ctx, "blit_bilateral2", "internal/blit_bilateral.vert.glsl",
                                              "internal/blit_bilateral.frag.glsl");
        assert(blit_bilateral_prog_->ready());

        initialized = true;
    }

    const Ren::RenderTarget render_targets[] = {{output_tex.ref, Ren::eLoadOp::DontCare, Ren::eStoreOp::Store}};

    if (!render_pass_.Setup(ctx.api_ctx(), render_targets, 1, {}, ctx.log())) {
        ctx.log()->Error("RpBilateralBlur: render_pass_ init failed!");
    }

    if (!output_fb_.Setup(ctx.api_ctx(), render_pass_, output_tex.desc.w, output_tex.desc.h, render_targets, 1, {},
                          {})) {
        ctx.log()->Error("RpBilateralBlur: output_fb_ init failed!");
    }
}
