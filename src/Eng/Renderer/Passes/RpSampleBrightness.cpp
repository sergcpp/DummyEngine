#include "RpSampleBrightness.h"

#include <Ren/Context.h>

#include "../../Utils/ShaderLoader.h"

void RpSampleBrightness::Setup(RpBuilder &builder, Ren::TexHandle tex_to_sample,
                               const char reduced_tex[]) {
    tex_to_sample_ = tex_to_sample;

    { // aux buffer which gathers frame luminance
        Ren::Tex2DParams params;
        params.w = 16;
        params.h = 8;
        params.format = Ren::eTexFormat::RawR16F;
        params.filter = Ren::eTexFilter::BilinearNoMipmap;
        params.repeat = Ren::eTexRepeat::ClampToEdge;

        reduced_tex_ = builder.WriteTexture(reduced_tex, params, *this);
    }
}

void RpSampleBrightness::LazyInit(Ren::Context &ctx, ShaderLoader &sh,
                                  RpAllocTex &reduced_tex) {
    if (!initialized_) {
        blit_red_prog_ = sh.LoadProgram(ctx, "blit_red", "internal/blit.vert.glsl",
                                        "internal/blit_reduced.frag.glsl");
        assert(blit_red_prog_->ready());

#if defined(USE_GL_RENDER)
        InitPBO();
#endif

        initialized_ = true;
    }

    if (!reduced_fb_.Setup(reduced_tex.ref->handle(), {}, {}, false)) {
        ctx.log()->Error("RpSampleBrightness: reduced_fb_ init failed!");
    }
}