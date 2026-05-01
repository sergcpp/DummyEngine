#pragma once

#include <Ren/Common.h>

#include "../framegraph/FgNode.h"

namespace Eng {
struct BindlessTextureData;
struct DrawList;
class PrimDraw;
class ShaderLoader;
struct view_state_t;

class ExOITBlendLayer final : public FgExecutor {
  public:
    struct Args {
        FgBufROHandle vtx_buf1;
        FgBufROHandle vtx_buf2;
        FgBufROHandle ndx_buf;
        FgBufROHandle instances;
        FgBufROHandle instance_indices;
        FgBufROHandle shared_data;
        FgBufROHandle materials;
        FgBufROHandle cells;
        FgBufROHandle items;
        FgBufROHandle lights;
        FgBufROHandle decals;
        FgImgROHandle noise;
        FgImgROHandle dummy_white;
        FgImgROHandle shadow_depth;
        FgImgROHandle ltc_luts;
        FgImgROHandle env;
        FgBufROHandle oit_depth;
        int depth_layer_index = -1;
        FgImgROHandle oit_specular;
        FgImgROHandle irradiance;
        FgImgROHandle distance;
        FgImgROHandle offset;
        FgImgROHandle back_color;
        FgImgROHandle back_depth;
        FgImgRWHandle depth;
        FgImgRWHandle color;
    };

    ExOITBlendLayer(PrimDraw &prim_draw, const DrawList **p_list, const view_state_t *view_state,
                    const BindlessTextureData *bindless_tex, const Args *args)
        : prim_draw_(prim_draw), p_list_(p_list), view_state_(view_state), bindless_tex_(bindless_tex), args_(args) {}

    void Execute(const FgContext &fg) override;

  private:
    PrimDraw &prim_draw_;
    bool initialized_ = false;

    // lazily initialized data
    Ren::ProgramHandle prog_oit_blit_depth_;
    Ren::PipelineHandle pi_simple_[3];
    Ren::PipelineHandle pi_vegetation_[2];

    // temp data (valid only between Setup and Execute calls)
    const DrawList **p_list_ = nullptr;
    const view_state_t *view_state_ = nullptr;
    const BindlessTextureData *bindless_tex_ = nullptr;
    const Args *args_ = nullptr;

    void LazyInit(const FgContext &fg, Ren::ImageRWHandle depth, Ren::ImageRWHandle color);
    void DrawTransparent(const FgContext &fg, Ren::ImageRWHandle depth, Ren::ImageRWHandle color);
};
} // namespace Eng
