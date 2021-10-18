#pragma once

#include "../Graph/GraphBuilder.h"

struct ViewState;

class RpCopyTex : public RenderPassBase {
    std::string name_;

    // lazily initialized data

    // temp data (valid only between Setup and Execute calls)
    Ren::Vec2i copy_res_;

    RpResource input_tex_;
    RpResource output_tex_;

  public:
    RpCopyTex(const char name[]) : name_(name) {}

    void Setup(RpBuilder &builder, Ren::Vec2i copy_res, const char input_tex_name[],
               Ren::WeakTex2DRef output_tex);
    void Execute(RpBuilder &builder) override;

    const char *name() const override { return name_.c_str(); }
};
