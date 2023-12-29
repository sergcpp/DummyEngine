#pragma once

#include <cstdint>
#include <cstring>

#include "Buffer.h"
#include "MemoryAllocator.h"
#include "TextureParams.h"

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4996)
#endif

namespace Ren {
class ILog;
class MemoryAllocators;

struct TexHandle {
    VkImage img = VK_NULL_HANDLE;
    SmallVector<VkImageView, 1> views;
    VkSampler sampler = VK_NULL_HANDLE;
    uint32_t generation = 0; // used to identify unique texture (name can be reused)

    TexHandle() { views.push_back(VK_NULL_HANDLE); }
    TexHandle(VkImage _img, VkImageView _view0, VkImageView _view1, VkSampler _sampler, uint32_t _generation)
        : img(_img), sampler(_sampler), generation(_generation) {
        assert(_view0 != VK_NULL_HANDLE);
        views.push_back(_view0);
        views.push_back(_view1);
    }

    explicit operator bool() const { return img != VK_NULL_HANDLE; }
};
static_assert(sizeof(TexHandle) == 56, "!");
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
    uint32_t cubemap_ready_ = 0;
    String name_;

    void Free();

    void InitFromRAWData(Buffer *sbuf, int data_off, void *_cmd_buf, MemoryAllocators *mem_allocs, const Tex2DParams &p,
                         ILog *log);
    void InitFromTGAFile(const void *data, Buffer &stage_buf, void *_cmd_buf, MemoryAllocators *mem_allocs,
                         const Tex2DParams &p, ILog *log);
    void InitFromTGA_RGBEFile(const void *data, Buffer &stage_buf, void *_cmd_buf, MemoryAllocators *mem_allocs,
                              const Tex2DParams &p, ILog *log);
    void InitFromDDSFile(const void *data, int size, Buffer &sbuf, void *_cmd_buf, MemoryAllocators *mem_allocs,
                         const Tex2DParams &p, ILog *log);
    void InitFromPNGFile(const void *data, int size, Buffer &sbuf, void *_cmd_buf, MemoryAllocators *mem_allocs,
                         const Tex2DParams &p, ILog *log);
    void InitFromKTXFile(const void *data, int size, Buffer &sbuf, void *_cmd_buf, MemoryAllocators *mem_allocs,
                         const Tex2DParams &p, ILog *log);

    void InitFromRAWData(Buffer &sbuf, int data_off[6], void *_cmd_buf, MemoryAllocators *mem_allocs,
                         const Tex2DParams &p, ILog *log);
    void InitFromTGAFile(const void *data[6], Buffer &sbuf, void *_cmd_buf, MemoryAllocators *mem_allocs,
                         const Tex2DParams &p, ILog *log);
    void InitFromTGA_RGBEFile(const void *data[6], Buffer &sbuf, void *_cmd_buf, MemoryAllocators *mem_allocs,
                              const Tex2DParams &p, ILog *log);
    void InitFromPNGFile(const void *data[6], const int size[6], Buffer &sbuf, void *_cmd_buf,
                         MemoryAllocators *mem_allocs, const Tex2DParams &p, ILog *log);
    void InitFromDDSFile(const void *data[6], const int size[6], Buffer &sbuf, void *_cmd_buf,
                         MemoryAllocators *mem_allocs, const Tex2DParams &p, ILog *log);
    void InitFromKTXFile(const void *data[6], const int size[6], Buffer &sbuf, void *_cmd_buf,
                         MemoryAllocators *mem_allocs, const Tex2DParams &p, ILog *log);

  public:
    Tex2DParams params;

    uint32_t first_user = 0xffffffff;
    mutable eResState resource_state = eResState::Undefined;

    Texture2D() = default;
    Texture2D(const char *name, ApiContext *api_ctx, const Tex2DParams &params, MemoryAllocators *mem_allocs,
              ILog *log);
    Texture2D(const char *name, ApiContext *api_ctx, VkImage img, VkImageView view, VkSampler sampler,
              const Tex2DParams &_params, ILog *log)
        : handle_{img, view, VK_NULL_HANDLE, sampler, 0}, params(_params), ready_(true), name_(name) {}
    Texture2D(const char *name, ApiContext *api_ctx, const void *data, const uint32_t size, const Tex2DParams &p,
              Buffer &stage_buf, void *_cmd_buf, MemoryAllocators *mem_allocs, eTexLoadStatus *load_status, ILog *log);
    Texture2D(const char *name, ApiContext *api_ctx, const void *data[6], const int size[6], const Tex2DParams &p,
              Buffer &stage_buf, void *_cmd_buf, MemoryAllocators *mem_allocs, eTexLoadStatus *load_status, ILog *log);
    Texture2D(const Texture2D &rhs) = delete;
    Texture2D(Texture2D &&rhs) noexcept { (*this) = std::move(rhs); }
    ~Texture2D();

