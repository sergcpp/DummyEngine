#pragma once

#include <vector>

#include <Ren/Program.h>
#include <Ren/Texture.h>
#include <Ren/TextureAtlas.h>

#include <Ren/MVec.h>

#ifdef __GNUC__
#define force_inline __attribute__((always_inline)) inline
#endif
#ifdef _MSC_VER
#define force_inline __forceinline
#endif

struct JsObject;

namespace Ren {
class Context;
}

namespace Gui {
const char GL_DEFINES_KEY[] = "gl_defines";

enum class eBlendMode { BlAlpha, BlColor };
enum class eDrawMode { DrPassthrough, DrDistanceField, DrBlitDistanceField };

using Ren::Vec2f;
using Ren::Vec2i;
using Ren::Vec3f;
using Ren::Vec4f;

struct vertex_t {
    float pos[3];
    uint8_t col[4];
    uint16_t uvs[4];
};
static_assert(sizeof(vertex_t) == 24, "!");

force_inline uint8_t f32_to_u8(float value) { return uint8_t(value * 255); }
force_inline uint16_t f32_to_u16(float value) { return uint16_t(value * 65535); }

extern const uint8_t ColorWhite[4];
extern const uint8_t ColorGrey[4];
extern const uint8_t ColorBlack[4];
extern const uint8_t ColorRed[4];
extern const uint8_t ColorGreen[4];
extern const uint8_t ColorBlue[4];
extern const uint8_t ColorCyan[4];
extern const uint8_t ColorMagenta[4];
extern const uint8_t ColorYellow[4];

class Renderer {
  public:
    Renderer(Ren::Context &ctx, const JsObject &config);
    ~Renderer();

    Renderer(const Renderer &rhs) = delete;
    Renderer &operator=(const Renderer &rhs) = delete;

    Ren::ProgramRef program() const { return ui_program_; }

    void SwapBuffers();
    void Draw();

    void PushClipArea(const Ren::Vec2f dims[2]);
    void PopClipArea();
    const Vec2f *GetClipArea() const;

    // Returns pointers to mapped vertex buffer. Do NOT read from it, it is write-combined
    // memory and will result in terrible latency!
    int AcquireVertexData(vertex_t **vertex_data, int *vertex_avail,
                          uint16_t **index_data, int *index_avail);
    void SubmitVertexData(int vertex_count, int index_count);

    // Simple drawing functions
    void PushImageQuad(eDrawMode draw_mode, int tex_layer, const Vec2f pos[2],
                       const Vec2f uvs_px[2]);
    void PushLine(eDrawMode draw_mode, int tex_layer, const uint8_t color[4],
                  const Vec4f &p0, const Vec4f &p1, const Vec2f &d0, const Vec2f &d1,
                  const Vec4f &thickness);
    void PushCurve(eDrawMode draw_mode, int tex_layer, const uint8_t color[4],
                   const Vec4f &p0, const Vec4f &p1, const Vec4f &p2, const Vec4f &p3,
                   const Vec4f &thickness);

  private:
    static const int FrameSyncWindow = 2;
    static const int MaxClipStackSize = 8;

    static_assert(FrameSyncWindow > 1, "!");

    Ren::Context &ctx_;

    int vertex_count_[FrameSyncWindow];
    int index_count_[FrameSyncWindow];
    int fill_range_index_, draw_range_index_;

    Ren::ProgramRef ui_program_;
#if defined(USE_GL_RENDER)
    uint32_t vao_;
    uint32_t vertex_buf_id_, index_buf_id_;
#endif
    // TODO: Replace with buffer mapping
    std::unique_ptr<vertex_t> vtx_data_;
    std::unique_ptr<uint16_t> ndx_data_;

    void *buf_range_fences_[FrameSyncWindow] = {};

    Ren::Vec2f clip_area_stack_[MaxClipStackSize][2];
    int clip_area_stack_size_ = 0;
};
} // namespace Gui
