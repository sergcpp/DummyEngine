#pragma once

#include <Ren/Texture.h>
#include <Ren/VertexInput.h>

#include "../Renderer_DrawList.h"
#include "../framegraph/FgNode.h"

struct ViewState;

namespace Eng {
class ExSampleLights final : public FgExecutor {
  public:
    struct Args {
        FgResRef shared_data;
        FgResRef random_seq;
        FgResRef lights_buf;
        FgResRef nodes_buf;

        FgResRef geo_data;
        FgResRef materials;
        FgResRef vtx_buf1;
        FgResRef ndx_buf;
        FgResRef tlas_buf;

        FgResRef albedo_tex;
        FgResRef depth_tex;
        FgResRef norm_tex;
        FgResRef spec_tex;

        Ren::IAccStructure *tlas = nullptr;

        struct {
            uint32_t root_node = 0xffffffff;
            FgResRef rt_blas_buf;
            FgResRef prim_ndx_buf;
            FgResRef mesh_instances_buf;
            FgResRef textures_buf;
        } swrt;

        FgResRef out_diffuse_tex;
        FgResRef out_specular_tex;
    };

    void Setup(FgBuilder &builder, const ViewState *view_state, const BindlessTextureData *bindless_tex,
               const Args *args) {
        view_state_ = view_state;
        bindless_tex_ = bindless_tex;
        args_ = args;
    }

    void Execute(FgBuilder &builder) override;

  private:
    bool initialized_ = false;

    // lazily initialized data
    Ren::PipelineRef pi_sample_lights_;

    // temp data (valid only between Setup and Execute calls)
    const ViewState *view_state_ = nullptr;
    const BindlessTextureData *bindless_tex_ = nullptr;

    const Args *args_ = nullptr;

    void LazyInit(Ren::Context &ctx, Eng::ShaderLoader &sh);

    void Execute_HWRT(FgBuilder &builder);
    void Execute_SWRT(FgBuilder &builder);
};
} // namespace Eng