    Texture2D &operator=(const Texture2D &rhs) = delete;
    Texture2D &operator=(Texture2D &&rhs) noexcept;

    void Init(const Tex2DParams &params, MemoryAllocators *mem_allocs, ILog *log);
    void Init(VkImage img, VkImageView view, VkSampler sampler, const Tex2DParams &_params, ILog *log) {
        handle_ = {img, view, VK_NULL_HANDLE, sampler, 0};
        params = _params;
        ready_ = true;
    }
    void Init(const void *data, const uint32_t size, const Tex2DParams &p, Buffer &stage_buf, void *_cmd_buf,
              MemoryAllocators *mem_allocs, eTexLoadStatus *load_status, ILog *log);
    void Init(const void *data[6], const int size[6], const Tex2DParams &p, Buffer &stage_buf, void *_cmd_buf,
              MemoryAllocators *mem_allocs, eTexLoadStatus *load_status, ILog *log);

    bool Realloc(int w, int h, int mip_count, int samples, Ren::eTexFormat format, Ren::eTexBlock block, bool is_srgb,
                 void *_cmd_buf, MemoryAllocators *mem_allocs, ILog *log);

    const TexHandle &handle() const { return handle_; }
    TexHandle &handle() { return handle_; }
    VkSampler vk_sampler() const { return handle_.sampler; }
    uint16_t initialized_mips() const { return initialized_mips_; }

    VkDescriptorImageInfo vk_desc_image_info(const int view_index = 0,
                                             VkImageLayout layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) const {
        VkDescriptorImageInfo ret;
        ret.sampler = handle_.sampler;
        ret.imageView = handle_.views[view_index];
        ret.imageLayout = layout;
        return ret;
    }

    ApiContext *api_ctx() { return api_ctx_; }

    const SamplingParams &sampling() const { return params.sampling; }

    bool ready() const { return ready_; }
    const String &name() const { return name_; }

    void SetSampling(SamplingParams sampling);
    void ApplySampling(SamplingParams sampling, ILog *log) { SetSampling(sampling); }

    void SetSubImage(int level, int offsetx, int offsety, int sizex, int sizey, Ren::eTexFormat format,
                     const void *data, int data_len);
    void SetSubImage(int level, int offsetx, int offsety, int sizex, int sizey, Ren::eTexFormat format,
                     const Buffer &sbuf, void *_cmd_buf, int data_off, int data_len);

    void DownloadTextureData(eTexFormat format, void *out_data) const;
    void CopyTextureData(const Buffer &sbuf, void *_cmd_buf, int data_off);
};

void CopyImageToImage(void *_cmd_buf, Texture2D &src_tex, uint32_t src_level, uint32_t src_x, uint32_t src_y,
                      Texture2D &dst_tex, uint32_t dst_level, uint32_t dst_x, uint32_t dst_y, uint32_t width,
                      uint32_t height);

void ClearColorImage(Texture2D &tex, const float rgba[4], void *_cmd_buf);

class Texture1D : public RefCounter {
    BufferRef buf_;
    Texture1DParams params_;
    String name_;

    VkBufferView buf_view_ = VK_NULL_HANDLE;

    void Free();

  public:
    Texture1D(const char *name, BufferRef buf, eTexFormat format, uint32_t offset, uint32_t size, ILog *log);
    Texture1D(const Texture1D &rhs) = delete;
    Texture1D(Texture1D &&rhs) noexcept { (*this) = std::move(rhs); }
    ~Texture1D();

    Texture1D &operator=(const Texture1D &rhs) = delete;
    Texture1D &operator=(Texture1D &&rhs) noexcept;

    const VkBufferView &view() const { return buf_view_; }

    const Texture1DParams &params() const { return params_; }

    const String &name() const { return name_; }

    void Init(BufferRef buf, eTexFormat format, uint32_t offset, uint32_t size, ILog *log);
};

VkFormat VKFormatFromTexFormat(eTexFormat format);

} // namespace Ren

#ifdef _MSC_VER
#pragma warning(pop)
#endif
