#pragma once

#include "../Graph/GraphBuilder.h"
#include "../Renderer_DrawList.h"

struct DrawList;

class RpSkinning : public RenderPassBase {
    // lazily initialized data
    bool initialized = false;
    Ren::Pipeline pi_skinning_;

    // temp data (valid only between Setup and Execute calls)
    DynArrayConstRef<SkinRegion> skin_regions_;

    RpResource skin_vtx_buf_;
    RpResource skin_transforms_buf_;
    RpResource shape_keys_buf_;
    RpResource delta_buf_;

    RpResource vtx_buf1_;
    RpResource vtx_buf2_;

    void LazyInit(Ren::Context &ctx, ShaderLoader &sh);
  public:
    void Setup(RpBuilder &builder, const DrawList &list, Ren::BufferRef vtx_buf1,
               Ren::BufferRef vtx_buf2, Ren::BufferRef delta_buf, Ren::BufferRef skin_vtx_buf,
               const char skin_transforms_buf[], const char shape_keys_buf[]);
    void Execute(RpBuilder &builder) override;

    const char *name() const override { return "SKINNING"; }
};