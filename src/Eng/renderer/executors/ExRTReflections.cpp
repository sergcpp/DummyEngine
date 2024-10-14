#include "ExRTReflections.h"

#include <Ren/Context.h>
#include <Ren/RastState.h>

#include "../../utils/ShaderLoader.h"

void Eng::ExRTReflections::Execute(FgBuilder &builder) {
    LazyInit(builder.ctx(), builder.sh());
    if (builder.ctx().capabilities.hwrt) {
        Execute_HWRT(builder);
    } else {
        Execute_SWRT(builder);
    }
}

void Eng::ExRTReflections::LazyInit(Ren::Context &ctx, Eng::ShaderLoader &sh) {
    if (!initialized_) {
        auto hwrt_select = [&ctx](std::string_view hwrt_shader, std::string_view swrt_shader) {
            return ctx.capabilities.hwrt ? hwrt_shader : swrt_shader;
        };
        auto subgroup_select = [&ctx](std::string_view subgroup_shader, std::string_view nosubgroup_shader) {
            return ctx.capabilities.subgroup ? subgroup_shader : nosubgroup_shader;
        };

        Ren::ProgramRef rt_reflections_prog;
        if (layered_) {
            rt_reflections_prog = sh.LoadProgram(
                ctx, hwrt_select(subgroup_select("internal/rt_reflections_hwrt@LAYERED.comp.glsl",
                                                 "internal/rt_reflections_hwrt@LAYERED;NO_SUBGROUP.comp.glsl"),
                                 subgroup_select("internal/rt_reflections_swrt@LAYERED.comp.glsl",
                                                 "internal/rt_reflections_swrt@LAYERED;NO_SUBGROUP.comp.glsl")));
        } else {
            rt_reflections_prog =
                sh.LoadProgram(ctx, hwrt_select(subgroup_select("internal/rt_reflections_hwrt.comp.glsl",
                                                                "internal/rt_reflections_hwrt@NO_SUBGROUP.comp.glsl"),
                                                subgroup_select("internal/rt_reflections_swrt.comp.glsl",
                                                                "internal/rt_reflections_swrt@NO_SUBGROUP.comp.glsl")));
        }
        assert(rt_reflections_prog->ready());

        if (!pi_rt_reflections_[0].Init(ctx.api_ctx(), std::move(rt_reflections_prog), ctx.log())) {
            ctx.log()->Error("ExRTReflections: Failed to initialize pipeline!");
        }

        if (layered_) {
            rt_reflections_prog = sh.LoadProgram(
                ctx,
                hwrt_select(subgroup_select("internal/rt_reflections_hwrt@LAYERED;GI_CACHE.comp.glsl",
                                            "internal/rt_reflections_hwrt@LAYERED;GI_CACHE;NO_SUBGROUP.comp.glsl"),
                            subgroup_select("internal/rt_reflections_swrt@LAYERED;GI_CACHE.comp.glsl",
                                            "internal/rt_reflections_swrt@LAYERED;GI_CACHE;NO_SUBGROUP.comp.glsl")));
        } else {
            rt_reflections_prog = sh.LoadProgram(
                ctx, hwrt_select(subgroup_select("internal/rt_reflections_hwrt@GI_CACHE.comp.glsl",
                                                 "internal/rt_reflections_hwrt@GI_CACHE;NO_SUBGROUP.comp.glsl"),
                                 subgroup_select("internal/rt_reflections_swrt@GI_CACHE.comp.glsl",
                                                 "internal/rt_reflections_swrt@GI_CACHE;NO_SUBGROUP.comp.glsl")));
        }
        assert(rt_reflections_prog->ready());

        if (!pi_rt_reflections_[1].Init(ctx.api_ctx(), std::move(rt_reflections_prog), ctx.log())) {
            ctx.log()->Error("ExRTReflections: Failed to initialize pipeline!");
        }

        rt_reflections_prog = sh.LoadProgram(
            ctx,
            hwrt_select(subgroup_select("internal/rt_reflections_hwrt@STOCH_LIGHTS;GI_CACHE.comp.glsl",
                                        "internal/rt_reflections_hwrt@STOCH_LIGHTS;GI_CACHE;NO_SUBGROUP.comp.glsl"),
                        subgroup_select("internal/rt_reflections_swrt@STOCH_LIGHTS;GI_CACHE.comp.glsl",
                                        "internal/rt_reflections_swrt@STOCH_LIGHTS;GI_CACHE;NO_SUBGROUP.comp.glsl")));
        assert(rt_reflections_prog->ready());

        if (!pi_rt_reflections_[2].Init(ctx.api_ctx(), std::move(rt_reflections_prog), ctx.log())) {
            ctx.log()->Error("ExRTReflections: Failed to initialize pipeline!");
        }

        ///

        rt_reflections_prog = sh.LoadProgram(
            ctx, hwrt_select(subgroup_select("internal/rt_reflections_hwrt@FOUR_BOUNCES.comp.glsl",
                                             "internal/rt_reflections_hwrt@FOUR_BOUNCES;NO_SUBGROUP.comp.glsl"),
                             subgroup_select("internal/rt_reflections_swrt@FOUR_BOUNCES.comp.glsl",
                                             "internal/rt_reflections_swrt@FOUR_BOUNCES;NO_SUBGROUP.comp.glsl")));
        assert(rt_reflections_prog->ready());

        if (!pi_rt_reflections_4bounce_[0].Init(ctx.api_ctx(), std::move(rt_reflections_prog), ctx.log())) {
            ctx.log()->Error("ExRTReflections: Failed to initialize pipeline!");
        }

        rt_reflections_prog = sh.LoadProgram(
            ctx,
            hwrt_select(subgroup_select("internal/rt_reflections_hwrt@FOUR_BOUNCES;GI_CACHE.comp.glsl",
                                        "internal/rt_reflections_hwrt@FOUR_BOUNCES;GI_CACHE;NO_SUBGROUP.comp.glsl"),
                        subgroup_select("internal/rt_reflections_swrt@FOUR_BOUNCES;GI_CACHE.comp.glsl",
                                        "internal/rt_reflections_swrt@FOUR_BOUNCES;GI_CACHE;NO_SUBGROUP.comp.glsl")));
        assert(rt_reflections_prog->ready());

        if (!pi_rt_reflections_4bounce_[1].Init(ctx.api_ctx(), std::move(rt_reflections_prog), ctx.log())) {
            ctx.log()->Error("ExRTReflections: Failed to initialize pipeline!");
        }

        rt_reflections_prog = sh.LoadProgram(
            ctx,
            hwrt_select(subgroup_select(
                            "internal/rt_reflections_hwrt@STOCH_LIGHTS;FOUR_BOUNCES;GI_CACHE.comp.glsl",
                            "internal/rt_reflections_hwrt@STOCH_LIGHTS;FOUR_BOUNCES;GI_CACHE;NO_SUBGROUP.comp.glsl"),
                        subgroup_select(
                            "internal/rt_reflections_swrt@STOCH_LIGHTS;FOUR_BOUNCES;GI_CACHE.comp.glsl",
                            "internal/rt_reflections_swrt@STOCH_LIGHTS;FOUR_BOUNCES;GI_CACHE;NO_SUBGROUP.comp.glsl")));
        assert(rt_reflections_prog->ready());

        if (!pi_rt_reflections_4bounce_[2].Init(ctx.api_ctx(), std::move(rt_reflections_prog), ctx.log())) {
            ctx.log()->Error("ExRTReflections: Failed to initialize pipeline!");
        }
        initialized_ = true;
    }
}