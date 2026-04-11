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
    bool initialized = false;

    // lazily initialized data

    // temp data (valid only between Setup and Execute calls)
    const view_state_t *view_state_ = nullptr;
    const BindlessTextureData *bindless_tex_ = nullptr;
    bool clear_depth_ = false;

    const DrawList **p_list_;

    FgBufROHandle vtx_buf1_;
    FgBufROHandle vtx_buf2_;
    FgBufROHandle ndx_buf_;
    FgBufROHandle instances_;
    FgBufROHandle instance_indices_;
    FgBufROHandle shared_data_;
    FgBufROHandle materials_;
    FgImgROHandle noise_;
    FgImgROHandle dummy_white_;

    FgImgRWHandle depth_;
    FgImgRWHandle velocity_;

    void LazyInit(Ren::Context &ctx, ShaderLoader &sh, Ren::ImageRWHandle depth, Ren::ImageRWHandle velocity);
    void DrawDepth(const FgContext &fg, Ren::ImageRWHandle depth, Ren::ImageRWHandle velocity);

    Ren::RenderPassHandle rp_depth_only_[2], rp_depth_velocity_[2];

    Ren::PipelineHandle pi_static_solid_[3], pi_static_transp_[3];
    Ren::PipelineHandle pi_moving_solid_[3], pi_moving_transp_[3];
    Ren::PipelineHandle pi_vege_static_solid_[2], pi_vege_static_transp_[2];
    Ren::PipelineHandle pi_vege_moving_solid_[2], pi_vege_moving_transp_[2];
    Ren::PipelineHandle pi_skin_static_solid_[2], pi_skin_static_transp_[2];
    Ren::PipelineHandle pi_skin_moving_solid_[2], pi_skin_moving_transp_[2];

  public:
    ExDepthFill(const DrawList **list, const view_state_t *view_state, bool clear_depth, const FgBufROHandle vtx_buf1,
                const FgBufROHandle vtx_buf2, const FgBufROHandle ndx_buf, const FgBufROHandle materials,
                const BindlessTextureData *bindless_tex, const FgBufROHandle instances,
                const FgBufROHandle instance_indices, const FgBufROHandle shared_data, const FgImgROHandle noise,
                const FgImgROHandle dummy_white, const FgImgRWHandle depth, const FgImgRWHandle velocity) {
        view_state_ = view_state;
        bindless_tex_ = bindless_tex;
        clear_depth_ = clear_depth;

        p_list_ = list;

        vtx_buf1_ = vtx_buf1;
        vtx_buf2_ = vtx_buf2;
        ndx_buf_ = ndx_buf;
        instances_ = instances;
        instance_indices_ = instance_indices;
        shared_data_ = shared_data;
        materials_ = materials;

        noise_ = noise;
        dummy_white_ = dummy_white;

        depth_ = depth;
        velocity_ = velocity;
    }

    void Execute(const FgContext &fg) override;
};
} // namespace Eng