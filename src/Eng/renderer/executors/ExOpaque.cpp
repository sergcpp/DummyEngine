#include "ExOpaque.h"

#include <Ren/Context.h>

#include "../../utils/ShaderLoader.h"

void Eng::ExOpaque::Execute(FgContext &fg) {
    Ren::WeakBufRef vtx_buf1 = fg.AccessROBufferRef(vtx_buf1_);
    Ren::WeakBufRef vtx_buf2 = fg.AccessROBufferRef(vtx_buf2_);
    Ren::WeakBufRef ndx_buf = fg.AccessROBufferRef(ndx_buf_);

    Ren::WeakImgRef color_tex = fg.AccessRWImageRef(color_tex_);
    Ren::WeakImgRef normal_tex = fg.AccessRWImageRef(normal_tex_);
    Ren::WeakImgRef spec_tex = fg.AccessRWImageRef(spec_tex_);
    Ren::WeakImgRef depth_tex = fg.AccessRWImageRef(depth_tex_);

    LazyInit(fg.ren_ctx(), fg.sh(), vtx_buf1, vtx_buf2, ndx_buf, color_tex, normal_tex, spec_tex, depth_tex);
    DrawOpaque(fg);
}

void Eng::ExOpaque::LazyInit(Ren::Context &ctx, Eng::ShaderLoader &sh, const Ren::WeakBufRef &vtx_buf1,
                             const Ren::WeakBufRef &vtx_buf2, const Ren::WeakBufRef &ndx_buf,
                             const Ren::WeakImgRef &color_tex, const Ren::WeakImgRef &normal_tex,
                             const Ren::WeakImgRef &spec_tex, const Ren::WeakImgRef &depth_tex) {
    const Ren::RenderTarget color_targets[] = {{color_tex, Ren::eLoadOp::Load, Ren::eStoreOp::Store},
                                               {normal_tex, Ren::eLoadOp::Load, Ren::eStoreOp::Store},
                                               {spec_tex, Ren::eLoadOp::Load, Ren::eStoreOp::Store}};
    const Ren::RenderTarget depth_target = {depth_tex, Ren::eLoadOp::Load, Ren::eStoreOp::Store, Ren::eLoadOp::Load,
                                            Ren::eStoreOp::Store};

    if (!initialized) {
        const int buf1_stride = 16, buf2_stride = 16;

        { // VertexInput for main drawing (uses all attributes)
            const Ren::VtxAttribDesc attribs[] = {
                // Attributes from buffer 1
                {vtx_buf1, VTX_POS_LOC, 3, Ren::eType::Float32, buf1_stride, 0},
                {vtx_buf1, VTX_UV1_LOC, 2, Ren::eType::Float16, buf1_stride, 3 * sizeof(float)},
                // Attributes from buffer 2
                {vtx_buf2, VTX_NOR_LOC, 4, Ren::eType::Int16_snorm, buf2_stride, 0},
                {vtx_buf2, VTX_TAN_LOC, 2, Ren::eType::Int16_snorm, buf2_stride, 4 * sizeof(uint16_t)},
                {vtx_buf2, VTX_AUX_LOC, 1, Ren::eType::Uint32, buf2_stride, 6 * sizeof(uint16_t)}};
            draw_pass_vi_ = sh.LoadVertexInput(attribs, ndx_buf);
        }

        rp_opaque_ = sh.LoadRenderPass(depth_target, color_targets);

        api_ctx_ = ctx.api_ctx();
#if defined(REN_VK_BACKEND)
        InitDescrSetLayout();
#endif

        initialized = true;
    }

    fb_to_use_ = (fb_to_use_ + 1) % 2;

    if (!opaque_draw_fb_[ctx.backend_frame()][fb_to_use_].Setup(ctx.api_ctx(), *rp_opaque_, depth_tex->params.w,
                                                                depth_tex->params.h, depth_target, depth_target,
                                                                color_targets, ctx.log())) {
        ctx.log()->Error("ExOpaque: opaque_draw_fb_ init failed!");
    }
}