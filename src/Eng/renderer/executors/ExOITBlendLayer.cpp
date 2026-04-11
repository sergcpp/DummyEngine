#include "ExOITBlendLayer.h"

#include <Ren/Context.h>

#include "../../utils/ShaderLoader.h"
#include "../Renderer_DrawList.h"
#include "../framegraph/FgBuilder.h"

Eng::ExOITBlendLayer::ExOITBlendLayer(
    PrimDraw &prim_draw, const DrawList **p_list, const view_state_t *view_state, const FgBufROHandle vtx_buf1,
    const FgBufROHandle vtx_buf2, const FgBufROHandle ndx_buf, const FgBufROHandle materials,
    const BindlessTextureData *bindless_tex, const FgBufROHandle cells, const FgBufROHandle items,
    const FgBufROHandle lights, const FgBufROHandle decals, const FgImgROHandle noise, const FgImgROHandle dummy_white,
    const FgImgROHandle shadow_depth, const FgImgROHandle ltc_luts, const FgImgROHandle env,
    const FgBufROHandle instances, const FgBufROHandle instance_indices, const FgBufROHandle shared_data,
    const FgImgRWHandle depth, const FgImgRWHandle color, const FgBufROHandle oit_depth,
    const FgImgROHandle oit_specular, const int depth_layer_index, const FgImgROHandle irradiance,
    const FgImgROHandle distance, const FgImgROHandle offset, const FgImgROHandle back_color,
    const FgImgROHandle back_depth)
    : prim_draw_(prim_draw), view_state_(view_state), bindless_tex_(bindless_tex), p_list_(p_list), vtx_buf1_(vtx_buf1),
      vtx_buf2_(vtx_buf2), ndx_buf_(ndx_buf), instances_(instances), instance_indices_(instance_indices),
      shared_data_(shared_data), materials_(materials), cells_(cells), items_(items), lights_(lights), decals_(decals),
      noise_(noise), dummy_white_(dummy_white), shadow_depth_(shadow_depth), ltc_luts_(ltc_luts), env_(env),
      oit_depth_(oit_depth), depth_layer_index_(depth_layer_index), oit_specular_(oit_specular),
      irradiance_(irradiance), distance_(distance), offset_(offset), back_color_(back_color), back_depth_(back_depth),
      depth_(depth), color_(color) {}

void Eng::ExOITBlendLayer::Execute(const FgContext &fg) {
    const Ren::ImageRWHandle depth = fg.AccessRWImage(depth_);
    const Ren::ImageRWHandle color = fg.AccessRWImage(color_);

    LazyInit(fg.ren_ctx(), fg.sh(), depth, color);
    DrawTransparent(fg, depth, color);
}

void Eng::ExOITBlendLayer::LazyInit(Ren::Context &ctx, Eng::ShaderLoader &sh, const Ren::ImageRWHandle depth,
                                    const Ren::ImageRWHandle color) {
    const Ren::RenderTarget color_targets[] = {{color, Ren::eLoadOp::Load, Ren::eStoreOp::Store}};
    const Ren::RenderTarget depth_target = {depth, Ren::eLoadOp::Load, Ren::eStoreOp::Store, Ren::eLoadOp::Load,
                                            Ren::eStoreOp::Store};
    if (!initialized) {
#if defined(REN_GL_BACKEND)
        const bool bindless = ctx.capabilities.bindless_texture;
#else
        const bool bindless = true;
#endif

        prog_oit_blit_depth_ = sh.FindOrCreateProgram("internal/blit.vert.glsl", "internal/blit_oit_depth.frag.glsl");

        Ren::ProgramHandle oit_blend_simple_prog, oit_blend_vegetation_prog;
        if (irradiance_) {
            if (oit_specular_) {
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
            if (oit_specular_) {
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

        initialized = true;
    }
}