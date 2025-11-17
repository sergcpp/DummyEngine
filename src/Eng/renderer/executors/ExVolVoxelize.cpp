#include "ExVolVoxelize.h"

#include <Ren/Context.h>
#include <Ren/DrawCall.h>
#include <Ren/RastState.h>

#include "../../utils/ShaderLoader.h"
#include "../shaders/vol_interface.h"

void Eng::ExVolVoxelize::Execute(FgContext &fg) {
    LazyInit(fg.ren_ctx(), fg.sh());
    if (fg.ren_ctx().capabilities.hwrt) {
        Execute_HWRT(fg);
    } else {
        Execute_SWRT(fg);
    }
}

void Eng::ExVolVoxelize::LazyInit(Ren::Context &ctx, Eng::ShaderLoader &sh) {
    if (!initialized_) {
        if (ctx.capabilities.hwrt) {
            pi_vol_voxelize_ = sh.LoadPipeline("internal/vol_voxelize@HWRT.comp.glsl");
        } else {
            pi_vol_voxelize_ = sh.LoadPipeline("internal/vol_voxelize.comp.glsl");
        }
        initialized_ = true;
    }
}

void Eng::ExVolVoxelize::Execute_SWRT(FgContext &fg) {
    const Ren::Buffer &unif_sh_data_buf = fg.AccessROBuffer(args_->shared_data);
    const Ren::Texture &stbn_tex = fg.AccessROTexture(args_->stbn_tex);

    const Ren::Buffer &geo_data_buf = fg.AccessROBuffer(args_->geo_data);
    const Ren::Buffer &materials_buf = fg.AccessROBuffer(args_->materials);
    const Ren::Buffer &tlas_buf = fg.AccessROBuffer(args_->tlas_buf);

    const Ren::Buffer &blas_buf = fg.AccessROBuffer(args_->swrt.rt_blas_buf);
    const Ren::Buffer &prim_ndx_buf = fg.AccessROBuffer(args_->swrt.prim_ndx_buf);
    const Ren::Buffer &mesh_instances_buf = fg.AccessROBuffer(args_->swrt.mesh_instances_buf);

    const Ren::Buffer &vtx_buf1 = fg.AccessROBuffer(args_->swrt.vtx_buf1);
    const Ren::Buffer &ndx_buf = fg.AccessROBuffer(args_->swrt.ndx_buf);

    Ren::Texture &out_emission_tex = fg.AccessRWTexture(args_->out_emission_tex);
    Ren::Texture &out_scatter_tex = fg.AccessRWTexture(args_->out_scatter_tex);

    if (view_state_->skip_volumetrics) {
        return;
    }

    const Ren::Binding bindings[] = {{Ren::eBindTarget::UBuf, BIND_UB_SHARED_DATA_BUF, unif_sh_data_buf},
                                     {Ren::eBindTarget::TexSampled, Fog::STBN_TEX_SLOT, stbn_tex},
                                     {Ren::eBindTarget::SBufRO, Fog::GEO_DATA_BUF_SLOT, geo_data_buf},
                                     {Ren::eBindTarget::SBufRO, Fog::MATERIAL_BUF_SLOT, materials_buf},
                                     {Ren::eBindTarget::UTBuf, Fog::BLAS_BUF_SLOT, blas_buf},
                                     {Ren::eBindTarget::UTBuf, Fog::TLAS_BUF_SLOT, tlas_buf},
                                     {Ren::eBindTarget::UTBuf, Fog::PRIM_NDX_BUF_SLOT, prim_ndx_buf},
                                     {Ren::eBindTarget::UTBuf, Fog::MESH_INSTANCES_BUF_SLOT, mesh_instances_buf},
                                     {Ren::eBindTarget::UTBuf, Fog::VTX_BUF1_SLOT, vtx_buf1},
                                     {Ren::eBindTarget::UTBuf, Fog::NDX_BUF_SLOT, ndx_buf},
                                     {Ren::eBindTarget::ImageRW, Fog::OUT_FR_EMISSION_IMG_SLOT, out_emission_tex},
                                     {Ren::eBindTarget::ImageRW, Fog::OUT_FR_SCATTER_IMG_SLOT, out_scatter_tex}};

    const auto froxel_res =
        Ren::Vec4i{out_emission_tex.params.w, out_emission_tex.params.h, out_emission_tex.params.d, 0};

    const Ren::Vec3u grp_count = Ren::Vec3u{(froxel_res[0] + Fog::GRP_SIZE_X - 1u) / Fog::GRP_SIZE_X,
                                            (froxel_res[1] + Fog::GRP_SIZE_Y - 1u) / Fog::GRP_SIZE_Y, 1u};

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

    DispatchCompute(*pi_vol_voxelize_, grp_count, bindings, &uniform_params, sizeof(uniform_params), fg.descr_alloc(),
                    fg.log());
}

#if defined(REN_GL_BACKEND)
void Eng::ExVolVoxelize::Execute_HWRT(FgContext &fg) { assert(false && "Not implemented!"); }
#endif