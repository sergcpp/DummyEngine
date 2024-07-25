#pragma once

#include "../Renderer_DrawList.h"
#include "../graph/SubPass.h"

#include <Ren/VertexInput.h>

namespace Eng {
class PrimDraw;

class RpOITBlendLayer : public RpExecutor {
    PrimDraw &prim_draw_;
    bool initialized = false;

    // lazily initialized data
    Ren::ProgramRef prog_oit_blit_depth_;
    Ren::RenderPass rp_oit_blend_;
    Ren::VertexInput vi_simple_, vi_vegetation_;
    Ren::Pipeline pi_simple_[4][3];
    Ren::Pipeline pi_vegetation_[4][2];
    Ren::Framebuffer main_draw_fb_[Ren::MaxFramesInFlight][2];
    int fb_to_use_ = 0;

    // temp data (valid only between Setup and Execute calls)
    const ViewState *view_state_ = nullptr;
    const BindlessTextureData *bindless_tex_ = nullptr;

    const DrawList **p_list_ = nullptr;

    RpResRef vtx_buf1_;
    RpResRef vtx_buf2_;
    RpResRef ndx_buf_;
    RpResRef instances_buf_;
    RpResRef instance_indices_buf_;
    RpResRef shared_data_buf_;
    RpResRef materials_buf_;
    RpResRef textures_buf_;
    RpResRef cells_buf_;
    RpResRef items_buf_;
    RpResRef lights_buf_;
    RpResRef decals_buf_;
    RpResRef noise_tex_;
    RpResRef dummy_white_;
    RpResRef shadow_map_;
    RpResRef ltc_luts_tex_;
    RpResRef env_tex_;
    RpResRef oit_depth_buf_;
    int depth_layer_index_ = -1;
    RpResRef oit_specular_tex_;

    RpResRef irradiance_tex_;
    RpResRef distance_tex_;
    RpResRef offset_tex_;

    RpResRef back_color_tex_;
    RpResRef back_depth_tex_;

    RpResRef depth_tex_;
    RpResRef color_tex_;

    void LazyInit(Ren::Context &ctx, Eng::ShaderLoader &sh, RpAllocBuf &vtx_buf1, RpAllocBuf &vtx_buf2,
                  RpAllocBuf &ndx_buf, RpAllocTex &depth_tex, RpAllocTex &color_tex);
    void DrawTransparent(RpBuilder &builder, RpAllocTex &depth_tex);

  public:
    RpOITBlendLayer(PrimDraw &prim_draw) : prim_draw_(prim_draw) {}

    void Setup(const DrawList **p_list, const ViewState *view_state, const RpResRef vtx_buf1, const RpResRef vtx_buf2,
               const RpResRef ndx_buf, const RpResRef materials_buf, const RpResRef textures_buf,
               const BindlessTextureData *bindless_tex, const RpResRef cells_buf, const RpResRef items_buf,
               const RpResRef lights_buf, const RpResRef decals_buf, const RpResRef noise_tex,
               const RpResRef dummy_white, const RpResRef shadow_map, const RpResRef ltc_luts_tex,
               const RpResRef env_tex, const RpResRef instances_buf, const RpResRef instance_indices_buf,
               const RpResRef shared_data_buf, const RpResRef depth_tex, const RpResRef color_tex,
               const RpResRef oit_depth_buf, const RpResRef oit_specular_tex, int depth_layer_index,
               const RpResRef irradiance_tex, const RpResRef distance_tex, const RpResRef offset_tex,
               const RpResRef back_color_tex, const RpResRef back_depth_tex) {
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
        textures_buf_ = textures_buf;
        cells_buf_ = cells_buf;
        items_buf_ = items_buf;
        lights_buf_ = lights_buf;
        decals_buf_ = decals_buf;
        noise_tex_ = noise_tex;
        dummy_white_ = dummy_white;
        shadow_map_ = shadow_map;
        ltc_luts_tex_ = ltc_luts_tex;
        env_tex_ = env_tex;

        depth_tex_ = depth_tex;
        color_tex_ = color_tex;
        oit_depth_buf_ = oit_depth_buf;
        oit_specular_tex_ = oit_specular_tex;
        depth_layer_index_ = depth_layer_index;

        irradiance_tex_ = irradiance_tex;
        distance_tex_ = distance_tex;
        offset_tex_ = offset_tex;

        back_color_tex_ = back_color_tex;
        back_depth_tex_ = back_depth_tex;
    }

    void Execute(RpBuilder &builder) override;
};
} // namespace Eng