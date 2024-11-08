#pragma once

#include "Fwd.h"
#include "Span.h"
#include "Storage.h"
#include "TextureParams.h"

namespace Ren {
class Buffer;
class TextureAtlasArray;

class TextureRegion : public RefCounter {
    String name_;
    TextureAtlasArray *atlas_ = nullptr;
    int texture_pos_[3] = {};
    Tex2DParams params_;
    bool ready_ = false;

    [[nodiscard]] bool InitFromDDSFile(Span<const uint8_t> data, Tex2DParams p, TextureAtlasArray *atlas);
    [[nodiscard]] bool InitFromRAWData(const Buffer &sbuf, int data_off, int data_len, CommandBuffer cmd_buf,
                                       const Tex2DParams &p, TextureAtlasArray *atlas);

  public:
    TextureRegion() = default;
    TextureRegion(std::string_view name, TextureAtlasArray *atlas, const int texture_pos[3]);
    TextureRegion(std::string_view name, Span<const uint8_t> data, const Tex2DParams &p, TextureAtlasArray *atlas,
                  eTexLoadStatus *load_status);
    TextureRegion(std::string_view name, const Buffer &sbuf, int data_off, int data_len, CommandBuffer cmd_buf,
                  const Tex2DParams &p, TextureAtlasArray *atlas, eTexLoadStatus *load_status);
    ~TextureRegion();

    TextureRegion(const TextureRegion &rhs) = default;
    TextureRegion(TextureRegion &&rhs) noexcept { (*this) = std::move(rhs); }

    TextureRegion &operator=(TextureRegion &&rhs) noexcept;

    [[nodiscard]] const String &name() const { return name_; }
    [[nodiscard]] const Tex2DParams &params() const { return params_; }
    [[nodiscard]] int pos(int i) const { return texture_pos_[i]; }

    [[nodiscard]] bool ready() const { return ready_; }

    void Init(Span<const uint8_t> data, const Tex2DParams &p, TextureAtlasArray *atlas, eTexLoadStatus *load_status);
    void Init(const Buffer &sbuf, int data_off, int data_len, CommandBuffer cmd_buf, const Tex2DParams &p,
              TextureAtlasArray *atlas, eTexLoadStatus *load_status);
};

typedef StrongRef<TextureRegion> TextureRegionRef;
typedef Storage<TextureRegion> TextureRegionStorage;
} // namespace Ren