#pragma once

#include <Ren/Common.h>
#include <Ren/Framebuffer.h>

#include "../framegraph/FgNode.h"

namespace Eng {
struct DrawList;
class ShaderLoader;
struct BindlessTextureData;
struct view_state_t;

class ExDepthFill final : public FgExecutor {
  public:
    struct Args {
        bool clear_depth = false;

        FgBufROHandle vtx_buf1;
        FgBufROHandle vtx_buf2;
        FgBufROHandle ndx_buf;
        FgBufROHandle instances;
        FgBufROHandle instance_indices;
        FgBufROHandle shared_data;
        FgBufROHandle materials;
        FgImgROHandle noise;
        FgImgROHandle dummy_white;
        FgImgRWHandle depth;
        FgImgRWHandle velocity;
    };

    ExDepthFill(const DrawList **p_list, const view_state_t *view_state, const BindlessTextureData *bindless_tex,
                const Args *args)
        : p_list_(p_list), view_state_(view_state), bindless_tex_(bindless_tex), args_(args) {}

    void Execute(const FgContext &fg) override;

  private:
    bool initialized_ = false;

    // lazily initialized data
    Ren::RenderPassHandle rp_depth_only_[2], rp_depth_velocity_[2];

    Ren::PipelineHandle pi_static_solid_[3], pi_static_transp_[3];
    Ren::PipelineHandle pi_moving_solid_[3], pi_moving_transp_[3];
    Ren::PipelineHandle pi_vege_static_solid_[2], pi_vege_static_transp_[2];
    Ren::PipelineHandle pi_vege_moving_solid_[2], pi_vege_moving_transp_[2];
    Ren::PipelineHandle pi_skin_static_solid_[2], pi_skin_static_transp_[2];
    Ren::PipelineHandle pi_skin_moving_solid_[2], pi_skin_moving_transp_[2];

    // temp data (valid only between Setup and Execute calls)
    const DrawList **p_list_ = nullptr;
    const view_state_t *view_state_ = nullptr;
    const BindlessTextureData *bindless_tex_ = nullptr;
    const Args *args_ = nullptr;

    void LazyInit(const FgContext &fg, Ren::ImageRWHandle depth, Ren::ImageRWHandle velocity);
    void DrawDepth(const FgContext &fg, Ren::ImageRWHandle depth, Ren::ImageRWHandle velocity);
};
} // namespace Eng
