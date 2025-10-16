#include "ExOITScheduleRays.h"

#include <Ren/Context.h>

#include "../../utils/ShaderLoader.h"

Eng::ExOITScheduleRays::ExOITScheduleRays(const DrawList **p_list, const view_state_t *view_state,
                                          const FgResRef vtx_buf1, const FgResRef vtx_buf2, const FgResRef ndx_buf,
                                          const FgResRef materials_buf, const BindlessTextureData *bindless_tex,
                                          const FgResRef noise_tex, const FgResRef dummy_white,
                                          const FgResRef instances_buf, const FgResRef instance_indices_buf,
                                          const FgResRef shared_data_buf, const FgResRef depth_tex,
                                          const FgResRef oit_depth_buf, const FgResRef ray_counter,
                                          const FgResRef ray_list) {
    view_state_ = view_state;
    bindless_tex_ = bindless_tex;

    p_list_ = p_list;

    vtx_buf1_ = vtx_buf1;
    vtx_buf2_ = vtx_buf2;
    ndx_buf_ = ndx_buf;
    instances_buf_ = instances_buf;
    instance_indices_buf_ = instance_indices_buf;
    shared_data_buf_ = shared_data_buf;
    materials_buf_ = materials_buf;
    noise_tex_ = noise_tex;
    dummy_white_ = dummy_white;
    oit_depth_buf_ = oit_depth_buf;
    ray_counter_ = ray_counter;
    ray_list_ = ray_list;

    depth_tex_ = depth_tex;
}

void Eng::ExOITScheduleRays::Execute(FgContext &fg) {
    FgAllocBuf &vtx_buf1 = fg.AccessROBuffer(vtx_buf1_);
    FgAllocBuf &vtx_buf2 = fg.AccessROBuffer(vtx_buf2_);
    FgAllocBuf &ndx_buf = fg.AccessROBuffer(ndx_buf_);
    FgAllocTex &depth_tex = fg.AccessRWTexture(depth_tex_);

    LazyInit(fg.ren_ctx(), fg.sh(), vtx_buf1, vtx_buf2, ndx_buf, depth_tex);
    DrawTransparent(fg, depth_tex);
}

