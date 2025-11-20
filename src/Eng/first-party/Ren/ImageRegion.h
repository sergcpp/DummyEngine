#pragma once

#include "Fwd.h"
#include "ImageParams.h"
#include "Span.h"
#include "Storage.h"

namespace Ren {
class Buffer;
class ImageAtlasArray;

class ImageRegion : public RefCounter {
    String name_;
    ImageAtlasArray *atlas_ = nullptr;
    int pos_[3] = {};
    bool ready_ = false;

    [[nodiscard]] bool InitFromDDSFile(Span<const uint8_t> data, ImgParams p, ImageAtlasArray *atlas);
    [[nodiscard]] bool InitFromRAWData(const Buffer &sbuf, int data_off, int data_len, CommandBuffer cmd_buf,
                                       const ImgParams &p, ImageAtlasArray *atlas);

  public:
    ImgParams params;

    ImageRegion() = default;
    ImageRegion(std::string_view name, ImageAtlasArray *atlas, const int texture_pos[3]);
    ImageRegion(std::string_view name, Span<const uint8_t> data, const ImgParams &p, CommandBuffer cmd_buf,
                ImageAtlasArray *atlas, eImgLoadStatus *load_status);
    ImageRegion(std::string_view name, const Buffer &sbuf, int data_off, int data_len, const ImgParams &p,
                CommandBuffer cmd_buf, ImageAtlasArray *atlas, eImgLoadStatus *load_status);
    ~ImageRegion();

    ImageRegion(const ImageRegion &rhs) = default;
    ImageRegion(ImageRegion &&rhs) noexcept { (*this) = std::move(rhs); }

    ImageRegion &operator=(ImageRegion &&rhs) noexcept;

    [[nodiscard]] const String &name() const { return name_; }
    [[nodiscard]] int pos(int i) const { return pos_[i]; }

    [[nodiscard]] bool ready() const { return ready_; }

    void Init(Span<const uint8_t> data, const ImgParams &p, CommandBuffer cmd_buf, ImageAtlasArray *atlas,
              eImgLoadStatus *load_status);
    void Init(const Buffer &sbuf, int data_off, int data_len, const ImgParams &p, CommandBuffer cmd_buf,
              ImageAtlasArray *atlas, eImgLoadStatus *load_status);
};

using ImageRegionRef = StrongRef<ImageRegion, NamedStorage<ImageRegion>>;
using ImageRegionStorage = NamedStorage<ImageRegion>;
} // namespace Ren