#pragma once

#include "../Graph/GraphBuilder.h"

struct ViewState;

class RpTAACopyTex : public RenderPassBase {
    // lazily initialized data

    // temp data (valid only between Setup and Execute calls)
    const ViewState *view_state_ = nullptr;

    RpResource shared_data_buf_;

    RpResource input_tex_;
    RpResource output_tex_;

  public:
    RpTAACopyTex() {}

    void Setup(RpBuilder &builder, const ViewState *view_state, const char input_tex_name[],
               Ren::WeakTex2DRef output_tex);
    void Execute(RpBuilder &builder) override;

    const char *name() const override { return "TAA COPY TEX"; }
};