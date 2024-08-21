#pragma once

#include <Ren/Texture.h>
#include <Ren/VertexInput.h>

#include "../Renderer_DrawList.h"
#include "../framegraph/FgNode.h"

struct ViewState;

namespace Eng {
class PrimDraw;
class ExRTShadows : public FgExecutor {
  public:
    struct Args {
        FgResRef geo_data;
        FgResRef materials;
        FgResRef vtx_buf1;
        FgResRef ndx_buf;
        FgResRef shared_data;
        FgResRef noise_tex;
        FgResRef depth_tex;
        FgResRef normal_tex;
        FgResRef tlas_buf;
        FgResRef tile_list_buf;
        FgResRef indir_args;

        Ren::IAccStructure *tlas = nullptr;

        struct {
            uint32_t root_node;
            FgResRef blas_buf;
            FgResRef prim_ndx_buf;
            FgResRef meshes_buf;
            FgResRef mesh_instances_buf;
            FgResRef textures_buf;
        } swrt;

        FgResRef out_shadow_tex;
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
    Ren::Pipeline pi_rt_shadows_;

    // temp data (valid only between Setup and Execute calls)
    const ViewState *view_state_ = nullptr;
    const BindlessTextureData *bindless_tex_ = nullptr;

    const Args *args_;

    void LazyInit(Ren::Context &ctx, Eng::ShaderLoader &sh);

    void Execute_HWRT_Pipeline(FgBuilder &builder);
    void Execute_HWRT_Inline(FgBuilder &builder);
    void Execute_SWRT(FgBuilder &builder);
};
} // namespace Eng