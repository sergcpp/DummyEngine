#include "GraphBuilder.h"

#include "../DebugMarker.h"

Ren::ILog *Graph::RpBuilder::log() { return ctx_.log(); }

Graph::ResourceHandle Graph::RpBuilder::CreateBuffer(const char *name,
                                                     const BufferDesc &desc) {
    uint32_t *buf_index = name_to_buffer_.Find(name);
    if (buf_index) {
        const AllocatedBuffer &buf = buffers_[*buf_index];
        return ResourceHandle{eResourceType::Buffer, buf._generation, *buf_index};
    }

    AllocatedBuffer new_buf;
    new_buf.read_count = 0;
    new_buf.write_count = 0;
    new_buf.ref = ctx_.CreateBuffer(name, desc.type, desc.access, desc.freq, desc.size);

    ResourceHandle ret;
    ret.type = eResourceType::Buffer;
    ret.index = buffers_.emplace(new_buf);

    name_to_buffer_[name] = ret.index;

    return ret;
}

Graph::ResourceHandle Graph::RpBuilder::ReadBuffer(ResourceHandle handle) {
    assert(handle.type == eResourceType::Buffer);

    AllocatedBuffer &buf = buffers_[handle.index];
    const uint16_t gen = buf._generation;
    
    ++buf.read_count;

    return ResourceHandle{eResourceType::Buffer, gen, handle.index};
}

Graph::ResourceHandle Graph::RpBuilder::WriteBuffer(ResourceHandle handle,
                                                    RenderPassBase &pass) {
    assert(handle.type == eResourceType::Buffer);

    AllocatedBuffer &buf = buffers_[handle.index];
    const uint16_t gen = buf._generation;

    ++buf.write_count;
    ++pass.ref_count_;

    return ResourceHandle{eResourceType::Buffer, gen, handle.index};
}

Graph::AllocatedBuffer &Graph::RpBuilder::GetReadBuffer(ResourceHandle handle) {
    assert(handle.type == eResourceType::Buffer);
    AllocatedBuffer &buf = buffers_.at(handle.index);
    assert(buf._generation == handle._generation);
    ++buf.read_count;
    return buf;
}

Graph::AllocatedBuffer &Graph::RpBuilder::GetWriteBuffer(ResourceHandle handle) {
    assert(handle.type == eResourceType::Buffer);
    AllocatedBuffer &buf = buffers_.at(handle.index);
    assert(buf._generation == handle._generation);
    ++buf.write_count;
    return buf;
}

void Graph::RpBuilder::Reset() {
    // reset counters
    for (AllocatedBuffer &buf : buffers_) {
        buf._generation = 0;
    }
}

void Graph::RpBuilder::Compile(RenderPassBase *first_pass) {
    struct {
        RenderPassBase *owner;
        ResourceHandle resource;
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

void Graph::RpBuilder::Execute(RenderPassBase *first_pass) {
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