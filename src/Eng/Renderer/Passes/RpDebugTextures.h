#pragma once

#include <Ren/Texture.h>
#include <Ren/VertexInput.h>

#include "../Graph/GraphBuilder.h"
#include "../Renderer_DrawList.h"

class PrimDraw;
struct ViewState;

class RpDebugTextures : public RenderPassBase {
    PrimDraw &prim_draw_;
    bool initialized = false;

    // lazily initialized data
    Ren::ProgramRef blit_prog_, blit_debug_prog_, blit_debug_ms_prog_, blit_debug_bvh_prog_, blit_debug_bvh_ms_prog_,
        blit_depth_prog_;
    Ren::Tex2DRef temp_tex_;
    Ren::BufferRef nodes_buf_;
    Ren::Tex1DRef nodes_tbo_;

    // temp data (valid only between Setup and Execute calls)
    uint64_t render_flags_ = 0;
    Ren::WeakTex2DRef output_tex_;
    const ViewState *view_state_ = nullptr;
    const Ren::Camera *draw_cam_ = nullptr;
    int depth_w_ = 0, depth_h_ = 0;
    const uint8_t *depth_pixels_ = nullptr;
    Ren::WeakTex2DRef down_tex_4x_;
    DynArrayConstRef<ShadowList> shadow_lists_;
    DynArrayConstRef<ShadowMapRegion> shadow_regions_;
    DynArrayConstRef<ShadReg> cached_shadow_regions_;

    const bvh_node_t *nodes_ = nullptr;
    uint32_t root_node_ = 0xffffffff, nodes_count_ = 0;

    RpResource shared_data_buf_;
    RpResource cells_buf_;
    RpResource items_buf_;

    RpResource shadowmap_tex_;
    RpResource color_tex_;
    RpResource normal_tex_;
    RpResource spec_tex_;
    RpResource depth_tex_;
    RpResource ssao_tex_;
    RpResource blur_tex_;
    RpResource reduced_tex_;

    void LazyInit(Ren::Context &ctx, ShaderLoader &sh);

#if defined(USE_GL_RENDER)
    Ren::VertexInput temp_vtx_input_;
    Ren::Framebuffer output_fb_;
#endif

    int BlitTex(Ren::RastState &applied_state, int x, int y, int w, int h, Ren::WeakTex2DRef tex, float mul);

    void DrawShadowMaps(Ren::Context &ctx, RpAllocTex &shadowmap_tex);

  public:
    RpDebugTextures(PrimDraw &prim_draw) : prim_draw_(prim_draw) {}

    void Setup(RpBuilder &builder, const ViewState *view_state, const DrawList &list, const Ren::Tex2DRef &down_tex_4x,
               const char shared_data_buf_name[], const char cells_buf_name[], const char items_buf_name[],
               const char shadow_map_name[], const char main_color_tex_name[], const char main_normal_tex_name[],
               const char main_spec_tex_name[], const char main_depth_tex_name[], const char ssao_tex_name[],
               const char blur_res_name[], const char reduced_tex_name[], Ren::WeakTex2DRef output_tex);
    void Execute(RpBuilder &builder) override;

    const char *name() const override { return "DEBUG TEXTURES"; }
};