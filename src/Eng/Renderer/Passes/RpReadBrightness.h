#pragma once

#include "../Graph/SubPass.h"
#include "../Renderer_DrawList.h"

struct RpReadBrightnessData {
    RpResRef input_tex;
    RpResRef output_buf;
    RpResRef exposure_tex;
};

class RpReadBrightness : public RpExecutor {
    bool initialized_ = false;
    float reduced_average_ = 1.0f;

    // lazily initialized data
    Ren::Framebuffer reduced_fb_;

    // temp data (valid only between Setup and Execute calls)
    const RpReadBrightnessData *pass_data_ = nullptr;

  public:
    RpReadBrightness() = default;
    ~RpReadBrightness() = default;

    void Setup(const RpReadBrightnessData *pass_data) { pass_data_ = pass_data; }
    void Execute(RpBuilder &builder) override;

    float reduced_average() const { return reduced_average_; }
};