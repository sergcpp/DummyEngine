#include "ExDepthHierarchy.h"

#include <Ren/Context.h>

#include "../../utils/ShaderLoader.h"
#include "../Renderer_Structs.h"

void Eng::ExDepthHierarchy::LazyInit(Ren::Context &ctx, Eng::ShaderLoader &sh) {
    if (!initialized) {
        auto subgroup_select = [&ctx](std::string_view subgroup_shader, std::string_view nosubgroup_shader) {
            return ctx.capabilities.subgroup ? subgroup_shader : nosubgroup_shader;
        };

        Ren::ProgramRef depth_hierarchy_prog =
            sh.LoadProgram(ctx, subgroup_select("internal/depth_hierarchy@MIPS_7.comp.glsl",
                                                "internal/depth_hierarchy@MIPS_7;NO_SUBGROUP.comp.glsl"));
        assert(depth_hierarchy_prog->ready());

        if (!pi_depth_hierarchy_.Init(ctx.api_ctx(), std::move(depth_hierarchy_prog), ctx.log())) {
            ctx.log()->Error("ExDepthHierarchy: failed to initialize pipeline!");
        }

        initialized = true;
    }
}
