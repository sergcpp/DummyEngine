#pragma once

#include "../Renderer_DrawList.h"
#include "../framegraph/FgNode.h"

#include <Ren/VertexInput.h>

namespace Eng {
class PrimDraw;

class ExOITBlendLayer : public FgExecutor {
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

    FgResRef vtx_buf1_;
    FgResRef vtx_buf2_;
    FgResRef ndx_buf_;
    FgResRef instances_buf_;
    FgResRef instance_indices_buf_;
    FgResRef shared_data_buf_;
    FgResRef materials_buf_;
    FgResRef textures_buf_;
    FgResRef cells_buf_;
    FgResRef items_buf_;
    FgResRef lights_buf_;
    FgResRef decals_buf_;
    FgResRef noise_tex_;
    FgResRef dummy_white_;
    FgResRef shadow_map_;
    FgResRef ltc_luts_tex_;
    FgResRef env_tex_;
    FgResRef oit_depth_buf_;
    int depth_layer_index_ = -1;
    FgResRef oit_specular_tex_;

    FgResRef irradiance_tex_;
    FgResRef distance_tex_;
    FgResRef offset_tex_;

    FgResRef back_color_tex_;
    FgResRef back_depth_tex_;

    FgResRef depth_tex_;
    FgResRef color_tex_;

    void LazyInit(Ren::Context &ctx, Eng::ShaderLoader &sh, FgAllocBuf &vtx_buf1, FgAllocBuf &vtx_buf2,
                  FgAllocBuf &ndx_buf, FgAllocTex &depth_tex, FgAllocTex &color_tex);
    void DrawTransparent(FgBuilder &builder, FgAllocTex &depth_tex);

  public:
    ExOITBlendLayer(PrimDraw &prim_draw) : prim_draw_(prim_draw) {}

    void Setup(const DrawList **p_list, const ViewState *view_state, const FgResRef vtx_buf1, const FgResRef vtx_buf2,
               const FgResRef ndx_buf, const FgResRef materials_buf, const FgResRef textures_buf,
               const BindlessTextureData *bindless_tex, const FgResRef cells_buf, const FgResRef items_buf,
               const FgResRef lights_buf, const FgResRef decals_buf, const FgResRef noise_tex,
               const FgResRef dummy_white, const FgResRef shadow_map, const FgResRef ltc_luts_tex,
               const FgResRef env_tex, const FgResRef instances_buf, const FgResRef instance_indices_buf,
               const FgResRef shared_data_buf, const FgResRef depth_tex, const FgResRef color_tex,
               const FgResRef oit_depth_buf, const FgResRef oit_specular_tex, int depth_layer_index,
               const FgResRef irradiance_tex, const FgResRef distance_tex, const FgResRef offset_tex,
               const FgResRef back_color_tex, const FgResRef back_depth_tex) {
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

    void Execute(FgBuilder &builder) override;
};
} // namespace Eng