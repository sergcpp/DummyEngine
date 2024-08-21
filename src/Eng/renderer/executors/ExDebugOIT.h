#pragma once

#include <Ren/Texture.h>
#include <Ren/VertexInput.h>

#include "../Renderer_DrawList.h"
#include "../framegraph/FgNode.h"

struct ViewState;

namespace Eng {
class ExDebugOIT : public FgExecutor {
  public:
    struct Args {
        int layer_index = 0;

        FgResRef oit_depth_buf;
        FgResRef output_tex;
    };

    void Setup(FgBuilder &builder, const ViewState *view_state, const Args *pass_data) {
        view_state_ = view_state;
        args_ = pass_data;
    }
    void Execute(FgBuilder &builder) override;

  private:
    bool initialized = false;

    // lazily initialized data
    Ren::Pipeline pi_debug_oit_;

    // temp data (valid only between Setup and Execute calls)
    const ViewState *view_state_ = nullptr;
    const Args *args_ = nullptr;

    void LazyInit(Ren::Context &ctx, Eng::ShaderLoader &sh);
};
} // namespace Eng