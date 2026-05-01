#include "ExOpaque.h"

#include <Ren/Context.h>

#include "../../utils/ShaderLoader.h"
#include "../Renderer_Structs.h"
#include "../framegraph/FgBuilder.h"

void Eng::ExOpaque::Execute(const FgContext &fg) {
    const Ren::ImageRWHandle color = fg.AccessRWImage(args_->color);
    const Ren::ImageRWHandle normal = fg.AccessRWImage(args_->normal);
    const Ren::ImageRWHandle spec = fg.AccessRWImage(args_->spec);
    const Ren::ImageRWHandle depth = fg.AccessRWImage(args_->depth);

    LazyInit(fg, color, normal, spec, depth);
    DrawOpaque(fg, color, normal, spec, depth);
}

void Eng::ExOpaque::LazyInit(const FgContext &fg, const Ren::ImageRWHandle color,
                             const Ren::ImageRWHandle normal, const Ren::ImageRWHandle spec,
                             const Ren::ImageRWHandle depth) {
    const Ren::RenderTarget color_targets[] = {{color, Ren::eLoadOp::Load, Ren::eStoreOp::Store},
                                               {normal, Ren::eLoadOp::Load, Ren::eStoreOp::Store},
                                               {spec, Ren::eLoadOp::Load, Ren::eStoreOp::Store}};
    const Ren::RenderTarget depth_target = {depth, Ren::eLoadOp::Load, Ren::eStoreOp::Store, Ren::eLoadOp::Load,
                                            Ren::eStoreOp::Store};

    if (!initialized_) {
        auto &sh = fg.sh();
        const int buf1_stride = 16, buf2_stride = 16;

        { // VertexInput for main drawing (uses all attributes)
            const Ren::VtxAttribDesc attribs[] = {
                {0, VTX_POS_LOC, 3, Ren::eType::Float32, buf1_stride, 0, 0},
                {0, VTX_UV1_LOC, 2, Ren::eType::Float16, buf1_stride, 0, 3 * sizeof(float)},
                {1, VTX_NOR_LOC, 4, Ren::eType::Int16_snorm, buf2_stride, 0, 0},
                {1, VTX_TAN_LOC, 2, Ren::eType::Int16_snorm, buf2_stride, 0, 4 * sizeof(uint16_t)},
                {1, VTX_AUX_LOC, 1, Ren::eType::Uint32, buf2_stride, 0, 6 * sizeof(uint16_t)}};
            draw_pass_vi_ = sh.FindOrCreateVertexInput(attribs);
        }

        rp_opaque_ = sh.FindOrCreateRenderPass(depth_target, color_targets);

#if defined(REN_VK_BACKEND)
        InitDescrSetLayout();
#endif

        initialized_ = true;
    }
}