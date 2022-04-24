#include "RpOpaque.h"

#include <Ren/Context.h>

#include "../../Utils/ShaderLoader.h"
#include "../Renderer_Structs.h"

void RpOpaque::Execute(RpBuilder &builder) {
    RpAllocBuf &vtx_buf1 = builder.GetReadBuffer(vtx_buf1_);
    RpAllocBuf &vtx_buf2 = builder.GetReadBuffer(vtx_buf2_);
    RpAllocBuf &ndx_buf = builder.GetReadBuffer(ndx_buf_);

    RpAllocTex &color_tex = builder.GetWriteTexture(color_tex_);
    RpAllocTex &normal_tex = builder.GetWriteTexture(normal_tex_);
    RpAllocTex &spec_tex = builder.GetWriteTexture(spec_tex_);
    RpAllocTex &depth_tex = builder.GetWriteTexture(depth_tex_);

    LazyInit(builder.ctx(), builder.sh(), vtx_buf1, vtx_buf2, ndx_buf, color_tex, normal_tex, spec_tex, depth_tex);
    DrawOpaque(builder);
}

void RpOpaque::LazyInit(Ren::Context &ctx, ShaderLoader &sh, RpAllocBuf &vtx_buf1, RpAllocBuf &vtx_buf2,
                        RpAllocBuf &ndx_buf, RpAllocTex &color_tex, RpAllocTex &normal_tex, RpAllocTex &spec_tex,
                        RpAllocTex &depth_tex) {
    const Ren::RenderTarget color_targets[] = {{color_tex.ref, Ren::eLoadOp::Load, Ren::eStoreOp::Store},
                                               {normal_tex.ref, Ren::eLoadOp::Load, Ren::eStoreOp::Store},
                                               {spec_tex.ref, Ren::eLoadOp::Load, Ren::eStoreOp::Store}};
    const Ren::RenderTarget depth_target = {depth_tex.ref, Ren::eLoadOp::Load, Ren::eStoreOp::Store, Ren::eLoadOp::Load,
                                            Ren::eStoreOp::Store};

    if (!initialized) {
        if (!rp_opaque_.Setup(ctx.api_ctx(), color_targets, 3, depth_target, ctx.log())) {
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
            {vtx_buf1.ref, REN_VTX_POS_LOC, 3, Ren::eType::Float32, buf1_stride, 0},
            {vtx_buf1.ref, REN_VTX_UV1_LOC, 2, Ren::eType::Float16, buf1_stride, 3 * sizeof(float)},
            // Attributes from buffer 2
            {vtx_buf2.ref, REN_VTX_NOR_LOC, 4, Ren::eType::Int16SNorm, buf2_stride, 0},
            {vtx_buf2.ref, REN_VTX_TAN_LOC, 2, Ren::eType::Int16SNorm, buf2_stride, 4 * sizeof(uint16_t)},
            {vtx_buf2.ref, REN_VTX_AUX_LOC, 1, Ren::eType::Uint32, buf2_stride, 6 * sizeof(uint16_t)}};

        draw_pass_vi_.Setup(attribs, COUNT_OF(attribs), ndx_buf.ref);
    }

    fb_to_use_ = (fb_to_use_ + 1) % 2;

    if (!opaque_draw_fb_[ctx.backend_frame()][fb_to_use_].Setup(ctx.api_ctx(), rp_opaque_, depth_tex.desc.w,
                                                                depth_tex.desc.h, depth_target, depth_target,
                                                                color_targets, COUNT_OF(color_targets), ctx.log())) {
        ctx.log()->Error("RpOpaque: opaque_draw_fb_ init failed!");
    }
}