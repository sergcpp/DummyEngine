#pragma once

#include "../Graph/SubPass.h"
#include "../Renderer_DrawList.h"

namespace Eng {
class PrimDraw;

struct RpSampleBrightnessData {
    RpResRef input_tex;
    RpResRef reduced_tex;
};

class RpSampleBrightness : public RpExecutor {
    PrimDraw &prim_draw_;
    Ren::Vec2i res_;
    bool initialized_ = false;
    int cur_offset_ = 0;

    // lazily initialized data
    Ren::ProgramRef blit_red_prog_;

    // temp data (valid only between Setup and Execute calls)
    const RpSampleBrightnessData *pass_data_ = nullptr;

    void LazyInit(Ren::Context &ctx, Eng::ShaderLoader &sh, RpAllocTex &reduced_tex);

    Ren::BufferRef readback_buf_;

  public:
    RpSampleBrightness(PrimDraw &prim_draw, Ren::Vec2i res) : prim_draw_(prim_draw), res_(res) {}
    ~RpSampleBrightness() = default;

    void Setup(RpSampleBrightnessData *pass_data) { pass_data_ = pass_data; }
    void Execute(RpBuilder &builder) override;

    Ren::Vec2i res() const { return res_; }
};
} // namespace Eng