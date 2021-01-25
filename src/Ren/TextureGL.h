#pragma once

#include <cstdint>
#include <cstring>

#include "Buffer.h"

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4996)
#endif

namespace Ren {
class ILog;

enum eTexFlags {
    TexNoOwnership = (1u << 0u),
    TexMutable = (1u << 1u),
    TexSigned = (1u << 2u),
    TexSRGB = (1u << 3u),
    TexNoRepeat = (1u << 4u),
    TexMIPMin = (1u << 5u),
    TexMIPMax = (1u << 6u),
    TexNoBias = (1u << 7u),
    TexUsageScene = (1u << 8u),
    TexUsageUI = (1u << 9u)
};

struct Tex2DParams {
    uint16_t w = 0, h = 0;
    float lod_bias = 0.0f;
    uint16_t flags = 0;
    uint8_t cube = 0;
    uint8_t samples = 1;
    uint8_t fallback_color[4] = {0, 255, 255, 255};
    eTexFormat format = eTexFormat::Undefined;
    eTexFilter filter = eTexFilter::NoFilter;
    eTexRepeat repeat = eTexRepeat::Repeat;
    eTexCompare compare = eTexCompare::None;
};
static_assert(sizeof(Tex2DParams) == 20, "!");

inline bool operator==(const Tex2DParams &lhs, const Tex2DParams &rhs) {
    return lhs.w == rhs.w && lhs.h == rhs.h && lhs.lod_bias == rhs.lod_bias &&
           lhs.flags == rhs.flags && lhs.cube == rhs.cube && lhs.samples == rhs.samples &&
           lhs.fallback_color[0] == rhs.fallback_color[0] &&
           lhs.fallback_color[1] == rhs.fallback_color[1] &&
           lhs.fallback_color[2] == rhs.fallback_color[2] &&
           lhs.fallback_color[3] == rhs.fallback_color[3] && lhs.format == rhs.format &&
           lhs.filter == rhs.filter && lhs.repeat == rhs.repeat &&
           lhs.compare == rhs.compare;
}
inline bool operator!=(const Tex2DParams &lhs, const Tex2DParams &rhs) {
    return !operator==(lhs, rhs);
}

enum class eTexLoadStatus {
    TexFound,
    TexFoundReinitialized,
    TexCreatedDefault,
    TexCreatedFromData
};

struct TexHandle {
    uint32_t id = 0;         // native gl name
    uint32_t generation = 0; // used to identify unique texture (name can be reused)

    TexHandle() = default;
    TexHandle(const uint32_t _id, const uint32_t _gen) : id(_id), generation(_gen) {}

    explicit operator bool() const { return id != 0; }
};
inline bool operator==(const TexHandle lhs, const TexHandle rhs) {
    return lhs.id == rhs.id && lhs.generation == rhs.generation;
}
inline bool operator!=(const TexHandle lhs, const TexHandle rhs) {
    return !operator==(lhs, rhs);
}

class Texture2D : public RefCounter {
    TexHandle handle_;
    Tex2DParams params_;
    bool ready_ = false;
    uint32_t cubemap_ready_ = 0;
    String name_;

    void Free();

    void InitFromRAWData(const void *data, const Tex2DParams &p, ILog *log);
    void InitFromTGAFile(const void *data, const Tex2DParams &p, ILog *log);
    void InitFromTGA_RGBEFile(const void *data, const Tex2DParams &p, ILog *log);
    void InitFromDDSFile(const void *data, int size, const Tex2DParams &p, ILog *log);
    void InitFromPNGFile(const void *data, int size, const Tex2DParams &p, ILog *log);
    void InitFromKTXFile(const void *data, int size, const Tex2DParams &p, ILog *log);

    void InitFromRAWData(const void *data[6], const Tex2DParams &p, ILog *log);
    void InitFromTGAFile(const void *data[6], const Tex2DParams &p, ILog *log);
    void InitFromTGA_RGBEFile(const void *data[6], const Tex2DParams &p, ILog *log);
    void InitFromPNGFile(const void *data[6], const int size[6], const Tex2DParams &p,
                         ILog *log);
    void InitFromDDSFile(const void *data[6], const int size[6], const Tex2DParams &p,
                         ILog *log);
    void InitFromKTXFile(const void *data[6], const int size[6], const Tex2DParams &p,
                         ILog *log);

