#pragma once

#include <Ren/DrawCall.h>
#include <Ren/Framebuffer.h>
#include <Ren/Fwd.h>
#include <Ren/MMat.h>
#include <Ren/Pipeline.h>
#include <Ren/RenderPass.h>
#include <Ren/Texture.h>
#include <Ren/VertexInput.h>

#if defined(USE_VK_RENDER)
#include <Ren/DescriptorPool.h>
#endif

namespace Eng {
class ShaderLoader;
class PrimDraw {
  public:
    struct Binding;
    struct RenderTarget;

    enum class ePrim { Quad, Sphere, _Count };

  private:
    bool initialized_ = false;

    Ren::SubAllocation quad_vtx1_, quad_vtx2_, quad_ndx_;
    Ren::SubAllocation sphere_vtx1_, sphere_vtx2_, sphere_ndx_;
    Ren::SubAllocation temp_vtx1_, temp_vtx2_, temp_ndx_;

    Ren::VertexInput fs_quad_vtx_input_, sphere_vtx_input_;

    Ren::Context *ctx_ = nullptr;
#if defined(USE_VK_RENDER)
    Ren::SmallVector<Ren::RenderPass, 128> render_passes_;
    Ren::SmallVector<Ren::Pipeline, 128> pipelines_[int(ePrim::_Count)];

    const Ren::RenderPass *FindOrCreateRenderPass(Ren::Span<const Ren::RenderTarget> color_targets,
                                                  Ren::RenderTarget depth_target);
    const Ren::Pipeline *FindOrCreatePipeline(ePrim prim, Ren::ProgramRef p, const Ren::RenderPass *rp,
                                              Ren::Span<const Ren::RenderTarget> color_targets,
                                              Ren::RenderTarget depth_target, const Ren::RastState *rs);
#endif
    Ren::SmallVector<Ren::Framebuffer, 128> framebuffers_;

    const Ren::Framebuffer *FindOrCreateFramebuffer(const Ren::RenderPass *rp,
                                                    Ren::Span<const Ren::RenderTarget> color_targets,
                                                    Ren::RenderTarget depth_target, Ren::RenderTarget stencil_target);

  public:
    ~PrimDraw();

    bool LazyInit(Ren::Context &ctx);
    void CleanUp();

    void Reset();

    struct RenderTarget {
        Ren::Framebuffer *fb;
        uint32_t clear_bits;
    };

    void DrawPrim(ePrim prim, const Ren::ProgramRef &p, Ren::Span<const Ren::RenderTarget> color_rts,
                  Ren::RenderTarget depth_rt, const Ren::RastState &new_rast_state, Ren::RastState &applied_rast_state,
                  Ren::Span<const Ren::Binding> bindings, const void *uniform_data, int uniform_data_len,
                  int uniform_data_offset, int instances = 1);
};
} // namespace Eng