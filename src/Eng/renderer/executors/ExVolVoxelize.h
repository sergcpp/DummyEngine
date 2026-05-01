#pragma once

#include "../framegraph/FgNode.h"

namespace Eng {
struct DrawList;
struct view_state_t;
class ShaderLoader;

class ExVolVoxelize final : public FgExecutor {
  public:
    struct Args {
        FgBufROHandle shared_data;
        FgImgROHandle stbn;
        FgBufROHandle geo_data;
        FgBufROHandle materials;
        FgBufROHandle tlas_buf;

        Ren::AccStructROHandle tlas;

        struct {
            uint32_t root_node = 0xffffffff;
            FgBufROHandle rt_blas_buf;
            FgBufROHandle prim_ndx;
            FgBufROHandle mesh_instances;
            FgBufROHandle vtx_buf1;
            FgBufROHandle ndx_buf;
        } swrt;

        FgImgRWHandle out_emission;
        FgImgRWHandle out_scatter;
    };

    ExVolVoxelize(const DrawList **p_list, const view_state_t *view_state, const Args *args)
        : p_list_(p_list), view_state_(view_state), args_(args) {}

    void Execute(const FgContext &fg) override;

  private:
    bool initialized_ = false;

    // lazily initialized data
    Ren::PipelineHandle pi_vol_voxelize_;

    // temp data (valid only between Setup and Execute calls)
    const DrawList **p_list_ = nullptr;
    const view_state_t *view_state_ = nullptr;
    const Args *args_ = nullptr;

    void LazyInit(const FgContext &fg);

    void Execute_HWRT(const FgContext &fg);
    void Execute_SWRT(const FgContext &fg);
};
} // namespace Eng