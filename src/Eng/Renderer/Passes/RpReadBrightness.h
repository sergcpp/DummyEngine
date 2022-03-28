#pragma once

#include "../Graph/GraphBuilder.h"
#include "../Renderer_DrawList.h"

struct RpReadBrightnessData {
    RpResource input_tex;
    RpResource output_buf;
};

class RpReadBrightness : public RenderPassExecutor {
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