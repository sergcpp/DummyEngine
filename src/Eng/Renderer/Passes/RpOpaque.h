#pragma once

#include "../Graph/GraphBuilder.h"
#include "../Renderer_DrawList.h"

#if defined(USE_GL_RENDER)
#include <Ren/VaoGL.h>
#endif

class RpOpaque : public RenderPassBase {
    bool initialized = false;
    int orphan_index_ = 0;

    // lazily initialized data
    Ren::Tex2DRef dummy_black_, dummy_white_;

    // temp data (valid only between Setup and Execute calls)
    const ViewState *view_state_ = nullptr;
    Ren::Tex2DRef brdf_lut_, noise_tex_, cone_rt_lut_;
    const PersistentBuffers *bufs_ = nullptr;

    const EnvironmentWeak *env_ = nullptr;
    const Ren::TextureAtlas *decals_atlas_ = nullptr;
    const ProbeStorage *probe_storage_ = nullptr;

    uint32_t render_flags_ = 0;
    const Ren::MaterialStorage *materials_ = nullptr;
    DynArrayConstRef<MainDrawBatch> main_batches_;
    DynArrayConstRef<uint32_t> main_batch_indices_;

    RpResource instances_buf_;
    RpResource shared_data_buf_;
    RpResource cells_buf_;
    RpResource items_buf_;
    RpResource lights_buf_;
    RpResource decals_buf_;
    RpResource shadowmap_tex_;
    RpResource ssao_tex_;

    RpResource color_tex_;
    RpResource normal_tex_;
    RpResource spec_tex_;
    RpResource depth_tex_;

    void LazyInit(Ren::Context &ctx, ShaderLoader &sh, RpAllocTex &color_tex,
                  RpAllocTex &normal_tex, RpAllocTex &spec_tex, RpAllocTex &depth_tex);
    void DrawOpaque(RpBuilder &builder);

#if defined(USE_GL_RENDER)
    Ren::Vao draw_pass_vao_;

    Ren::Framebuffer opaque_draw_fb_;
#endif
  public:
    void Setup(RpBuilder &builder, const DrawList &list, const ViewState *view_state,
               const PersistentBuffers *bufs, int orphan_index, Ren::Tex2DRef brdf_lut,
               Ren::Tex2DRef noise_tex, Ren::Tex2DRef cone_rt_lut,
               const char instances_buf[], const char shared_data_buf[],
               const char cells_buf[], const char items_buf[], const char lights_buf[],
               const char decals_buf[], const char shadowmap_tex[], const char ssao_tex[],
               const char out_color[], const char out_normals[], const char out_spec[],
               const char out_depth[]);
    void Execute(RpBuilder &builder) override;

    // TODO: remove this
    int alpha_blend_start_index_ = -1;

    const char *name() const override { return "OPAQUE PASS"; }
};