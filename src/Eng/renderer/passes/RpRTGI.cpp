#include "RpRTGI.h"

#include <Ren/Context.h>
#include <Ren/RastState.h>
#include <Ren/Texture.h>

#include "../../utils/ShaderLoader.h"
#include "../Renderer_Structs.h"

void Eng::RpRTGI::Execute(RpBuilder &builder) {
    LazyInit(builder.ctx(), builder.sh());
    if (builder.ctx().capabilities.hwrt) {
        Execute_HWRT(builder);
    } else {
        Execute_SWRT(builder);
    }
}

void Eng::RpRTGI::LazyInit(Ren::Context &ctx, Eng::ShaderLoader &sh) {
    if (!initialized_) {
        Ren::ProgramRef rt_gi_prog =
            sh.LoadProgram(ctx, ctx.capabilities.hwrt ? "internal/rt_gi_hwrt.comp.glsl@GI_CACHE"
                                                      : "internal/rt_gi_swrt.comp.glsl@GI_CACHE");
        assert(rt_gi_prog->ready());

        if (!pi_rt_gi_[0].Init(ctx.api_ctx(), std::move(rt_gi_prog), ctx.log())) {
            ctx.log()->Error("RpRTGI: Failed to initialize pipeline!");
        }

        rt_gi_prog = sh.LoadProgram(ctx, ctx.capabilities.hwrt ? "internal/rt_gi_hwrt.comp.glsl@GI_CACHE;STOCH_LIGHTS"
                                                               : "internal/rt_gi_swrt.comp.glsl@GI_CACHE;STOCH_LIGHTS");
        assert(rt_gi_prog->ready());

        if (!pi_rt_gi_[1].Init(ctx.api_ctx(), std::move(rt_gi_prog), ctx.log())) {
            ctx.log()->Error("RpRTGI: Failed to initialize pipeline!");
        }

        rt_gi_prog = sh.LoadProgram(ctx, ctx.capabilities.hwrt ? "internal/rt_gi_hwrt.comp.glsl@TWO_BOUNCES;GI_CACHE"
                                                               : "internal/rt_gi_swrt.comp.glsl@TWO_BOUNCES;GI_CACHE");
        assert(rt_gi_prog->ready());

        if (!pi_rt_gi_2bounce_[0].Init(ctx.api_ctx(), std::move(rt_gi_prog), ctx.log())) {
            ctx.log()->Error("RpRTGI: Failed to initialize pipeline!");
        }

        rt_gi_prog = sh.LoadProgram(ctx, ctx.capabilities.hwrt
                                             ? "internal/rt_gi_hwrt.comp.glsl@TWO_BOUNCES;GI_CACHE;STOCH_LIGHTS"
                                             : "internal/rt_gi_swrt.comp.glsl@TWO_BOUNCES;GI_CACHE;STOCH_LIGHTS");
        assert(rt_gi_prog->ready());

        if (!pi_rt_gi_2bounce_[1].Init(ctx.api_ctx(), std::move(rt_gi_prog), ctx.log())) {
            ctx.log()->Error("RpRTGI: Failed to initialize pipeline!");
        }
        initialized_ = true;
    }
}
