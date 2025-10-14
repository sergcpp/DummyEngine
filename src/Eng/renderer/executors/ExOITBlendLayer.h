#pragma once

#include "../Renderer_DrawList.h"
#include "../framegraph/FgNode.h"

#include <Ren/VertexInput.h>

namespace Eng {
class PrimDraw;

class ExOITBlendLayer final : public FgExecutor {
    PrimDraw &prim_draw_;
    bool initialized = false;

    // lazily initialized data
    Ren::ProgramRef prog_oit_blit_depth_;
    Ren::PipelineRef pi_simple_[3];
    Ren::PipelineRef pi_vegetation_[2];
    Ren::Framebuffer main_draw_fb_[Ren::MaxFramesInFlight][2];
    int fb_to_use_ = 0;

    // temp data (valid only between Setup and Execute calls)
    const view_state_t *view_state_ = nullptr;
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
    void DrawTransparent(FgContext &ctx, FgAllocTex &depth_tex);

  public:
    ExOITBlendLayer(PrimDraw &prim_draw, const DrawList **p_list, const view_state_t *view_state, FgResRef vtx_buf1,
                    FgResRef vtx_buf2, FgResRef ndx_buf, FgResRef materials_buf, FgResRef textures_buf,
                    const BindlessTextureData *bindless_tex, FgResRef cells_buf, FgResRef items_buf,
                    FgResRef lights_buf, FgResRef decals_buf, FgResRef noise_tex, FgResRef dummy_white,
                    FgResRef shadow_map, FgResRef ltc_luts_tex, FgResRef env_tex, FgResRef instances_buf,
                    FgResRef instance_indices_buf, FgResRef shared_data_buf, FgResRef depth_tex, FgResRef color_tex,
                    FgResRef oit_depth_buf, FgResRef oit_specular_tex, int depth_layer_index, FgResRef irradiance_tex,
                    FgResRef distance_tex, FgResRef offset_tex, FgResRef back_color_tex, FgResRef back_depth_tex);

    void Execute(FgContext &ctx) override;
};
} // namespace Eng