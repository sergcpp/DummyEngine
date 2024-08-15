#include "RpRTReflections.h"

#include <Ren/Context.h>
#include <Ren/RastState.h>
#include <Ren/Texture.h>

#include "../../utils/ShaderLoader.h"
#include "../PrimDraw.h"
#include "../Renderer_Structs.h"

void Eng::RpRTReflections::Execute(RpBuilder &builder) {
    LazyInit(builder.ctx(), builder.sh());
    if (builder.ctx().capabilities.hwrt) {
        Execute_HWRT(builder);
    } else {
        Execute_SWRT(builder);
    }
}

void Eng::RpRTReflections::LazyInit(Ren::Context &ctx, Eng::ShaderLoader &sh) {
    if (!initialized_) {
        Ren::ProgramRef rt_reflections_prog;
        if (layered_) {
            rt_reflections_prog =
                sh.LoadProgram(ctx, ctx.capabilities.hwrt ? "internal/rt_reflections_hwrt.comp.glsl@LAYERED"
                                                          : "internal/rt_reflections_swrt.comp.glsl@LAYERED");
        } else {
            rt_reflections_prog = sh.LoadProgram(ctx, ctx.capabilities.hwrt ? "internal/rt_reflections_hwrt.comp.glsl"
                                                                            : "internal/rt_reflections_swrt.comp.glsl");
        }
        assert(rt_reflections_prog->ready());

        if (!pi_rt_reflections_[0].Init(ctx.api_ctx(), std::move(rt_reflections_prog), ctx.log())) {
            ctx.log()->Error("RpRTReflections: Failed to initialize pipeline!");
        }

        if (layered_) {
            rt_reflections_prog =
                sh.LoadProgram(ctx, ctx.capabilities.hwrt ? "internal/rt_reflections_hwrt.comp.glsl@LAYERED;GI_CACHE"
                                                          : "internal/rt_reflections_swrt.comp.glsl@LAYERED;GI_CACHE");
        } else {
            rt_reflections_prog =
                sh.LoadProgram(ctx, ctx.capabilities.hwrt ? "internal/rt_reflections_hwrt.comp.glsl@GI_CACHE"
                                                          : "internal/rt_reflections_swrt.comp.glsl@GI_CACHE");
        }
        assert(rt_reflections_prog->ready());

        if (!pi_rt_reflections_[1].Init(ctx.api_ctx(), std::move(rt_reflections_prog), ctx.log())) {
            ctx.log()->Error("RpRTReflections: Failed to initialize pipeline!");
        }

        rt_reflections_prog =
            sh.LoadProgram(ctx, ctx.capabilities.hwrt ? "internal/rt_reflections_hwrt.comp.glsl@STOCH_LIGHTS;GI_CACHE"
                                                      : "internal/rt_reflections_swrt.comp.glsl@STOCH_LIGHTS;GI_CACHE");
        assert(rt_reflections_prog->ready());

        if (!pi_rt_reflections_[2].Init(ctx.api_ctx(), std::move(rt_reflections_prog), ctx.log())) {
            ctx.log()->Error("RpRTReflections: Failed to initialize pipeline!");
        }

        ///

        rt_reflections_prog =
            sh.LoadProgram(ctx, ctx.capabilities.hwrt ? "internal/rt_reflections_hwrt.comp.glsl@FOUR_BOUNCES"
                                                      : "internal/rt_reflections_swrt.comp.glsl@FOUR_BOUNCES");
        assert(rt_reflections_prog->ready());

        if (!pi_rt_reflections_4bounce_[0].Init(ctx.api_ctx(), std::move(rt_reflections_prog), ctx.log())) {
            ctx.log()->Error("RpRTReflections: Failed to initialize pipeline!");
        }

        rt_reflections_prog =
            sh.LoadProgram(ctx, ctx.capabilities.hwrt ? "internal/rt_reflections_hwrt.comp.glsl@FOUR_BOUNCES;GI_CACHE"
                                                      : "internal/rt_reflections_swrt.comp.glsl@FOUR_BOUNCES;GI_CACHE");
        assert(rt_reflections_prog->ready());

        if (!pi_rt_reflections_4bounce_[1].Init(ctx.api_ctx(), std::move(rt_reflections_prog), ctx.log())) {
            ctx.log()->Error("RpRTReflections: Failed to initialize pipeline!");
        }

        rt_reflections_prog = sh.LoadProgram(
            ctx, ctx.capabilities.hwrt ? "internal/rt_reflections_hwrt.comp.glsl@STOCH_LIGHTS;FOUR_BOUNCES;GI_CACHE"
                                       : "internal/rt_reflections_swrt.comp.glsl@STOCH_LIGHTS;FOUR_BOUNCES;GI_CACHE");
        assert(rt_reflections_prog->ready());

        if (!pi_rt_reflections_4bounce_[2].Init(ctx.api_ctx(), std::move(rt_reflections_prog), ctx.log())) {
            ctx.log()->Error("RpRTReflections: Failed to initialize pipeline!");
        }
        initialized_ = true;
    }
}