  public:
    Texture2D() = default;
    Texture2D(const char *name, const Tex2DParams &params, ILog *log);
    // TODO: remove this!
    Texture2D(const char *name, uint32_t tex_id, const Tex2DParams &params, ILog *log)
        : handle_{tex_id, 0}, params_(params), ready_(true), name_(name) {}
    Texture2D(const char *name, const void *data, int size, const Tex2DParams &params,
              eTexLoadStatus *load_status, ILog *log);
    Texture2D(const char *name, const void *data[6], const int size[6],
              const Tex2DParams &params, eTexLoadStatus *load_status, ILog *log);
    Texture2D(const Texture2D &rhs) = delete;
    Texture2D(Texture2D &&rhs) noexcept { (*this) = std::move(rhs); }
    ~Texture2D();

    Texture2D &operator=(const Texture2D &rhs) = delete;
    Texture2D &operator=(Texture2D &&rhs) noexcept;

    void Init(const Tex2DParams &params, ILog *log);
    void Init(const void *data, int size, const Tex2DParams &params,
              eTexLoadStatus *load_status, ILog *log);
    void Init(const void *data[6], const int size[6], const Tex2DParams &params,
              eTexLoadStatus *load_status, ILog *log);

    TexHandle handle() const { return handle_; }
    uint32_t id() const { return handle_.id; }
    uint32_t generation() const { return handle_.generation; }

    const Tex2DParams &params() const { return params_; }

    bool ready() const { return ready_; }
    const String &name() const { return name_; }

    void SetFilter(eTexFilter f, eTexRepeat r, eTexCompare c, float lod_bias);
    void SetSubImage(int level, int offsetx, int offsety, int sizex, int sizey,
                     Ren::eTexFormat format, const void *data);

    void ReadTextureData(eTexFormat format, void *out_data) const;
};

using Tex2DRef = StrongRef<Texture2D>;
using WeakTex2DRef = WeakRef<Texture2D>;
using Texture2DStorage = Storage<Texture2D>;

struct Texture1DParams {
    uint16_t offset = 0, size = 0;
    eTexFormat format = eTexFormat::Undefined;
    uint8_t _padding;
};
static_assert(sizeof(Texture1DParams) == 6, "!");

class Texture1D : public RefCounter {
    TexHandle handle_;
    BufferRef buf_;
    Texture1DParams params_;
    String name_;

    void Free();

  public:
    Texture1D(const char *name, BufferRef buf, eTexFormat format, uint32_t offset,
              uint32_t size, ILog *log);
    Texture1D(const Texture1D &rhs) = delete;
    Texture1D(Texture1D &&rhs) noexcept { (*this) = std::move(rhs); }
    ~Texture1D();

    Texture1D &operator=(const Texture1D &rhs) = delete;
    Texture1D &operator=(Texture1D &&rhs) noexcept;

    TexHandle handle() const { return handle_; }
    uint32_t id() const { return handle_.id; }
    int generation() const { return handle_.generation; }

    const Texture1DParams &params() const { return params_; }

    const String &name() const { return name_; }

    void Init(BufferRef buf, eTexFormat format, uint32_t offset, uint32_t size,
              ILog *log);
};

using Tex1DRef = StrongRef<Texture1D>;
using WeakTex1DRef = WeakRef<Texture1D>;
using Texture1DStorage = Storage<Texture1D>;

uint32_t GLFormatFromTexFormat(eTexFormat format);
uint32_t GLInternalFormatFromTexFormat(eTexFormat format, bool is_srgb);
uint32_t GLTypeFromTexFormat(eTexFormat format);
uint32_t GLBindTarget(eBindTarget binding);

void GLUnbindTextureUnits(int start, int count);

const int MaxColorAttachments = 4;

class Framebuffer {
    uint32_t id_ = 0;
    int color_attachments_count_ = 0;
    TexHandle color_attachments_[MaxColorAttachments];
    TexHandle depth_attachment_, stencil_attachment_;

  public:
    Framebuffer() = default;
    ~Framebuffer();

    Framebuffer(const Framebuffer &rhs) = delete;
    Framebuffer &operator=(const Framebuffer &rhs) = delete;

    uint32_t id() const { return id_; }

    bool Setup(const TexHandle color_attachments[], int color_attachments_count,
               TexHandle depth_attachment, TexHandle stencil_attachment,
               bool is_multisampled);
};
} // namespace Ren

#ifdef _MSC_VER
#pragma warning(pop)
#endif
