#include "RpOITBlendLayer.h"

#include <Ren/Context.h>

#include "../../utils/ShaderLoader.h"
#include "../Renderer_Structs.h"

void Eng::RpOITBlendLayer::Execute(RpBuilder &builder) {
    RpAllocBuf &vtx_buf1 = builder.GetReadBuffer(vtx_buf1_);
    RpAllocBuf &vtx_buf2 = builder.GetReadBuffer(vtx_buf2_);
    RpAllocBuf &ndx_buf = builder.GetReadBuffer(ndx_buf_);
    RpAllocTex &depth_tex = builder.GetWriteTexture(depth_tex_);
    RpAllocTex &color_tex = builder.GetWriteTexture(color_tex_);

    LazyInit(builder.ctx(), builder.sh(), vtx_buf1, vtx_buf2, ndx_buf, depth_tex, color_tex);
    DrawTransparent(builder, depth_tex);
}

void Eng::RpOITBlendLayer::LazyInit(Ren::Context &ctx, Eng::ShaderLoader &sh, RpAllocBuf &vtx_buf1,
                                    RpAllocBuf &vtx_buf2, RpAllocBuf &ndx_buf, RpAllocTex &depth_tex,
                                    RpAllocTex &color_tex) {
    const Ren::RenderTarget color_targets[] = {{color_tex.ref, Ren::eLoadOp::Load, Ren::eStoreOp::Store}};
    const Ren::RenderTarget depth_target = {depth_tex.ref, Ren::eLoadOp::Load, Ren::eStoreOp::Store, Ren::eLoadOp::Load,
                                            Ren::eStoreOp::Store};
    if (!initialized) {
#if defined(USE_GL_RENDER)
        const bool bindless = ctx.capabilities.bindless_texture;
#else
        const bool bindless = true;
#endif

        prog_oit_blit_depth_ = sh.LoadProgram(ctx, "internal/blit.vert.glsl", "internal/blit_oit_depth.frag.glsl");
        assert(prog_oit_blit_depth_->ready());

        Ren::ProgramRef oit_blend_simple_prog[4] = {
            sh.LoadProgram(
                ctx, bindless ? "internal/oit_blend_layer.vert.glsl" : "internal/oit_blend_layer.vert.glsl@NO_BINDLESS",
                bindless ? "internal/oit_blend_layer.frag.glsl" : "internal/oit_blend_layer.frag.glsl@NO_BINDLESS"),
            sh.LoadProgram(
                ctx, bindless ? "internal/oit_blend_layer.vert.glsl" : "internal/oit_blend_layer.vert.glsl@NO_BINDLESS",
                bindless ? "internal/oit_blend_layer.frag.glsl@SPECULAR"
                         : "internal/oit_blend_layer.frag.glsl@SPECULAR;NO_BINDLESS"),
            sh.LoadProgram(
                ctx, bindless ? "internal/oit_blend_layer.vert.glsl" : "internal/oit_blend_layer.vert.glsl@NO_BINDLESS",
                bindless ? "internal/oit_blend_layer.frag.glsl@GI_CACHE"
                         : "internal/oit_blend_layer.frag.glsl@GI_CACHE;NO_BINDLESS"),
            sh.LoadProgram(
                ctx, bindless ? "internal/oit_blend_layer.vert.glsl" : "internal/oit_blend_layer.vert.glsl@NO_BINDLESS",
                bindless ? "internal/oit_blend_layer.frag.glsl@GI_CACHE;SPECULAR"
                         : "internal/oit_blend_layer.frag.glsl@GI_CACHE;SPECULAR;NO_BINDLESS")};
        assert(oit_blend_simple_prog[0]->ready());
        assert(oit_blend_simple_prog[1]->ready());
        assert(oit_blend_simple_prog[2]->ready());
        assert(oit_blend_simple_prog[3]->ready());
        Ren::ProgramRef oit_blend_vegetation_prog[4] = {
            sh.LoadProgram(ctx,
                           bindless ? "internal/oit_blend_layer.vert.glsl@VEGETATION"
                                    : "internal/oit_blend_layer.vert.glsl@VEGETATION;NO_BINDLESS",
                           bindless ? "internal/oit_blend_layer.frag.glsl"
                                    : "internal/oit_blend_layer.frag.glsl@NO_BINDLESS"),
            sh.LoadProgram(ctx,
                           bindless ? "internal/oit_blend_layer.vert.glsl@VEGETATION"
                                    : "internal/oit_blend_layer.vert.glsl@VEGETATION;NO_BINDLESS",
                           bindless ? "internal/oit_blend_layer.frag.glsl@SPECULAR"
                                    : "internal/oit_blend_layer.frag.glsl@SPECULAR;NO_BINDLESS"),
            sh.LoadProgram(ctx,
                           bindless ? "internal/oit_blend_layer.vert.glsl@VEGETATION"
                                    : "internal/oit_blend_layer.vert.glsl@VEGETATION;NO_BINDLESS",
                           bindless ? "internal/oit_blend_layer.frag.glsl@GI_CACHE"
                                    : "internal/oit_blend_layer.frag.glsl@GI_CACHE;NO_BINDLESS"),
            sh.LoadProgram(ctx,
                           bindless ? "internal/oit_blend_layer.vert.glsl@VEGETATION"
                                    : "internal/oit_blend_layer.vert.glsl@VEGETATION;NO_BINDLESS",
                           bindless ? "internal/oit_blend_layer.frag.glsl@GI_CACHE;SPECULAR"
                                    : "internal/oit_blend_layer.frag.glsl@GI_CACHE;SPECULAR;NO_BINDLESS")};
        assert(oit_blend_vegetation_prog[0]->ready());
        assert(oit_blend_vegetation_prog[1]->ready());
        assert(oit_blend_vegetation_prog[2]->ready());
        assert(oit_blend_vegetation_prog[3]->ready());

        if (!rp_oit_blend_.Setup(ctx.api_ctx(), color_targets, depth_target, ctx.log())) {
            ctx.log()->Error("[RpOITBlendLayer::LazyInit]: Failed to init render pass!");
        }

        const int buf1_stride = 16, buf2_stride = 16;

        { // VAO for simple and skinned meshes
            const Ren::VtxAttribDesc attribs[] = {
                // Attributes from buffer 1
                {vtx_buf1.ref, VTX_POS_LOC, 3, Ren::eType::Float32, buf1_stride, 0},
                {vtx_buf1.ref, VTX_UV1_LOC, 2, Ren::eType::Float16, buf1_stride, 3 * sizeof(float)},
                // Attributes from buffer 2
                {vtx_buf2.ref, VTX_NOR_LOC, 4, Ren::eType::Int16SNorm, buf2_stride, 0},
                {vtx_buf2.ref, VTX_TAN_LOC, 2, Ren::eType::Int16SNorm, buf2_stride, 4 * sizeof(uint16_t)}};
            if (!vi_simple_.Setup(attribs, ndx_buf.ref)) {
                ctx.log()->Error("[RpOITBlendLayer::LazyInit]: vi_simple_ init failed!");
            }
        }

        { // VAO for vegetation meshes (uses additional vertex color attribute)
            const Ren::VtxAttribDesc attribs[] = {
                // Attributes from buffer 1
                {vtx_buf1.ref, VTX_POS_LOC, 3, Ren::eType::Float32, buf1_stride, 0},
                {vtx_buf1.ref, VTX_UV1_LOC, 2, Ren::eType::Float16, buf1_stride, 3 * sizeof(float)},
                // Attributes from buffer 2
                {vtx_buf2.ref, VTX_NOR_LOC, 4, Ren::eType::Int16SNorm, buf2_stride, 0},
                {vtx_buf2.ref, VTX_TAN_LOC, 2, Ren::eType::Int16SNorm, buf2_stride, 4 * sizeof(uint16_t)},
                {vtx_buf2.ref, VTX_AUX_LOC, 1, Ren::eType::Uint32, buf2_stride, 6 * sizeof(uint16_t)}};
            if (!vi_vegetation_.Setup(attribs, ndx_buf.ref)) {
                ctx.log()->Error("[RpOITBlendLayer::LazyInit]: vi_vegetation_ init failed!");
            }
        }

        for (int i = 0; i < 4; ++i) {
            { // simple and skinned
                Ren::RastState rast_state;
                rast_state.depth.test_enabled = true;
                rast_state.depth.write_enabled = false;
                rast_state.depth.compare_op = uint8_t(Ren::eCompareOp::Equal);

                rast_state.blend.enabled = true;
                rast_state.blend.src_color = unsigned(Ren::eBlendFactor::SrcAlpha);
                rast_state.blend.dst_color = unsigned(Ren::eBlendFactor::OneMinusSrcAlpha);
                rast_state.blend.src_alpha = unsigned(Ren::eBlendFactor::Zero);
                rast_state.blend.dst_alpha = unsigned(Ren::eBlendFactor::One);

                if (!pi_simple_[i][2].Init(ctx.api_ctx(), rast_state, oit_blend_simple_prog[i], &vi_simple_,
                                           &rp_oit_blend_, 0, ctx.log())) {
                    ctx.log()->Error("[RpOITBlendLayer::LazyInit]: Failed to initialize pipeline!");
                }

                rast_state.poly.cull = uint8_t(Ren::eCullFace::Back);

                if (!pi_simple_[i][0].Init(ctx.api_ctx(), rast_state, oit_blend_simple_prog[i], &vi_simple_,
                                           &rp_oit_blend_, 0, ctx.log())) {
                    ctx.log()->Error("[RpOITBlendLayer::LazyInit]: Failed to initialize pipeline!");
                }

                rast_state.poly.cull = uint8_t(Ren::eCullFace::Front);

                if (!pi_simple_[i][1].Init(ctx.api_ctx(), rast_state, oit_blend_simple_prog[i], &vi_simple_,
                                           &rp_oit_blend_, 0, ctx.log())) {
                    ctx.log()->Error("[RpOITBlendLayer::LazyInit]: Failed to initialize pipeline!");
                }
            }
            { // vegetation
                Ren::RastState rast_state;
                rast_state.depth.test_enabled = true;
                rast_state.depth.write_enabled = false;
                rast_state.depth.compare_op = uint8_t(Ren::eCompareOp::Equal);

                rast_state.blend.enabled = true;
                rast_state.blend.src_color = unsigned(Ren::eBlendFactor::SrcAlpha);
                rast_state.blend.dst_color = unsigned(Ren::eBlendFactor::OneMinusSrcAlpha);
                rast_state.blend.src_alpha = unsigned(Ren::eBlendFactor::Zero);
                rast_state.blend.dst_alpha = unsigned(Ren::eBlendFactor::One);

                if (!pi_vegetation_[i][1].Init(ctx.api_ctx(), rast_state, oit_blend_vegetation_prog[i], &vi_vegetation_,
                                               &rp_oit_blend_, 0, ctx.log())) {
                    ctx.log()->Error("[RpOITBlendLayer::LazyInit]: Failed to initialize pipeline!");
                }

                rast_state.poly.cull = uint8_t(Ren::eCullFace::Back);

                if (!pi_vegetation_[i][0].Init(ctx.api_ctx(), rast_state, oit_blend_vegetation_prog[i], &vi_vegetation_,
                                               &rp_oit_blend_, 0, ctx.log())) {
                    ctx.log()->Error("[RpOITBlendLayer::LazyInit]: Failed to initialize pipeline!");
                }
            }
        }

        initialized = true;
    }

    fb_to_use_ = (fb_to_use_ + 1) % 2;

    if (!main_draw_fb_[ctx.backend_frame()][fb_to_use_].Setup(ctx.api_ctx(), rp_oit_blend_, depth_tex.desc.w,
                                                              depth_tex.desc.h, depth_target, depth_target,
                                                              color_targets, ctx.log())) {
        ctx.log()->Error("[RpOITBlendLayer::LazyInit]: main_draw_fb_ init failed!");
    }
}