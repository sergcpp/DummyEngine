#include "RpDebugRT.h"

#include <Ren/Context.h>
#include <Ren/RastState.h>
#include <Ren/Texture.h>

#include "../../Utils/ShaderLoader.h"
#include "../Renderer_Structs.h"
#include "../Shaders/rt_debug_interface.h"

void RpDebugRT::Execute(RpBuilder &builder) {
    LazyInit(builder.ctx(), builder.sh());

#if !defined(USE_GL_RENDER)
    if (builder.ctx().capabilities.raytracing) {
        Execute_HWRT(builder);
    } else
#endif
    {
        Execute_SWRT(builder);
    }
}

void RpDebugRT::LazyInit(Ren::Context &ctx, Eng::ShaderLoader &sh) {
    if (!initialized) {
#if defined(USE_VK_RENDER)
        if (ctx.capabilities.raytracing) {
            Ren::ProgramRef debug_hwrt_prog =
                sh.LoadProgram(ctx, "rt_debug", "internal/rt_debug.rgen.glsl", "internal/rt_debug.rchit.glsl",
                               "internal/rt_debug.rahit.glsl", "internal/rt_debug.rmiss.glsl", nullptr);
            assert(debug_hwrt_prog->ready());

            if (!pi_debug_hwrt_.Init(ctx.api_ctx(), debug_hwrt_prog, ctx.log())) {
                ctx.log()->Error("RpDebugRT: Failed to initialize pipeline!");
            }
        }
#endif
        Ren::ProgramRef debug_swrt_prog = sh.LoadProgram(ctx, "rt_debug_swrt", "internal/rt_debug_swrt.comp.glsl");
        assert(debug_swrt_prog->ready());

        if (!pi_debug_swrt_.Init(ctx.api_ctx(), debug_swrt_prog, ctx.log())) {
            ctx.log()->Error("RpDebugRT: Failed to initialize pipeline!");
        }

        initialized = true;
    }
}
