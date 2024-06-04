#include "RpTransparent.h"

#include <Ren/Context.h>

#include "../../utils/ShaderLoader.h"
#include "../Renderer_Structs.h"

void Eng::RpTransparent::Execute(RpBuilder &builder) {
    RpAllocBuf &vtx_buf1 = builder.GetReadBuffer(vtx_buf1_);
    RpAllocBuf &vtx_buf2 = builder.GetReadBuffer(vtx_buf2_);
    RpAllocBuf &ndx_buf = builder.GetReadBuffer(ndx_buf_);

    RpAllocTex &color_tex = builder.GetWriteTexture(color_tex_);
    RpAllocTex &normal_tex = builder.GetWriteTexture(normal_tex_);
    RpAllocTex &spec_tex = builder.GetWriteTexture(spec_tex_);
    RpAllocTex &depth_tex = builder.GetWriteTexture(depth_tex_);

    LazyInit(builder.ctx(), builder.sh(), vtx_buf1, vtx_buf2, ndx_buf, color_tex, normal_tex, spec_tex, depth_tex);
    DrawTransparent(builder, color_tex);
}

void Eng::RpTransparent::DrawTransparent(RpBuilder &builder, RpAllocTex &color_tex) {
    RpAllocBuf &instances_buf = builder.GetReadBuffer(instances_buf_);
    RpAllocBuf &instance_indices_buf = builder.GetReadBuffer(instance_indices_buf_);
    RpAllocBuf &unif_shared_data_buf = builder.GetReadBuffer(shared_data_buf_);
    RpAllocBuf &materials_buf = builder.GetReadBuffer(materials_buf_);
    RpAllocBuf &cells_buf = builder.GetReadBuffer(cells_buf_);
    RpAllocBuf &items_buf = builder.GetReadBuffer(items_buf_);
    RpAllocBuf &lights_buf = builder.GetReadBuffer(lights_buf_);
    RpAllocBuf &decals_buf = builder.GetReadBuffer(decals_buf_);

    RpAllocTex &shad_tex = builder.GetReadTexture(shad_tex_);
    RpAllocTex &ssao_tex = builder.GetReadTexture(ssao_tex_);

    DrawTransparent_Simple(builder, instances_buf, instance_indices_buf, unif_shared_data_buf, materials_buf, cells_buf,
                           items_buf, lights_buf, decals_buf, shad_tex, color_tex, ssao_tex);
}

void Eng::RpTransparent::LazyInit(Ren::Context &ctx, Eng::ShaderLoader &sh, RpAllocBuf &vtx_buf1, RpAllocBuf &vtx_buf2,
                                  RpAllocBuf &ndx_buf, RpAllocTex &color_tex, RpAllocTex &normal_tex,
                                  RpAllocTex &spec_tex, RpAllocTex &depth_tex) {
    const Ren::RenderTarget color_targets[] = {{color_tex.ref, Ren::eLoadOp::Load, Ren::eStoreOp::Store},
                                               {normal_tex.ref, Ren::eLoadOp::Load, Ren::eStoreOp::Store},
                                               {spec_tex.ref, Ren::eLoadOp::Load, Ren::eStoreOp::Store}};
    const Ren::RenderTarget depth_target = {depth_tex.ref, Ren::eLoadOp::Load, Ren::eStoreOp::Store, Ren::eLoadOp::Load,
                                            Ren::eStoreOp::Store};

    if (!initialized) {
        if (!rp_transparent_.Setup(ctx.api_ctx(), color_targets, depth_target, ctx.log())) {
            ctx.log()->Error("[RpOpaque::LazyInit]: Failed to init render pass!");
        }

        api_ctx_ = ctx.api_ctx();
#if defined(USE_VK_RENDER)
        InitDescrSetLayout();
#endif

        initialized = true;
    }

    const int buf1_stride = 16, buf2_stride = 16;

    { // VertexInput for main drawing (uses all attributes)
        const Ren::VtxAttribDesc attribs[] = {
            // Attributes from buffer 1
            {vtx_buf1.ref, VTX_POS_LOC, 3, Ren::eType::Float32, buf1_stride, 0},
            {vtx_buf1.ref, VTX_UV1_LOC, 2, Ren::eType::Float16, buf1_stride, 3 * sizeof(float)},
            // Attributes from buffer 2
            {vtx_buf2.ref, VTX_NOR_LOC, 4, Ren::eType::Int16SNorm, buf1_stride, 0},
            {vtx_buf2.ref, VTX_TAN_LOC, 2, Ren::eType::Int16SNorm, buf1_stride, 4 * sizeof(uint16_t)},
            {vtx_buf2.ref, VTX_AUX_LOC, 1, Ren::eType::Uint32, buf1_stride, 6 * sizeof(uint16_t)}};

        draw_pass_vi_.Setup(attribs, ndx_buf.ref);
    }

    fb_to_use_ = (fb_to_use_ + 1) % 2;

    if (!transparent_draw_fb_[ctx.backend_frame()][fb_to_use_].Setup(ctx.api_ctx(), rp_transparent_, color_tex.desc.w,
                                                                     color_tex.desc.h, depth_target, depth_target,
                                                                     color_targets, ctx.log())) {
        ctx.log()->Error("RpTransparent: transparent_draw_fb_ init failed!");
    }

#if !defined(USE_VK_RENDER)
    if (!color_only_fb_[fb_to_use_].Setup(ctx.api_ctx(), {}, color_tex.desc.w, color_tex.desc.h, color_tex.ref,
                                          depth_tex.ref, depth_tex.ref, view_state_->is_multisampled, ctx.log())) {
        ctx.log()->Error("RpTransparent: color_only_fb_ init failed!");
    }

    // if (!resolved_fb_.Setup(ctx.api_ctx(), {}, transparent_tex.desc.w, transparent_tex.desc.h, transparent_tex.ref,
    // {},
    //                         {}, false)) {
    //     ctx.log()->Error("RpTransparent: resolved_fb_ init failed!");
    // }
#endif

    /*if (moments_b0_.id && moments_z_and_z2_.id && moments_z3_and_z4_.id) {
        const Ren::TexHandle attachments[] = {moments_b0_, moments_z_and_z2_,
                                              moments_z3_and_z4_};
        if (!moments_fb_.Setup(attachments, 3, depth_tex.ref->handle(), {},
                               view_state_->is_multisampled)) {
            ctx.log()->Error("RpTransparent: moments_fb_ init failed!");
        }
    }*/
}