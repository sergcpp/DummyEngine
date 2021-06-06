#pragma once

#include <Ren/Texture.h>
#include <Sys/AsyncFileReader.h>

class TextureUpdateFileBuf : public Sys::FileReadBufBase {
    Ren::TextureStageBuf stage_buf_;

  public:
    TextureUpdateFileBuf() { Realloc(24 * 1024 * 1024); }
    ~TextureUpdateFileBuf() override {
        stage_buf_.Free();
        mem_ = nullptr;
    }

    Ren::TextureStageBuf &stage_buf() { return stage_buf_; }

    uint8_t *Alloc(const size_t new_size) override {
        stage_buf_.Alloc(uint32_t(new_size));
        return stage_buf_.mapped_ptr();
    }

    void Free() override {
        stage_buf_.Free();
        mem_ = nullptr;
    }

    Ren::SyncFence fence;
};