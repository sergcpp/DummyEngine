#pragma once

#include "../Graph/GraphBuilder.h"
#include "../Renderer_DrawList.h"

#include <Ren/VertexInput.h>

class RpOpaque : public RenderPassExecutor {
    bool initialized = false;

    // lazily initialized data
    Ren::VertexInput draw_pass_vi_;
    Ren::RenderPass rp_opaque_;
    Ren::Framebuffer opaque_draw_fb_[Ren::MaxFramesInFlight];
#if defined(USE_VK_RENDER)
    VkDescriptorSetLayout descr_set_layout_ = VK_NULL_HANDLE;
#endif

    // temp data (valid only between Setup and Execute calls)
    Ren::ApiContext *api_ctx_ = nullptr;
    const ViewState *view_state_ = nullptr;
    const Ren::Pipeline *pipelines_ = nullptr;
    const BindlessTextureData *bindless_tex_ = nullptr;

    const Ren::TextureAtlas *decals_atlas_ = nullptr;
    const Ren::ProbeStorage *probe_storage_ = nullptr;

    uint64_t render_flags_ = 0;
    const Ren::MaterialStorage *materials_ = nullptr;
    DynArrayConstRef<CustomDrawBatch> main_batches_;
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
    RpResource shad_tex_;
    RpResource lm_tex_[4];
    RpResource ssao_tex_;
    RpResource brdf_lut_;
    RpResource noise_tex_;
    RpResource cone_rt_lut_;
    RpResource dummy_black_;
    RpResource dummy_white_;

    RpResource color_tex_;
    RpResource normal_tex_;
    RpResource spec_tex_;
    RpResource depth_tex_;

    void LazyInit(Ren::Context &ctx, ShaderLoader &sh, RpAllocBuf &vtx_buf1, RpAllocBuf &vtx_buf2, RpAllocBuf &ndx_buf,
                  RpAllocTex &color_tex, RpAllocTex &normal_tex, RpAllocTex &spec_tex, RpAllocTex &depth_tex);
    void DrawOpaque(RpBuilder &builder);

#if defined(USE_VK_RENDER)
    void InitDescrSetLayout();
#endif
  public:
    ~RpOpaque();

    void Setup(const DrawList &list, const ViewState *view_state, const RpResource &vtx_buf1,
               const RpResource &vtx_buf2, const RpResource &ndx_buf, const RpResource &materials_buf,
               const RpResource &textures_buf, const Ren::Pipeline pipelines[], const BindlessTextureData *bindless_tex,
               const RpResource &brdf_lut, const RpResource &noise_tex, const RpResource &cone_rt_lut,
               const RpResource &dummy_black, const RpResource &dummy_white, const RpResource &instances_buf,
               const RpResource &instance_indices_buf, const RpResource &shared_data_buf, const RpResource &cells_buf,
               const RpResource &items_buf, const RpResource &lights_buf, const RpResource &decals_buf,
               const RpResource &shadowmap_tex, const RpResource &ssao_tex, const RpResource lm_tex[],
               const RpResource &out_color, const RpResource &out_normals, const RpResource &out_spec,
               const RpResource &out_depth) {
        view_state_ = view_state;
        pipelines_ = pipelines;
        bindless_tex_ = bindless_tex;

        materials_ = list.materials;
        decals_atlas_ = list.decals_atlas;
        probe_storage_ = list.probe_storage;

        render_flags_ = list.render_flags;
        main_batches_ = list.custom_batches;
        main_batch_indices_ = list.custom_batch_indices;

        vtx_buf1_ = vtx_buf1;
        vtx_buf2_ = vtx_buf2;
        ndx_buf_ = ndx_buf;
        instances_buf_ = instances_buf;
        instance_indices_buf_ = instance_indices_buf;
        shared_data_buf_ = shared_data_buf;
        cells_buf_ = cells_buf;
        items_buf_ = items_buf;
        lights_buf_ = lights_buf;
        decals_buf_ = decals_buf;
        shad_tex_ = shadowmap_tex;

        materials_buf_ = materials_buf;
        ssao_tex_ = ssao_tex;

        for (int i = 0; i < 4; ++i) {
            lm_tex_[i] = lm_tex[i];
        }

        brdf_lut_ = brdf_lut;
        noise_tex_ = noise_tex;
        cone_rt_lut_ = cone_rt_lut;

        dummy_black_ = dummy_black;
        dummy_white_ = dummy_white;

        textures_buf_ = textures_buf;

        color_tex_ = out_color;
        normal_tex_ = out_normals;
        spec_tex_ = out_spec;
        depth_tex_ = out_depth;
    }
    void Execute(RpBuilder &builder) override;
};