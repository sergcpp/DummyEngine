#include "RpSampleBrightness.h"

void RpSampleBrightness::Setup(Graph::RpBuilder &builder, Ren::TexHandle tex_to_sample,
                               Ren::TexHandle reduce_tex) {
    tex_to_sample_ = tex_to_sample;
    reduce_tex_ = reduce_tex;

    input_count_ = 0;
    output_count_ = 0;
}

void RpSampleBrightness::LazyInit(Ren::Context &ctx, ShaderLoader &sh) {
    if (!initialized_) {
        blit_red_prog_ = sh.LoadProgram(ctx, "blit_red", "internal/blit.vert.glsl",
                                        "internal/blit_reduced.frag.glsl");
        assert(blit_red_prog_->ready());

#if defined(USE_GL_RENDER)
        InitPBO();
#endif

        initialized_ = true;
    }

    if (!reduced_fb_.Setup(&reduce_tex_, 1, {}, {}, false)) {
        ctx.log()->Error("RpSampleBrightness: reduced_fb_ init failed!");
    }
}