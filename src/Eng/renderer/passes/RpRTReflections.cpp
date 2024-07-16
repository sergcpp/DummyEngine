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
        Execute_HWRT_Inline(builder);
        // Execute_HWRT_Pipeline(builder);
    } else {
        Execute_SWRT(builder);
    }
}

void Eng::RpRTReflections::LazyInit(Ren::Context &ctx, Eng::ShaderLoader &sh) {
    if (!initialized) {
#if defined(USE_VK_RENDER)
        if (ctx.capabilities.hwrt) {
            Ren::ProgramRef rt_reflections_prog =
                sh.LoadProgram(ctx, "internal/rt_reflections.rgen.glsl", "internal/rt_reflections.rchit.glsl",
                               "internal/rt_reflections.rahit.glsl", "internal/rt_reflections.rmiss.glsl", {});
            assert(rt_reflections_prog->ready());

            if (!pi_rt_reflections_.Init(ctx.api_ctx(), std::move(rt_reflections_prog), ctx.log())) {
                ctx.log()->Error("RpRTReflections: Failed to initialize pipeline!");
            }

            Ren::ProgramRef rt_reflections_inline_prog;
            if (pass_data_->oit_depth_buf) {
                rt_reflections_inline_prog = sh.LoadProgram(ctx, "internal/rt_reflections_hwrt.comp.glsl@LAYERED");
            } else {
                rt_reflections_inline_prog = sh.LoadProgram(ctx, "internal/rt_reflections_hwrt.comp.glsl");
            }
            assert(rt_reflections_inline_prog->ready());

            if (!pi_rt_reflections_inline_[0].Init(ctx.api_ctx(), std::move(rt_reflections_inline_prog), ctx.log())) {
                ctx.log()->Error("RpRTReflections: Failed to initialize pipeline!");
            }

            if (pass_data_->oit_depth_buf) {
                rt_reflections_inline_prog =
                    sh.LoadProgram(ctx, "internal/rt_reflections_hwrt.comp.glsl@LAYERED;GI_CACHE");
            } else {
                rt_reflections_inline_prog = sh.LoadProgram(ctx, "internal/rt_reflections_hwrt.comp.glsl@GI_CACHE");
            }
            assert(rt_reflections_inline_prog->ready());

            if (!pi_rt_reflections_inline_[1].Init(ctx.api_ctx(), std::move(rt_reflections_inline_prog), ctx.log())) {
                ctx.log()->Error("RpRTReflections: Failed to initialize pipeline!");
            }

            rt_reflections_inline_prog = sh.LoadProgram(ctx, "internal/rt_reflections_hwrt.comp.glsl@FOUR_BOUNCES");
            assert(rt_reflections_inline_prog->ready());

            if (!pi_rt_reflections_4bounce_inline_[0].Init(ctx.api_ctx(), std::move(rt_reflections_inline_prog),
                                                           ctx.log())) {
                ctx.log()->Error("RpRTReflections: Failed to initialize pipeline!");
            }

            rt_reflections_inline_prog =
                sh.LoadProgram(ctx, "internal/rt_reflections_hwrt.comp.glsl@FOUR_BOUNCES;GI_CACHE");
            assert(rt_reflections_inline_prog->ready());

            if (!pi_rt_reflections_4bounce_inline_[1].Init(ctx.api_ctx(), std::move(rt_reflections_inline_prog),
                                                           ctx.log())) {
                ctx.log()->Error("RpRTReflections: Failed to initialize pipeline!");
            }
        }
#endif

        Ren::ProgramRef rt_reflections_swrt_prog = sh.LoadProgram(ctx, "internal/rt_reflections_swrt.comp.glsl");
        assert(rt_reflections_swrt_prog->ready());

        if (!pi_rt_reflections_swrt_[0].Init(ctx.api_ctx(), std::move(rt_reflections_swrt_prog), ctx.log())) {
            ctx.log()->Error("RpRTReflections: Failed to initialize pipeline!");
        }

        rt_reflections_swrt_prog = sh.LoadProgram(ctx, "internal/rt_reflections_swrt.comp.glsl@GI_CACHE");
        assert(rt_reflections_swrt_prog->ready());

        if (!pi_rt_reflections_swrt_[1].Init(ctx.api_ctx(), std::move(rt_reflections_swrt_prog), ctx.log())) {
            ctx.log()->Error("RpRTReflections: Failed to initialize pipeline!");
        }

        rt_reflections_swrt_prog = sh.LoadProgram(ctx, "internal/rt_reflections_swrt.comp.glsl@LAYERED");
        assert(rt_reflections_swrt_prog->ready());

        if (!pi_rt_reflections_swrt_[2].Init(ctx.api_ctx(), std::move(rt_reflections_swrt_prog), ctx.log())) {
            ctx.log()->Error("RpRTReflections: Failed to initialize pipeline!");
        }

        rt_reflections_swrt_prog = sh.LoadProgram(ctx, "internal/rt_reflections_swrt.comp.glsl@LAYERED;GI_CACHE");
        assert(rt_reflections_swrt_prog->ready());

        if (!pi_rt_reflections_swrt_[3].Init(ctx.api_ctx(), std::move(rt_reflections_swrt_prog), ctx.log())) {
            ctx.log()->Error("RpRTReflections: Failed to initialize pipeline!");
        }

        ///

        rt_reflections_swrt_prog = sh.LoadProgram(ctx, "internal/rt_reflections_swrt.comp.glsl@FOUR_BOUNCES");
        assert(rt_reflections_swrt_prog->ready());

        if (!pi_rt_reflections_4bounce_swrt_[0].Init(ctx.api_ctx(), std::move(rt_reflections_swrt_prog), ctx.log())) {
            ctx.log()->Error("RpRTReflections: Failed to initialize pipeline!");
        }

        rt_reflections_swrt_prog = sh.LoadProgram(ctx, "internal/rt_reflections_swrt.comp.glsl@FOUR_BOUNCES;GI_CACHE");
        assert(rt_reflections_swrt_prog->ready());

        if (!pi_rt_reflections_4bounce_swrt_[1].Init(ctx.api_ctx(), std::move(rt_reflections_swrt_prog), ctx.log())) {
            ctx.log()->Error("RpRTReflections: Failed to initialize pipeline!");
        }

        initialized = true;
    }
}