#pragma once

#include "../Graph/GraphBuilder.h"
#include "../Renderer_DrawList.h"

#include <Ren/VertexInput.h>

class RpGBufferFill : public RenderPassExecutor {
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

    void Setup(const DrawList &list, const ViewState *view_state, const RpResource &vtx_buf1,
               const RpResource &vtx_buf2, const RpResource &ndx_buf, const RpResource &materials_buf,
               const RpResource &textures_buf, const BindlessTextureData *bindless_tex, const RpResource &noise_tex,
               const RpResource &dummy_black, const RpResource &instances_buf, const RpResource &instance_indices_buf,
               const RpResource &shared_data_buf, const RpResource &cells_buf, const RpResource &items_buf,
               const RpResource &decals_buf, const RpResource &out_albedo, const RpResource &out_normals,
               const RpResource &out_spec, const RpResource &out_depth) {
        view_state_ = view_state;
        bindless_tex_ = bindless_tex;

        materials_ = list.materials;
        decals_atlas_ = list.decals_atlas;

        render_flags_ = list.render_flags;
        main_batches_ = list.basic_batches;
        main_batch_indices_ = list.basic_batch_indices;

        vtx_buf1_ = vtx_buf1;
        vtx_buf2_ = vtx_buf2;
        ndx_buf_ = ndx_buf;
        instances_buf_ = instances_buf;
        instance_indices_buf_ = instance_indices_buf;
        shared_data_buf_ = shared_data_buf;
        cells_buf_ = cells_buf;
        items_buf_ = items_buf;
        decals_buf_ = decals_buf;
        materials_buf_ = materials_buf;

        noise_tex_ = noise_tex;
        dummy_black_ = dummy_black;

        textures_buf_ = textures_buf;

        out_albedo_tex_ = out_albedo;
        out_normal_tex_ = out_normals;
        out_spec_tex_ = out_spec;
        out_depth_tex_ = out_depth;
    }

    void Execute(RpBuilder &builder) override;

    // TODO: remove this
    int alpha_blend_start_index_ = -1;
};