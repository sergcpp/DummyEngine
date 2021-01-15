#include "GraphBuilder.h"

#include "../DebugMarker.h"

Ren::ILog *RpBuilder::log() { return ctx_.log(); }

RpResource RpBuilder::CreateBuffer(const char *name, const RpBufDesc &desc) {
    const uint32_t *buf_index = name_to_buffer_.Find(name);
    if (buf_index) {
        const RpAllocBuf &buf = buffers_[*buf_index];
        assert(buf.ref->type() == desc.type && buf.ref->size() == desc.size &&
               buf.ref->access() == desc.access && buf.ref->freq() == desc.freq);
        return RpResource{eRpResType::Buffer, buf._generation, *buf_index};
    }

    RpAllocBuf new_buf;
    new_buf.read_count = 0;
    new_buf.write_count = 0;
    new_buf.ref = ctx_.CreateBuffer(name, desc.type, desc.access, desc.freq, desc.size);

    RpResource ret;
    ret.type = eRpResType::Buffer;
    ret.index = buffers_.emplace(new_buf);

    name_to_buffer_[name] = ret.index;

    return ret;
}

RpResource RpBuilder::CreateTexture(const char *name, const Ren::Tex2DParams &p) {
    const uint32_t *tex_index = name_to_texture_.Find(name);
    if (tex_index) {
        const RpAllocTex &tex = textures_[*tex_index];
        assert(tex.ref->params() == p);
        return RpResource{eRpResType::Texture, tex._generation, *tex_index};
    }

    RpAllocTex new_tex;
    new_tex.read_count = 0;
    new_tex.write_count = 0;

    Ren::eTexLoadStatus status;
    new_tex.ref = ctx_.LoadTexture2D(name, p, &status);
    assert(status == Ren::eTexLoadStatus::TexCreatedDefault ||
           status == Ren::eTexLoadStatus::TexFound ||
           status == Ren::eTexLoadStatus::TexFoundReinitialized);

    RpResource ret;
    ret.type = eRpResType::Texture;
    ret.index = textures_.emplace(new_tex);

    name_to_texture_[name] = ret.index;

    return ret;
}

RpResource RpBuilder::ReadBuffer(RpResource handle) {
    assert(handle.type == eRpResType::Buffer);

    RpAllocBuf &buf = buffers_[handle.index];
    const uint16_t gen = buf._generation;

    ++buf.read_count;

    return RpResource{eRpResType::Buffer, gen, handle.index};
}

RpResource RpBuilder::ReadBuffer(const char *name) {
    const uint32_t *buf_index = name_to_buffer_.Find(name);
    assert(buf_index && "Buffer does not exist!");

    RpAllocBuf &buf = buffers_[*buf_index];
    const uint16_t gen = buf._generation;

    ++buf.read_count;

    return RpResource{eRpResType::Buffer, gen, *buf_index};
}

RpResource RpBuilder::ReadTexture(RpResource handle) {
    assert(handle.type == eRpResType::Texture);

    RpAllocTex &tex = textures_[handle.index];
    const uint16_t gen = tex._generation;

    ++tex.read_count;

    return RpResource{eRpResType::Texture, gen, handle.index};
}

RpResource RpBuilder::WriteBuffer(RpResource handle, RenderPassBase &pass) {
    assert(handle.type == eRpResType::Buffer);

    RpAllocBuf &buf = buffers_[handle.index];
    const uint16_t gen = buf._generation;

    ++buf.write_count;
    ++pass.ref_count_;

    return RpResource{eRpResType::Buffer, gen, handle.index};
}

RpResource RpBuilder::WriteTexture(RpResource handle, RenderPassBase &pass) {
    assert(handle.type == eRpResType::Texture);

    RpAllocTex &tex = textures_[handle.index];
    const uint16_t gen = tex._generation;

    ++tex.write_count;
    ++pass.ref_count_;

    return RpResource{eRpResType::Texture, gen, handle.index};
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
    ++buf.write_count;
    return buf;
}

RpAllocTex &RpBuilder::GetWriteTexture(RpResource handle) {
    assert(handle.type == eRpResType::Texture);
    RpAllocTex &tex = textures_.at(handle.index);
    assert(tex._generation == handle._generation);
    ++tex.write_count;
    return tex;
}

void RpBuilder::Reset() {
    // reset counters
    for (RpAllocBuf &buf : buffers_) {
        buf._generation = 0;
    }
}

void RpBuilder::Compile(RenderPassBase *first_pass) {
    struct {
        RenderPassBase *owner;
        RpResource resource;
    } stack[32] = {};
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
                if (--buffers_[el.owner->input_[i].index].read_count == 0) {
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
        cur_pass = cur_pass->p_next;
    }
}