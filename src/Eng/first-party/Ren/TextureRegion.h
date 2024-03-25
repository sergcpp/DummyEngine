#pragma once

#include "Span.h"
#include "Storage.h"
#include "TextureParams.h"

namespace Ren {
class Buffer;
class TextureAtlasArray;

class TextureRegion : public Ren::RefCounter {
    Ren::String name_;
    TextureAtlasArray *atlas_ = nullptr;
    int texture_pos_[3] = {};
    Tex2DParams params_;
    bool ready_ = false;

    void InitFromTGAFile(Span<const uint8_t> data, Buffer &stage_buf, void *cmd_buf, const Tex2DParams &p,
                         Ren::TextureAtlasArray *atlas);
    void InitFromPNGFile(Span<const uint8_t> data, Buffer &stage_buf, void *cmd_buf, const Tex2DParams &p,
                         Ren::TextureAtlasArray *atlas);

    void InitFromRAWData(const Buffer &sbuf, int data_off, int data_len, void *cmd_buf, const Tex2DParams &p,
                         Ren::TextureAtlasArray *atlas);

  public:
    TextureRegion() = default;
    TextureRegion(std::string_view name, TextureAtlasArray *atlas, const int texture_pos[3]);
    TextureRegion(std::string_view name, Span<const uint8_t> data, Buffer &stage_buf, void *cmd_buf,
                  const Tex2DParams &p, Ren::TextureAtlasArray *atlas, eTexLoadStatus *load_status);
    TextureRegion(std::string_view name, const Buffer &sbuf, int data_off, int data_len, void *cmd_buf,
                  const Tex2DParams &p, Ren::TextureAtlasArray *atlas, eTexLoadStatus *load_status);
    ~TextureRegion();

    TextureRegion(const TextureRegion &rhs) = default;
    TextureRegion(TextureRegion &&rhs) noexcept { (*this) = std::move(rhs); }

    TextureRegion &operator=(TextureRegion &&rhs) noexcept;

    const Ren::String &name() const { return name_; }
    const Tex2DParams &params() const { return params_; }
    int pos(int i) const { return texture_pos_[i]; }

    bool ready() const { return ready_; }

    void Init(Span<const uint8_t> data, Buffer &stage_buf, void *cmd_buf, const Tex2DParams &p,
              Ren::TextureAtlasArray *atlas, eTexLoadStatus *load_status);
    void Init(const Buffer &sbuf, int data_off, int data_len, void *cmd_buf, const Tex2DParams &p,
              Ren::TextureAtlasArray *atlas, eTexLoadStatus *load_status);
};

typedef Ren::StrongRef<TextureRegion> TextureRegionRef;
typedef Ren::Storage<TextureRegion> TextureRegionStorage;
} // namespace Ren