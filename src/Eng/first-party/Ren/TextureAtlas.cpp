#include "TextureAtlas.h"

Ren::TextureAtlasArray::TextureAtlasArray(ApiContext *api_ctx, const std::string_view name, const int w, const int h,
                                          const int layer_count, const int mip_count, const eTexFormat format,
                                          const eTexFilter filter)
    : Texture2DArray(api_ctx, name, w, h, layer_count, mip_count, format, filter,
                     Bitmask(eTexUsage::Transfer) | eTexUsage::RenderTarget | eTexUsage::Sampled) {
    splitters_.resize(layer_count, TextureSplitter{w, h});
}

Ren::TextureAtlasArray &Ren::TextureAtlasArray::operator=(TextureAtlasArray &&rhs) noexcept {
    if (this == &rhs) {
        return (*this);
    }

    Texture2DArray::operator=(std::move(rhs));

    splitters_ = std::move(rhs.splitters_);

    return (*this);
}

int Ren::TextureAtlasArray::Allocate(const Buffer &sbuf, const int data_off, const int data_len, CommandBuffer cmd_buf,
                                     const eTexFormat format, const int res[2], int out_pos[3], const int border) {
    const int alloc_res[] = {res[0] < splitters_[0].resx() ? res[0] + border : res[0],
                             res[1] < splitters_[1].resy() ? res[1] + border : res[1]};

    for (int i = 0; i < layer_count_; i++) {
        const int index = splitters_[i].Allocate(alloc_res, out_pos);
        if (index != -1) {
            out_pos[2] = i;

            SetSubImage(0, i, out_pos[0], out_pos[1], res[0], res[1], format, sbuf, data_off, data_len, cmd_buf);

            return index;
        }
    }

    return -1;
}

bool Ren::TextureAtlasArray::Free(const int pos[3]) {
    // TODO: fill with black in debug
    return splitters_[pos[2]].Free(pos);
}
