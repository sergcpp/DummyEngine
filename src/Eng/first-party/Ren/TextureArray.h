#pragma once

#include "Resource.h"
#include "Sampler.h"
#include "Texture.h"

namespace Ren {
class Texture2DArray {
  public:
    Texture2DArray() = default;
    Texture2DArray(ApiContext *api_ctx, std::string_view name, int w, int h, int layer_count, int mip_count,
                   eTexFormat format, eTexFilter filter, Bitmask<eTexUsage> usage);
    ~Texture2DArray() { Free(); }

    void Free();
    void FreeImmediate();

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

#if defined(REN_VK_BACKEND)
    VkImage img() const { return img_; }
    VkImageView img_view() const { return img_view_; }
    const Sampler &sampler() const { return sampler_; }
#elif defined(REN_GL_BACKEND)
    uint32_t id() const { return tex_id_; }
#endif
    ApiContext *api_ctx() { return api_ctx_; }

    void SetSubImage(int level, int layer, int offsetx, int offsety, int sizex, int sizey, eTexFormat format,
                     const Buffer &sbuf, int data_off, int data_len, CommandBuffer cmd_buf);
    void Clear(const float rgba[4], CommandBuffer cmd_buf);

    mutable eResState resource_state = eResState::Undefined;

  protected:
    ApiContext *api_ctx_ = nullptr;
    std::string name_;
    int w_ = 0, h_ = 0;
    int mip_count_ = 0;
    int layer_count_ = 0;
    eTexFormat format_ = eTexFormat::Undefined;
    eTexFilter filter_ = eTexFilter::Nearest;
#if defined(REN_VK_BACKEND)
    VkImage img_ = VK_NULL_HANDLE;
    VkDeviceMemory mem_ = VK_NULL_HANDLE;
    VkImageView img_view_ = VK_NULL_HANDLE;
    Sampler sampler_;
#elif defined(REN_GL_BACKEND)
    uint32_t tex_id_ = 0xffffffff;
#endif
};
} // namespace Ren