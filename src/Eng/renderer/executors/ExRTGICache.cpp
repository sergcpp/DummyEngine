#include "ExRTGICache.h"

#include <Ren/Context.h>

#include "../../utils/ShaderLoader.h"

void Eng::ExRTGICache::Execute(FgContext &ctx) {
    LazyInit(ctx.ren_ctx(), ctx.sh());
    if (ctx.ren_ctx().capabilities.hwrt) {
        Execute_HWRT(ctx);
    } else {
        Execute_SWRT(ctx);
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

        pi_rt_gi_cache_[0][0] = sh.LoadPipeline(hwrt_select(
            subgroup_select("internal/rt_gi_cache_hwrt.comp.glsl", "internal/rt_gi_cache_hwrt@NO_SUBGROUP.comp.glsl"),
            subgroup_select("internal/rt_gi_cache_swrt.comp.glsl", "internal/rt_gi_cache_swrt@NO_SUBGROUP.comp.glsl")));
        if (!pi_rt_gi_cache_[0][0]) {
            ctx.log()->Error("ExRTGICache: Failed to initialize pipeline!");
        }

        pi_rt_gi_cache_[0][1] =
            sh.LoadPipeline(hwrt_select(subgroup_select("internal/rt_gi_cache_hwrt@PARTIAL.comp.glsl",
                                                        "internal/rt_gi_cache_hwrt@PARTIAL;NO_SUBGROUP.comp.glsl"),
                                        subgroup_select("internal/rt_gi_cache_swrt@PARTIAL.comp.glsl",
                                                        "internal/rt_gi_cache_swrt@PARTIAL;NO_SUBGROUP.comp.glsl")));
        if (!pi_rt_gi_cache_[0][1]) {
            ctx.log()->Error("ExRTGICache: Failed to initialize pipeline!");
        }

        pi_rt_gi_cache_[1][0] = sh.LoadPipeline(
            hwrt_select(subgroup_select("internal/rt_gi_cache_hwrt@STOCH_LIGHTS.comp.glsl",
                                        "internal/rt_gi_cache_hwrt@STOCH_LIGHTS;NO_SUBGROUP.comp.glsl"),
                        subgroup_select("internal/rt_gi_cache_swrt@STOCH_LIGHTS.comp.glsl",
                                        "internal/rt_gi_cache_swrt@STOCH_LIGHTS;NO_SUBGROUP.comp.glsl")));
        if (!pi_rt_gi_cache_[1][0]) {
            ctx.log()->Error("ExRTGICache: Failed to initialize pipeline!");
        }

        pi_rt_gi_cache_[1][1] = sh.LoadPipeline(
            hwrt_select(subgroup_select("internal/rt_gi_cache_hwrt@STOCH_LIGHTS;PARTIAL.comp.glsl",
                                        "internal/rt_gi_cache_hwrt@STOCH_LIGHTS;PARTIAL;NO_SUBGROUP.comp.glsl"),
                        subgroup_select("internal/rt_gi_cache_swrt@STOCH_LIGHTS;PARTIAL.comp.glsl",
                                        "internal/rt_gi_cache_swrt@STOCH_LIGHTS;PARTIAL;NO_SUBGROUP.comp.glsl")));
        if (!pi_rt_gi_cache_[1][1]) {
            ctx.log()->Error("ExRTGICache: Failed to initialize pipeline!");
        }

        initialized_ = true;
    }
}
