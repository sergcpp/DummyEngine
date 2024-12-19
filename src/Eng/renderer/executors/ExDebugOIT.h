#pragma once

#include <Ren/Texture.h>
#include <Ren/VertexInput.h>

#include "../Renderer_DrawList.h"
#include "../framegraph/FgNode.h"

struct ViewState;

namespace Eng {
class ExDebugOIT final : public FgExecutor {
  public:
    struct Args {
        int layer_index = 0;

        FgResRef oit_depth_buf;
        FgResRef output_tex;
    };

    ExDebugOIT(FgBuilder &builder, const ViewState *view_state, const Args *pass_data);

    void Execute(FgBuilder &builder) override;

  private:
    // lazily initialized data
    Ren::PipelineRef pi_debug_oit_;

    // temp data (valid only between Setup and Execute calls)
    const ViewState *view_state_ = nullptr;
    const Args *args_ = nullptr;
};
} // namespace Eng