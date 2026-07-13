#include "ExDebugRT.h"

#include <Ren/Context.h>
#include <Ren/DrawCall.h>

#include "../../utils/ShaderLoader.h"
#include "../framegraph/FgBuilder.h"
#include "../shaders/rt_debug_interface.h"

Eng::ExDebugRT::ExDebugRT(ShaderLoader &sh, const view_state_t *view_state, const BindlessTextureData *bindless_tex,
                          const Args *args) {
    view_state_ = view_state;
    bindless_tex_ = bindless_tex;
    args_ = args;
#if defined(REN_VK_BACKEND)
    if (sh.ren_ctx().capabilities.hwrt) {
        Ren::ProgramHandle debug_hwrt_prog =
            sh.FindOrCreateProgram2("internal/rt_debug.rgen.glsl", "internal/rt_debug.rchit.glsl",
                                    "internal/rt_debug.rahit.glsl", "internal/rt_debug.rmiss.glsl", {});
        pi_debug_ = sh.FindOrCreatePipeline(debug_hwrt_prog);
    } else
#endif
    {
        pi_debug_ = sh.FindOrCreatePipeline("internal/rt_debug_swrt.comp.glsl");
    }
}

void Eng::ExDebugRT::Execute(const FgContext &fg) {
    if (fg.ren_ctx().capabilities.hwrt) {
        Execute_HWRT(fg);
    } else {
        Execute_SWRT(fg);
    }
}

void Eng::ExDebugRT::Execute_SWRT(const FgContext &fg) {
    const Ren::BufferROHandle geo_data = fg.AccessROBuffer(args_->geo_data);
    const Ren::BufferROHandle materials = fg.AccessROBuffer(args_->materials);
    const Ren::BufferROHandle vtx_buf1 = fg.AccessROBuffer(args_->vtx_buf1);
    const Ren::BufferROHandle vtx_buf2 = fg.AccessROBuffer(args_->vtx_buf2);
    const Ren::BufferROHandle ndx_buf = fg.AccessROBuffer(args_->ndx_buf);
    const Ren::BufferROHandle lights = fg.AccessROBuffer(args_->lights);
    const Ren::BufferROHandle rt_tlas_buf = fg.AccessROBuffer(args_->tlas_buf);
    const Ren::BufferROHandle rt_blas_buf = fg.AccessROBuffer(args_->swrt.rt_blas);
    const Ren::BufferROHandle prim_ndx = fg.AccessROBuffer(args_->swrt.prim_ndx);
    const Ren::BufferROHandle mesh_instances = fg.AccessROBuffer(args_->swrt.mesh_instances);
    const Ren::BufferROHandle unif_sh_data = fg.AccessROBuffer(args_->shared_data);
    const Ren::ImageROHandle env = fg.AccessROImage(args_->env);
    const Ren::ImageROHandle shadow_depth = fg.AccessROImage(args_->shadow_depth);
    const Ren::ImageROHandle shadow_color = fg.AccessROImage(args_->shadow_color);
    const Ren::ImageROHandle ltc_luts = fg.AccessROImage(args_->ltc_luts);
    const Ren::BufferROHandle cells = fg.AccessROBuffer(args_->cells);
    const Ren::BufferROHandle items = fg.AccessROBuffer(args_->items);

    const Ren::ImageROHandle irr = fg.AccessROImage(args_->irradiance);
    const Ren::ImageROHandle dist = fg.AccessROImage(args_->distance);
    const Ren::ImageROHandle off = fg.AccessROImage(args_->offset);

    const Ren::BufferROHandle cache_entries = fg.AccessROBuffer(args_->cache_entries);
    const Ren::BufferROHandle cache_voxels = fg.AccessROBuffer(args_->cache_voxels);

    const Ren::ImageRWHandle output = fg.AccessRWImage(args_->output);

    const Ren::Binding bindings[] = {
        {Ren::eBindTarget::UBuf, BIND_UB_SHARED_DATA_BUF, unif_sh_data},
        {Ren::eBindTarget::BindlessDescriptors, BIND_BINDLESS_TEX, bindless_tex_->rt_inline_textures},
        {Ren::eBindTarget::SBufRO, RTDebug::GEO_DATA_BUF_SLOT, geo_data},
        {Ren::eBindTarget::SBufRO, RTDebug::MATERIAL_BUF_SLOT, materials},
        {Ren::eBindTarget::UTBuf, RTDebug::VTX_BUF1_SLOT, vtx_buf1},
        {Ren::eBindTarget::UTBuf, RTDebug::VTX_BUF2_SLOT, vtx_buf2},
        {Ren::eBindTarget::UTBuf, RTDebug::NDX_BUF_SLOT, ndx_buf},
        {Ren::eBindTarget::UTBuf, RTDebug::BLAS_BUF_SLOT, rt_blas_buf},
        {Ren::eBindTarget::UTBuf, RTDebug::TLAS_BUF_SLOT, rt_tlas_buf},
        {Ren::eBindTarget::UTBuf, RTDebug::PRIM_NDX_BUF_SLOT, prim_ndx},
        {Ren::eBindTarget::UTBuf, RTDebug::MESH_INSTANCES_BUF_SLOT, mesh_instances},
        {Ren::eBindTarget::SBufRO, RTDebug::LIGHTS_BUF_SLOT, lights},
        {Ren::eBindTarget::TexSampled, RTDebug::ENV_TEX_SLOT, env},
        {Ren::eBindTarget::TexSampled, RTDebug::SHADOW_DEPTH_TEX_SLOT, shadow_depth},
        {Ren::eBindTarget::TexSampled, RTDebug::SHADOW_COLOR_TEX_SLOT, shadow_color},
        {Ren::eBindTarget::TexSampled, RTDebug::LTC_LUTS_TEX_SLOT, ltc_luts},
        {Ren::eBindTarget::UTBuf, RTDebug::CELLS_BUF_SLOT, cells},
        {Ren::eBindTarget::UTBuf, RTDebug::ITEMS_BUF_SLOT, items},
        {Ren::eBindTarget::TexSampled, RTDebug::IRRADIANCE_TEX_SLOT, irr},
        {Ren::eBindTarget::TexSampled, RTDebug::DISTANCE_TEX_SLOT, dist},
        {Ren::eBindTarget::TexSampled, RTDebug::OFFSET_TEX_SLOT, off},
        {Ren::eBindTarget::SBufRO, RTDebug::CACHE_ENTRIES_BUF_SLOT, cache_entries},
        {Ren::eBindTarget::SBufRO, RTDebug::CACHE_VOXELS_BUF_SLOT, cache_voxels},
        {Ren::eBindTarget::ImageRW, RTDebug::OUT_IMG_SLOT, output}};

    const auto grp_count = Ren::Vec3u(Ren::DivCeil(view_state_->ren_res[0], RTDebug::GRP_SIZE_X),
                                      Ren::DivCeil(view_state_->ren_res[1], RTDebug::GRP_SIZE_Y), 1u);

    RTDebug::Params uniform_params;
    uniform_params.img_size = Ren::Vec2u{view_state_->ren_res};
    uniform_params.pixel_spread_angle = view_state_->pixel_spread_angle;
    uniform_params.root_node = args_->swrt.root_node;
    uniform_params.cull_mask = args_->cull_mask;

    DispatchCompute(fg.cmd_buf(), pi_debug_, fg.storages(), grp_count, bindings, &uniform_params,
                    sizeof(uniform_params), fg.descr_alloc(), fg.log());
}

#if defined(REN_GL_BACKEND)
void Eng::ExDebugRT::Execute_HWRT(const FgContext &fg) { assert(false && "Not implemented!"); }
#endif
