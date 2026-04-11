#pragma once

#include <Ren/Framebuffer.h>

#include "../framegraph/FgNode.h"

namespace Eng {
struct BindlessTextureData;
struct DrawList;
class ShaderLoader;

class ExShadowDepth final : public FgExecutor {
    bool initialized = false;
    int w_, h_;

    // lazily initialized data
    Ren::PipelineHandle pi_solid_[3], pi_alpha_[3];
    Ren::PipelineHandle pi_vege_solid_, pi_vege_alpha_;

    // temp data (valid only between Setup and Execute calls)
    const DrawList **p_list_ = nullptr;
    const BindlessTextureData *bindless_tex_ = nullptr;

    // inputs
    FgBufROHandle vtx_buf1_;
    FgBufROHandle vtx_buf2_;
    FgBufROHandle ndx_buf_;
    FgBufROHandle instances_;
    FgBufROHandle instance_indices_;
    FgBufROHandle shared_data_;
    FgBufROHandle materials_;
    FgImgROHandle noise_;
    FgImgROHandle dummy_white_;

    // outputs
    FgImgRWHandle shadow_depth_;

    void LazyInit(Ren::Context &ctx, ShaderLoader &sh, Ren::ImageRWHandle shadow_depth);
    void DrawShadowMaps(const FgContext &fg, Ren::ImageRWHandle shadow_depth);

  public:
    ExShadowDepth(const int w, const int h, const DrawList **p_list, const FgBufROHandle vtx_buf1,
                  const FgBufROHandle vtx_buf2, const FgBufROHandle ndx_buf, const FgBufROHandle materials,
                  const BindlessTextureData *bindless_tex, const FgBufROHandle instances,
                  const FgBufROHandle instance_indices, const FgBufROHandle shared_data, const FgImgROHandle noise,
                  const FgImgROHandle dummy_white, const FgImgRWHandle shadow_depth)
        : w_(w), h_(h) {
        p_list_ = p_list;
        bindless_tex_ = bindless_tex;

        vtx_buf1_ = vtx_buf1;
        vtx_buf2_ = vtx_buf2;
        ndx_buf_ = ndx_buf;

        instances_ = instances;
        instance_indices_ = instance_indices;
        shared_data_ = shared_data;
        materials_ = materials;

        noise_ = noise;
        dummy_white_ = dummy_white;

        shadow_depth_ = shadow_depth;
    }

    void Execute(const FgContext &fg) override;
};
} // namespace Eng