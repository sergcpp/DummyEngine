#pragma once

#include "../framegraph/FgNode.h"

namespace Eng {
struct BindlessTextureData;
class PrimDraw;
class ShaderLoader;
struct view_state_t;

class ExRTShadows final : public FgExecutor {
  public:
    struct Args {
        FgBufROHandle geo_data;
        FgBufROHandle materials;
        FgBufROHandle vtx_buf1;
        FgBufROHandle ndx_buf;
        FgBufROHandle shared_data;
        FgImgROHandle noise;
        FgImgROHandle depth;
        FgImgROHandle normal;
        FgBufROHandle tlas_buf;
        FgBufROHandle tile_list;
        FgBufROHandle indir_args;

        Ren::AccStructROHandle tlas;

        struct {
            uint32_t root_node = 0xffffffff;
            FgBufROHandle blas_buf;
            FgBufROHandle prim_ndx;
            FgBufROHandle mesh_instances;
        } swrt;

        FgImgRWHandle out_shadow;
    };

    ExRTShadows(const view_state_t *view_state, const BindlessTextureData *bindless_tex, const Args *args)
        : view_state_(view_state), bindless_tex_(bindless_tex), args_(args) {}

    void Execute(const FgContext &fg) override;

  private:
    bool initialized_ = false;

    // lazily initialized data
    Ren::PipelineHandle pi_rt_shadows_;

    // temp data (valid only between Setup and Execute calls)
    const view_state_t *view_state_ = nullptr;
    const BindlessTextureData *bindless_tex_ = nullptr;

    const Args *args_ = nullptr;

    void LazyInit(const FgContext &fg);

    void Execute_HWRT(const FgContext &fg);
    void Execute_SWRT(const FgContext &fg);
};
} // namespace Eng