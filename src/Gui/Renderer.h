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
const char UI_PROGRAM_NAME[] = "ui_program";
const char UI_PROGRAM2_NAME[] = "ui_program2";

enum ePrimitiveType { PrimTriangle };
enum eBlendMode { BlAlpha, BlColor };
enum eDrawMode { DrPassthrough, DrDistanceField };

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

force_inline uint8_t f32_to_u8(float value) {
    return uint8_t(value * 255);
}

force_inline uint16_t f32_to_u16(float value) {
    return uint16_t(value * 65535);
}

class Renderer {
public:
    Renderer(Ren::Context &ctx, const JsObject &config);
    ~Renderer();

    Renderer(const Renderer &rhs) = delete;
    Renderer &operator=(const Renderer &rhs) = delete;

    void BeginDraw();
    void EndDraw();

    // Returns pointers to mapped vertex buffer. Do NOT read from it, it is write-combined memory and will result in terrible latencies!
    int AcquireVertexData(vertex_t **vertex_data, int *vertex_avail, uint16_t **index_data, int *index_avail);
    void SubmitVertexData(int vertex_count, int index_count, bool force_new_buffer);

    void DrawImageQuad(eDrawMode draw_mode, int tex_layer, const Vec2f pos[2], const Vec2f uvs_px[2]);
private:
    static const int FrameSyncWindow = 2;
    static const int BuffersCount = 4;

    Ren::Context &ctx_;

    int vertex_count_[BuffersCount];
    int index_count_[BuffersCount];
    int cur_buffer_index_, cur_range_index_;
    int cur_vertex_count_, cur_index_count_;

    Ren::ProgramRef ui_program2_;
#if defined(USE_GL_RENDER)
    uint32_t vao_[BuffersCount];
    uint32_t vertex_buf_id_[BuffersCount], index_buf_id_[BuffersCount];
#endif
    vertex_t *cur_mapped_vtx_data_ = nullptr;
    uint16_t *cur_mapped_ndx_data_ = nullptr;

    void *buf_range_fences_[FrameSyncWindow] = {};

    void DrawCurrentBuffer();
};
}

