#include "ExOITBlendLayer.h"

#include <Ren/Context.h>

#include "../../utils/ShaderLoader.h"
#include "../framegraph/FgBuilder.h"
#include "../renderer/Renderer_Structs.h"

void Eng::ExOITBlendLayer::Execute(const FgContext &fg) {
    const Ren::ImageRWHandle depth = fg.AccessRWImage(args_->depth);
    const Ren::ImageRWHandle color = fg.AccessRWImage(args_->color);

    LazyInit(fg, depth, color);
    DrawTransparent(fg, depth, color);
}

void Eng::ExOITBlendLayer::LazyInit(const FgContext &fg, const Ren::ImageRWHandle depth,
                                    const Ren::ImageRWHandle color) {
    const Ren::RenderTarget color_targets[] = {{color, Ren::eLoadOp::Load, Ren::eStoreOp::Store}};
    const Ren::RenderTarget depth_target = {depth, Ren::eLoadOp::Load, Ren::eStoreOp::Store, Ren::eLoadOp::Load,
                                            Ren::eStoreOp::Store};
    if (!initialized_) {
        auto &ctx = fg.ren_ctx();
        auto &sh = fg.sh();
#if defined(REN_GL_BACKEND)
        const bool bindless = ctx.capabilities.bindless_texture;
#else
        const bool bindless = true;
#endif

        prog_oit_blit_depth_ = sh.FindOrCreateProgram("internal/blit.vert.glsl", "internal/blit_oit_depth.frag.glsl");

        Ren::ProgramHandle oit_blend_simple_prog, oit_blend_vegetation_prog;
        if (args_->irradiance) {
            if (args_->oit_specular) {
                oit_blend_simple_prog = sh.FindOrCreateProgram(
                    bindless ? "internal/oit_blend_layer.vert.glsl" : "internal/oit_blend_layer@NO_BINDLESS.vert.glsl",
                    bindless ? "internal/oit_blend_layer@GI_CACHE;SPECULAR.frag.glsl"
                             : "internal/oit_blend_layer@GI_CACHE;SPECULAR;NO_BINDLESS.frag.glsl");
                oit_blend_vegetation_prog = sh.FindOrCreateProgram(
                    bindless ? "internal/oit_blend_layer@VEGETATION.vert.glsl"
                             : "internal/oit_blend_layer@VEGETATION;NO_BINDLESS.vert.glsl",
                    bindless ? "internal/oit_blend_layer@GI_CACHE;SPECULAR.frag.glsl"
                             : "internal/oit_blend_layer@GI_CACHE;SPECULAR;NO_BINDLESS.frag.glsl");
            } else {
                oit_blend_simple_prog = sh.FindOrCreateProgram(
                    bindless ? "internal/oit_blend_layer.vert.glsl" : "internal/oit_blend_layer@NO_BINDLESS.vert.glsl",
                    bindless ? "internal/oit_blend_layer@GI_CACHE.frag.glsl"
                             : "internal/oit_blend_layer@GI_CACHE;NO_BINDLESS.frag.glsl");
                oit_blend_vegetation_prog =
                    sh.FindOrCreateProgram(bindless ? "internal/oit_blend_layer@VEGETATION.vert.glsl"
                                                    : "internal/oit_blend_layer@VEGETATION;NO_BINDLESS.vert.glsl",
                                           bindless ? "internal/oit_blend_layer@GI_CACHE.frag.glsl"
                                                    : "internal/oit_blend_layer@GI_CACHE;NO_BINDLESS.frag.glsl");
            }
        } else {
            if (args_->oit_specular) {
                oit_blend_simple_prog = sh.FindOrCreateProgram(
                    bindless ? "internal/oit_blend_layer.vert.glsl" : "internal/oit_blend_layer@NO_BINDLESS.vert.glsl",
                    bindless ? "internal/oit_blend_layer@SPECULAR.frag.glsl"
                             : "internal/oit_blend_layer@SPECULAR;NO_BINDLESS.frag.glsl");
                oit_blend_vegetation_prog =
                    sh.FindOrCreateProgram(bindless ? "internal/oit_blend_layer@VEGETATION.vert.glsl"
                                                    : "internal/oit_blend_layer@VEGETATION;NO_BINDLESS.vert.glsl",
                                           bindless ? "internal/oit_blend_layer@SPECULAR.frag.glsl"
                                                    : "internal/oit_blend_layer@SPECULAR;NO_BINDLESS.frag.glsl");
            } else {
                oit_blend_simple_prog = sh.FindOrCreateProgram(
                    bindless ? "internal/oit_blend_layer.vert.glsl" : "internal/oit_blend_layer@NO_BINDLESS.vert.glsl",
                    bindless ? "internal/oit_blend_layer.frag.glsl" : "internal/oit_blend_layer@NO_BINDLESS.frag.glsl");
                oit_blend_vegetation_prog = sh.FindOrCreateProgram(
                    bindless ? "internal/oit_blend_layer@VEGETATION.vert.glsl"
                             : "internal/oit_blend_layer@VEGETATION;NO_BINDLESS.vert.glsl",
                    bindless ? "internal/oit_blend_layer.frag.glsl" : "internal/oit_blend_layer@NO_BINDLESS.frag.glsl");
            }
        }

        const Ren::RenderPassHandle rp_oit_blend = sh.FindOrCreateRenderPass(depth_target, color_targets);

        const int buf1_stride = 16, buf2_stride = 16;

        Ren::VertexInputHandle vi_simple, vi_vegetation;
        { // VertexInput for simple and skinned meshes
            const Ren::VtxAttribDesc attribs[] = {
                {0, VTX_POS_LOC, 3, Ren::eType::Float32, buf1_stride, 0, 0},
                {0, VTX_UV1_LOC, 2, Ren::eType::Float16, buf1_stride, 0, 3 * sizeof(float)},
                {1, VTX_NOR_LOC, 4, Ren::eType::Int16_snorm, buf2_stride, 0, 0},
                {1, VTX_TAN_LOC, 2, Ren::eType::Int16_snorm, buf2_stride, 0, 4 * sizeof(uint16_t)}};
            vi_simple = sh.FindOrCreateVertexInput(attribs);
        }

        { // VertexInput for vegetation meshes (uses additional vertex color attribute)
            const Ren::VtxAttribDesc attribs[] = {
                {0, VTX_POS_LOC, 3, Ren::eType::Float32, buf1_stride, 0, 0},
                {0, VTX_UV1_LOC, 2, Ren::eType::Float16, buf1_stride, 0, 3 * sizeof(float)},
                {1, VTX_NOR_LOC, 4, Ren::eType::Int16_snorm, buf2_stride, 0, 0},
                {1, VTX_TAN_LOC, 2, Ren::eType::Int16_snorm, buf2_stride, 0, 4 * sizeof(uint16_t)},
                {1, VTX_AUX_LOC, 1, Ren::eType::Uint32, buf2_stride, 0, 6 * sizeof(uint16_t)}};
            vi_vegetation = sh.FindOrCreateVertexInput(attribs);
        }

        { // simple and skinned
            Ren::RastState rast_state;
            rast_state.depth.test_enabled = true;
            rast_state.depth.write_enabled = false;
            rast_state.depth.compare_op = uint8_t(Ren::eCompareOp::Equal);

            rast_state.blend.enabled = true;
            rast_state.blend.src_color = unsigned(Ren::eBlendFactor::SrcAlpha);
            rast_state.blend.dst_color = unsigned(Ren::eBlendFactor::OneMinusSrcAlpha);
            rast_state.blend.src_alpha = unsigned(Ren::eBlendFactor::One);
            rast_state.blend.dst_alpha = unsigned(Ren::eBlendFactor::Zero);

            pi_simple_[2] = sh.FindOrCreatePipeline(rast_state, oit_blend_simple_prog, vi_simple, rp_oit_blend, 0);

            rast_state.poly.cull = uint8_t(Ren::eCullFace::Back);

            pi_simple_[0] = sh.FindOrCreatePipeline(rast_state, oit_blend_simple_prog, vi_simple, rp_oit_blend, 0);

            rast_state.poly.cull = uint8_t(Ren::eCullFace::Front);

            pi_simple_[1] = sh.FindOrCreatePipeline(rast_state, oit_blend_simple_prog, vi_simple, rp_oit_blend, 0);
        }
        { // vegetation
            Ren::RastState rast_state;
            rast_state.depth.test_enabled = true;
            rast_state.depth.write_enabled = false;
            rast_state.depth.compare_op = uint8_t(Ren::eCompareOp::Equal);

            rast_state.blend.enabled = true;
            rast_state.blend.src_color = unsigned(Ren::eBlendFactor::SrcAlpha);
            rast_state.blend.dst_color = unsigned(Ren::eBlendFactor::OneMinusSrcAlpha);
            rast_state.blend.src_alpha = unsigned(Ren::eBlendFactor::One);
            rast_state.blend.dst_alpha = unsigned(Ren::eBlendFactor::Zero);

            pi_vegetation_[1] =
                sh.FindOrCreatePipeline(rast_state, oit_blend_vegetation_prog, vi_vegetation, rp_oit_blend, 0);

            rast_state.poly.cull = uint8_t(Ren::eCullFace::Back);

            pi_vegetation_[0] =
                sh.FindOrCreatePipeline(rast_state, oit_blend_vegetation_prog, vi_vegetation, rp_oit_blend, 0);
        }

        initialized_ = true;
    }
}