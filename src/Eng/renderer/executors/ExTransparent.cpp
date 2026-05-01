#include "ExTransparent.h"

#include <Ren/Context.h>

#include "../../utils/ShaderLoader.h"
#include "../Renderer_Structs.h"
#include "../framegraph/FgBuilder.h"

void Eng::ExTransparent::Execute(const FgContext &fg) {
    const Ren::ImageRWHandle color = fg.AccessRWImage(args_->color);
    const Ren::ImageRWHandle normal = fg.AccessRWImage(args_->normal);
    const Ren::ImageRWHandle spec = fg.AccessRWImage(args_->spec);
    const Ren::ImageRWHandle depth = fg.AccessRWImage(args_->depth);

    LazyInit(fg, color, normal, spec, depth);
    DrawTransparent(fg, color, normal, spec, depth);
}

void Eng::ExTransparent::DrawTransparent(const FgContext &fg, const Ren::ImageRWHandle color,
                                         const Ren::ImageRWHandle normal, const Ren::ImageRWHandle spec,
                                         const Ren::ImageRWHandle depth) {
    const Ren::BufferROHandle instances = fg.AccessROBuffer(args_->instances);
    const Ren::BufferROHandle instance_indices = fg.AccessROBuffer(args_->instance_indices);
    const Ren::BufferROHandle unif_shared_data = fg.AccessROBuffer(args_->shared_data);
    const Ren::BufferROHandle materials = fg.AccessROBuffer(args_->materials);
    const Ren::BufferROHandle cells = fg.AccessROBuffer(args_->cells);
    const Ren::BufferROHandle items = fg.AccessROBuffer(args_->items);
    const Ren::BufferROHandle lights = fg.AccessROBuffer(args_->lights);
    const Ren::BufferROHandle decals = fg.AccessROBuffer(args_->decals);

    const Ren::ImageROHandle shad = fg.AccessROImage(args_->shadow_depth);
    const Ren::ImageROHandle ssao = fg.AccessROImage(args_->ssao);

    DrawTransparent_Simple(fg, instances, instance_indices, unif_shared_data, materials, cells, items, lights, decals,
                           shad, color, normal, spec, depth, ssao);
}

void Eng::ExTransparent::LazyInit(const FgContext &fg, const Ren::ImageRWHandle color,
                                  const Ren::ImageRWHandle normal, const Ren::ImageRWHandle spec,
                                  const Ren::ImageRWHandle depth) {
    const Ren::RenderTarget color_targets[] = {{color, Ren::eLoadOp::Load, Ren::eStoreOp::Store},
                                               {normal, Ren::eLoadOp::Load, Ren::eStoreOp::Store},
                                               {spec, Ren::eLoadOp::Load, Ren::eStoreOp::Store}};
    const Ren::RenderTarget depth_target = {depth, Ren::eLoadOp::Load, Ren::eStoreOp::Store, Ren::eLoadOp::Load,
                                            Ren::eStoreOp::Store};

    if (!initialized_) {
        auto &sh = fg.sh();
        rp_transparent_ = sh.FindOrCreateRenderPass(depth_target, color_targets);

        [[maybe_unused]] const int buf1_stride = 16, buf2_stride = 16;

        { // VertexInput for main drawing (uses all attributes)
            const Ren::VtxAttribDesc attribs[] = {
                {0, VTX_POS_LOC, 3, Ren::eType::Float32, buf1_stride, 0, 0},
                {0, VTX_UV1_LOC, 2, Ren::eType::Float16, buf1_stride, 0, 3 * sizeof(float)},
                {1, VTX_NOR_LOC, 4, Ren::eType::Int16_snorm, buf1_stride, 0, 0},
                {1, VTX_TAN_LOC, 2, Ren::eType::Int16_snorm, buf1_stride, 0, 4 * sizeof(uint16_t)},
                {1, VTX_AUX_LOC, 1, Ren::eType::Uint32, buf1_stride, 0, 6 * sizeof(uint16_t)}};
            draw_pass_vi_ = sh.FindOrCreateVertexInput(attribs);
        }

#if defined(REN_VK_BACKEND)
        InitDescrSetLayout();
#endif

        initialized_ = true;
    }

    /*if (moments_b0_.id && moments_z_and_z2_.id && moments_z3_and_z4_.id) {
        const Ren::ImgHandle attachments[] = {moments_b0_, moments_z_and_z2_,
                                              moments_z3_and_z4_};
        if (!moments_fb_.Setup(attachments, 3, depth_tex.ref->handle(), {},
                               view_state_->is_multisampled)) {
            ctx.log()->Error("ExTransparent: moments_fb_ init failed!");
        }
    }*/
}