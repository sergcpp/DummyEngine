#include "ExOITBlendLayer.h"

#include <Ren/Context.h>

#include "../../utils/ShaderLoader.h"

void Eng::ExOITBlendLayer::Execute(FgBuilder &builder) {
    FgAllocBuf &vtx_buf1 = builder.GetReadBuffer(vtx_buf1_);
    FgAllocBuf &vtx_buf2 = builder.GetReadBuffer(vtx_buf2_);
    FgAllocBuf &ndx_buf = builder.GetReadBuffer(ndx_buf_);
    FgAllocTex &depth_tex = builder.GetWriteTexture(depth_tex_);
    FgAllocTex &color_tex = builder.GetWriteTexture(color_tex_);

    LazyInit(builder.ctx(), builder.sh(), vtx_buf1, vtx_buf2, ndx_buf, depth_tex, color_tex);
    DrawTransparent(builder, depth_tex);
}

void Eng::ExOITBlendLayer::LazyInit(Ren::Context &ctx, Eng::ShaderLoader &sh, FgAllocBuf &vtx_buf1,
                                    FgAllocBuf &vtx_buf2, FgAllocBuf &ndx_buf, FgAllocTex &depth_tex,
                                    FgAllocTex &color_tex) {
    const Ren::RenderTarget color_targets[] = {{color_tex.ref, Ren::eLoadOp::Load, Ren::eStoreOp::Store}};
    const Ren::RenderTarget depth_target = {depth_tex.ref, Ren::eLoadOp::Load, Ren::eStoreOp::Store, Ren::eLoadOp::Load,
                                            Ren::eStoreOp::Store};
    if (!initialized) {
#if defined(REN_GL_BACKEND)
        const bool bindless = ctx.capabilities.bindless_texture;
#else
        const bool bindless = true;
#endif

        prog_oit_blit_depth_ = sh.LoadProgram("internal/blit.vert.glsl", "internal/blit_oit_depth.frag.glsl");

        Ren::ProgramRef oit_blend_simple_prog[4] = {
            sh.LoadProgram(
                bindless ? "internal/oit_blend_layer.vert.glsl" : "internal/oit_blend_layer@NO_BINDLESS.vert.glsl",
                bindless ? "internal/oit_blend_layer.frag.glsl" : "internal/oit_blend_layer@NO_BINDLESS.frag.glsl"),
            sh.LoadProgram(bindless ? "internal/oit_blend_layer.vert.glsl"
                                    : "internal/oit_blend_layer@NO_BINDLESS.vert.glsl",
                           bindless ? "internal/oit_blend_layer@SPECULAR.frag.glsl"
                                    : "internal/oit_blend_layer@SPECULAR;NO_BINDLESS.frag.glsl"),
            sh.LoadProgram(bindless ? "internal/oit_blend_layer.vert.glsl"
                                    : "internal/oit_blend_layer@NO_BINDLESS.vert.glsl",
                           bindless ? "internal/oit_blend_layer@GI_CACHE.frag.glsl"
                                    : "internal/oit_blend_layer@GI_CACHE;NO_BINDLESS.frag.glsl"),
            sh.LoadProgram(bindless ? "internal/oit_blend_layer.vert.glsl"
                                    : "internal/oit_blend_layer@NO_BINDLESS.vert.glsl",
                           bindless ? "internal/oit_blend_layer@GI_CACHE;SPECULAR.frag.glsl"
                                    : "internal/oit_blend_layer@GI_CACHE;SPECULAR;NO_BINDLESS.frag.glsl")};
        Ren::ProgramRef oit_blend_vegetation_prog[4] = {
            sh.LoadProgram(bindless ? "internal/oit_blend_layer@VEGETATION.vert.glsl"
                                    : "internal/oit_blend_layer@VEGETATION;NO_BINDLESS.vert.glsl",
                           bindless ? "internal/oit_blend_layer.frag.glsl"
                                    : "internal/oit_blend_layer@NO_BINDLESS.frag.glsl"),
            sh.LoadProgram(bindless ? "internal/oit_blend_layer@VEGETATION.vert.glsl"
                                    : "internal/oit_blend_layer@VEGETATION;NO_BINDLESS.vert.glsl",
                           bindless ? "internal/oit_blend_layer@SPECULAR.frag.glsl"
                                    : "internal/oit_blend_layer@SPECULAR;NO_BINDLESS.frag.glsl"),
            sh.LoadProgram(bindless ? "internal/oit_blend_layer@VEGETATION.vert.glsl"
                                    : "internal/oit_blend_layer@VEGETATION;NO_BINDLESS.vert.glsl",
                           bindless ? "internal/oit_blend_layer@GI_CACHE.frag.glsl"
                                    : "internal/oit_blend_layer@GI_CACHE;NO_BINDLESS.frag.glsl"),
            sh.LoadProgram(bindless ? "internal/oit_blend_layer@VEGETATION.vert.glsl"
                                    : "internal/oit_blend_layer@VEGETATION;NO_BINDLESS.vert.glsl",
                           bindless ? "internal/oit_blend_layer@GI_CACHE;SPECULAR.frag.glsl"
                                    : "internal/oit_blend_layer@GI_CACHE;SPECULAR;NO_BINDLESS.frag.glsl")};

        if (!rp_oit_blend_.Setup(ctx.api_ctx(), depth_target, color_targets, ctx.log())) {
            ctx.log()->Error("[ExOITBlendLayer::LazyInit]: Failed to init render pass!");
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
                ctx.log()->Error("[ExOITBlendLayer::LazyInit]: vi_simple_ init failed!");
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
                ctx.log()->Error("[ExOITBlendLayer::LazyInit]: vi_vegetation_ init failed!");
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
                    ctx.log()->Error("[ExOITBlendLayer::LazyInit]: Failed to initialize pipeline!");
                }

                rast_state.poly.cull = uint8_t(Ren::eCullFace::Back);

                if (!pi_simple_[i][0].Init(ctx.api_ctx(), rast_state, oit_blend_simple_prog[i], &vi_simple_,
                                           &rp_oit_blend_, 0, ctx.log())) {
                    ctx.log()->Error("[ExOITBlendLayer::LazyInit]: Failed to initialize pipeline!");
                }

                rast_state.poly.cull = uint8_t(Ren::eCullFace::Front);

                if (!pi_simple_[i][1].Init(ctx.api_ctx(), rast_state, oit_blend_simple_prog[i], &vi_simple_,
                                           &rp_oit_blend_, 0, ctx.log())) {
                    ctx.log()->Error("[ExOITBlendLayer::LazyInit]: Failed to initialize pipeline!");
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
                    ctx.log()->Error("[ExOITBlendLayer::LazyInit]: Failed to initialize pipeline!");
                }

                rast_state.poly.cull = uint8_t(Ren::eCullFace::Back);

                if (!pi_vegetation_[i][0].Init(ctx.api_ctx(), rast_state, oit_blend_vegetation_prog[i], &vi_vegetation_,
                                               &rp_oit_blend_, 0, ctx.log())) {
                    ctx.log()->Error("[ExOITBlendLayer::LazyInit]: Failed to initialize pipeline!");
                }
            }
        }

        initialized = true;
    }

    fb_to_use_ = (fb_to_use_ + 1) % 2;

    if (!main_draw_fb_[ctx.backend_frame()][fb_to_use_].Setup(ctx.api_ctx(), rp_oit_blend_, depth_tex.desc.w,
                                                              depth_tex.desc.h, depth_target, depth_target,
                                                              color_targets, ctx.log())) {
        ctx.log()->Error("[ExOITBlendLayer::LazyInit]: main_draw_fb_ init failed!");
    }
}