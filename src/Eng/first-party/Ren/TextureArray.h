#pragma once

#include "Resource.h"
#include "Sampler.h"
#include "Texture.h"

namespace Ren {
class Texture2DArray {
  public:
    Texture2DArray() = default;
    Texture2DArray(ApiContext *api_ctx, std::string_view name, int w, int h, int layer_count, eTexFormat format,
                   eTexFilter filter, eTexUsageBits usage);
    ~Texture2DArray();

    Texture2DArray(const Texture2DArray &rhs) = delete;
    Texture2DArray(Texture2DArray &&rhs) noexcept { (*this) = std::move(rhs); }

    Texture2DArray &operator=(const Texture2DArray &rhs) = delete;
    Texture2DArray &operator=(Texture2DArray &&rhs) noexcept;

    std::string_view name() const { return name_; }

    int mip_count() const { return mip_count_; }
    int layer_count() const { return layer_count_; }

    int w() const { return w_; }
    int h() const { return h_; }

    eTexFormat format() const { return format_; }

#if defined(USE_VK_RENDER)
    VkImage img() const { return img_; }
    VkImageView img_view() const { return img_view_; }
    const Sampler &sampler() const { return sampler_; }
#elif defined(USE_GL_RENDER)
    uint32_t id() const { return tex_id_; }
#endif

    void SetSubImage(int level, int layer, int offsetx, int offsety, int sizex, int sizey, eTexFormat format,
                     const Buffer &sbuf, int data_off, int data_len, void *_cmd_buf);
    void Clear(const float rgba[4], void *_cmd_buf);

    mutable eResState resource_state = eResState::Undefined;

  protected:
    std::string name_;
    ApiContext *api_ctx_ = nullptr;
    int w_ = 0, h_ = 0;
    int mip_count_ = 0;
    int layer_count_ = 0;
    eTexFormat format_ = eTexFormat::Undefined;
    eTexFilter filter_ = eTexFilter::NoFilter;
#if defined(USE_VK_RENDER)
    VkImage img_ = VK_NULL_HANDLE;
    VkDeviceMemory mem_ = VK_NULL_HANDLE;
    VkImageView img_view_ = VK_NULL_HANDLE;
    Sampler sampler_;
#elif defined(USE_GL_RENDER)
    uint32_t tex_id_ = 0xffffffff;
#endif
};
} // namespace Ren