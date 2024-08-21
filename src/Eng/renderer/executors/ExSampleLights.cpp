#include "ExSampleLights.h"

#include <Ren/Context.h>
#include <Ren/RastState.h>
#include <Ren/Texture.h>

#include "../../utils/ShaderLoader.h"
#include "../Renderer_Structs.h"

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
        if (ctx.capabilities.hwrt) {
            Ren::ProgramRef prog = sh.LoadProgram(ctx, "internal/sample_lights.comp.glsl@HWRT");
            assert(prog->ready());

            if (!pi_sample_lights_.Init(ctx.api_ctx(), std::move(prog), ctx.log())) {
                ctx.log()->Error("ExSampleLights: Failed to initialize pipeline!");
            }
        } else {
            Ren::ProgramRef prog = sh.LoadProgram(ctx, "internal/sample_lights.comp.glsl");
            assert(prog->ready());

            if (!pi_sample_lights_.Init(ctx.api_ctx(), std::move(prog), ctx.log())) {
                ctx.log()->Error("ExSampleLights: Failed to initialize pipeline!");
            }
        }
        initialized_ = true;
    }
}