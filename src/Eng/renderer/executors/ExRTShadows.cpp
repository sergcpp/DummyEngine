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
        pi_rt_shadows_ = sh.LoadPipeline(ctx.capabilities.hwrt ? "internal/rt_shadows_hwrt.comp.glsl"
                                                               : "internal/rt_shadows_swrt.comp.glsl");
        if (!pi_rt_shadows_) {
            ctx.log()->Error("ExRTShadows: Failed to initialize pipeline!");
        }
        initialized_ = true;
    }
}
