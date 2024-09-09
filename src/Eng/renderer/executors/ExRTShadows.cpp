#include "ExRTShadows.h"

#include <Ren/Context.h>
#include <Ren/RastState.h>

#include "../../utils/ShaderLoader.h"

void Eng::ExRTShadows::Execute(FgBuilder &builder) {
    LazyInit(builder.ctx(), builder.sh());
    if (builder.ctx().capabilities.hwrt) {
        Execute_HWRT_Inline(builder);
    } else {
        Execute_SWRT(builder);
    }
}

void Eng::ExRTShadows::LazyInit(Ren::Context &ctx, Eng::ShaderLoader &sh) {
    if (!initialized_) {
        Ren::ProgramRef rt_shadows_prog = sh.LoadProgram(
            ctx, ctx.capabilities.hwrt ? "internal/rt_shadows_hwrt.comp.glsl" : "internal/rt_shadows_swrt.comp.glsl");
        assert(rt_shadows_prog->ready());
        if (!pi_rt_shadows_.Init(ctx.api_ctx(), std::move(rt_shadows_prog), ctx.log())) {
            ctx.log()->Error("ExRTShadows: Failed to initialize pipeline!");
        }
        initialized_ = true;
    }
}