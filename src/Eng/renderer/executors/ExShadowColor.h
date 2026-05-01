#pragma once

#include <Ren/Framebuffer.h>

#include "../framegraph/FgNode.h"

namespace Eng {
struct BindlessTextureData;
struct DrawList;
class ShaderLoader;

class ExShadowColor final : public FgExecutor {
  public:
    struct Args {
        FgBufROHandle vtx_buf1;
        FgBufROHandle vtx_buf2;
        FgBufROHandle ndx_buf;
        FgBufROHandle instances;
        FgBufROHandle instance_indices;
        FgBufROHandle shared_data;
        FgBufROHandle materials;
        FgImgROHandle noise;
        FgImgROHandle dummy_white;
        FgImgRWHandle shadow_depth;
        FgImgRWHandle shadow_color;
    };

    ExShadowColor(int w, int h, const DrawList **p_list, const BindlessTextureData *bindless_tex, const Args *args)
        : w_(w), h_(h), p_list_(p_list), bindless_tex_(bindless_tex), args_(args) {}

    void Execute(const FgContext &fg) override;

  private:
    bool initialized_ = false;
    int w_, h_;

    // lazily initialized data
    Ren::PipelineHandle pi_solid_[3], pi_alpha_[3];
    Ren::PipelineHandle pi_vege_solid_, pi_vege_alpha_;

    // temp data (valid only between Setup and Execute calls)
    const DrawList **p_list_ = nullptr;
    const BindlessTextureData *bindless_tex_ = nullptr;
    const Args *args_ = nullptr;

    void LazyInit(const FgContext &fg, Ren::ImageRWHandle shadow_depth, Ren::ImageRWHandle shadow_color);
    void DrawShadowMaps(const FgContext &fg, Ren::ImageRWHandle shadow_depth, Ren::ImageRWHandle shadow_color);
};
} // namespace Eng
