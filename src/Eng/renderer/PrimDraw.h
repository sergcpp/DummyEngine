#pragma once

#include <Ren/DrawCall.h>
#include <Ren/Framebuffer.h>
#include <Ren/Fwd.h>
#include <Ren/MMat.h>
#include <Ren/Pipeline.h>
#include <Ren/RenderPass.h>
#include <Ren/Texture.h>
#include <Ren/VertexInput.h>

#if defined(REN_VK_BACKEND)
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

    Ren::VertexInputRef fs_quad_vtx_input_, sphere_vtx_input_;

    Ren::Context *ctx_ = nullptr;
#if defined(REN_VK_BACKEND)
    std::vector<Ren::PipelineRef> pipelines_;
#endif
    std::vector<Ren::Framebuffer> framebuffers_;

    const Ren::Framebuffer *FindOrCreateFramebuffer(const Ren::RenderPass *rp, Ren::RenderTarget depth_target,
                                                    Ren::RenderTarget stencil_target,
                                                    Ren::Span<const Ren::RenderTarget> color_targets);

  public:
    ~PrimDraw();

    bool LazyInit(Ren::Context &ctx);
    void CleanUp();

    struct RenderTarget {
        Ren::Framebuffer *fb;
        uint32_t clear_bits;
    };

    void DrawPrim(ePrim prim, const Ren::ProgramRef &p, Ren::RenderTarget depth_rt,
                  Ren::Span<const Ren::RenderTarget> color_rts, const Ren::RastState &new_rast_state,
                  Ren::RastState &applied_rast_state, Ren::Span<const Ren::Binding> bindings, const void *uniform_data,
                  int uniform_data_len, int uniform_data_offset, int instances = 1);
};
} // namespace Eng