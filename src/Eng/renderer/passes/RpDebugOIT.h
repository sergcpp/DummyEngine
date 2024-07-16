#pragma once

#include <Ren/Texture.h>
#include <Ren/VertexInput.h>

#include "../Renderer_DrawList.h"
#include "../graph/SubPass.h"

struct ViewState;

namespace Eng {
class RpDebugOIT : public RpExecutor {
  public:
    struct PassData {
        int layer_index = 0;

        RpResRef oit_depth_buf;
        RpResRef output_tex;
    };

    void Setup(RpBuilder &builder, const ViewState *view_state, const PassData *pass_data) {
        view_state_ = view_state;
        pass_data_ = pass_data;
    }
    void Execute(RpBuilder &builder) override;

  private:
    bool initialized = false;

    // lazily initialized data
    Ren::Pipeline pi_debug_oit_;

    // temp data (valid only between Setup and Execute calls)
    const ViewState *view_state_ = nullptr;
    const PassData *pass_data_ = nullptr;

    void LazyInit(Ren::Context &ctx, Eng::ShaderLoader &sh);
};
} // namespace Eng