#include "GraphBuilder.h"

#include <Ren/Context.h>

#include "../DebugMarker.h"

Ren::ILog *RpBuilder::log() { return ctx_.log(); }

RpResource RpBuilder::ReadBuffer(RpResource handle, RenderPassBase &pass) {
    assert(handle.type == eRpResType::Buffer);

    RpAllocBuf &buf = buffers_[handle.index];
    const RpResource ret = {eRpResType::Buffer, buf._generation, handle.index};

    ++buf.read_count;

#ifndef NDEBUG
    for (int i = 0; i < pass.input_count_; i++) {
        assert(pass.input_[i].type != eRpResType::Buffer ||
               pass.input_[i].index != ret.index);
    }
#endif
    pass.input_[pass.input_count_++] = ret;

    return ret;
}

RpResource RpBuilder::ReadBuffer(const char *name, RenderPassBase &pass) {
    const uint32_t *buf_index = name_to_buffer_.Find(name);
    assert(buf_index && "Buffer does not exist!");

    RpAllocBuf &buf = buffers_[*buf_index];
    const RpResource ret = {eRpResType::Buffer, buf._generation, *buf_index};

    ++buf.read_count;

#ifndef NDEBUG
    for (int i = 0; i < pass.input_count_; i++) {
        assert(pass.input_[i].type != eRpResType::Buffer ||
               pass.input_[i].index != ret.index);
    }
#endif
    pass.input_[pass.input_count_++] = ret;

    return ret;
}

RpResource RpBuilder::ReadTexture(RpResource handle, RenderPassBase &pass) {
    assert(handle.type == eRpResType::Texture);

    RpAllocTex &tex = textures_[handle.index];
    const RpResource ret = {eRpResType::Texture, tex._generation, handle.index};

    ++tex.read_count;

#ifndef NDEBUG
    for (int i = 0; i < pass.input_count_; i++) {
        assert(pass.input_[i].type != eRpResType::Texture ||
               pass.input_[i].index != ret.index);
    }
#endif
    pass.input_[pass.input_count_++] = ret;

    return ret;
}

RpResource RpBuilder::ReadTexture(const char *name, RenderPassBase &pass) {
    const uint32_t *tex_index = name_to_texture_.Find(name);
    assert(tex_index && "Texture does not exist!");

    RpAllocTex &tex = textures_[*tex_index];
    const RpResource ret = {eRpResType::Texture, tex._generation, *tex_index};

    ++tex.read_count;

#ifndef NDEBUG
    for (int i = 0; i < pass.input_count_; i++) {
        assert(pass.input_[i].type != eRpResType::Texture ||
               pass.input_[i].index != ret.index);
    }
#endif
    pass.input_[pass.input_count_++] = ret;

    return ret;
}

RpResource RpBuilder::WriteBuffer(RpResource handle, RenderPassBase &pass) {
    assert(handle.type == eRpResType::Buffer);

    RpAllocBuf &buf = buffers_[handle.index];
    const auto ret = RpResource{eRpResType::Buffer, buf._generation, handle.index};

    ++buf.write_count;
    ++pass.ref_count_;

#ifndef NDEBUG
    for (int i = 0; i < pass.output_count_; i++) {
        assert(pass.output_[i].type != eRpResType::Buffer ||
               pass.output_[i].index != ret.index);
    }
#endif
    pass.output_[pass.output_count_++] = ret;

    return ret;
}

RpResource RpBuilder::WriteBuffer(const char *name, RenderPassBase &pass) {
    const uint32_t *buf_index = name_to_buffer_.Find(name);
    assert(buf_index && "Buffer does not exist!");

    RpAllocBuf &buf = buffers_[*buf_index];
    const auto ret = RpResource{eRpResType::Buffer, buf._generation, *buf_index};

    ++buf.write_count;
    ++pass.ref_count_;

#ifndef NDEBUG
    for (int i = 0; i < pass.output_count_; i++) {
        assert(pass.output_[i].type != eRpResType::Buffer ||
               pass.output_[i].index != ret.index);
    }
#endif
    pass.output_[pass.output_count_++] = ret;

    return ret;
}

RpResource RpBuilder::WriteBuffer(const char *name, const RpBufDesc &desc,
                                  RenderPassBase &pass) {
    RpResource ret;
    ret.type = eRpResType::Buffer;

    const uint32_t *pbuf_index = name_to_buffer_.Find(name);
    if (!pbuf_index) {
        RpAllocBuf new_buf;
        new_buf.read_count = 0;
        new_buf.write_count = 0;
        new_buf.name = name;
        new_buf.desc = desc;

        ret.index = buffers_.emplace(new_buf);
        name_to_buffer_[new_buf.name] = ret.index;
    } else {
        ret.index = *pbuf_index;
    }

    RpAllocBuf &buf = buffers_[ret.index];
    assert(buf.desc == desc);
    ret._generation = buf._generation;

    ++buf.write_count;
    ++pass.ref_count_;

#ifndef NDEBUG
    for (int i = 0; i < pass.output_count_; i++) {
        assert(pass.output_[i].type != eRpResType::Buffer ||
               pass.output_[i].index != ret.index);
    }
#endif
    pass.output_[pass.output_count_++] = ret;

    return ret;
}

RpResource RpBuilder::WriteTexture(RpResource handle, RenderPassBase &pass) {
    assert(handle.type == eRpResType::Texture);

    RpAllocTex &tex = textures_[handle.index];
    const uint16_t gen = tex._generation;

    ++tex.write_count;
    ++pass.ref_count_;

    return RpResource{eRpResType::Texture, gen, handle.index};
}

