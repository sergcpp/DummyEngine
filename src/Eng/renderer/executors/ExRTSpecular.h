#pragma once

#include <Ren/Image.h>
#include <Ren/VertexInput.h>

#include "../Renderer_DrawList.h"
#include "../framegraph/FgNode.h"

struct view_state_t;

namespace Eng {
class PrimDraw;
class ShaderLoader;
class ExRTSpecular final : public FgExecutor {
  public:
    struct Args {
        FgImgROHandle noise;
        FgBufROHandle geo_data;
        FgBufROHandle materials;
        FgBufROHandle vtx_buf1;
        FgBufROHandle vtx_buf2;
        FgBufROHandle ndx_buf;
        FgBufROHandle shared_data;
        FgImgROHandle depth;
        FgImgROHandle normal;
        FgBufROHandle ray_list;
        FgBufROHandle indir_args;
        FgBufROHandle tlas_buf;

        FgBufROHandle oit_depth;

        Ren::AccStructROHandle tlas;

        struct {
            uint32_t root_node = 0xffffffff;
            FgBufROHandle rt_blas;
            FgBufROHandle prim_ndx;
            FgBufROHandle mesh_instances;
        } swrt;

        bool layered = false;
        bool second_bounce = false;

        FgBufRWHandle inout_ray_counter;
        FgBufRWHandle out_ray_hits;
    };

    explicit ExRTSpecular(const view_state_t *view_state, const BindlessTextureData *bindless_tex, const Args *args)
        : view_state_(view_state), bindless_tex_(bindless_tex), args_(args) {}

    void Execute(const FgContext &fg) override;

  private:
    bool initialized_ = false;

    // lazily initialized data
    Ren::PipelineHandle pi_rt_specular_;

    // temp data (valid only between Setup and Execute calls)
    const view_state_t *view_state_ = nullptr;
    const BindlessTextureData *bindless_tex_ = nullptr;

    const Args *args_ = nullptr;

    void LazyInit(Ren::Context &ctx, ShaderLoader &sh);

    void Execute_HWRT(const FgContext &fg);
    void Execute_SWRT(const FgContext &fg);
};
} // namespace Eng