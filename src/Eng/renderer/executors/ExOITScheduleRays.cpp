#include "ExOITScheduleRays.h"

#include <Ren/Context.h>

#include "../../utils/ShaderLoader.h"
#include "../Renderer_DrawList.h"
#include "../framegraph/FgBuilder.h"

Eng::ExOITScheduleRays::ExOITScheduleRays(const DrawList **p_list, const view_state_t *view_state,
                                          const FgBufROHandle vtx_buf1, const FgBufROHandle vtx_buf2,
                                          const FgBufROHandle ndx_buf, const FgBufROHandle materials,
                                          const BindlessTextureData *bindless_tex, const FgImgROHandle noise,
                                          const FgImgROHandle dummy_white, const FgBufROHandle instances,
                                          const FgBufROHandle instance_indices, const FgBufROHandle shared_data,
                                          const FgImgRWHandle depth, const FgBufROHandle oit_depth,
                                          const FgBufRWHandle ray_counter, const FgBufRWHandle ray_list,
                                          const FgBufRWHandle ray_bitmask) {
    view_state_ = view_state;
    bindless_tex_ = bindless_tex;

    p_list_ = p_list;

    vtx_buf1_ = vtx_buf1;
    vtx_buf2_ = vtx_buf2;
    ndx_buf_ = ndx_buf;
    instances_ = instances;
    instance_indices_ = instance_indices;
    shared_data_ = shared_data;
    materials_ = materials;
    noise_ = noise;
    dummy_white_ = dummy_white;
    oit_depth_ = oit_depth;
    ray_counter_ = ray_counter;
    ray_list_ = ray_list;
    ray_bitmask_ = ray_bitmask;

    depth_ = depth;
}

void Eng::ExOITScheduleRays::Execute(const FgContext &fg) {
    const Ren::ImageRWHandle depth = fg.AccessRWImage(depth_);

    LazyInit(fg, depth);
    DrawTransparent(fg, depth);
}

void Eng::ExOITScheduleRays::LazyInit(const FgContext &fg, const Ren::ImageRWHandle depth) {
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

        const Ren::ProgramHandle oit_blend_simple_prog = sh.FindOrCreateProgram(
            bindless ? "internal/oit_schedule_rays.vert.glsl" : "internal/oit_schedule_rays@NO_BINDLESS.vert.glsl",
            bindless ? "internal/oit_schedule_rays.frag.glsl" : "internal/oit_schedule_rays@NO_BINDLESS.frag.glsl");
        const Ren::ProgramHandle oit_blend_vegetation_prog = sh.FindOrCreateProgram(
            bindless ? "internal/oit_schedule_rays@VEGETATION.vert.glsl"
                     : "internal/oit_schedule_rays@VEGETATION;NO_BINDLESS.vert.glsl",
            bindless ? "internal/oit_schedule_rays.frag.glsl" : "internal/oit_schedule_rays@NO_BINDLESS.frag.glsl");

        const Ren::RenderPassHandle rp_oit_schedule_rays = sh.FindOrCreateRenderPass(depth_target, {});

        { // simple and skinned
            Ren::RastState rast_state;
            rast_state.depth.test_enabled = true;
            rast_state.depth.write_enabled = false;
            rast_state.depth.compare_op = uint8_t(Ren::eCompareOp::Greater);

            pi_simple_[2] =
                sh.FindOrCreatePipeline(rast_state, oit_blend_simple_prog, vi_simple, rp_oit_schedule_rays, 0);

            rast_state.poly.cull = uint8_t(Ren::eCullFace::Back);

            pi_simple_[0] =
                sh.FindOrCreatePipeline(rast_state, oit_blend_simple_prog, vi_simple, rp_oit_schedule_rays, 0);

            rast_state.poly.cull = uint8_t(Ren::eCullFace::Front);

            pi_simple_[1] =
                sh.FindOrCreatePipeline(rast_state, oit_blend_simple_prog, vi_simple, rp_oit_schedule_rays, 0);
        }
        { // vegetation
            Ren::RastState rast_state;
            rast_state.depth.test_enabled = true;
            rast_state.depth.write_enabled = false;
            rast_state.depth.compare_op = uint8_t(Ren::eCompareOp::Greater);

            pi_vegetation_[1] =
                sh.FindOrCreatePipeline(rast_state, oit_blend_vegetation_prog, vi_vegetation, rp_oit_schedule_rays, 0);

            rast_state.poly.cull = uint8_t(Ren::eCullFace::Back);

            pi_vegetation_[0] =
                sh.FindOrCreatePipeline(rast_state, oit_blend_vegetation_prog, vi_vegetation, rp_oit_schedule_rays, 0);
        }

        initialized_ = true;
    }
}