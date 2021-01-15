#pragma once

#include "Storage.h"

namespace Ren {
class TextureAtlasArray;

class TextureRegion : public Ren::RefCounter {
    Ren::String         name_;
    TextureAtlasArray   *atlas_ = nullptr;
    int                 texture_pos_[3] = {};
    Tex2DParams     params_;
    bool                ready_ = false;

    void InitFromRAWData(const void *data, int size, const Tex2DParams &p, Ren::TextureAtlasArray *atlas);
    void InitFromTGAFile(const void *data, int size, const Tex2DParams &p, Ren::TextureAtlasArray *atlas);
    void InitFromPNGFile(const void *data, int size, const Tex2DParams &p, Ren::TextureAtlasArray *atlas);
public:
    TextureRegion() = default;
    TextureRegion(const char *name, Ren::TextureAtlasArray *atlas, const int texture_pos[3]);
    TextureRegion(
            const char *name, const void *data, int size, const Tex2DParams &p,
            Ren::TextureAtlasArray *atlas, eTexLoadStatus *load_status);
    ~TextureRegion();

    TextureRegion(const TextureRegion &rhs) = default;
    TextureRegion(TextureRegion &&rhs) noexcept {
        (*this) = std::move(rhs);
    }

    TextureRegion &operator=(TextureRegion &&rhs) noexcept;

    const Ren::String &name() const { return name_; }
    const Tex2DParams &params() const { return params_; }
    int pos(int i) const { return texture_pos_[i]; }

    bool ready() const {
        return ready_;
    }

    void Init(const void *data, int size, const Tex2DParams &p, Ren::TextureAtlasArray *atlas, eTexLoadStatus *load_status);
};

typedef Ren::StrongRef<TextureRegion> TextureRegionRef;
typedef Ren::Storage<TextureRegion> TextureRegionStorage;
}