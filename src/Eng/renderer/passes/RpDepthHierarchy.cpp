#include "RpDepthHierarchy.h"

#include <Ren/Context.h>
#include <Ren/Program.h>

#include "../Renderer_Structs.h"
#include "../../utils/ShaderLoader.h"

void Eng::RpDepthHierarchy::LazyInit(Ren::Context &ctx, Eng::ShaderLoader &sh) {
    if (!initialized) {
        Ren::ProgramRef depth_hierarchy_prog =
            sh.LoadProgram(ctx, "depth_hierarchy", "internal/depth_hierarchy.comp.glsl@MIPS_7");
        assert(depth_hierarchy_prog->ready());

        if (!pi_depth_hierarchy_.Init(ctx.api_ctx(), std::move(depth_hierarchy_prog), ctx.log())) {
            ctx.log()->Error("RpDepthHierarchy: failed to initialize pipeline!");
        }

        initialized = true;
    }
}
