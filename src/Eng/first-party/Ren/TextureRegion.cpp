#include "TextureRegion.h"

#include "ScopeExit.h"
#include "TextureAtlas.h"
#include "Utils.h"

#include "stb/stb_image.h"

Ren::TextureRegion::TextureRegion(std::string_view name, TextureAtlasArray *atlas, const int texture_pos[3])
    : name_(name), atlas_(atlas) {
    memcpy(texture_pos_, texture_pos, 3 * sizeof(int));
}

Ren::TextureRegion::TextureRegion(std::string_view name, Span<const uint8_t> data, Buffer &stage_buf,
                                  CommandBuffer cmd_buf, const Tex2DParams &p, TextureAtlasArray *atlas,
                                  eTexLoadStatus *load_status)
    : name_(name) {
    Init(data, stage_buf, cmd_buf, p, atlas, load_status);
}

Ren::TextureRegion::TextureRegion(std::string_view name, const Buffer &sbuf, int data_off, int data_len,
                                  CommandBuffer cmd_buf, const Tex2DParams &p, TextureAtlasArray *atlas,
                                  eTexLoadStatus *load_status)
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
    ready_ = std::exchange(rhs.ready_, false);

    return (*this);
}

void Ren::TextureRegion::Init(Span<const uint8_t> data, Buffer &stage_buf, CommandBuffer cmd_buf, const Tex2DParams &p,
                              TextureAtlasArray *atlas, eTexLoadStatus *load_status) {
    if (data.empty()) {
        uint8_t *out_col = stage_buf.Map();
        out_col[0] = 0;
        out_col[1] = out_col[2] = out_col[3] = 255;
        stage_buf.Unmap();

        Tex2DParams _p;
        _p.w = _p.h = 1;
        _p.format = eTexFormat::RawRGBA8888;
        [[maybe_unused]] const bool res = InitFromRAWData(stage_buf, 0, 4, cmd_buf, _p, atlas);
        // mark it as not ready
        ready_ = false;
        (*load_status) = eTexLoadStatus::CreatedDefault;
    } else {
        if (atlas_) {
            atlas_->Free(texture_pos_);
        }

        if (name_.EndsWith(".dds") != 0 || name_.EndsWith(".DDS") != 0) {
            ready_ = InitFromDDSFile(data, stage_buf, cmd_buf, p, atlas);
        } else {
            ready_ = InitFromImageFile(data, stage_buf, cmd_buf, p, atlas);
        }
        (*load_status) = ready_ ? eTexLoadStatus::CreatedFromData : eTexLoadStatus::Error;
    }
}

void Ren::TextureRegion::Init(const Buffer &sbuf, int data_off, int data_len, CommandBuffer cmd_buf,
                              const Tex2DParams &p, TextureAtlasArray *atlas, eTexLoadStatus *load_status) {
    if (atlas_) {
        atlas_->Free(texture_pos_);
    }
    ready_ = InitFromRAWData(sbuf, data_off, data_len, cmd_buf, p, atlas);
    (*load_status) = ready_ ? eTexLoadStatus::CreatedFromData : eTexLoadStatus::Error;
}

bool Ren::TextureRegion::InitFromDDSFile(Span<const uint8_t> data, Buffer &stage_buf, CommandBuffer cmd_buf,
                                         Tex2DParams p, TextureAtlasArray *atlas) {
    uint32_t offset = 0;
    if (data.size() - offset < sizeof(DDSHeader)) {
        return false;
    }

    DDSHeader header;
    memcpy(&header, &data[offset], sizeof(DDSHeader));
    offset += sizeof(DDSHeader);

    ParseDDSHeader(header, &p);
    if (header.sPixelFormat.dwFourCC ==
        ((unsigned('D') << 0u) | (unsigned('X') << 8u) | (unsigned('1') << 16u) | (unsigned('0') << 24u))) {
        if (data.size() - offset < sizeof(DDS_HEADER_DXT10)) {
            return false;
        }

        DDS_HEADER_DXT10 dx10_header = {};
        memcpy(&dx10_header, &data[offset], sizeof(DDS_HEADER_DXT10));
        offset += sizeof(DDS_HEADER_DXT10);

        p.format = TexFormatFromDXGIFormat(dx10_header.dxgiFormat);
    }

    const int img_data_len = GetMipDataLenBytes(p.w, p.h, p.format, p.block);

    if (data.size() - offset < img_data_len) {
        return false;
    }

    { // update staging buffer
        uint8_t *img_stage = stage_buf.Map();
        memcpy(img_stage, &data[offset], img_data_len);
        stage_buf.Unmap();
    }

    return InitFromRAWData(stage_buf, 0, img_data_len, cmd_buf, p, atlas);
}

bool Ren::TextureRegion::InitFromImageFile(Span<const uint8_t> data, Buffer &stage_buf, CommandBuffer cmd_buf,
                                           Tex2DParams p, TextureAtlasArray *atlas) {
    int w, h, channels;
    unsigned char *const image_data = stbi_load_from_memory(data.data(), int(data.size()), &w, &h, &channels, 0);
    if (!image_data) {
        return false;
    }
    SCOPE_EXIT({ free(image_data); })

    { // update staging buffer
        uint8_t *img_stage = stage_buf.Map();
        memcpy(img_stage, image_data, w * h * channels);
        stage_buf.Unmap();
    }

    p.w = w;
    p.h = h;
    p.format = eTexFormat::RawRGBA8888;

    return InitFromRAWData(stage_buf, 0, int(w * h * channels), cmd_buf, p, atlas);
}

bool Ren::TextureRegion::InitFromRAWData(const Buffer &sbuf, int data_off, int data_len, CommandBuffer cmd_buf,
                                         const Tex2DParams &p, TextureAtlasArray *atlas) {
    const int res[2] = {p.w, p.h};
    const int node = atlas->Allocate(sbuf, data_off, data_len, cmd_buf, p.format, res, texture_pos_, 1);
    if (node == -1) {
        return false;
    }
    atlas_ = atlas;
    params_ = p;
    return true;
}