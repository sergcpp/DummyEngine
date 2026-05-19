#pragma once

#include "../framegraph/FgNode.h"

namespace Eng {
struct BindlessTextureData;
class ShaderLoader;
struct view_state_t;
struct probe_volume_t;

class ExSampleLightsProbe final : public FgExecutor {
  public:
    struct Args {
        FgBufROHandle shared_data;
        FgBufROHandle random_seq;
        FgBufROHandle lights;
        FgBufROHandle nodes;

        FgBufROHandle geo_data;
        FgBufROHandle materials;
        FgBufROHandle vtx_buf1;
        FgBufROHandle ndx_buf;
        FgBufROHandle tlas_buf;

        FgImgROHandle offset;

        Ren::AccStructROHandle tlas;

        struct {
            uint32_t root_node = 0xffffffff;
            FgBufROHandle rt_blas_buf;
            FgBufROHandle prim_ndx;
            FgBufROHandle mesh_instances;
        } swrt;

        FgBufRWHandle out_sh1_data;

        bool partial_update = false;
        Ren::Span<const probe_volume_t> probe_volumes;
    };

    ExSampleLightsProbe(const view_state_t *view_state, const BindlessTextureData *bindless_tex, const Args *args)
        : view_state_(view_state), bindless_tex_(bindless_tex), args_(args) {}

    void Execute(const FgContext &fg) override;

  private:
    bool initialized_ = false;

    // lazily initialized data
    Ren::PipelineHandle pi_sample_lights_[2];

    // temp data (valid only between Setup and Execute calls)
    const view_state_t *view_state_ = nullptr;
    const BindlessTextureData *bindless_tex_ = nullptr;

    const Args *args_ = nullptr;

    void LazyInit(const FgContext &fg);

    void Execute_HWRT(const FgContext &fg);
    void Execute_SWRT(const FgContext &fg);
};
} // namespace Eng