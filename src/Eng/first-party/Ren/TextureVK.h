#pragma once

#include <cstdint>
#include <cstring>

#include "Buffer.h"
#include "MemoryAllocator.h"
#include "TextureParams.h"

#include "VK.h"

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4996)
#endif

namespace Ren {
class ILog;
class MemAllocators;

struct TexHandle {
    VkImage img = {};
    SmallVector<VkImageView, 1> views;
    VkSampler sampler = {};
    uint32_t generation = 0; // used to identify unique texture (name can be reused)

    TexHandle() { views.push_back({}); }
    TexHandle(VkImage _img, VkImageView _view0, VkImageView _view1, VkSampler _sampler, uint32_t _generation)
        : img(_img), sampler(_sampler), generation(_generation) {
        assert(_view0 != VkImageView{});
        views.push_back(_view0);
        views.push_back(_view1);
    }

    explicit operator bool() const { return img != VkImage{}; }
};
static_assert(sizeof(TexHandle) == 48, "!");

inline bool operator==(const TexHandle &lhs, const TexHandle &rhs) {
    return lhs.img == rhs.img && lhs.views == rhs.views && lhs.sampler == rhs.sampler &&
           lhs.generation == rhs.generation;
}
inline bool operator!=(const TexHandle &lhs, const TexHandle &rhs) { return !operator==(lhs, rhs); }
inline bool operator<(const TexHandle &lhs, const TexHandle &rhs) {
    if (lhs.img < rhs.img) {
        return true;
    } else if (lhs.img == rhs.img) {
        if (lhs.views[0] < rhs.views[0]) { // we always compare only the first view
            return true;
        } else {
            return lhs.generation < rhs.generation;
        }
    }
    return false;
}

class TextureStageBuf;

class Texture2D : public RefCounter {
    ApiContext *api_ctx_ = nullptr;
    TexHandle handle_;
    MemAllocation alloc_;
    uint16_t initialized_mips_ = 0;
    bool ready_ = false;
    String name_;

    void InitFromRAWData(Buffer *sbuf, int data_off, CommandBuffer cmd_buf, MemAllocators *mem_allocs,
                         const Tex2DParams &p, ILog *log);
    void InitFromTGAFile(Span<const uint8_t> data, MemAllocators *mem_allocs, const Tex2DParams &p, ILog *log);
    void InitFromDDSFile(Span<const uint8_t> data, MemAllocators *mem_allocs, const Tex2DParams &p, ILog *log);
    void InitFromKTXFile(Span<const uint8_t> data, MemAllocators *mem_allocs, const Tex2DParams &p, ILog *log);

    void InitFromRAWData(Buffer &sbuf, int data_off[6], CommandBuffer cmd_buf, MemAllocators *mem_allocs,
                         const Tex2DParams &p, ILog *log);
    void InitFromTGAFile(Span<const uint8_t> data[6], MemAllocators *mem_allocs, const Tex2DParams &p, ILog *log);
    void InitFromDDSFile(Span<const uint8_t> data[6], MemAllocators *mem_allocs, const Tex2DParams &p, ILog *log);
    void InitFromKTXFile(Span<const uint8_t> data[6], MemAllocators *mem_allocs, const Tex2DParams &p, ILog *log);

  public:
    Tex2DParams params;

    uint32_t first_user = 0xffffffff;
    mutable eResState resource_state = eResState::Undefined;

    Texture2D() = default;
    Texture2D(std::string_view name, ApiContext *api_ctx, const Tex2DParams &params, MemAllocators *mem_allocs,
              ILog *log);
    Texture2D(std::string_view name, ApiContext *api_ctx, const TexHandle &handle, const Tex2DParams &_params,
              MemAllocation &&alloc, ILog *log)
        : api_ctx_(api_ctx), ready_(true), name_(name) {
        Init(handle, _params, std::move(alloc), log);
    }
    Texture2D(std::string_view name, ApiContext *api_ctx, Span<const uint8_t> data, const Tex2DParams &p,
              MemAllocators *mem_allocs, eTexLoadStatus *load_status, ILog *log);
    Texture2D(std::string_view name, ApiContext *api_ctx, Span<const uint8_t> data[6], const Tex2DParams &p,
              MemAllocators *mem_allocs, eTexLoadStatus *load_status, ILog *log);
    Texture2D(const Texture2D &rhs) = delete;
    Texture2D(Texture2D &&rhs) noexcept { (*this) = std::move(rhs); }
    ~Texture2D();

    void Free();
    void FreeImmediate();

    Texture2D &operator=(const Texture2D &rhs) = delete;
    Texture2D &operator=(Texture2D &&rhs) noexcept;

