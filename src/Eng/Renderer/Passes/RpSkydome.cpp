#include "RpSkydome.h"

#include <Ren/Context.h>

#include "../../Utils/ShaderLoader.h"
#include "../Renderer_Structs.h"

void RpSkydome::Execute(RpBuilder &builder) {
    RpAllocBuf &vtx_buf1 = builder.GetReadBuffer(vtx_buf1_);
    RpAllocBuf &vtx_buf2 = builder.GetReadBuffer(vtx_buf2_);
    RpAllocBuf &ndx_buf = builder.GetReadBuffer(ndx_buf_);

    RpAllocTex &color_tex = builder.GetWriteTexture(color_tex_);
    RpAllocTex &spec_tex = builder.GetWriteTexture(spec_tex_);
    RpAllocTex &depth_tex = builder.GetWriteTexture(depth_tex_);

    LazyInit(builder.ctx(), builder.sh(), vtx_buf1, vtx_buf2, ndx_buf, color_tex, spec_tex, depth_tex);
    DrawSkydome(builder, vtx_buf1, vtx_buf2, ndx_buf, color_tex, spec_tex, depth_tex);
}

void RpSkydome::LazyInit(Ren::Context &ctx, ShaderLoader &sh, RpAllocBuf &vtx_buf1, RpAllocBuf &vtx_buf2,
                         RpAllocBuf &ndx_buf, RpAllocTex &color_tex, RpAllocTex &spec_tex, RpAllocTex &depth_tex) {
    const Ren::RenderTarget color_load_targets[] = {{color_tex.ref, Ren::eLoadOp::Load, Ren::eStoreOp::Store},
                                                    {} /* normals texture */,
                                                    {spec_tex.ref, Ren::eLoadOp::Load, Ren::eStoreOp::Store}};
    const Ren::RenderTarget color_clear_targets[] = {{color_tex.ref, Ren::eLoadOp::DontCare, Ren::eStoreOp::Store},
                                                     {} /* normals texture */,
                                                     {spec_tex.ref, Ren::eLoadOp::DontCare, Ren::eStoreOp::Store}};
    const Ren::RenderTarget depth_load_target = {depth_tex.ref, Ren::eLoadOp::Load, Ren::eStoreOp::Store,
                                                 Ren::eLoadOp::Load, Ren::eStoreOp::DontCare};
    const Ren::RenderTarget depth_clear_target = {depth_tex.ref, Ren::eLoadOp::DontCare, Ren::eStoreOp::Store,
                                                  Ren::eLoadOp::Clear, Ren::eStoreOp::DontCare};

    if (!initialized) {
        Ren::ProgramRef skydome_prog =
            sh.LoadProgram(ctx, "skydome", "internal/skydome.vert.glsl", "internal/skydome.frag.glsl");
        assert(skydome_prog->ready());

        if (!render_pass_[0].Setup(ctx.api_ctx(), color_load_targets, 3, depth_load_target, ctx.log())) {
            ctx.log()->Error("[RpSkydome::LazyInit]: Failed to create render pass!");
        }

        if (!render_pass_[1].Setup(ctx.api_ctx(), color_clear_targets, 3, depth_clear_target, ctx.log())) {
            ctx.log()->Error("[RpSkydome::LazyInit]: Failed to create render pass!");
        }

        const int buf1_stride = 16, buf2_stride = 16;
        const Ren::VtxAttribDesc attribs[] = {
            {vtx_buf1.ref->handle(), REN_VTX_POS_LOC, 3, Ren::eType::Float32, buf1_stride, 0}};

        if (!vtx_input_.Setup(attribs, 1, ndx_buf.ref)) {
            ctx.log()->Error("[RpSkydome::LazyInit]: Failed to initialize vertex input!");
        }

        Ren::RastState rast_state;
        rast_state.poly.cull = uint8_t(Ren::eCullFace::Back);
        rast_state.depth.test_enabled = true;
        rast_state.depth.compare_op = unsigned(Ren::eCompareOp::Less);
        rast_state.blend.enabled = false;

        rast_state.stencil.enabled = true;
        rast_state.stencil.write_mask = 0xff;
        rast_state.stencil.pass = unsigned(Ren::eStencilOp::Replace);

        if (!pipeline_[0].Init(ctx.api_ctx(), rast_state, skydome_prog, &vtx_input_, &render_pass_[0], 0, ctx.log())) {
            ctx.log()->Error("[RpSkydome::LazyInit]: Failed to initialize pipeline!");
        }

        rast_state.depth.compare_op = unsigned(Ren::eCompareOp::Always);

        if (!pipeline_[1].Init(ctx.api_ctx(), rast_state, skydome_prog, &vtx_input_, &render_pass_[1], 0, ctx.log())) {
            ctx.log()->Error("[RpSkydome::LazyInit]: Failed to initialize pipeline!");
        }

        initialized = true;
    }

    if (!framebuf_[ctx.backend_frame()].Setup(ctx.api_ctx(), render_pass_[0], depth_tex.desc.w, depth_tex.desc.h,
                                              depth_load_target, depth_load_target, color_load_targets, 3)) {
        ctx.log()->Error("[RpSkydome::LazyInit]: fbo init failed!");
    }
}
