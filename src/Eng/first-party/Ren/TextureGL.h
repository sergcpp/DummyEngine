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
    return lhs.id == rhs.id && lhs.views == rhs.views && lhs.generation == rhs.generation;
}
inline bool operator!=(const TexHandle lhs, const TexHandle rhs) { return !operator==(lhs, rhs); }
inline bool operator<(const TexHandle lhs, const TexHandle rhs) {
    if (lhs.id < rhs.id) {
        return true;
    } else if (lhs.id == rhs.id) {
        if (lhs.views < rhs.views) {
            return true;
        } else if (lhs.views == rhs.views) {
            return lhs.generation < rhs.generation;
        }
    }
    return false;
}

class MemAllocators;

class Texture : public RefCounter {
    TexHandle handle_;
    String name_;

    void InitFromRAWData(const Buffer *sbuf, int data_off, const TexParams &p, ILog *log);
    void InitFromRAWData(const Buffer &sbuf, int data_off[6], const TexParams &p, ILog *log);

  public:
    TexParamsPacked params;

    uint32_t first_user = 0xffffffff;
    mutable eResState resource_state = eResState::Undefined;

    Texture() = default;
    Texture(std::string_view name, ApiContext *api_ctx, const TexParams &p, MemAllocators *mem_allocs, ILog *log);
    Texture(std::string_view name, ApiContext *api_ctx, const TexHandle &handle, const TexParams &p,
            MemAllocation &&alloc, ILog *log)
        : name_(name) {
        Init(handle, p, std::move(alloc), log);
    }
    Texture(std::string_view name, ApiContext *api_ctx, Span<const uint8_t> data, const TexParams &p,
            MemAllocators *mem_allocs, eTexLoadStatus *load_status, ILog *log);
    Texture(std::string_view name, ApiContext *api_ctx, Span<const uint8_t> data[6], const TexParams &p,
            MemAllocators *mem_allocs, eTexLoadStatus *load_status, ILog *log);
    Texture(const Texture &rhs) = delete;
    Texture(Texture &&rhs) noexcept { (*this) = std::move(rhs); }
    ~Texture();

    void Free();
    void FreeImmediate() { Free(); }

    Texture &operator=(const Texture &rhs) = delete;
    Texture &operator=(Texture &&rhs) noexcept;

    uint64_t GetBindlessHandle() const;

    void Init(const TexParams &p, MemAllocators *mem_allocs, ILog *log);
    void Init(const TexHandle &handle, const TexParams &_params, MemAllocation &&alloc, ILog *log);
    void Init(Span<const uint8_t> data, const TexParams &p, MemAllocators *mem_allocs, eTexLoadStatus *load_status,
              ILog *log);
    void Init(Span<const uint8_t> data[6], const TexParams &p, MemAllocators *mem_allocs, eTexLoadStatus *load_status,
              ILog *log);

    void Realloc(int w, int h, int mip_count, int samples, eTexFormat format, CommandBuffer cmd_buf,
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

    void SetSubImage(int layer, int level, int offsetx, int offsety, int offsetz, int sizex, int sizey, int sizez,
                     eTexFormat format, const Buffer &sbuf, CommandBuffer cmd_buf, int data_off, int data_len);
    void SetSubImage(int level, int offsetx, int offsety, int offsetz, int sizex, int sizey, int sizez,
                     eTexFormat format, const Buffer &sbuf, CommandBuffer cmd_buf, int data_off, int data_len) {
        SetSubImage(0, level, offsetx, offsety, offsetz, sizex, sizey, sizez, format, sbuf, cmd_buf, data_off,
                    data_len);
    }
    void SetSubImage(int offsetx, int offsety, int offsetz, int sizex, int sizey, int sizez, eTexFormat format,
                     const Buffer &sbuf, CommandBuffer cmd_buf, int data_off, int data_len) {
        SetSubImage(0, 0, offsetx, offsety, offsetz, sizex, sizey, sizez, format, sbuf, cmd_buf, data_off, data_len);
    }
    void SetSubImage(int offsetx, int offsety, int sizex, int sizey, eTexFormat format, const Buffer &sbuf,
                     CommandBuffer cmd_buf, int data_off, int data_len) {
        SetSubImage(0, 0, offsetx, offsety, 0, sizex, sizey, 1, format, sbuf, cmd_buf, data_off, data_len);
    }

    void CopyTextureData(const Buffer &sbuf, CommandBuffer cmd_buf, int data_off, int data_len) const;
};

void CopyImageToImage(CommandBuffer cmd_buf, Texture &src_tex, uint32_t src_level, uint32_t src_x, uint32_t src_y,
                      uint32_t src_z, Texture &dst_tex, uint32_t dst_level, uint32_t dst_x, uint32_t dst_y,
                      uint32_t dst_z, uint32_t dst_face, uint32_t w, uint32_t h, uint32_t d);

void ClearImage(const Texture &tex, const float rgba[4], CommandBuffer cmd_buf);

uint32_t GLFormatFromTexFormat(eTexFormat format);
uint32_t GLInternalFormatFromTexFormat(eTexFormat format);
uint32_t GLTypeFromTexFormat(eTexFormat format);

void GLUnbindTextureUnits(int start, int count);
} // namespace Ren

#ifdef _MSC_VER
#pragma warning(pop)
#endif
