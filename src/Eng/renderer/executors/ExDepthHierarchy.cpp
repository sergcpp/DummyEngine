#include "ExDepthHierarchy.h"

#include <Ren/Context.h>

#include "../../utils/ShaderLoader.h"
#include "../Renderer_Structs.h"

Eng::ExDepthHierarchy::ExDepthHierarchy(FgContext &ctx, const view_state_t *view_state, const FgResRef depth_tex,
                                        const FgResRef atomic_counter, const FgResRef output_tex) {
    view_state_ = view_state;

    depth_tex_ = depth_tex;
    atomic_buf_ = atomic_counter;
    output_tex_ = output_tex;

    auto subgroup_select = [&ctx](std::string_view subgroup_shader, std::string_view nosubgroup_shader) {
        return ctx.ren_ctx().capabilities.subgroup ? subgroup_shader : nosubgroup_shader;
    };

    pi_depth_hierarchy_ = ctx.sh().LoadPipeline(subgroup_select(
        "internal/depth_hierarchy@MIPS_7.comp.glsl", "internal/depth_hierarchy@MIPS_7;NO_SUBGROUP.comp.glsl"));
}
