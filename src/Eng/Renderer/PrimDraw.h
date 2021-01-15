#pragma once

#include <Ren/Fwd.h>

#if defined(USE_GL_RENDER)
#include <Ren/VaoGL.h>
#endif

class ShaderLoader;

class PrimDraw {
    bool initialized_ = false;

    uint32_t quad_vtx1_offset_ = 0xffffffff, quad_vtx2_offset_ = 0xffffffff,
             quad_ndx_offset_ = 0xffffffff;

    uint32_t sphere_vtx1_offset_ = 0xffffffff, sphere_vtx2_offset_ = 0xffffffff,
             sphere_ndx_offset_ = 0xffffffff;

    uint32_t temp_buf1_vtx_offset_ = 0xffffffff, temp_buf2_vtx_offset_ = 0xffffffff,
             temp_buf_ndx_offset_ = 0xffffffff;

#if defined(USE_GL_RENDER)
    Ren::Vao fs_quad_vao_, sphere_vao_;
#endif
  public:
    bool LazyInit(Ren::Context &ctx);
    void CleanUp(Ren::Context &ctx);

#if defined(USE_GL_RENDER)
    uint32_t fs_quad_vao() const { return fs_quad_vao_.id(); }

    // TODO: refactor this
    uint32_t temp_buf1_vtx_offset() const { return temp_buf1_vtx_offset_; }

    uint32_t temp_buf_ndx_offset() const { return temp_buf_ndx_offset_; }
#endif

    struct Handle {
#if defined(USE_GL_RENDER)
        uint32_t id = 0;
#endif
        Handle() = default;
        Handle(Ren::TexHandle h) : id(h.id) {}
        Handle(Ren::BufHandle h) : id(h.id) {}
    };

    struct Binding {
        Ren::eBindTarget trg;
        uint16_t loc = 0;
        uint16_t offset = 0;
        uint16_t size = 0;
        Handle handle;

        Binding() = default;
        Binding(Ren::eBindTarget _trg, uint16_t _loc, Handle _handle)
            : trg(_trg), loc(_loc), handle(_handle) {}
        Binding(Ren::eBindTarget _trg, uint16_t _loc, size_t _offset, size_t _size,
                Handle _handle)
            : trg(_trg), loc(_loc), offset(uint16_t(_offset)), size(uint16_t(_size)),
              handle(_handle) {}
    };
    static_assert(sizeof(Binding) == 12, "!");

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

        Uniform(uint16_t _loc, const Ren::Mat4f *_pfdata)
            : type(Ren::eType::Float32), size(16), loc(_loc),
              pfdata(Ren::ValuePtr(_pfdata)) {}
        Uniform(uint16_t _loc, const Ren::Vec4f &_fdata)
            : type(Ren::eType::Float32), size(4), loc(_loc), fdata(_fdata) {}
        Uniform(uint16_t _loc, const Ren::Vec3f &_fdata)
            : type(Ren::eType::Float32), size(3), loc(_loc),
              fdata(_fdata[0], _fdata[1], _fdata[2], 0.0f) {}
        Uniform(uint16_t _loc, const Ren::Vec2f _fdata)
            : type(Ren::eType::Float32), size(2), loc(_loc),
              fdata(_fdata[0], _fdata[1], 0.0f, 0.0f) {}
        Uniform(uint16_t _loc, const float _fdata)
            : type(Ren::eType::Float32), size(1), loc(_loc), fdata(_fdata) {}
        Uniform(uint16_t _loc, const Ren::Mat4i *_pidata)
            : type(Ren::eType::Int32), size(16), loc(_loc),
              pidata(Ren::ValuePtr(_pidata)) {}
        Uniform(uint16_t _loc, const Ren::Vec4i &_idata)
            : type(Ren::eType::Int32), size(4), loc(_loc), idata(_idata) {}
        Uniform(uint16_t _loc, const Ren::Vec3i &_idata)
            : type(Ren::eType::Int32), size(3), loc(_loc),
              idata(_idata[0], _idata[1], _idata[2], 0) {}
        Uniform(uint16_t _loc, const Ren::Vec2i _idata)
            : type(Ren::eType::Int32), size(2), loc(_loc),
              idata(_idata[0], _idata[1], 0, 0) {}
        Uniform(uint16_t _loc, const int _idata)
            : type(Ren::eType::Int32), size(1), loc(_loc), idata(_idata) {}
    };
    static_assert(sizeof(Uniform) == 24, "!");

    struct RenderTarget {
#if defined(USE_GL_RENDER)
        uint32_t fb;
        uint32_t clear_bits;
#endif
    };

    enum class ePrim { Quad, Sphere };
    void DrawPrim(ePrim prim, const RenderTarget &rt, Ren::Program *p,
                  const Binding bindings[], int bindings_count, const Uniform uniforms[],
                  int uniforms_count);
};