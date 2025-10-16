#include "ExTransparent.h"

#include <Ren/Context.h>

#include "../../utils/ShaderLoader.h"
#include "../Renderer_Structs.h"

void Eng::ExTransparent::Execute(FgContext &fg) {
    FgAllocBuf &vtx_buf1 = fg.AccessROBuffer(vtx_buf1_);
    FgAllocBuf &vtx_buf2 = fg.AccessROBuffer(vtx_buf2_);
    FgAllocBuf &ndx_buf = fg.AccessROBuffer(ndx_buf_);

    FgAllocTex &color_tex = fg.AccessRWTexture(color_tex_);
    FgAllocTex &normal_tex = fg.AccessRWTexture(normal_tex_);
    FgAllocTex &spec_tex = fg.AccessRWTexture(spec_tex_);
    FgAllocTex &depth_tex = fg.AccessRWTexture(depth_tex_);

    LazyInit(fg.ren_ctx(), fg.sh(), vtx_buf1, vtx_buf2, ndx_buf, color_tex, normal_tex, spec_tex, depth_tex);
    DrawTransparent(fg, color_tex);
}

void Eng::ExTransparent::DrawTransparent(FgContext &fg, FgAllocTex &color_tex) {
    FgAllocBuf &instances_buf = fg.AccessROBuffer(instances_buf_);
    FgAllocBuf &instance_indices_buf = fg.AccessROBuffer(instance_indices_buf_);
    FgAllocBuf &unif_shared_data_buf = fg.AccessROBuffer(shared_data_buf_);
    FgAllocBuf &materials_buf = fg.AccessROBuffer(materials_buf_);
    FgAllocBuf &cells_buf = fg.AccessROBuffer(cells_buf_);
    FgAllocBuf &items_buf = fg.AccessROBuffer(items_buf_);
    FgAllocBuf &lights_buf = fg.AccessROBuffer(lights_buf_);
    FgAllocBuf &decals_buf = fg.AccessROBuffer(decals_buf_);

    FgAllocTex &shad_tex = fg.AccessROTexture(shad_tex_);
    FgAllocTex &ssao_tex = fg.AccessROTexture(ssao_tex_);

    DrawTransparent_Simple(fg, instances_buf, instance_indices_buf, unif_shared_data_buf, materials_buf, cells_buf,
                           items_buf, lights_buf, decals_buf, shad_tex, color_tex, ssao_tex);
}

void Eng::ExTransparent::LazyInit(Ren::Context &ctx, Eng::ShaderLoader &sh, FgAllocBuf &vtx_buf1, FgAllocBuf &vtx_buf2,
                                  FgAllocBuf &ndx_buf, FgAllocTex &color_tex, FgAllocTex &normal_tex,
                                  FgAllocTex &spec_tex, FgAllocTex &depth_tex) {
    const Ren::RenderTarget color_targets[] = {{color_tex.ref, Ren::eLoadOp::Load, Ren::eStoreOp::Store},
                                               {normal_tex.ref, Ren::eLoadOp::Load, Ren::eStoreOp::Store},
                                               {spec_tex.ref, Ren::eLoadOp::Load, Ren::eStoreOp::Store}};
    const Ren::RenderTarget depth_target = {depth_tex.ref, Ren::eLoadOp::Load, Ren::eStoreOp::Store, Ren::eLoadOp::Load,
                                            Ren::eStoreOp::Store};

    if (!initialized) {
        rp_transparent_ = sh.LoadRenderPass(depth_target, color_targets);

        [[maybe_unused]] const int buf1_stride = 16, buf2_stride = 16;

        { // VertexInput for main drawing (uses all attributes)
            const Ren::VtxAttribDesc attribs[] = {
                // Attributes from buffer 1
                {vtx_buf1.ref, VTX_POS_LOC, 3, Ren::eType::Float32, buf1_stride, 0},
                {vtx_buf1.ref, VTX_UV1_LOC, 2, Ren::eType::Float16, buf1_stride, 3 * sizeof(float)},
                // Attributes from buffer 2
                {vtx_buf2.ref, VTX_NOR_LOC, 4, Ren::eType::Int16_snorm, buf1_stride, 0},
                {vtx_buf2.ref, VTX_TAN_LOC, 2, Ren::eType::Int16_snorm, buf1_stride, 4 * sizeof(uint16_t)},
                {vtx_buf2.ref, VTX_AUX_LOC, 1, Ren::eType::Uint32, buf1_stride, 6 * sizeof(uint16_t)}};
            draw_pass_vi_ = sh.LoadVertexInput(attribs, ndx_buf.ref);
        }

        api_ctx_ = ctx.api_ctx();
#if defined(REN_VK_BACKEND)
        InitDescrSetLayout();
#endif

        initialized = true;
    }

    fb_to_use_ = (fb_to_use_ + 1) % 2;

    if (!transparent_draw_fb_[ctx.backend_frame()][fb_to_use_].Setup(ctx.api_ctx(), *rp_transparent_, color_tex.desc.w,
                                                                     color_tex.desc.h, depth_target, depth_target,
                                                                     color_targets, ctx.log())) {
        ctx.log()->Error("ExTransparent: transparent_draw_fb_ init failed!");
    }

#if !defined(REN_VK_BACKEND)
    if (!color_only_fb_[fb_to_use_].Setup(ctx.api_ctx(), {}, color_tex.desc.w, color_tex.desc.h, color_tex.ref,
                                          depth_tex.ref, depth_tex.ref, false, ctx.log())) {
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