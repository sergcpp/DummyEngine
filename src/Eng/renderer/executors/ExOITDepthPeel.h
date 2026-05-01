#pragma once

#include <Ren/Common.h>
#include <Ren/Framebuffer.h>

#include "../framegraph/FgNode.h"

namespace Eng {
struct BindlessTextureData;
struct DrawList;
class PrimDraw;
class ShaderLoader;
struct view_state_t;

class ExOITDepthPeel final : public FgExecutor {
    Ren::PipelineHandle pi_simple_[3];

    // lazily initialized data

    // temp data (valid only between Setup and Execute calls)
    const view_state_t *view_state_ = nullptr;
    const BindlessTextureData *bindless_tex_ = nullptr;

    const DrawList **p_list_ = nullptr;

    FgBufROHandle vtx_buf1_;
    FgBufROHandle vtx_buf2_;
    FgBufROHandle ndx_buf_;
    FgBufROHandle instances_;
    FgBufROHandle instance_indices_;
    FgBufROHandle shared_data_;
    FgBufROHandle materials_;
    FgImgROHandle dummy_white_;

    FgImgRWHandle depth_;
    FgBufRWHandle out_depth_;

    void LazyInit(const FgContext &fg, Ren::ImageRWHandle depth);
    void DrawTransparent(const FgContext &fg, Ren::ImageRWHandle depth);

  public:
    ExOITDepthPeel(const DrawList **p_list, const view_state_t *view_state, FgBufROHandle vtx_buf1,
                   FgBufROHandle vtx_buf2, FgBufROHandle ndx_buf, FgBufROHandle materials,
                   const BindlessTextureData *bindless_tex, FgImgROHandle dummy_white, FgBufROHandle instances,
                   FgBufROHandle instance_indices, FgBufROHandle shared_data, FgImgRWHandle depth,
                   FgBufRWHandle out_depth);

    void Execute(const FgContext &fg) override;
};
} // namespace Eng