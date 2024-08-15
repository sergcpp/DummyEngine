#include "RpRTGICache.h"

#include <Ren/Context.h>

#include "../../utils/ShaderLoader.h"
#include "../graph/GraphBuilder.h"

void Eng::RpRTGICache::Execute(RpBuilder &builder) {
    LazyInit(builder.ctx(), builder.sh());
    if (builder.ctx().capabilities.hwrt) {
        Execute_HWRT(builder);
    } else {
        Execute_SWRT(builder);
    }
}

void Eng::RpRTGICache::LazyInit(Ren::Context &ctx, Eng::ShaderLoader &sh) {
    if (!initialized_) {
        Ren::ProgramRef rt_gi_cache_prog = sh.LoadProgram(
            ctx, ctx.capabilities.hwrt ? "internal/rt_gi_cache_hwrt.comp.glsl" : "internal/rt_gi_cache_swrt.comp.glsl");
        assert(rt_gi_cache_prog->ready());

        if (!pi_rt_gi_cache_[0].Init(ctx.api_ctx(), std::move(rt_gi_cache_prog), ctx.log())) {
            ctx.log()->Error("RpRTGICache: Failed to initialize pipeline!");
        }

        rt_gi_cache_prog =
            sh.LoadProgram(ctx, ctx.capabilities.hwrt ? "internal/rt_gi_cache_hwrt.comp.glsl@STOCH_LIGHTS"
                                                      : "internal/rt_gi_cache_swrt.comp.glsl@STOCH_LIGHTS");
        assert(rt_gi_cache_prog->ready());

        if (!pi_rt_gi_cache_[1].Init(ctx.api_ctx(), std::move(rt_gi_cache_prog), ctx.log())) {
            ctx.log()->Error("RpRTGICache: Failed to initialize pipeline!");
        }
        initialized_ = true;
    }
}