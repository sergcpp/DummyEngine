#include "TextureRegion.h"

#include "TextureAtlas.h"
#include "Utils.h"

#include "stb/stb_image.h"

Ren::TextureRegion::TextureRegion(const char *name, TextureAtlasArray *atlas, const int texture_pos[3])
    : name_(name), atlas_(atlas) {
    memcpy(texture_pos_, texture_pos, 3 * sizeof(int));
}

Ren::TextureRegion::TextureRegion(const char *name, const void *data, int size, Buffer &stage_buf, void *cmd_buf,
                                  const Tex2DParams &p, Ren::TextureAtlasArray *atlas, eTexLoadStatus *load_status)
    : name_(name) {
    Init(data, size, stage_buf, cmd_buf, p, atlas, load_status);
}

Ren::TextureRegion::TextureRegion(const char *name, const Buffer &sbuf, int data_off, int data_len, void *cmd_buf,
                                  const Tex2DParams &p, TextureAtlasArray *atlas, eTexLoadStatus *load_status)
    : name_(name) {
    Init(sbuf, data_off, data_len, cmd_buf, p, atlas, load_status);
}

Ren::TextureRegion::~TextureRegion() {
    if (atlas_) {
        atlas_->Free(texture_pos_);
    }
}

Ren::TextureRegion &Ren::TextureRegion::operator=(TextureRegion &&rhs) noexcept {
    RefCounter::operator=(std::move(rhs));

    if (atlas_) {
        atlas_->Free(texture_pos_);
    }

    name_ = std::move(rhs.name_);
    atlas_ = std::exchange(rhs.atlas_, nullptr);
    memcpy(texture_pos_, rhs.texture_pos_, 3 * sizeof(int));
    params_ = rhs.params_;
    ready_ = rhs.ready_;

    return (*this);
}

void Ren::TextureRegion::Init(const void *data, int size, Buffer &stage_buf, void *cmd_buf, const Tex2DParams &p,
                              Ren::TextureAtlasArray *atlas, eTexLoadStatus *load_status) {
    if (!data) {
        uint8_t *out_col = stage_buf.Map(BufMapWrite);
        out_col[0] = 0;
        out_col[1] = out_col[2] = out_col[3] = 255;

        stage_buf.FlushMappedRange(0, stage_buf.AlignMapOffset(4));
        stage_buf.Unmap();

        Tex2DParams _p;
        _p.w = _p.h = 1;
        _p.format = eTexFormat::RawRGBA8888;
        InitFromRAWData(stage_buf, 0, 4, cmd_buf, _p, atlas);
        // mark it as not ready
        ready_ = false;
        if (load_status) {
            (*load_status) = eTexLoadStatus::CreatedDefault;
        }
    } else {
        if (atlas_) {
            atlas_->Free(texture_pos_);
        }

        if (name_.EndsWith(".tga") != 0 || name_.EndsWith(".TGA") != 0) {
            InitFromTGAFile(data, size, stage_buf, cmd_buf, p, atlas);
        } else if (name_.EndsWith(".png") != 0 || name_.EndsWith(".PNG") != 0) {
            InitFromPNGFile(data, size, stage_buf, cmd_buf, p, atlas);
        } else {
            assert("Wrong function is used for raw data!");
        }
        ready_ = true;
        if (load_status) {
            (*load_status) = eTexLoadStatus::CreatedFromData;
        }
    }
}

void Ren::TextureRegion::Init(const Buffer &sbuf, int data_off, int data_len, void *cmd_buf, const Tex2DParams &p,
                              Ren::TextureAtlasArray *atlas, eTexLoadStatus *load_status) {
    if (atlas_) {
        atlas_->Free(texture_pos_);
    }

    InitFromRAWData(sbuf, data_off, data_len, cmd_buf, p, atlas);

    ready_ = true;
    if (load_status) {
        (*load_status) = eTexLoadStatus::CreatedFromData;
    }
}

void Ren::TextureRegion::InitFromTGAFile(const void *data, const int /*size*/, Buffer &stage_buf, void *cmd_buf,
                                         const Tex2DParams &p, TextureAtlasArray *atlas) {
    uint8_t *img_stage = stage_buf.Map(BufMapWrite);
    uint32_t img_size = stage_buf.size();

    int w = 0, h = 0;
    eTexFormat format = eTexFormat::Undefined;
    const bool res = ReadTGAFile(data, w, h, format, img_stage, img_size);
    assert(res);
    assert(format == params_.format && "Format conversion is not implemented yet!");

    stage_buf.FlushMappedRange(0, stage_buf.AlignMapOffset(img_size));
    stage_buf.Unmap();

    Tex2DParams _p = p;
    _p.w = w;
    _p.h = h;
    _p.format = format;

    InitFromRAWData(stage_buf, 0, int(img_size), cmd_buf, _p, atlas);
}

void Ren::TextureRegion::InitFromPNGFile(const void *data, const int size, Buffer &stage_buf, void *cmd_buf,
                                         const Tex2DParams &p, TextureAtlasArray *atlas) {
    int w, h, channels;
    unsigned char *const image_data = stbi_load_from_memory((const uint8_t *)data, size, &w, &h, &channels, 0);
    if (image_data) {
        uint8_t *img_stage = stage_buf.Map(BufMapWrite);
        memcpy(img_stage, image_data, w * h * channels);
        stage_buf.FlushMappedRange(0, stage_buf.AlignMapOffset(w * h * channels));
        stage_buf.Unmap();

        Tex2DParams _p = p;
        _p.w = w;
        _p.h = h;
        _p.format = eTexFormat::RawRGBA8888;

        InitFromRAWData(stage_buf, 0, int(w * h * channels), cmd_buf, _p, atlas);

        free(image_data);
    }
}

void Ren::TextureRegion::InitFromRAWData(const Buffer &sbuf, int data_off, int data_len, void *cmd_buf,
                                         const Tex2DParams &p, TextureAtlasArray *atlas) {
    const int res[2] = {p.w, p.h};
    const int node = atlas->Allocate(sbuf, data_off, data_len, cmd_buf, p.format, res, texture_pos_, 1);
    if (node != -1) {
        atlas_ = atlas;
        params_ = p;
    }
}