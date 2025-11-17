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
static_assert(sizeof(TexHandle) == 48);

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

class Texture : public RefCounter {
    ApiContext *api_ctx_ = nullptr;
    TexHandle handle_;
    MemAllocation alloc_;
    String name_;

    void InitFromRAWData(Buffer *sbuf, int data_off, CommandBuffer cmd_buf, MemAllocators *mem_allocs,
                         const TexParams &p, ILog *log);
    void InitFromRAWData(Buffer &sbuf, int data_off[6], CommandBuffer cmd_buf, MemAllocators *mem_allocs,
                         const TexParams &p, ILog *log);

  public:
    TexParamsPacked params;

    uint32_t first_user = 0xffffffff;
    mutable eResState resource_state = eResState::Undefined;

    Texture() = default;
    Texture(std::string_view name, ApiContext *api_ctx, const TexParams &params, MemAllocators *mem_allocs, ILog *log);
    Texture(std::string_view name, ApiContext *api_ctx, const TexHandle &handle, const TexParams &_params,
            MemAllocation &&alloc, ILog *log)
        : api_ctx_(api_ctx), name_(name) {
        Init(handle, _params, std::move(alloc), log);
    }
    Texture(std::string_view name, ApiContext *api_ctx, Span<const uint8_t> data, const TexParams &p,
            MemAllocators *mem_allocs, eTexLoadStatus *load_status, ILog *log);
    Texture(std::string_view name, ApiContext *api_ctx, Span<const uint8_t> data[6], const TexParams &p,
            MemAllocators *mem_allocs, eTexLoadStatus *load_status, ILog *log);
    Texture(const Texture &rhs) = delete;
    Texture(Texture &&rhs) noexcept { (*this) = std::move(rhs); }
    ~Texture();

    void Free();
    void FreeImmediate();

    Texture &operator=(const Texture &rhs) = delete;
    Texture &operator=(Texture &&rhs) noexcept;

    void Init(const TexParams &p, MemAllocators *mem_allocs, ILog *log);
    void Init(const TexHandle &handle, const TexParams &p, MemAllocation &&alloc, ILog *log);
    void Init(Span<const uint8_t> data, const TexParams &p, MemAllocators *mem_allocs, eTexLoadStatus *load_status,
              ILog *log);
    void Init(Span<const uint8_t> data[6], const TexParams &p, MemAllocators *mem_allocs, eTexLoadStatus *load_status,
              ILog *log);

    bool Realloc(int w, int h, int mip_count, int samples, eTexFormat format, CommandBuffer cmd_buf,
                 MemAllocators *mem_allocs, ILog *log);

    [[nodiscard]] const TexHandle &handle() const { return handle_; }
    [[nodiscard]] TexHandle &handle() { return handle_; }
    [[nodiscard]] VkSampler vk_sampler() const { return handle_.sampler; }
    [[nodiscard]] const MemAllocation &mem_alloc() const { return alloc_; }

    [[nodiscard]] VkDescriptorImageInfo
    vk_desc_image_info(const int view_index = 0,
                       VkImageLayout layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) const {
        VkDescriptorImageInfo ret;
        ret.sampler = handle_.sampler;
        ret.imageView = handle_.views[view_index];
        ret.imageLayout = layout;
        return ret;
    }

    ApiContext *api_ctx() const { return api_ctx_; }

    [[nodiscard]] const String &name() const { return name_; }

    void SetSampling(SamplingParams sampling);
    void ApplySampling(SamplingParams sampling, ILog *log) { SetSampling(sampling); }

    int AddImageView(eTexFormat format, int mip_level, int mip_count, int base_layer, int layer_count);

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

void CopyImageToImage(CommandBuffer cmd_buf, const Texture &src_tex, uint32_t src_level, uint32_t src_x, uint32_t src_y,
                      uint32_t src_z, Texture &dst_tex, uint32_t dst_level, uint32_t dst_x, uint32_t dst_y,
                      uint32_t dst_z, uint32_t dst_face, uint32_t w, uint32_t h, uint32_t d);

void ClearImage(Texture &tex, const ClearColor &col, CommandBuffer cmd_buf);

VkFormat VKFormatFromTexFormat(eTexFormat format);

} // namespace Ren

#ifdef _MSC_VER
#pragma warning(pop)
#endif
