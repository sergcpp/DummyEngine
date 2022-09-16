#include "RpDebugRT.h"

#include <Ren/Context.h>
#include <Ren/RastState.h>
#include <Ren/Texture.h>

#include "../../Utils/ShaderLoader.h"
#include "../PrimDraw.h"
#include "../Renderer_Structs.h"

#include "../assets/shaders/internal/rt_debug_interface.glsl"

void RpDebugRT::Execute(RpBuilder &builder) {
    LazyInit(builder.ctx(), builder.sh());

#if !defined(USE_GL_RENDER)
    if (builder.ctx().capabilities.raytracing) {
        Execute_HWRT(builder);
    } else
#endif
    {
        Execute_SWRT(builder);
    }
}

void RpDebugRT::LazyInit(Ren::Context &ctx, ShaderLoader &sh) {
    if (!initialized) {
#if defined(USE_VK_RENDER)
        if (ctx.capabilities.raytracing) {
            Ren::ProgramRef debug_hwrt_prog =
                sh.LoadProgram(ctx, "rt_debug", "internal/rt_debug.rgen.glsl", "internal/rt_debug.rchit.glsl",
                               "internal/rt_debug.rahit.glsl", "internal/rt_debug.rmiss.glsl", nullptr);
            assert(debug_hwrt_prog->ready());

            if (!pi_debug_hwrt_.Init(ctx.api_ctx(), debug_hwrt_prog, ctx.log())) {
                ctx.log()->Error("RpDebugRT: Failed to initialize pipeline!");
            }
        }
#endif
        Ren::ProgramRef debug_swrt_prog = sh.LoadProgram(ctx, "rt_debug_swrt", "internal/rt_debug_swrt.comp.glsl");
        assert(debug_swrt_prog->ready());

        if (!pi_debug_swrt_.Init(ctx.api_ctx(), debug_swrt_prog, ctx.log())) {
            ctx.log()->Error("RpDebugRT: Failed to initialize pipeline!");
        }

        initialized = true;
    }
}

void RpDebugRT::Execute_SWRT(RpBuilder &builder) {
    RpAllocBuf &geo_data_buf = builder.GetReadBuffer(pass_data_->geo_data_buf);
    RpAllocBuf &materials_buf = builder.GetReadBuffer(pass_data_->materials_buf);
    RpAllocBuf &vtx_buf1 = builder.GetReadBuffer(pass_data_->vtx_buf1);
    RpAllocBuf &vtx_buf2 = builder.GetReadBuffer(pass_data_->vtx_buf2);
    RpAllocBuf &ndx_buf = builder.GetReadBuffer(pass_data_->ndx_buf);
    RpAllocBuf &rt_blas_buf = builder.GetReadBuffer(pass_data_->swrt.rt_blas_buf);
    RpAllocBuf &rt_tlas_buf = builder.GetReadBuffer(pass_data_->swrt.rt_tlas_buf);
    RpAllocBuf &prim_ndx_buf = builder.GetReadBuffer(pass_data_->swrt.prim_ndx_buf);
    RpAllocBuf &meshes_buf = builder.GetReadBuffer(pass_data_->swrt.meshes_buf);
    RpAllocBuf &mesh_instances_buf = builder.GetReadBuffer(pass_data_->swrt.mesh_instances_buf);
    RpAllocBuf &unif_sh_data_buf = builder.GetReadBuffer(pass_data_->shared_data);
    RpAllocTex &env_tex = builder.GetReadTexture(pass_data_->env_tex);
    RpAllocTex &dummy_black = builder.GetReadTexture(pass_data_->dummy_black);
    RpAllocTex *lm_tex[5];
    for (int i = 0; i < 5; ++i) {
        if (pass_data_->lm_tex[i]) {
            lm_tex[i] = &builder.GetReadTexture(pass_data_->lm_tex[i]);
        } else {
            lm_tex[i] = &dummy_black;
        }
    }
    RpAllocTex *output_tex = &builder.GetWriteTexture(pass_data_->output_tex);

    const Ren::Binding bindings[] = {
        //{Ren::eBindTarget::SBuf, RTDebug::GEO_DATA_BUF_SLOT, *geo_data_buf.ref},
        //{Ren::eBindTarget::SBuf, RTDebug::MATERIAL_BUF_SLOT, *materials_buf.ref},
        {Ren::eBindTarget::SBuf, RTDebug::VTX_BUF1_SLOT, *vtx_buf1.ref},
        {Ren::eBindTarget::SBuf, RTDebug::VTX_BUF2_SLOT, *vtx_buf2.ref},
        {Ren::eBindTarget::SBuf, RTDebug::NDX_BUF_SLOT, *ndx_buf.ref},
        {Ren::eBindTarget::SBuf, RTDebug::BLAS_BUF_SLOT, *rt_blas_buf.ref},
        {Ren::eBindTarget::SBuf, RTDebug::TLAS_BUF_SLOT, *rt_tlas_buf.ref},
        {Ren::eBindTarget::SBuf, RTDebug::PRIM_NDX_BUF_SLOT, *prim_ndx_buf.ref},
        {Ren::eBindTarget::SBuf, RTDebug::MESHES_BUF_SLOT, *meshes_buf.ref},
        {Ren::eBindTarget::SBuf, RTDebug::MESH_INSTANCES_BUF_SLOT, *mesh_instances_buf.ref},
        {Ren::eBindTarget::UBuf, REN_UB_SHARED_DATA_LOC, *unif_sh_data_buf.ref},
        {Ren::eBindTarget::Image, RTDebug::OUT_IMG_SLOT, *output_tex->ref}};

    const auto grp_count =
        Ren::Vec3u{(view_state_->act_res[0] + RTDebug::LOCAL_GROUP_SIZE_X - 1u) / RTDebug::LOCAL_GROUP_SIZE_X,
                   (view_state_->act_res[1] + RTDebug::LOCAL_GROUP_SIZE_Y - 1u) / RTDebug::LOCAL_GROUP_SIZE_Y, 1u};

    RTDebug::Params uniform_params;
    uniform_params.img_size[0] = view_state_->act_res[0];
    uniform_params.img_size[1] = view_state_->act_res[1];
    uniform_params.pixel_spread_angle = std::atan(
        2.0f * std::tan(0.5f * view_state_->vertical_fov * Ren::Pi<float>() / 180.0f) / float(view_state_->act_res[1]));
    uniform_params.root_node = pass_data_->swrt.root_node;

    Ren::DispatchCompute(pi_debug_swrt_, grp_count, bindings, &uniform_params, sizeof(uniform_params),
                         builder.ctx().default_descr_alloc(), builder.ctx().log());
}