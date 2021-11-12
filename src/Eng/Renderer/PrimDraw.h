#pragma once

#include <Ren/DrawCall.h>
#include <Ren/Fwd.h>
#include <Ren/MMat.h>
#include <Ren/Pipeline.h>
#include <Ren/RenderPass.h>
#include <Ren/Texture.h>
#include <Ren/VertexInput.h>

#if defined(USE_VK_RENDER)
#include <Ren/DescriptorPool.h>
#endif

class ProbeStorage;
class ShaderLoader;

namespace Ren {
class Framebuffer;
}

class PrimDraw {
  public:
    struct Binding;
    struct RenderTarget;

  private:
    bool initialized_ = false;

    uint32_t quad_vtx1_offset_ = 0xffffffff, quad_vtx2_offset_ = 0xffffffff, quad_ndx_offset_ = 0xffffffff;

    uint32_t sphere_vtx1_offset_ = 0xffffffff, sphere_vtx2_offset_ = 0xffffffff, sphere_ndx_offset_ = 0xffffffff;

    uint32_t temp_buf1_vtx_offset_ = 0xffffffff, temp_buf2_vtx_offset_ = 0xffffffff, temp_buf_ndx_offset_ = 0xffffffff;

    Ren::MeshRef skydome_mesh_;

    Ren::VertexInput fs_quad_vtx_input_, sphere_vtx_input_;

    Ren::Context *ctx_ = nullptr;
#if defined(USE_VK_RENDER)
    Ren::SmallVector<Ren::Pipeline, 16> pipelines_;

    const Ren::Pipeline *FindOrCreatePipeline(Ren::ProgramRef p, const Ren::RenderPass *rp, const Ren::RastState *rs,
                                              const Ren::Binding bindings[], const int bindings_count);
#endif
  public:
    ~PrimDraw();

    bool LazyInit(Ren::Context &ctx);
    void CleanUp();

    void Reset();

#if defined(USE_GL_RENDER)
    uint32_t fs_quad_vao() const { return fs_quad_vtx_input_.gl_vao(); }

    // TODO: refactor this
    uint32_t temp_buf1_vtx_offset() const { return temp_buf1_vtx_offset_; }

    uint32_t temp_buf_ndx_offset() const { return temp_buf_ndx_offset_; }
#endif

    const Ren::Mesh *skydome_mesh() const { return skydome_mesh_.get(); }

    struct Uniform {
        Ren::eType type;
        uint8_t size;
        uint16_t loc;
        union {
            Ren::Vec4f fdata;
            Ren::Vec4i idata;
            const float *pfdata;
            const int *pidata;
        };

        Uniform(int _loc, const Ren::Mat4f *_pfdata)
            : type(Ren::eType::Float32), size(16), loc(_loc), pfdata(Ren::ValuePtr(_pfdata)) {}
        Uniform(int _loc, const Ren::Vec4f &_fdata)
            : type(Ren::eType::Float32), size(4), loc(_loc), fdata(_fdata) {}
        Uniform(int _loc, const Ren::Vec3f &_fdata)
            : type(Ren::eType::Float32), size(3), loc(_loc), fdata(_fdata[0], _fdata[1], _fdata[2], 0.0f) {}
        Uniform(int _loc, const Ren::Vec2f _fdata)
            : type(Ren::eType::Float32), size(2), loc(_loc), fdata(_fdata[0], _fdata[1], 0.0f, 0.0f) {}
        Uniform(int _loc, const float _fdata) : type(Ren::eType::Float32), size(1), loc(_loc), fdata(_fdata) {}
        Uniform(int _loc, const Ren::Mat4i *_pidata)
            : type(Ren::eType::Int32), size(16), loc(_loc), pidata(Ren::ValuePtr(_pidata)) {}
        Uniform(int _loc, const Ren::Vec4i &_idata) : type(Ren::eType::Int32), size(4), loc(_loc), idata(_idata) {}
        Uniform(int _loc, const Ren::Vec3i &_idata)
            : type(Ren::eType::Int32), size(3), loc(_loc), idata(_idata[0], _idata[1], _idata[2], 0) {}
        Uniform(int _loc, const Ren::Vec2i _idata)
            : type(Ren::eType::Int32), size(2), loc(_loc), idata(_idata[0], _idata[1], 0, 0) {}
        Uniform(int _loc, const int _idata) : type(Ren::eType::Int32), size(1), loc(_loc), idata(_idata) {}
    };
    static_assert(sizeof(Uniform) == 20 || sizeof(Uniform) == 24, "!");

    struct RenderTarget {
        Ren::Framebuffer *fb;
        uint32_t clear_bits;
    };

    enum class ePrim { Quad, Sphere };
    void DrawPrim(ePrim prim, const RenderTarget &rt, Ren::Program *p, const Ren::Binding bindings[],
                  int bindings_count, const Uniform uniforms[], int uniforms_count);
    void DrawPrim(ePrim prim, const Ren::ProgramRef &p, const Ren::Framebuffer &fb, const Ren::RenderPass &rp,
                  const Ren::RastState &new_rast_state, Ren::RastState &applied_rast_state,
                  const Ren::Binding bindings[], int bindings_count, const void *uniform_data, int uniform_data_len,
                  int uniform_data_offset);
};