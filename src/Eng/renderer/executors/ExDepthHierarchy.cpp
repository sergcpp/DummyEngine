#include "ExDepthHierarchy.h"

#include <Ren/Context.h>
#include <Ren/Program.h>

#include "../../utils/ShaderLoader.h"
#include "../Renderer_Structs.h"

void Eng::ExDepthHierarchy::LazyInit(Ren::Context &ctx, Eng::ShaderLoader &sh) {
    if (!initialized) {
        Ren::ProgramRef depth_hierarchy_prog = sh.LoadProgram(ctx, "internal/depth_hierarchy@MIPS_7.comp.glsl");
        assert(depth_hierarchy_prog->ready());

        if (!pi_depth_hierarchy_.Init(ctx.api_ctx(), std::move(depth_hierarchy_prog), ctx.log())) {
            ctx.log()->Error("ExDepthHierarchy: failed to initialize pipeline!");
        }

        initialized = true;
    }
}
