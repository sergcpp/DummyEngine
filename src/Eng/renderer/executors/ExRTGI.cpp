#include "ExRTGI.h"

#include <Ren/Context.h>
#include <Ren/RastState.h>
#include <Ren/Texture.h>

#include "../../utils/ShaderLoader.h"
#include "../Renderer_Structs.h"

void Eng::ExRTGI::Execute(FgBuilder &builder) {
    LazyInit(builder.ctx(), builder.sh());
    if (builder.ctx().capabilities.hwrt) {
        Execute_HWRT(builder);
    } else {
        Execute_SWRT(builder);
    }
}

void Eng::ExRTGI::LazyInit(Ren::Context &ctx, Eng::ShaderLoader &sh) {
    if (!initialized_) {
        auto hwrt_select = [&ctx](std::string_view hwrt_shader, std::string_view swrt_shader) {
            return ctx.capabilities.hwrt ? hwrt_shader : swrt_shader;
        };
        auto subgroup_select = [&ctx](std::string_view subgroup_shader, std::string_view nosubgroup_shader) {
            return ctx.capabilities.subgroup ? subgroup_shader : nosubgroup_shader;
        };

        Ren::ProgramRef rt_gi_prog =
            sh.LoadProgram(ctx, hwrt_select(subgroup_select("internal/rt_gi_hwrt@GI_CACHE.comp.glsl",
                                                            "internal/rt_gi_hwrt@GI_CACHE;NO_SUBGROUP.comp.glsl"),
                                            subgroup_select("internal/rt_gi_swrt@GI_CACHE.comp.glsl",
                                                            "internal/rt_gi_swrt@GI_CACHE;NO_SUBGROUP.comp.glsl")));
        assert(rt_gi_prog->ready());

        if (!pi_rt_gi_[0].Init(ctx.api_ctx(), std::move(rt_gi_prog), ctx.log())) {
            ctx.log()->Error("ExRTGI: Failed to initialize pipeline!");
        }

        rt_gi_prog = sh.LoadProgram(
            ctx, hwrt_select(subgroup_select("internal/rt_gi_hwrt@GI_CACHE;STOCH_LIGHTS.comp.glsl",
                                             "internal/rt_gi_hwrt@GI_CACHE;STOCH_LIGHTS;NO_SUBGROUP.comp.glsl"),
                             subgroup_select("internal/rt_gi_swrt@GI_CACHE;STOCH_LIGHTS.comp.glsl",
                                             "internal/rt_gi_swrt@GI_CACHE;STOCH_LIGHTS;NO_SUBGROUP.comp.glsl")));
        assert(rt_gi_prog->ready());

        if (!pi_rt_gi_[1].Init(ctx.api_ctx(), std::move(rt_gi_prog), ctx.log())) {
            ctx.log()->Error("ExRTGI: Failed to initialize pipeline!");
        }

        rt_gi_prog = sh.LoadProgram(
            ctx, hwrt_select(subgroup_select("internal/rt_gi_hwrt@TWO_BOUNCES;GI_CACHE.comp.glsl",
                                             "internal/rt_gi_hwrt@TWO_BOUNCES;GI_CACHE;NO_SUBGROUP.comp.glsl"),
                             subgroup_select("internal/rt_gi_swrt@TWO_BOUNCES;GI_CACHE.comp.glsl",
                                             "internal/rt_gi_swrt@TWO_BOUNCES;GI_CACHE;NO_SUBGROUP.comp.glsl")));
        assert(rt_gi_prog->ready());

        if (!pi_rt_gi_2bounce_[0].Init(ctx.api_ctx(), std::move(rt_gi_prog), ctx.log())) {
            ctx.log()->Error("ExRTGI: Failed to initialize pipeline!");
        }

        rt_gi_prog = sh.LoadProgram(
            ctx, hwrt_select(
                     subgroup_select("internal/rt_gi_hwrt@TWO_BOUNCES;GI_CACHE;STOCH_LIGHTS.comp.glsl",
                                     "internal/rt_gi_hwrt@TWO_BOUNCES;GI_CACHE;STOCH_LIGHTS;NO_SUBGROUP.comp.glsl"),
                     subgroup_select("internal/rt_gi_swrt@TWO_BOUNCES;GI_CACHE;STOCH_LIGHTS.comp.glsl",
                                     "internal/rt_gi_swrt@TWO_BOUNCES;GI_CACHE;STOCH_LIGHTS;NO_SUBGROUP.comp.glsl")));
        assert(rt_gi_prog->ready());

        if (!pi_rt_gi_2bounce_[1].Init(ctx.api_ctx(), std::move(rt_gi_prog), ctx.log())) {
            ctx.log()->Error("ExRTGI: Failed to initialize pipeline!");
        }
        initialized_ = true;
    }
}
