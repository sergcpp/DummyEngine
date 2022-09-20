#include "RpRTReflections.h"

#include <Ren/Context.h>
#include <Ren/RastState.h>
#include <Ren/Texture.h>

#include "../../Utils/ShaderLoader.h"
#include "../PrimDraw.h"
#include "../Renderer_Structs.h"

#include "../assets/shaders/internal/rt_reflections_interface.glsl"

void RpRTReflections::ExecuteHWRTPipeline(RpBuilder &builder) { assert(false && "Not implemented!"); }

void RpRTReflections::ExecuteHWRTInline(RpBuilder& builder) { assert(false && "Not implemented!"); }

void RpRTReflections::ExecuteSWRT(RpBuilder &builder) { assert(false && "Not implemented!"); }

void RpRTReflections::LazyInit(Ren::Context &ctx, ShaderLoader &sh) {
    if (!initialized) {
        /*Ren::ProgramRef rt_reflections_prog = sh.LoadProgram(
            ctx, "rt_reflections", "internal/rt_reflections.rgen.glsl", "internal/rt_reflections.rchit.glsl",
            "internal/rt_reflections.rahit.glsl", "internal/rt_reflections.rmiss.glsl", nullptr);
        assert(rt_reflections_prog->ready());

        if (!pi_rt_reflections_.Init(ctx.api_ctx(), std::move(rt_reflections_prog), ctx.log())) {
            ctx.log()->Error("RpDebugRT: Failed to initialize pipeline!");
        }*/

        initialized = true;
    }
}