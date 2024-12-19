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
            pi_sample_lights_ = sh.LoadPipeline(subgroup_select("internal/sample_lights@HWRT.comp.glsl",
                                                                "internal/sample_lights@HWRT;NO_SUBGROUP.comp.glsl"));
        } else {
            pi_sample_lights_ = sh.LoadPipeline(
                subgroup_select("internal/sample_lights.comp.glsl", "internal/sample_lights@NO_SUBGROUP.comp.glsl"));
        }
        initialized_ = true;
    }
}