#include "ExVolVoxelize.h"

#include <Ren/Context.h>
#include <Ren/RastState.h>
#include <Ren/Texture.h>
#include <Ren/VKCtx.h>

#include "../../utils/ShaderLoader.h"
#include "../PrimDraw.h"
#include "../Renderer_Structs.h"
#include "../shaders/vol_interface.h"

void Eng::ExVolVoxelize::Execute_HWRT(FgBuilder &builder) {
    FgAllocBuf &unif_sh_data_buf = builder.GetReadBuffer(args_->shared_data);
    FgAllocBuf &bn_pmj_seq_buf = builder.GetReadBuffer(args_->bn_pmj_seq);

    FgAllocBuf &geo_data_buf = builder.GetReadBuffer(args_->geo_data);
    FgAllocBuf &materials_buf = builder.GetReadBuffer(args_->materials);
    FgAllocBuf &tlas_buf = builder.GetReadBuffer(args_->tlas_buf);

    FgAllocTex &out_emission_tex = builder.GetWriteTexture(args_->out_emission_tex);
    FgAllocTex &out_scatter_tex = builder.GetWriteTexture(args_->out_scatter_tex);

    if (view_state_->skip_volumetrics) {
        return;
    }

    auto *acc_struct = static_cast<Ren::AccStructureVK *>(args_->tlas);

    const Ren::Binding bindings[] = {{Ren::eBindTarget::UBuf, BIND_UB_SHARED_DATA_BUF, *unif_sh_data_buf.ref},
                                     {Ren::eBindTarget::UTBuf, Fog::BN_PMJ_SEQ_BUF_SLOT, *bn_pmj_seq_buf.ref},
                                     {Ren::eBindTarget::SBufRO, Fog::GEO_DATA_BUF_SLOT, *geo_data_buf.ref},
                                     {Ren::eBindTarget::SBufRO, Fog::MATERIAL_BUF_SLOT, *materials_buf.ref},
                                     {Ren::eBindTarget::AccStruct, Fog::TLAS_SLOT, *acc_struct},
                                     {Ren::eBindTarget::ImageRW, Fog::OUT_FR_EMISSION_IMG_SLOT, *out_emission_tex.ref},
                                     {Ren::eBindTarget::ImageRW, Fog::OUT_FR_SCATTER_IMG_SLOT, *out_scatter_tex.ref}};

    const auto froxel_res =
        Ren::Vec4i{out_emission_tex.ref->params.w, out_emission_tex.ref->params.h, out_emission_tex.ref->params.d, 0};

    const Ren::Vec3u grp_count =
        Ren::Vec3u{(froxel_res[0] + Fog::LOCAL_GROUP_SIZE_X - 1u) / Fog::LOCAL_GROUP_SIZE_X,
                   (froxel_res[1] + Fog::LOCAL_GROUP_SIZE_Y - 1u) / Fog::LOCAL_GROUP_SIZE_Y, 1u};

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

    DispatchCompute(*pi_vol_voxelize_, grp_count, bindings, &uniform_params, sizeof(uniform_params),
                    builder.ctx().default_descr_alloc(), builder.ctx().log());
}
