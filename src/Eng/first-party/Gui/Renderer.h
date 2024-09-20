#pragma once

#include <optional>
#include <vector>

#include "../Ren/Context.h"
#include "../Ren/Framebuffer.h"
#include "../Ren/Pipeline.h"
#include "../Ren/Program.h"
#include "../Ren/RenderPass.h"
#include "../Ren/Texture.h"
#include "../Ren/TextureAtlas.h"
#include "../Ren/VertexInput.h"

#include "MVec.h"
#include "SmallVector.h"

namespace Gui {
enum class eBlendMode { Alpha, Color };
enum class eDrawMode { Passthrough, DistanceField, BlitDistanceField };

struct vertex_t {
    float pos[3];
    uint8_t col[4];
    uint16_t uvs[4];
};
static_assert(sizeof(vertex_t) == 24, "!");

inline uint8_t f32_to_u8(const float value) { return uint8_t(value * 255); }
inline uint16_t f32_to_u16(const float value) { return uint16_t(value * 65535); }

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
    Renderer(Ren::Context &ctx);
    ~Renderer();

    Renderer(const Renderer &rhs) = delete;
    Renderer &operator=(const Renderer &rhs) = delete;

    bool Init();
    void Draw(int w, int h);

    void PushClipArea(const Vec2f dims[2]);
    void PopClipArea();
    std::optional<Vec4f> GetClipArea() const;

    // Returns pointers to mapped vertex buffer. Do NOT read from it, it is write-combined
    // memory and will result in terrible latency!
    int AcquireVertexData(vertex_t **vertex_data, int *vertex_avail, uint16_t **index_data, int *index_avail);
    void SubmitVertexData(int vertex_count, int index_count);

    // Simple drawing functions
    void PushImageQuad(eDrawMode draw_mode, int tex_layer, const Vec2f pos[2], const Vec2f uvs_px[2]);
    void PushLine(eDrawMode draw_mode, int tex_layer, const uint8_t color[4], const Vec4f &p0, const Vec4f &p1,
                  const Vec2f &d0, const Vec2f &d1, const Vec4f &thickness);
    void PushCurve(eDrawMode draw_mode, int tex_layer, const uint8_t color[4], const Vec4f &p0, const Vec4f &p1,
                   const Vec4f &p2, const Vec4f &p3, const Vec4f &thickness);

  private:
    static const int MaxVerticesPerRange = 64 * 1024;
    static const int MaxIndicesPerRange = 128 * 1024;

    static int g_instance_count;

    Ren::Context &ctx_;
    int instance_index_ = -1;
    std::string name_;

    int vtx_count_[Ren::MaxFramesInFlight];
    int ndx_count_[Ren::MaxFramesInFlight];

    Ren::RenderPass render_pass_;
    Ren::VertexInput vtx_input_;
    Ren::Pipeline pipeline_;
    SmallVector<Ren::Framebuffer, Ren::MaxFramesInFlight> framebuffers_;

    // buffers for the case if persistent mapping is not available
    std::unique_ptr<vertex_t[]> stage_vtx_data_;
    std::unique_ptr<uint16_t[]> stage_ndx_data_;

    Ren::BufferRef vertex_stage_buf_, index_stage_buf_;
    Ren::BufferRef vertex_buf_, index_buf_;

    vertex_t *vtx_stage_data_;
    uint16_t *ndx_stage_data_;

#ifndef NDEBUG
    Ren::SyncFence buf_range_fences_[Ren::MaxFramesInFlight];
#endif

    SmallVector<Vec4f, 16> clip_area_stack_;
};
} // namespace Gui