    void Init(const Tex2DParams &p, MemAllocators *mem_allocs, ILog *log);
    void Init(const TexHandle &handle, const Tex2DParams &p, MemAllocation &&alloc, ILog *log);
    void Init(Span<const uint8_t> data, const Tex2DParams &p, MemAllocators *mem_allocs, eTexLoadStatus *load_status,
              ILog *log);
    void Init(Span<const uint8_t> data[6], const Tex2DParams &p, MemAllocators *mem_allocs, eTexLoadStatus *load_status,
              ILog *log);

    bool Realloc(int w, int h, int mip_count, int samples, eTexFormat format, bool is_srgb, CommandBuffer cmd_buf,
                 MemAllocators *mem_allocs, ILog *log);

    [[nodiscard]] const TexHandle &handle() const { return handle_; }
    [[nodiscard]] TexHandle &handle() { return handle_; }
    [[nodiscard]] VkSampler vk_sampler() const { return handle_.sampler; }
    [[nodiscard]] const MemAllocation &mem_alloc() const { return alloc_; }
    [[nodiscard]] uint16_t initialized_mips() const { return initialized_mips_; }

    [[nodiscard]] VkDescriptorImageInfo
    vk_desc_image_info(const int view_index = 0,
                       VkImageLayout layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) const {
        VkDescriptorImageInfo ret;
        ret.sampler = handle_.sampler;
        ret.imageView = handle_.views[view_index];
        ret.imageLayout = layout;
        return ret;
    }

    ApiContext *api_ctx() { return api_ctx_; }

    [[nodiscard]] const SamplingParams &sampling() const { return params.sampling; }

    [[nodiscard]] bool ready() const { return ready_; }
    [[nodiscard]] const String &name() const { return name_; }

    void SetSampling(SamplingParams sampling);
    void ApplySampling(SamplingParams sampling, ILog *log) { SetSampling(sampling); }

    void SetSubImage(int level, int offsetx, int offsety, int sizex, int sizey, eTexFormat format, const Buffer &sbuf,
                     CommandBuffer cmd_buf, int data_off, int data_len);
    void CopyTextureData(const Buffer &sbuf, CommandBuffer cmd_buf, int data_off, int data_len) const;
};

void CopyImageToImage(CommandBuffer cmd_buf, Texture2D &src_tex, uint32_t src_level, uint32_t src_x, uint32_t src_y,
                      Texture2D &dst_tex, uint32_t dst_level, uint32_t dst_x, uint32_t dst_y, uint32_t dst_face,
                      uint32_t width, uint32_t height);

void ClearImage(Texture2D &tex, const float rgba[4], CommandBuffer cmd_buf);

class Texture1D : public RefCounter {
    ApiContext *api_ctx_ = nullptr;
    WeakBufferRef buf_;
    Texture1DParams params_;
    String name_;

    VkBufferView buf_view_ = VK_NULL_HANDLE;

  public:
    Texture1D(std::string_view name, const BufferRef &buf, eTexFormat format, uint32_t offset, uint32_t size,
              ILog *log);
    Texture1D(const Texture1D &rhs) = delete;
    Texture1D(Texture1D &&rhs) noexcept { (*this) = std::move(rhs); }
    ~Texture1D() { Free(); }

    void Free();
    void FreeImmediate();

    Texture1D &operator=(const Texture1D &rhs) = delete;
    Texture1D &operator=(Texture1D &&rhs) noexcept;

    const VkBufferView &view() const { return buf_view_; }

    const Texture1DParams &params() const { return params_; }

    const String &name() const { return name_; }

    void Init(const BufferRef &buf, eTexFormat format, uint32_t offset, uint32_t size, ILog *log);
};

class Texture3D : public RefCounter {
    String name_;
    ApiContext *api_ctx_ = nullptr;
    TexHandle handle_;
    MemAllocation alloc_;

    void Free();

  public:
    Tex3DParams params;
    mutable eResState resource_state = eResState::Undefined;

    Texture3D() = default;
    Texture3D(std::string_view name, ApiContext *ctx, const Tex3DParams &params, MemAllocators *mem_allocs, ILog *log);
    Texture3D(const Texture3D &rhs) = delete;
    Texture3D(Texture3D &&rhs) noexcept { (*this) = std::move(rhs); }
    ~Texture3D();

    Texture3D &operator=(const Texture3D &rhs) = delete;
    Texture3D &operator=(Texture3D &&rhs) noexcept;

    const String &name() const { return name_; }
    ApiContext *api_ctx() const { return api_ctx_; }
    const TexHandle &handle() const { return handle_; }
    TexHandle &handle() { return handle_; }
    VkSampler vk_sampler() const { return handle_.sampler; }

    void Init(const Tex3DParams &params, MemAllocators *mem_allocs, ILog *log);

    void SetSubImage(int offsetx, int offsety, int offsetz, int sizex, int sizey, int sizez, eTexFormat format,
                     const Buffer &sbuf, CommandBuffer cmd_buf, int data_off, int data_len);
};

VkFormat VKFormatFromTexFormat(eTexFormat format);

} // namespace Ren

#ifdef _MSC_VER
#pragma warning(pop)
#endif
