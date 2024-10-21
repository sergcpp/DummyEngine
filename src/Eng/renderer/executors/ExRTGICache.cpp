#include "ExRTGICache.h"

#include <Ren/Context.h>

#include "../../utils/ShaderLoader.h"

void Eng::ExRTGICache::Execute(FgBuilder &builder) {
    LazyInit(builder.ctx(), builder.sh());
    if (builder.ctx().capabilities.hwrt) {
        Execute_HWRT(builder);
    } else {
        Execute_SWRT(builder);
    }
}

void Eng::ExRTGICache::LazyInit(Ren::Context &ctx, Eng::ShaderLoader &sh) {
    if (!initialized_) {
        auto hwrt_select = [&ctx](std::string_view hwrt_shader, std::string_view swrt_shader) {
            return ctx.capabilities.hwrt ? hwrt_shader : swrt_shader;
        };
        auto subgroup_select = [&ctx](std::string_view subgroup_shader, std::string_view nosubgroup_shader) {
            return ctx.capabilities.subgroup ? subgroup_shader : nosubgroup_shader;
        };

        Ren::ProgramRef rt_gi_cache_prog =
            sh.LoadProgram(ctx, hwrt_select(subgroup_select("internal/rt_gi_cache_hwrt.comp.glsl",
                                                            "internal/rt_gi_cache_hwrt@NO_SUBGROUP.comp.glsl"),
                                            subgroup_select("internal/rt_gi_cache_swrt.comp.glsl",
                                                            "internal/rt_gi_cache_swrt@NO_SUBGROUP.comp.glsl")));
        assert(rt_gi_cache_prog->ready());

        if (!pi_rt_gi_cache_[0][0].Init(ctx.api_ctx(), std::move(rt_gi_cache_prog), ctx.log())) {
            ctx.log()->Error("ExRTGICache: Failed to initialize pipeline!");
        }

        rt_gi_cache_prog = sh.LoadProgram(
            ctx, hwrt_select(subgroup_select("internal/rt_gi_cache_hwrt@PARTIAL.comp.glsl",
                                             "internal/rt_gi_cache_hwrt@PARTIAL;NO_SUBGROUP.comp.glsl"),
                             subgroup_select("internal/rt_gi_cache_swrt@PARTIAL.comp.glsl",
                                             "internal/rt_gi_cache_swrt@PARTIAL;NO_SUBGROUP.comp.glsl")));
        assert(rt_gi_cache_prog->ready());

        if (!pi_rt_gi_cache_[0][1].Init(ctx.api_ctx(), std::move(rt_gi_cache_prog), ctx.log())) {
            ctx.log()->Error("ExRTGICache: Failed to initialize pipeline!");
        }

        rt_gi_cache_prog = sh.LoadProgram(
            ctx, hwrt_select(subgroup_select("internal/rt_gi_cache_hwrt@STOCH_LIGHTS.comp.glsl",
                                             "internal/rt_gi_cache_hwrt@STOCH_LIGHTS;NO_SUBGROUP.comp.glsl"),
                             subgroup_select("internal/rt_gi_cache_swrt@STOCH_LIGHTS.comp.glsl",
                                             "internal/rt_gi_cache_swrt@STOCH_LIGHTS;NO_SUBGROUP.comp.glsl")));
        assert(rt_gi_cache_prog->ready());

        if (!pi_rt_gi_cache_[1][0].Init(ctx.api_ctx(), std::move(rt_gi_cache_prog), ctx.log())) {
            ctx.log()->Error("ExRTGICache: Failed to initialize pipeline!");
        }

        rt_gi_cache_prog = sh.LoadProgram(
            ctx, hwrt_select(subgroup_select("internal/rt_gi_cache_hwrt@STOCH_LIGHTS;PARTIAL.comp.glsl",
                                             "internal/rt_gi_cache_hwrt@STOCH_LIGHTS;PARTIAL;NO_SUBGROUP.comp.glsl"),
                             subgroup_select("internal/rt_gi_cache_swrt@STOCH_LIGHTS;PARTIAL.comp.glsl",
                                             "internal/rt_gi_cache_swrt@STOCH_LIGHTS;PARTIAL;NO_SUBGROUP.comp.glsl")));
        assert(rt_gi_cache_prog->ready());

        if (!pi_rt_gi_cache_[1][1].Init(ctx.api_ctx(), std::move(rt_gi_cache_prog), ctx.log())) {
            ctx.log()->Error("ExRTGICache: Failed to initialize pipeline!");
        }

        initialized_ = true;
    }
}
