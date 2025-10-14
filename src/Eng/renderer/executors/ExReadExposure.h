#pragma once

#include "../Renderer_DrawList.h"
#include "../framegraph/FgNode.h"

namespace Eng {
class ExReadExposure final : public FgExecutor {
  public:
    struct Args {
        FgResRef input_tex;
        FgResRef output_buf;
    };

    void Setup(const Args *args) { args_ = args; }
    void Execute(FgContext &ctx) override;

    [[nodiscard]] float exposure() const { return exposure_; }

  private:
    float exposure_ = 1.0f;

    // temp data (valid only between Setup and Execute calls)
    const Args *args_ = nullptr;
};
} // namespace Eng