void Eng::ExOITScheduleRays::LazyInit(Ren::Context &ctx, Eng::ShaderLoader &sh, FgAllocBuf &vtx_buf1,
                                      FgAllocBuf &vtx_buf2, FgAllocBuf &ndx_buf, FgAllocTex &depth_tex) {
    const Ren::RenderTarget depth_target = {depth_tex.ref, Ren::eLoadOp::Load, Ren::eStoreOp::Store, Ren::eLoadOp::Load,
                                            Ren::eStoreOp::Store};
    if (!initialized) {
#if defined(REN_GL_BACKEND)
        const bool bindless = ctx.capabilities.bindless_texture;
#else
        const bool bindless = true;
#endif

        const int buf1_stride = 16, buf2_stride = 16;

        Ren::VertexInputRef vi_simple, vi_vegetation;

        { // VertexInput for simple and skinned meshes
            const Ren::VtxAttribDesc attribs[] = {
                // Attributes from buffer 1
                {vtx_buf1.ref, VTX_POS_LOC, 3, Ren::eType::Float32, buf1_stride, 0},
                {vtx_buf1.ref, VTX_UV1_LOC, 2, Ren::eType::Float16, buf1_stride, 3 * sizeof(float)},
                // Attributes from buffer 2
                {vtx_buf2.ref, VTX_NOR_LOC, 4, Ren::eType::Int16_snorm, buf2_stride, 0},
                {vtx_buf2.ref, VTX_TAN_LOC, 2, Ren::eType::Int16_snorm, buf2_stride, 4 * sizeof(uint16_t)}};
            vi_simple = sh.LoadVertexInput(attribs, ndx_buf.ref);
        }
        { // VertexInput for vegetation meshes (uses additional vertex color attribute)
            const Ren::VtxAttribDesc attribs[] = {
                // Attributes from buffer 1
                {vtx_buf1.ref, VTX_POS_LOC, 3, Ren::eType::Float32, buf1_stride, 0},
                {vtx_buf1.ref, VTX_UV1_LOC, 2, Ren::eType::Float16, buf1_stride, 3 * sizeof(float)},
                // Attributes from buffer 2
                {vtx_buf2.ref, VTX_NOR_LOC, 4, Ren::eType::Int16_snorm, buf2_stride, 0},
                {vtx_buf2.ref, VTX_TAN_LOC, 2, Ren::eType::Int16_snorm, buf2_stride, 4 * sizeof(uint16_t)},
                {vtx_buf2.ref, VTX_AUX_LOC, 1, Ren::eType::Uint32, buf2_stride, 6 * sizeof(uint16_t)}};
            vi_vegetation = sh.LoadVertexInput(attribs, ndx_buf.ref);
        }

        Ren::ProgramRef oit_blend_simple_prog = sh.LoadProgram(
            bindless ? "internal/oit_schedule_rays.vert.glsl" : "internal/oit_schedule_rays@NO_BINDLESS.vert.glsl",
            bindless ? "internal/oit_schedule_rays.frag.glsl" : "internal/oit_schedule_rays@NO_BINDLESS.frag.glsl");
        Ren::ProgramRef oit_blend_vegetation_prog = sh.LoadProgram(
            bindless ? "internal/oit_schedule_rays@VEGETATION.vert.glsl"
                     : "internal/oit_schedule_rays@VEGETATION;NO_BINDLESS.vert.glsl",
            bindless ? "internal/oit_schedule_rays.frag.glsl" : "internal/oit_schedule_rays@NO_BINDLESS.frag.glsl");

        Ren::RenderPassRef rp_oit_schedule_rays = sh.LoadRenderPass(depth_target, {});

        { // simple and skinned
            Ren::RastState rast_state;
            rast_state.depth.test_enabled = true;
            rast_state.depth.write_enabled = false;
            rast_state.depth.compare_op = uint8_t(Ren::eCompareOp::Greater);

            pi_simple_[2] = sh.LoadPipeline(rast_state, oit_blend_simple_prog, vi_simple, rp_oit_schedule_rays, 0);

            rast_state.poly.cull = uint8_t(Ren::eCullFace::Back);

            pi_simple_[0] = sh.LoadPipeline(rast_state, oit_blend_simple_prog, vi_simple, rp_oit_schedule_rays, 0);

            rast_state.poly.cull = uint8_t(Ren::eCullFace::Front);

            pi_simple_[1] = sh.LoadPipeline(rast_state, oit_blend_simple_prog, vi_simple, rp_oit_schedule_rays, 0);
        }
        { // vegetation
            Ren::RastState rast_state;
            rast_state.depth.test_enabled = true;
            rast_state.depth.write_enabled = false;
            rast_state.depth.compare_op = uint8_t(Ren::eCompareOp::Greater);

            pi_vegetation_[1] =
                sh.LoadPipeline(rast_state, oit_blend_vegetation_prog, vi_vegetation, rp_oit_schedule_rays, 0);

            rast_state.poly.cull = uint8_t(Ren::eCullFace::Back);

            pi_vegetation_[0] =
                sh.LoadPipeline(rast_state, oit_blend_vegetation_prog, vi_vegetation, rp_oit_schedule_rays, 0);
        }

        initialized = true;
    }

    fb_to_use_ = (fb_to_use_ + 1) % 2;

    if (!main_draw_fb_[ctx.backend_frame()][fb_to_use_].Setup(ctx.api_ctx(), *pi_simple_[0]->render_pass(),
                                                              depth_tex.desc.w, depth_tex.desc.h, depth_target,
                                                              depth_target, {}, ctx.log())) {
        ctx.log()->Error("[ExOITScheduleRays::LazyInit]: main_draw_fb_ init failed!");
    }
}