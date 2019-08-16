#pragma once

#include "../Graph/GraphBuilder.h"
#include "../Renderer_DrawList.h"

#include <Ren/VertexInput.h>

class RpShadowMaps : public RenderPassBase {
    bool initialized = false;
    int w_, h_;

    // lazily initialized data
    Ren::RenderPass rp_depth_only_;
    Ren::VertexInput vi_depth_pass_solid_, vi_depth_pass_vege_solid_, vi_depth_pass_transp_, vi_depth_pass_vege_transp_;

    Ren::Pipeline pi_solid_, pi_transp_;
    Ren::Pipeline pi_vege_solid_, pi_vege_transp_;

    Ren::Framebuffer shadow_fb_;

    // temp data (valid only between Setup and Execute calls)
    const Ren::MaterialStorage *materials_ = nullptr;
    const BindlessTextureData *bindless_tex_ = nullptr;
    DynArrayConstRef<DepthDrawBatch> shadow_batches_;
    DynArrayConstRef<uint32_t> shadow_batch_indices_;
    DynArrayConstRef<ShadowList> shadow_lists_;
    DynArrayConstRef<ShadowMapRegion> shadow_regions_;

    // inputs
    RpResource vtx_buf1_;
    RpResource vtx_buf2_;
    RpResource ndx_buf_;
    RpResource instances_buf_;
    RpResource shared_data_buf_;
    RpResource materials_buf_;
    RpResource textures_buf_;
    RpResource noise_tex_;

    // outputs
    RpResource shadowmap_tex_;

    void LazyInit(Ren::Context &ctx, ShaderLoader &sh, RpAllocBuf &vtx_buf1, RpAllocBuf &vtx_buf2, RpAllocBuf &ndx_buf,
                  RpAllocTex &shadowmap_tex);
    void DrawShadowMaps(RpBuilder &builder, RpAllocTex &shadowmap_tex);

  public:
    RpShadowMaps(int w, int h) : w_(w), h_(h) {}

    void Setup(RpBuilder &builder, const DrawList &list, Ren::BufferRef vtx_buf1, Ren::BufferRef vtx_buf2,
               Ren::BufferRef ndx_buf, Ren::BufferRef materials_buf, const BindlessTextureData *bindless_tex,
               const char instances_buf[], const char shared_data_buf[], const char shadowmap_tex[],
               Ren::Tex2DRef noise_tex);
    void Execute(RpBuilder &builder) override;

    const char *name() const override { return "SHADOW MAPS"; }
};