RpResource RpBuilder::WriteTexture(const char *name, RenderPassBase &pass) {
    const uint32_t *tex_index = name_to_texture_.Find(name);
    assert(tex_index && "Buffer does not exist!");

    RpAllocTex &tex = textures_[*tex_index];
    const uint16_t gen = tex._generation;

    ++tex.write_count;
    ++pass.ref_count_;

    return RpResource{eRpResType::Texture, gen, *tex_index};
}

RpResource RpBuilder::WriteTexture(const char *name, const Ren::Tex2DParams &p,
                                   RenderPassBase &pass) {
    RpResource ret;
    ret.type = eRpResType::Texture;

    const uint32_t *ptex_index = name_to_texture_.Find(name);
    if (!ptex_index) {
        RpAllocTex new_tex;
        new_tex.read_count = 0;
        new_tex.write_count = 0;
        new_tex.name = name;
        new_tex.desc = p;

        ret.index = textures_.emplace(new_tex);
        name_to_texture_[new_tex.name] = ret.index;
    } else {
        ret.index = *ptex_index;
    }

    RpAllocTex &tex = textures_[ret.index];
    tex.desc = p;
    ret._generation = tex._generation;

    ++tex.write_count;
    ++pass.ref_count_;

#ifndef NDEBUG
    for (int i = 0; i < pass.output_count_; i++) {
        assert(pass.output_[i].type != eRpResType::Texture ||
               pass.output_[i].index != ret.index);
    }
#endif

    pass.output_[pass.output_count_++] = ret;

    return ret;
}

RpAllocBuf &RpBuilder::GetReadBuffer(RpResource handle) {
    assert(handle.type == eRpResType::Buffer);
    RpAllocBuf &buf = buffers_.at(handle.index);
    assert(buf._generation == handle._generation);
    ++buf.read_count;
    return buf;
}

RpAllocTex &RpBuilder::GetReadTexture(RpResource handle) {
    assert(handle.type == eRpResType::Texture);
    RpAllocTex &tex = textures_.at(handle.index);
    assert(tex._generation == handle._generation);
    ++tex.read_count;
    return tex;
}

RpAllocBuf &RpBuilder::GetWriteBuffer(RpResource handle) {
    assert(handle.type == eRpResType::Buffer);
    RpAllocBuf &buf = buffers_.at(handle.index);
    assert(buf._generation == handle._generation);

    if (!buf.ref) {
        buf.ref = ctx_.CreateBuffer(buf.name.c_str(), buf.desc.type, buf.desc.access,
                                    buf.desc.freq, buf.desc.size);
    }

    ++buf.write_count;
    return buf;
}

RpAllocTex &RpBuilder::GetWriteTexture(RpResource handle) {
    assert(handle.type == eRpResType::Texture);
    RpAllocTex &tex = textures_.at(handle.index);
    assert(tex._generation == handle._generation);

    if (!tex.ref || tex.desc != tex.ref->params()) {
        Ren::eTexLoadStatus status;
        tex.ref = ctx_.LoadTexture2D(tex.name.c_str(), tex.desc, &status);
        assert(status == Ren::eTexLoadStatus::CreatedDefault ||
               status == Ren::eTexLoadStatus::Found ||
               status == Ren::eTexLoadStatus::Reinitialized);
    }

    ++tex.write_count;
    return tex;
}

void RpBuilder::Reset() {
    // reset counters
    for (RpAllocBuf &buf : buffers_) {
        buf._generation = 0;
    }
    for (RpAllocTex &tex : textures_) {
        tex._generation = 0;
    }
    temp_samplers.clear();
}

void RpBuilder::Compile(RenderPassBase *first_pass) {
    struct {
        RenderPassBase *owner;
        RpResource resource;
    } stack[64] = {};
    uint32_t stack_size = 0;

    { // gather unreferenced resources
        RenderPassBase *cur_pass = first_pass;
        while (cur_pass) {
            for (int i = 0; i < cur_pass->output_count_; i++) {
                if (cur_pass->output_[i].read_count == 0) {
                    stack[stack_size++] = {cur_pass, cur_pass->output_[i]};
                }
            }
            cur_pass = cur_pass->p_next;
        }
    }

    while (stack_size) {
        auto el = stack[--stack_size];
        if (--el.owner->ref_count_ == 0) {
            for (int i = 0; i < el.owner->input_count_; i++) {
                if (el.owner->input_[i].type == eRpResType::Buffer &&
                    --buffers_[el.owner->input_[i].index].read_count == 0) {
                    stack[stack_size++] = {el.owner, el.owner->input_[i]};
                } else if (el.owner->input_[i].type == eRpResType::Texture &&
                           --textures_[el.owner->input_[i].index].read_count == 0) {
                    stack[stack_size++] = {el.owner, el.owner->input_[i]};
                }
            }
        }
    }
}

void RpBuilder::Execute(RenderPassBase *first_pass) {
    Reset();

    RenderPassBase *cur_pass = first_pass;
    while (cur_pass) {
#ifndef NDEBUG
        Ren::ResetGLState();
#endif

        DebugMarker _(cur_pass->name());
        cur_pass->Execute(*this);
        cur_pass->input_count_ = cur_pass->output_count_ = 0;
        cur_pass = cur_pass->p_next;
    }
}