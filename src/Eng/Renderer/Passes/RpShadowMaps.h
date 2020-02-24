#pragma once

#include "../Graph/GraphBuilder.h"
#include "../Renderer_DrawList.h"

#if defined(USE_GL_RENDER)
#include <Ren/VaoGL.h>
#endif

class RpShadowMaps : public Graph::RenderPassBase {
    bool initialized = false;
    Ren::ProgramRef shadow_solid_prog_, shadow_vege_solid_prog_, shadow_transp_prog_,
        shadow_vege_transp_prog_;

    int orphan_index_ = 0;

    // lazily initialized data

    // temp data (valid only between Setup and Execute calls)
    Ren::TexHandle shadow_tex_;
    Ren::Tex1DRef instances_tbo_;

    DynArrayConstRef<DepthDrawBatch> shadow_batches_;
    DynArrayConstRef<uint32_t> shadow_batch_indices_;
    DynArrayConstRef<ShadowList> shadow_lists_;
    DynArrayConstRef<ShadowMapRegion> shadow_regions_;

    Ren::Framebuffer shadow_fb_;

    void LazyInit(Ren::Context &ctx, ShaderLoader &sh);
    void DrawShadowMaps(Ren::Context &ctx);

#if defined(USE_GL_RENDER)
    Ren::Vao depth_pass_solid_vao_, depth_pass_vege_solid_vao_, depth_pass_transp_vao_,
        depth_pass_vege_transp_vao_;
#endif
  public:
    void Setup(Graph::RpBuilder &builder, const DrawList &list, int orphan_index,
               Ren::TexHandle shadow_tex, Graph::ResourceHandle in_instances_buf,
               Ren::Tex1DRef instances_tbo);
    void Execute(Graph::RpBuilder &builder) override;

    const char *name() const override { return "SHADOW MAPS"; }
};