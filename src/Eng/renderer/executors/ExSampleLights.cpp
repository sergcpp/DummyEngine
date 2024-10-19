#include "ExSampleLights.h"

#include <Ren/Context.h>
#include <Ren/RastState.h>

#include "../../utils/ShaderLoader.h"

void Eng::ExSampleLights::Execute(FgBuilder &builder) {
    LazyInit(builder.ctx(), builder.sh());
    if (builder.ctx().capabilities.hwrt) {
        Execute_HWRT(builder);
    } else {
        Execute_SWRT(builder);
    }
}

void Eng::ExSampleLights::LazyInit(Ren::Context &ctx, Eng::ShaderLoader &sh) {
    if (!initialized_) {
        auto subgroup_select = [&ctx](std::string_view subgroup_shader, std::string_view nosubgroup_shader) {
            return ctx.capabilities.subgroup ? subgroup_shader : nosubgroup_shader;
        };

        if (ctx.capabilities.hwrt) {
            Ren::ProgramRef prog =
                sh.LoadProgram(ctx, subgroup_select("internal/sample_lights@HWRT.comp.glsl",
                                                    "internal/sample_lights@HWRT;NO_SUBGROUP.comp.glsl"));
            assert(prog->ready());

            if (!pi_sample_lights_.Init(ctx.api_ctx(), std::move(prog), ctx.log())) {
                ctx.log()->Error("ExSampleLights: Failed to initialize pipeline!");
            }
        } else {
            Ren::ProgramRef prog = sh.LoadProgram(ctx, subgroup_select("internal/sample_lights.comp.glsl",
                                                                       "internal/sample_lights@NO_SUBGROUP.comp.glsl"));
            assert(prog->ready());

            if (!pi_sample_lights_.Init(ctx.api_ctx(), std::move(prog), ctx.log())) {
                ctx.log()->Error("ExSampleLights: Failed to initialize pipeline!");
            }
        }
        initialized_ = true;
    }
}