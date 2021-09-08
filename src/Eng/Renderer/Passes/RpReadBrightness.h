#pragma once

#include "../Graph/GraphBuilder.h"
#include "../Renderer_DrawList.h"

class RpReadBrightness : public RenderPassBase {
    bool initialized_ = false;
    float reduced_average_ = 1.0f;

    // lazily initialized data
    Ren::Framebuffer reduced_fb_;

    // temp data (valid only between Setup and Execute calls)
    RpResource input_tex_;

    RpResource output_buf_;

  public:
    RpReadBrightness() = default;
    ~RpReadBrightness() = default;

    void Setup(RpBuilder &builder, const char intput_tex_name[], Ren::WeakBufferRef output_buf);
    void Execute(RpBuilder &builder) override;

    const char *name() const override { return "BRIGHTNESS READBACK"; }
    float reduced_average() const { return reduced_average_; }
};