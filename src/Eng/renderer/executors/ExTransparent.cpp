#include "ExTransparent.h"

#include <Ren/Context.h>

#include "../../utils/ShaderLoader.h"
#include "../Renderer_Structs.h"

void Eng::ExTransparent::Execute(FgContext &fg) {
    Ren::WeakBufRef vtx_buf1 = fg.AccessROBufferRef(vtx_buf1_);
    Ren::WeakBufRef vtx_buf2 = fg.AccessROBufferRef(vtx_buf2_);
    Ren::WeakBufRef ndx_buf = fg.AccessROBufferRef(ndx_buf_);

    Ren::WeakTexRef color_tex = fg.AccessRWTextureRef(color_tex_);
    Ren::WeakTexRef normal_tex = fg.AccessRWTextureRef(normal_tex_);
    Ren::WeakTexRef spec_tex = fg.AccessRWTextureRef(spec_tex_);
    Ren::WeakTexRef depth_tex = fg.AccessRWTextureRef(depth_tex_);

    LazyInit(fg.ren_ctx(), fg.sh(), vtx_buf1, vtx_buf2, ndx_buf, color_tex, normal_tex, spec_tex, depth_tex);
    DrawTransparent(fg, color_tex);
}

void Eng::ExTransparent::DrawTransparent(FgContext &fg, const Ren::WeakTexRef &color_tex) {
    const Ren::Buffer &instances_buf = fg.AccessROBuffer(instances_buf_);
    const Ren::Buffer &instance_indices_buf = fg.AccessROBuffer(instance_indices_buf_);
    const Ren::Buffer &unif_shared_data_buf = fg.AccessROBuffer(shared_data_buf_);
    const Ren::Buffer &materials_buf = fg.AccessROBuffer(materials_buf_);
    const Ren::Buffer &cells_buf = fg.AccessROBuffer(cells_buf_);
    const Ren::Buffer &items_buf = fg.AccessROBuffer(items_buf_);
    const Ren::Buffer &lights_buf = fg.AccessROBuffer(lights_buf_);
    const Ren::Buffer &decals_buf = fg.AccessROBuffer(decals_buf_);

    const Ren::Texture &shad_tex = fg.AccessROTexture(shad_tex_);
    const Ren::Texture &ssao_tex = fg.AccessROTexture(ssao_tex_);

    DrawTransparent_Simple(fg, instances_buf, instance_indices_buf, unif_shared_data_buf, materials_buf, cells_buf,
                           items_buf, lights_buf, decals_buf, shad_tex, color_tex, ssao_tex);
}

void Eng::ExTransparent::LazyInit(Ren::Context &ctx, Eng::ShaderLoader &sh, const Ren::WeakBufRef &vtx_buf1,
                                  const Ren::WeakBufRef &vtx_buf2, const Ren::WeakBufRef &ndx_buf,
                                  const Ren::WeakTexRef &color_tex, const Ren::WeakTexRef &normal_tex,
                                  const Ren::WeakTexRef &spec_tex, const Ren::WeakTexRef &depth_tex) {
    const Ren::RenderTarget color_targets[] = {{color_tex, Ren::eLoadOp::Load, Ren::eStoreOp::Store},
                                               {normal_tex, Ren::eLoadOp::Load, Ren::eStoreOp::Store},
                                               {spec_tex, Ren::eLoadOp::Load, Ren::eStoreOp::Store}};
    const Ren::RenderTarget depth_target = {depth_tex, Ren::eLoadOp::Load, Ren::eStoreOp::Store, Ren::eLoadOp::Load,
                                            Ren::eStoreOp::Store};

    if (!initialized) {
        rp_transparent_ = sh.LoadRenderPass(depth_target, color_targets);

        [[maybe_unused]] const int buf1_stride = 16, buf2_stride = 16;

        { // VertexInput for main drawing (uses all attributes)
            const Ren::VtxAttribDesc attribs[] = {
                // Attributes from buffer 1
                {vtx_buf1, VTX_POS_LOC, 3, Ren::eType::Float32, buf1_stride, 0},
                {vtx_buf1, VTX_UV1_LOC, 2, Ren::eType::Float16, buf1_stride, 3 * sizeof(float)},
                // Attributes from buffer 2
                {vtx_buf2, VTX_NOR_LOC, 4, Ren::eType::Int16_snorm, buf1_stride, 0},
                {vtx_buf2, VTX_TAN_LOC, 2, Ren::eType::Int16_snorm, buf1_stride, 4 * sizeof(uint16_t)},
                {vtx_buf2, VTX_AUX_LOC, 1, Ren::eType::Uint32, buf1_stride, 6 * sizeof(uint16_t)}};
            draw_pass_vi_ = sh.LoadVertexInput(attribs, ndx_buf);
        }

        api_ctx_ = ctx.api_ctx();
#if defined(REN_VK_BACKEND)
        InitDescrSetLayout();
#endif

        initialized = true;
    }

    fb_to_use_ = (fb_to_use_ + 1) % 2;

    if (!transparent_draw_fb_[ctx.backend_frame()][fb_to_use_].Setup(
            ctx.api_ctx(), *rp_transparent_, color_tex->params.w, color_tex->params.h, depth_target, depth_target,
            color_targets, ctx.log())) {
        ctx.log()->Error("ExTransparent: transparent_draw_fb_ init failed!");
    }

#if !defined(REN_VK_BACKEND)
    if (!color_only_fb_[fb_to_use_].Setup(ctx.api_ctx(), {}, color_tex->params.w, color_tex->params.h, color_tex,
                                          depth_tex, depth_tex, false, ctx.log())) {
        ctx.log()->Error("ExTransparent: color_only_fb_ init failed!");
    }

    // if (!resolved_fb_.Setup(ctx.api_ctx(), {}, transparent_tex.desc.w, transparent_tex.desc.h, transparent_tex.ref,
    // {},
    //                         {}, false)) {
    //     ctx.log()->Error("ExTransparent: resolved_fb_ init failed!");
    // }
#endif

    /*if (moments_b0_.id && moments_z_and_z2_.id && moments_z3_and_z4_.id) {
        const Ren::TexHandle attachments[] = {moments_b0_, moments_z_and_z2_,
                                              moments_z3_and_z4_};
        if (!moments_fb_.Setup(attachments, 3, depth_tex.ref->handle(), {},
                               view_state_->is_multisampled)) {
            ctx.log()->Error("ExTransparent: moments_fb_ init failed!");
        }
    }*/
}