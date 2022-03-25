#pragma once

#include "../Graph/GraphBuilder.h"
#include "../Renderer_DrawList.h"

#include <Ren/VertexInput.h>

class RpGBufferFill : public RenderPassBase {
    bool initialized = false;

    // lazily initialized data
    Ren::RenderPass rp_main_draw_;

    Ren::VertexInput vi_simple_, vi_vegetation_;

    Ren::Pipeline pi_simple_[2];
    Ren::Pipeline pi_vegetation_[2];

    Ren::Framebuffer main_draw_fb_[Ren::MaxFramesInFlight];
#if defined(USE_VK_RENDER)
    VkDescriptorSetLayout descr_set_layout_ = VK_NULL_HANDLE;
#endif

    // temp data (valid only between Setup and Execute calls)
    Ren::ApiContext *api_ctx_ = nullptr;
    const ViewState *view_state_ = nullptr;
    const BindlessTextureData *bindless_tex_ = nullptr;

    const Ren::TextureAtlas *decals_atlas_ = nullptr;

    uint64_t render_flags_ = 0;
    const Ren::MaterialStorage *materials_ = nullptr;
    DynArrayConstRef<BasicDrawBatch> main_batches_;
    DynArrayConstRef<uint32_t> main_batch_indices_;

    RpResource vtx_buf1_;
    RpResource vtx_buf2_;
    RpResource ndx_buf_;
    RpResource instances_buf_;
    RpResource instance_indices_buf_;
    RpResource shared_data_buf_;
    RpResource materials_buf_;
    RpResource textures_buf_;
    RpResource cells_buf_;
    RpResource items_buf_;
    RpResource lights_buf_;
    RpResource decals_buf_;
    RpResource noise_tex_;
    RpResource dummy_black_;

    RpResource out_albedo_tex_;
    RpResource out_normal_tex_;
    RpResource out_spec_tex_;
    RpResource out_depth_tex_;

    void LazyInit(Ren::Context &ctx, ShaderLoader &sh, RpAllocBuf &vtx_buf1, RpAllocBuf &vtx_buf2, RpAllocBuf &ndx_buf,
                  RpAllocTex &albedo_tex, RpAllocTex &normal_tex, RpAllocTex &spec_tex, RpAllocTex &depth_tex);
    void DrawOpaque(RpBuilder &builder);

#if defined(USE_VK_RENDER)
    void InitDescrSetLayout();
#endif
  public:
    ~RpGBufferFill();

    void Setup(RpBuilder &builder, const DrawList &list, const ViewState *view_state, const Ren::BufferRef &vtx_buf1,
               const Ren::BufferRef &vtx_buf2, const Ren::BufferRef &ndx_buf, const Ren::BufferRef &materials_buf,
               const BindlessTextureData *bindless_tex, const Ren::Tex2DRef &noise_tex,
               const Ren::Tex2DRef &dummy_black, const char instances_buf[], const char instance_indices_buf[],
               const char shared_data_buf[], const char cells_buf[], const char items_buf[], const char decals_buf[],
               const char out_albedo[], const char out_normals[], const char out_spec[], const char out_depth[]);
    void Execute(RpBuilder &builder) override;

    // TODO: remove this
    int alpha_blend_start_index_ = -1;

    const char *name() const override { return "GBUFFER FILL"; }
};