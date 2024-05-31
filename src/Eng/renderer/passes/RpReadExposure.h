#pragma once

#include "../graph/SubPass.h"
#include "../Renderer_DrawList.h"

namespace Eng {
struct RpReadExposureData {
    RpResRef input_tex;
    RpResRef output_buf;
};

class RpReadExposure : public RpExecutor {
    bool initialized_ = false;
    float exposure_ = 1.0f;

    // temp data (valid only between Setup and Execute calls)
    const RpReadExposureData *pass_data_ = nullptr;

  public:
    RpReadExposure() = default;
    ~RpReadExposure() = default;

    void Setup(const RpReadExposureData *pass_data) { pass_data_ = pass_data; }
    void Execute(RpBuilder &builder) override;

    float exposure() const { return exposure_; }
};
} // namespace Eng