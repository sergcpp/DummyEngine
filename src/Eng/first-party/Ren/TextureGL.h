#pragma once

#include <cstdint>
#include <cstring>

#include "Buffer.h"
#include "TextureParams.h"

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4996)
#endif

namespace Ren {
class ILog;

struct TexHandle {
    uint32_t id = 0; // native gl name
    SmallVector<uint32_t, 1> views;
    uint32_t generation = 0; // used to identify unique texture (name can be reused)

    TexHandle() = default;
    TexHandle(const uint32_t _id, const uint32_t _gen) : id(_id), generation(_gen) {}

    explicit operator bool() const { return id != 0; }
};
static_assert(sizeof(TexHandle) == 40, "!");

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

class MemAllocators;

class Texture2D : public RefCounter {
    TexHandle handle_;
    String name_;

    void InitFromRAWData(const Buffer *sbuf, int data_off, const TexParams &p, ILog *log);
    void InitFromRAWData(const Buffer &sbuf, int data_off[6], const TexParams &p, ILog *log);

  public:
    TexParams params;

    uint32_t first_user = 0xffffffff;

    Texture2D() = default;
    Texture2D(std::string_view name, ApiContext *api_ctx, const TexParams &p, MemAllocators *mem_allocs, ILog *log);
    Texture2D(std::string_view name, ApiContext *api_ctx, const TexHandle &handle, const TexParams &_params,
              MemAllocation &&alloc, ILog *log)
        : name_(name) {
        Init(handle, _params, std::move(alloc), log);
    }
    Texture2D(std::string_view name, ApiContext *api_ctx, Span<const uint8_t> data, const TexParams &p,
              MemAllocators *mem_allocs, eTexLoadStatus *load_status, ILog *log);
    Texture2D(std::string_view name, ApiContext *api_ctx, Span<const uint8_t> data[6], const TexParams &p,
              MemAllocators *mem_allocs, eTexLoadStatus *load_status, ILog *log);
    Texture2D(const Texture2D &rhs) = delete;
    Texture2D(Texture2D &&rhs) noexcept { (*this) = std::move(rhs); }
    ~Texture2D();

    void Free();
    void FreeImmediate() { Free(); }

    Texture2D &operator=(const Texture2D &rhs) = delete;
    Texture2D &operator=(Texture2D &&rhs) noexcept;

    uint64_t GetBindlessHandle() const;

    void Init(const TexParams &p, MemAllocators *mem_allocs, ILog *log);
    void Init(const TexHandle &handle, const TexParams &_params, MemAllocation &&alloc, ILog *log);
    void Init(Span<const uint8_t> data, const TexParams &p, MemAllocators *mem_allocs, eTexLoadStatus *load_status,
              ILog *log);
    void Init(Span<const uint8_t> data[6], const TexParams &p, MemAllocators *mem_allocs, eTexLoadStatus *load_status,
              ILog *log);

    void Realloc(int w, int h, int mip_count, int samples, eTexFormat format, bool is_srgb, CommandBuffer cmd_buf,
                 MemAllocators *mem_allocs, ILog *log);

    [[nodiscard]] TexHandle handle() const { return handle_; }
    [[nodiscard]] uint32_t id() const { return handle_.id; }
    [[nodiscard]] uint32_t generation() const { return handle_.generation; }
    [[nodiscard]] const MemAllocation &mem_alloc() const {
        static MemAllocation dummy;
        return dummy;
    }
    [[nodiscard]] const String &name() const { return name_; }

    void SetSampling(SamplingParams sampling) { params.sampling = sampling; }
    void ApplySampling(SamplingParams sampling, ILog *log);

    void SetSubImage(int level, int offsetx, int offsety, int sizex, int sizey, eTexFormat format, const void *data,
                     int data_len);
    SyncFence SetSubImage(int level, int offsetx, int offsety, int sizex, int sizey, eTexFormat format,
                          const Buffer &sbuf, CommandBuffer cmd_buf, int data_off, int data_len);
    void CopyTextureData(const Buffer &sbuf, CommandBuffer cmd_buf, int data_off, int data_len) const;

    mutable eResState resource_state = eResState::Undefined;
};

void CopyImageToImage(CommandBuffer cmd_buf, Texture2D &src_tex, uint32_t src_level, uint32_t src_x, uint32_t src_y,
                      Texture2D &dst_tex, uint32_t dst_level, uint32_t dst_x, uint32_t dst_y, uint32_t dst_face,
                      uint32_t width, uint32_t height);

void ClearImage(Texture2D &tex, const float rgba[4], CommandBuffer cmd_buf);

class Texture1D : public RefCounter {
    TexHandle handle_;
    WeakBufferRef buf_;
    Texture1DParams params_;
    String name_;

  public:
    Texture1D(std::string_view name, const BufferRef &buf, eTexFormat format, uint32_t offset, uint32_t size,
              ILog *log);
    Texture1D(const Texture1D &rhs) = delete;
    Texture1D(Texture1D &&rhs) noexcept { (*this) = std::move(rhs); }
    ~Texture1D();

    void Free();
    void FreeImmediate() { Free(); }

    Texture1D &operator=(const Texture1D &rhs) = delete;
    Texture1D &operator=(Texture1D &&rhs) noexcept;

    TexHandle handle() const { return handle_; }
    uint32_t id() const { return handle_.id; }
    int generation() const { return handle_.generation; }

    const Texture1DParams &params() const { return params_; }

    const String &name() const { return name_; }

    void Init(const BufferRef &buf, eTexFormat format, uint32_t offset, uint32_t size, ILog *log);
};

class Texture3D : public RefCounter {
    String name_;
    ApiContext *api_ctx_ = nullptr;
    TexHandle handle_;

    void Free();

  public:
    TexParams params;
    mutable eResState resource_state = eResState::Undefined;

    Texture3D() = default;
    Texture3D(std::string_view name, ApiContext *ctx, const TexParams &params, MemAllocators *mem_allocs, ILog *log);
    Texture3D(const Texture3D &rhs) = delete;
    Texture3D(Texture3D &&rhs) noexcept { (*this) = std::move(rhs); }
    ~Texture3D();

    Texture3D &operator=(const Texture3D &rhs) = delete;
    Texture3D &operator=(Texture3D &&rhs) noexcept;

    const String &name() const { return name_; }
    ApiContext *api_ctx() const { return api_ctx_; }
    const TexHandle &handle() const { return handle_; }
    uint32_t id() const { return handle_.id; }
    TexHandle &handle() { return handle_; }

    void Init(const TexParams &params, MemAllocators *mem_allocs, ILog *log);

    void SetSubImage(int offsetx, int offsety, int offsetz, int sizex, int sizey, int sizez, eTexFormat format,
                     const Buffer &sbuf, CommandBuffer cmd_buf, int data_off, int data_len);
};

uint32_t GLFormatFromTexFormat(eTexFormat format);
uint32_t GLInternalFormatFromTexFormat(eTexFormat format, bool is_srgb);
uint32_t GLTypeFromTexFormat(eTexFormat format);

void GLUnbindTextureUnits(int start, int count);
} // namespace Ren

#ifdef _MSC_VER
#pragma warning(pop)
#endif
