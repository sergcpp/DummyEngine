#pragma once

#include "../Renderer_Structs.h"
#include "../framegraph/FgNode.h"

namespace Eng {
class PrimDraw;
struct view_state_t;
class ShaderLoader;

class ExSkydomeCube final : public FgExecutor {
  public:
    struct Args {
        FgBufROHandle shared_data;
        FgImgROHandle transmittance_lut;
        FgImgROHandle multiscatter_lut;
        FgImgROHandle moon;
        FgImgROHandle weather;
        FgImgROHandle cirrus;
        FgImgROHandle curl;
        FgImgROHandle noise3d;

        FgImgRWHandle color;
    };

    ExSkydomeCube(PrimDraw &prim_draw, const view_state_t *view_state, const Args *args)
        : prim_draw_(prim_draw), view_state_(view_state), args_(args) {}

    void Execute(const FgContext &fg) override;

  private:
    PrimDraw &prim_draw_;
    bool initialized_ = false;
    uint32_t generation_ = 0xffffffff, generation_in_progress_ = 0xffffffff;
    int last_updated_faceq_ = 23;

    // temp data (valid only between Setup and Execute calls)
    const view_state_t *view_state_ = nullptr;
    const Args *args_ = nullptr;

    // lazily initialized data
    Ren::ProgramHandle prog_skydome_phys_;
    Ren::PipelineHandle pi_skydome_downsample_;

    void LazyInit(const FgContext &fg);
};

class ExSkydomeScreen final : public FgExecutor {
  public:
    struct Args {
        eSkyQuality sky_quality = eSkyQuality::Medium;

        FgBufROHandle shared_data;
        FgImgROHandle env;
        struct {
            FgImgROHandle transmittance_lut;
            FgImgROHandle multiscatter_lut;
            FgImgROHandle moon;
            FgImgROHandle weather;
            FgImgROHandle cirrus;
            FgImgROHandle curl;
            FgImgROHandle noise3d;
        } phys;

        FgImgROHandle depth_ro;
        FgImgRWHandle depth_rw;
        FgImgRWHandle color;
    };

    ExSkydomeScreen(PrimDraw &prim_draw, const view_state_t *view_state, const Args *args)
        : prim_draw_(prim_draw), view_state_(view_state), args_(args) {}

    void Execute(const FgContext &fg) override;

    static Ren::Vec2u sample_pos(int frame_index);

  private:
    PrimDraw &prim_draw_;
    bool initialized_ = false;

    // temp data (valid only between Setup and Execute calls)
    const view_state_t *view_state_ = nullptr;
    const Args *args_ = nullptr;

    // lazily initialized data
    Ren::ProgramHandle prog_skydome_simple_, prog_skydome_phys_[2];

    void LazyInit(const FgContext &fg);
};
} // namespace Eng