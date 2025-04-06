#include "ExDebugRT.h"

#include <Ren/Context.h>

#include "../../utils/ShaderLoader.h"

Eng::ExDebugRT::ExDebugRT(FgBuilder &builder, const view_state_t *view_state, const BindlessTextureData *bindless_tex,
                          const Args *args) {
    view_state_ = view_state;
    bindless_tex_ = bindless_tex;
    args_ = args;
#if defined(REN_VK_BACKEND)
    if (builder.ctx().capabilities.hwrt) {
        Ren::ProgramRef debug_hwrt_prog =
            builder.sh().LoadProgram2("internal/rt_debug.rgen.glsl", "internal/rt_debug@GI_CACHE.rchit.glsl",
                                      "internal/rt_debug.rahit.glsl", "internal/rt_debug.rmiss.glsl", {});
        pi_debug_ = builder.sh().LoadPipeline(debug_hwrt_prog);
    } else
#endif
    {
        pi_debug_ = builder.sh().LoadPipeline("internal/rt_debug_swrt@GI_CACHE.comp.glsl");
    }
}

void Eng::ExDebugRT::Execute(FgBuilder &builder) {
#if !defined(REN_GL_BACKEND)
    if (builder.ctx().capabilities.hwrt) {
        Execute_HWRT(builder);
    } else
#endif
    {
        Execute_SWRT(builder);
    }
}
