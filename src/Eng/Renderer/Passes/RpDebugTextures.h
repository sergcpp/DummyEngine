#pragma once

#include <Ren/Texture.h>

#include "../Graph/GraphBuilder.h"

class PrimDraw;
struct ViewState;

class RpDebugTextures : public RenderPassBase {
    PrimDraw &prim_draw_;
    bool initialized = false;

    // lazily initialized data
    Ren::ProgramRef blit_prog_, blit_debug_prog_, blit_debug_ms_prog_,
        blit_debug_bvh_prog_, blit_debug_bvh_ms_prog_, blit_depth_prog_;
    Ren::Tex2DRef temp_tex_;
    Ren::BufferRef nodes_buf_;
    Ren::Tex1DRef nodes_tbo_;

    // temp data (valid only between Setup and Execute calls)
    int orphan_index_ = -1;
    uint32_t render_flags_ = 0;
    Ren::TexHandle depth_tex_;
    Ren::TexHandle output_tex_;
    const ViewState *view_state_ = nullptr;
    const Ren::Camera *draw_cam_ = nullptr;
    const uint8_t *depth_pixels_ = nullptr;
    const uint8_t *depth_tiles_ = nullptr;
    Ren::WeakTex2DRef color_tex_, spec_tex_, norm_tex_, down_tex_4x_;
    Ren::WeakTex2DRef reduced_tex_, blur_tex_, ssao_tex_;
    Ren::WeakTex2DRef shadow_tex_;
    DynArrayConstRef<ShadowList> shadow_lists_;
    DynArrayConstRef<ShadowMapRegion> shadow_regions_;
    DynArrayConstRef<ShadReg> cached_shadow_regions_;

    const bvh_node_t *nodes_;
    uint32_t root_node_ = 0xffffffff, nodes_count_ = 0;

    void LazyInit(Ren::Context &ctx, ShaderLoader &sh);

#if defined(USE_GL_RENDER)
    Ren::Vao temp_vao_;
    Ren::Framebuffer output_fb_;
#endif

    int BlitTex(Ren::RastState &applied_state, int x, int y, int w, int h,
                Ren::WeakTex2DRef tex, float mul);

    void DrawShadowMaps(Ren::Context &ctx);

  public:
    RpDebugTextures(PrimDraw &prim_draw) : prim_draw_(prim_draw) {}

    void Setup(RpBuilder &builder, const ViewState *view_state,
               const DrawList &list, int orphan_index, Ren::TexHandle depth_tex,
               const Ren::Tex2DRef &color_tex, const Ren::Tex2DRef &spec_tex,
               const Ren::Tex2DRef &norm_tex, const Ren::Tex2DRef &down_tex_4x,
               const Ren::Tex2DRef &reduced_tex, const Ren::Tex2DRef &blur_tex,
               const Ren::Tex2DRef &ssao_tex, const Ren::Tex2DRef &shadow_tex,
               Ren::TexHandle output_tex);
    void Execute(RpBuilder &builder) override;

    const char *name() const override { return "DEBUG TEXTURES"; }
};