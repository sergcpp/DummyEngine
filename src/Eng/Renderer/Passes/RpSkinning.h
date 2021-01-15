#pragma once

#include "../Graph/GraphBuilder.h"
#include "../Renderer_DrawList.h"

struct DrawList;

class RpSkinning : public RenderPassBase {
    int orphan_index_ = 0;

    // lazily initialized data
    bool initialized = false;
    Ren::ProgramRef skinning_prog_;

    // temp data (valid only between Setup and Execute calls)
    DynArrayConstRef<SkinRegion> skin_regions_;

    Ren::BufferRef vtx_buf1_, vtx_buf2_, delta_buf_, skin_vtx_buf_;

    void LazyInit(Ren::Context &ctx, ShaderLoader &sh);

  public:
    void Setup(RpBuilder &builder, const DrawList &list, int orphan_index,
               Ren::BufferRef vtx_buf1, Ren::BufferRef vtx_buf2, Ren::BufferRef delta_buf,
               Ren::BufferRef skin_vtx_buf);
    void Execute(RpBuilder &builder) override;

    const char *name() const override { return "SKINNING"; }
};