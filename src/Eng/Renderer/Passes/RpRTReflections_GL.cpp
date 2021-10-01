#include "RpRTReflections.h"

#include <Ren/Context.h>
#include <Ren/RastState.h>
#include <Ren/Texture.h>

#include "../../Utils/ShaderLoader.h"
#include "../PrimDraw.h"
#include "../Renderer_Structs.h"

#include "../assets/shaders/internal/rt_reflections_interface.glsl"

void RpRTReflections::Execute(RpBuilder &builder) {
    LazyInit(builder.ctx(), builder.sh());

    RpAllocBuf &geo_data_buf = builder.GetReadBuffer(geo_data_buf_);
    RpAllocBuf &materials_buf = builder.GetReadBuffer(materials_buf_);
    RpAllocBuf &vtx_buf1 = builder.GetReadBuffer(vtx_buf1_);
    RpAllocBuf &vtx_buf2 = builder.GetReadBuffer(vtx_buf2_);
    RpAllocBuf &ndx_buf = builder.GetReadBuffer(ndx_buf_);
    RpAllocBuf &unif_sh_data_buf = builder.GetReadBuffer(shared_data_buf_);
    RpAllocTex &depth_tex = builder.GetReadTexture(depth_tex_);
    RpAllocTex &normal_tex = builder.GetReadTexture(normal_tex_);
    RpAllocTex &spec_tex = builder.GetReadTexture(spec_tex_);
    RpAllocTex &ssr_tex = builder.GetReadTexture(ssr_tex_);
    RpAllocTex &prev_tex = builder.GetReadTexture(prev_tex_);
    RpAllocTex &brdf_lut = builder.GetReadTexture(brdf_lut_);
    RpAllocTex &env_tex = builder.GetReadTexture(env_tex_);
    RpAllocTex &dummy_black = builder.GetReadTexture(dummy_black_);
    RpAllocTex *lm_tex[5];
    for (int i = 0; i < 5; ++i) {
        if (lm_tex_[i]) {
            lm_tex[i] = &builder.GetReadTexture(lm_tex_[i]);
        } else {
            lm_tex[i] = &dummy_black;
        }
    }

    RpAllocTex &output_tex = builder.GetWriteTexture(output_tex_);

    // TODO: software fallback for raytracing
}

void RpRTReflections::LazyInit(Ren::Context &ctx, ShaderLoader &sh) {
    if (!initialized) {
        /*Ren::ProgramRef rt_reflections_prog = sh.LoadProgram(
            ctx, "rt_reflections", "internal/rt_reflections.rgen.glsl", "internal/rt_reflections.rchit.glsl",
            "internal/rt_reflections.rahit.glsl", "internal/rt_reflections.rmiss.glsl", nullptr);
        assert(rt_reflections_prog->ready());

        if (!pi_rt_reflections_.Init(ctx.api_ctx(), std::move(rt_reflections_prog), ctx.log())) {
            ctx.log()->Error("RpDebugRT: Failed to initialize pipeline!");
        }*/

        initialized = true;
    }
}