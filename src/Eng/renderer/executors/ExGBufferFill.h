#pragma once

#include <Ren/Common.h>

#include "../framegraph/FgNode.h"

namespace Eng {
struct BindlessTextureData;
struct DrawList;
struct view_state_t;
class ShaderLoader;

class ExGBufferFill final : public FgExecutor {
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
        FgBufROHandle decals;
        FgImgROHandle noise;
        FgImgROHandle dummy_white;
        FgImgROHandle dummy_black;
        FgImgRWHandle out_albedo;
        FgImgRWHandle out_normal;
        FgImgRWHandle out_spec;
        FgImgRWHandle out_depth;
    };

    ExGBufferFill(const DrawList **p_list, const view_state_t *view_state, const BindlessTextureData *bindless_tex,
                  const Args *args)
        : p_list_(p_list), view_state_(view_state), bindless_tex_(bindless_tex), args_(args) {}

    void Execute(const FgContext &fg) override;

  private:
    bool initialized_ = false;

    // lazily initialized data
    Ren::PipelineHandle pi_simple_[3];
    Ren::PipelineHandle pi_vegetation_[2];

    // temp data (valid only between Setup and Execute calls)
    const DrawList **p_list_ = nullptr;
    const view_state_t *view_state_ = nullptr;
    const BindlessTextureData *bindless_tex_ = nullptr;
    const Args *args_ = nullptr;

    void LazyInit(const FgContext &fg, Ren::ImageRWHandle albedo, Ren::ImageRWHandle normal,
                  Ren::ImageRWHandle spec, Ren::ImageRWHandle depth);
    void DrawOpaque(const FgContext &fg, Ren::ImageRWHandle albedo, Ren::ImageRWHandle normal,
                    Ren::ImageRWHandle spec, Ren::ImageRWHandle depth);
};
} // namespace Eng
