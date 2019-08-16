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
inline bool operator!=(const TexHandle lhs, const TexHandle rhs) { return !operator==(lhs, rhs); }
inline bool operator<(const TexHandle lhs, const TexHandle rhs) {
    if (lhs.id < rhs.id) {
        return true;
    } else if (lhs.id == rhs.id) {
        return lhs.generation < rhs.generation;
    }
    return false;
}

class MemoryAllocators;

class Texture2D : public RefCounter {
    TexHandle handle_;
    uint16_t initialized_mips_ = 0;
    bool ready_ = false;
    uint32_t cubemap_ready_ = 0;
    String name_;

    void Free();

    void InitFromRAWData(const Buffer *sbuf, int data_off, const Tex2DParams &p, ILog *log);
    void InitFromTGAFile(const void *data, Buffer &sbuf, const Tex2DParams &p, ILog *log);
    void InitFromTGA_RGBEFile(const void *data, Buffer &sbuf, const Tex2DParams &p, ILog *log);
    void InitFromDDSFile(const void *data, int size, Buffer &sbuf, const Tex2DParams &p, ILog *log);
    void InitFromPNGFile(const void *data, int size, Buffer &sbuf, const Tex2DParams &p, ILog *log);
    void InitFromKTXFile(const void *data, int size, Buffer &sbuf, const Tex2DParams &p, ILog *log);

    void InitFromRAWData(const Buffer &sbuf, int data_off[6], const Tex2DParams &p, ILog *log);
    void InitFromTGAFile(const void *data[6], Buffer &sbuf, const Tex2DParams &p, ILog *log);
    void InitFromTGA_RGBEFile(const void *data[6], Buffer &sbuf, const Tex2DParams &p, ILog *log);
    void InitFromPNGFile(const void *data[6], const int size[6], Buffer &sbuf, const Tex2DParams &p, ILog *log);
    void InitFromDDSFile(const void *data[6], const int size[6], Buffer &sbuf, const Tex2DParams &p, ILog *log);
    void InitFromKTXFile(const void *data[6], const int size[6], Buffer &sbuf, const Tex2DParams &p, ILog *log);

  public:
    Tex2DParams params;

    uint32_t first_user = 0xffffffff;

    Texture2D() = default;
    Texture2D(const char *name, ApiContext *api_ctx, const Tex2DParams &p, MemoryAllocators *mem_allocs, ILog *log);
    // TODO: remove this!
    Texture2D(const char *name, ApiContext *api_ctx, uint32_t tex_id, MemoryAllocators *mem_allocs,
              const Tex2DParams &p, ILog *log)
        : handle_{tex_id, 0}, params(p), ready_(true), name_(name) {}
    Texture2D(const char *name, ApiContext *api_ctx, const void *data, const uint32_t size, const Tex2DParams &p,
              Buffer &stage_buf, void *_cmd_buf, MemoryAllocators *mem_allocs, eTexLoadStatus *load_status, ILog *log);
    Texture2D(const char *name, ApiContext *api_ctx, const void *data[6], const int size[6], const Tex2DParams &p,
              Buffer &stage_buf, void *_cmd_buf, MemoryAllocators *mem_allocs, eTexLoadStatus *load_status, ILog *log);
    Texture2D(const Texture2D &rhs) = delete;
    Texture2D(Texture2D &&rhs) noexcept { (*this) = std::move(rhs); }
    ~Texture2D();

    Texture2D &operator=(const Texture2D &rhs) = delete;
    Texture2D &operator=(Texture2D &&rhs) noexcept;

    uint64_t GetBindlessHandle() const;

    void Init(const Tex2DParams &p, MemoryAllocators *mem_allocs, ILog *log);
    void Init(const void *data, uint32_t size, const Tex2DParams &p, Buffer &stage_buf, void *_cmd_buf,
              MemoryAllocators *mem_allocs, eTexLoadStatus *load_status, ILog *log);
    void Init(const void *data[6], const int size[6], const Tex2DParams &p, Buffer &stage_buf, void *_cmd_buf,
              MemoryAllocators *mem_allocs, eTexLoadStatus *load_status, ILog *log);

    void Realloc(int w, int h, int mip_count, int samples, Ren::eTexFormat format, Ren::eTexBlock block, bool is_srgb,
                 void *_cmd_buf, MemoryAllocators *mem_allocs, ILog *log);

    TexHandle handle() const { return handle_; }
    uint32_t id() const { return handle_.id; }
    uint32_t generation() const { return handle_.generation; }
    uint16_t initialized_mips() const { return initialized_mips_; }

    const SamplingParams &sampling() const { return params.sampling; }

    bool ready() const { return ready_; }
    const String &name() const { return name_; }

    void SetSampling(SamplingParams sampling) { params.sampling = sampling; }
    void ApplySampling(SamplingParams sampling, ILog *log);

    void SetSubImage(int level, int offsetx, int offsety, int sizex, int sizey, Ren::eTexFormat format,
                     const void *data, int data_len);
    SyncFence SetSubImage(int level, int offsetx, int offsety, int sizex, int sizey, Ren::eTexFormat format,
                          const Buffer &sbuf, void *_cmd_buf, int data_off, int data_len);

    void DownloadTextureData(eTexFormat format, void *out_data) const;
    void CopyTextureData(const Buffer &sbuf, void *_cmd_buf, int data_off);

    mutable eResState resource_state = eResState::Undefined;
};

void CopyImageToImage(void *_cmd_buf, Texture2D &src_tex, uint32_t src_level, uint32_t src_x, uint32_t src_y,
                      Texture2D &dst_tex, uint32_t dst_level, uint32_t dst_x, uint32_t dst_y, uint32_t width,
                      uint32_t height);

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
    Texture1D(const char *name, BufferRef buf, eTexFormat format, uint32_t offset, uint32_t size, ILog *log);
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

    void Init(BufferRef buf, eTexFormat format, uint32_t offset, uint32_t size, ILog *log);
};

uint32_t GLFormatFromTexFormat(eTexFormat format);
uint32_t GLInternalFormatFromTexFormat(eTexFormat format, bool is_srgb);
uint32_t GLTypeFromTexFormat(eTexFormat format);
uint32_t GLBindTarget(eBindTarget binding);

void GLUnbindTextureUnits(int start, int count);
} // namespace Ren

#ifdef _MSC_VER
#pragma warning(pop)
#endif
