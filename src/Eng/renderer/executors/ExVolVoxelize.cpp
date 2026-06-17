#include "ExVolVoxelize.h"

#include <Ren/Context.h>
#include <Ren/DrawCall.h>
#include <Ren/RastState.h>

#include "../../utils/ShaderLoader.h"
#include "../Renderer_DrawList.h"
#include "../framegraph/FgBuilder.h"
#include "../shaders/vol_interface.h"

void Eng::ExVolVoxelize::Execute(const FgContext &fg) {
    LazyInit(fg);
    if (fg.ren_ctx().capabilities.hwrt) {
        Execute_HWRT(fg);
    } else {
        Execute_SWRT(fg);
    }
}

void Eng::ExVolVoxelize::LazyInit(const FgContext &fg) {
    auto &ctx = fg.ren_ctx();
    auto &sh = fg.sh();
    if (!initialized_) {
        if (ctx.capabilities.hwrt) {
            pi_vol_voxelize_ = sh.FindOrCreatePipeline("internal/vol_voxelize@HWRT.comp.glsl");
        } else {
            pi_vol_voxelize_ = sh.FindOrCreatePipeline("internal/vol_voxelize.comp.glsl");
        }
        initialized_ = true;
    }
}

void Eng::ExVolVoxelize::Execute_SWRT(const FgContext &fg) {
    const Ren::BufferROHandle unif_sh_data = fg.AccessROBuffer(args_->shared_data);
    const Ren::ImageROHandle stbn = fg.AccessROImage(args_->stbn);

    const Ren::BufferROHandle geo_data = fg.AccessROBuffer(args_->geo_data);
    const Ren::BufferROHandle materials = fg.AccessROBuffer(args_->materials);
    const Ren::BufferROHandle tlas_buf = fg.AccessROBuffer(args_->tlas_buf);

    const Ren::BufferROHandle blas_buf = fg.AccessROBuffer(args_->swrt.rt_blas_buf);
    const Ren::BufferROHandle prim_ndx = fg.AccessROBuffer(args_->swrt.prim_ndx);
    const Ren::BufferROHandle mesh_instances = fg.AccessROBuffer(args_->swrt.mesh_instances);

    const Ren::BufferROHandle vtx_buf1 = fg.AccessROBuffer(args_->swrt.vtx_buf1);
    const Ren::BufferROHandle ndx_buf = fg.AccessROBuffer(args_->swrt.ndx_buf);

    const Ren::ImageRWHandle out_emission = fg.AccessRWImage(args_->out_emission);
    const Ren::ImageRWHandle out_scatter = fg.AccessRWImage(args_->out_scatter);

    if (view_state_->skip_volumetrics) {
        return;
    }

    const Ren::Binding bindings[] = {{Ren::eBindTarget::UBuf, BIND_UB_SHARED_DATA_BUF, unif_sh_data},
                                     {Ren::eBindTarget::TexSampled, Fog::STBN_TEX_SLOT, stbn},
                                     {Ren::eBindTarget::SBufRO, Fog::GEO_DATA_BUF_SLOT, geo_data},
                                     {Ren::eBindTarget::SBufRO, Fog::MATERIAL_BUF_SLOT, materials},
                                     {Ren::eBindTarget::UTBuf, Fog::BLAS_BUF_SLOT, blas_buf},
                                     {Ren::eBindTarget::UTBuf, Fog::TLAS_BUF_SLOT, tlas_buf},
                                     {Ren::eBindTarget::UTBuf, Fog::PRIM_NDX_BUF_SLOT, prim_ndx},
                                     {Ren::eBindTarget::UTBuf, Fog::MESH_INSTANCES_BUF_SLOT, mesh_instances},
                                     {Ren::eBindTarget::UTBuf, Fog::VTX_BUF1_SLOT, vtx_buf1},
                                     {Ren::eBindTarget::UTBuf, Fog::NDX_BUF_SLOT, ndx_buf},
                                     {Ren::eBindTarget::ImageRW, Fog::OUT_FR_EMISSION_IMG_SLOT, out_emission},
                                     {Ren::eBindTarget::ImageRW, Fog::OUT_FR_SCATTER_IMG_SLOT, out_scatter}};

    const Ren::ImageCold &img_cold = fg.storages().images[out_emission].second;
    const auto froxel_res = Ren::Vec4u{img_cold.params.w, img_cold.params.h, img_cold.params.d, 0};

    const Ren::Vec3u grp_count = Ren::Vec3u(Ren::DivCeil(froxel_res[0], Fog::GRP_SIZE_2D_X),
                                            Ren::DivCeil(froxel_res[1], Fog::GRP_SIZE_2D_Y), 1u);

    Fog::Params uniform_params;
    uniform_params.froxel_res = froxel_res;
    uniform_params.scatter_color =
        Ren::Saturate(Ren::Vec4f{(*p_list_)->env.fog.scatter_color, (*p_list_)->env.fog.absorption});
    uniform_params.emission_color =
        Ren::Vec4f{(*p_list_)->env.fog.emission_color, std::max((*p_list_)->env.fog.density, 0.0f)};
    uniform_params.anisotropy = Ren::Clamp((*p_list_)->env.fog.anisotropy, -0.99f, 0.99f);
    uniform_params.bbox_min = Ren::Vec4f{(*p_list_)->env.fog.bbox_min};
    uniform_params.bbox_max = Ren::Vec4f{(*p_list_)->env.fog.bbox_max};
    uniform_params.frame_index = view_state_->frame_index;
    uniform_params.hist_weight = (view_state_->pre_exposure / view_state_->prev_pre_exposure);

    DispatchCompute(fg.cmd_buf(), pi_vol_voxelize_, fg.storages(), grp_count, bindings, &uniform_params,
                    sizeof(uniform_params), fg.descr_alloc(), fg.log());
}

#if defined(REN_GL_BACKEND)
void Eng::ExVolVoxelize::Execute_HWRT(const FgContext &fg) { assert(false && "Not implemented!"); }
#endif