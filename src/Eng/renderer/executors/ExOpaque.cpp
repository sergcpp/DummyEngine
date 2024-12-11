#include "ExOpaque.h"

#include <Ren/Context.h>

#include "../../utils/ShaderLoader.h"

void Eng::ExOpaque::Execute(FgBuilder &builder) {
    FgAllocBuf &vtx_buf1 = builder.GetReadBuffer(vtx_buf1_);
    FgAllocBuf &vtx_buf2 = builder.GetReadBuffer(vtx_buf2_);
    FgAllocBuf &ndx_buf = builder.GetReadBuffer(ndx_buf_);

    FgAllocTex &color_tex = builder.GetWriteTexture(color_tex_);
    FgAllocTex &normal_tex = builder.GetWriteTexture(normal_tex_);
    FgAllocTex &spec_tex = builder.GetWriteTexture(spec_tex_);
    FgAllocTex &depth_tex = builder.GetWriteTexture(depth_tex_);

    LazyInit(builder.ctx(), builder.sh(), vtx_buf1, vtx_buf2, ndx_buf, color_tex, normal_tex, spec_tex, depth_tex);
    DrawOpaque(builder);
}

void Eng::ExOpaque::LazyInit(Ren::Context &ctx, Eng::ShaderLoader &sh, FgAllocBuf &vtx_buf1, FgAllocBuf &vtx_buf2,
                             FgAllocBuf &ndx_buf, FgAllocTex &color_tex, FgAllocTex &normal_tex, FgAllocTex &spec_tex,
                             FgAllocTex &depth_tex) {
    const Ren::RenderTarget color_targets[] = {{color_tex.ref, Ren::eLoadOp::Load, Ren::eStoreOp::Store},
                                               {normal_tex.ref, Ren::eLoadOp::Load, Ren::eStoreOp::Store},
                                               {spec_tex.ref, Ren::eLoadOp::Load, Ren::eStoreOp::Store}};
    const Ren::RenderTarget depth_target = {depth_tex.ref, Ren::eLoadOp::Load, Ren::eStoreOp::Store, Ren::eLoadOp::Load,
                                            Ren::eStoreOp::Store};

    if (!initialized) {
        if (!rp_opaque_.Setup(ctx.api_ctx(), depth_target, color_targets, ctx.log())) {
            ctx.log()->Error("[ExOpaque::LazyInit]: Failed to init render pass!");
        }

        api_ctx_ = ctx.api_ctx();
#if defined(REN_VK_BACKEND)
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
            {vtx_buf2.ref, VTX_NOR_LOC, 4, Ren::eType::Int16SNorm, buf2_stride, 0},
            {vtx_buf2.ref, VTX_TAN_LOC, 2, Ren::eType::Int16SNorm, buf2_stride, 4 * sizeof(uint16_t)},
            {vtx_buf2.ref, VTX_AUX_LOC, 1, Ren::eType::Uint32, buf2_stride, 6 * sizeof(uint16_t)}};

        draw_pass_vi_.Setup(attribs, ndx_buf.ref);
    }

    fb_to_use_ = (fb_to_use_ + 1) % 2;

    if (!opaque_draw_fb_[ctx.backend_frame()][fb_to_use_].Setup(ctx.api_ctx(), rp_opaque_, depth_tex.desc.w,
                                                                depth_tex.desc.h, depth_target, depth_target,
                                                                color_targets, ctx.log())) {
        ctx.log()->Error("ExOpaque: opaque_draw_fb_ init failed!");
    }
}