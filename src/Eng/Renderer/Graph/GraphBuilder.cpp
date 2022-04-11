#include "GraphBuilder.h"

#include <Ren/Context.h>
#include <Ren/DebugMarker.h>

#include "RenderPass.h"

Ren::ILog *RpBuilder::log() { return ctx_.log(); }

RenderPass &RpBuilder::AddPass(const char *name) {
    auto *new_rp = reinterpret_cast<RenderPass *>(alloc_.allocate(sizeof(RenderPass)));
    alloc_.construct(new_rp, int(render_passes_.size()), name, *this);
    render_passes_.emplace_back(new_rp);
    return *render_passes_.back();
}

RenderPass *RpBuilder::FindPass(const char *name) {
    auto it = std::find_if(std::begin(render_passes_), std::end(render_passes_),
                           [name](const RenderPass *pass) { return strcmp(pass->name(), name) == 0; });
    if (it != std::end(render_passes_)) {
        return (*it);
    }
    return nullptr;
}

RpResRef RpBuilder::ReadBuffer(const RpResRef handle, const Ren::eResState desired_state, const Ren::eStageBits stages,
                               RenderPass &pass) {
    assert(handle.type == eRpResType::Buffer);

    RpAllocBuf &buf = buffers_[handle.index];
    const RpResource ret = {eRpResType::Buffer, buf._generation, desired_state, stages, handle.index};

    assert(buf.write_count == handle.write_count);
    ++buf.read_count;

#ifndef NDEBUG
    // Ensure uniqueness
    for (size_t i = 0; i < pass.input_.size(); i++) {
        assert(pass.input_[i].type != eRpResType::Buffer || pass.input_[i].index != ret.index);
    }
#endif
    pass.input_.push_back(ret);

    return ret;
}

RpResRef RpBuilder::ReadBuffer(const Ren::WeakBufferRef &ref, const Ren::eResState desired_state,
                               const Ren::eStageBits stages, RenderPass &pass) {
    RpResource ret;
    ret.type = eRpResType::Buffer;

    const uint16_t *pbuf_index = name_to_buffer_.Find(ref->name().c_str());
    if (!pbuf_index) {
        RpAllocBuf new_buf;
        new_buf.read_count = 0;
        new_buf.write_count = 0;
        new_buf.used_in_stages = Ren::eStageBits::None;
        new_buf.name = ref->name().c_str();
        new_buf.desc = RpBufDesc{ref->type(), ref->size()};

        ret.index = buffers_.emplace(new_buf);
        name_to_buffer_[new_buf.name] = ret.index;
    } else {
        ret.index = *pbuf_index;
    }

    RpAllocBuf &buf = buffers_[ret.index];
    assert(buf.desc.size <= ref->size() && buf.desc.type == ref->type());
    buf.ref = ref;
    ret._generation = buf._generation;
    ret.desired_state = desired_state;
    ret.stages = stages;

    ++buf.read_count;
    ++pass.ref_count_;

#ifndef NDEBUG
    for (size_t i = 0; i < pass.input_.size(); i++) {
        assert(pass.input_[i].type != eRpResType::Buffer || pass.input_[i].index != ret.index);
    }
#endif
    pass.input_.push_back(ret);

    return ret;
}

RpResRef RpBuilder::ReadTexture(const RpResRef handle, const Ren::eResState desired_state, const Ren::eStageBits stages,
                                RenderPass &pass) {
    assert(handle.type == eRpResType::Texture);

    RpAllocTex &tex = textures_[handle.index];
    const RpResource ret = {eRpResType::Texture, tex._generation, desired_state, stages, handle.index};

    assert(tex.write_count == handle.write_count);
    ++tex.read_count;

#ifndef NDEBUG
    for (size_t i = 0; i < pass.input_.size(); i++) {
        assert(pass.input_[i].type != eRpResType::Texture || pass.input_[i].index != ret.index);
    }
#endif
    pass.input_.push_back(ret);

    return ret;
}

RpResRef RpBuilder::ReadTexture(const char *name, const Ren::eResState desired_state, const Ren::eStageBits stages,
                                RenderPass &pass) {
    const uint16_t *tex_index = name_to_texture_.Find(name);
    assert(tex_index && "Texture does not exist!");

    RpAllocTex &tex = textures_[*tex_index];
    const RpResource ret = {eRpResType::Texture, tex._generation, desired_state, stages, *tex_index};

    ++tex.read_count;

#ifndef NDEBUG
    for (size_t i = 0; i < pass.input_.size(); i++) {
        assert(pass.input_[i].type != eRpResType::Texture || pass.input_[i].index != ret.index);
    }
#endif
    pass.input_.push_back(ret);

    return ret;
}

RpResRef RpBuilder::ReadTexture(const Ren::WeakTex2DRef &ref, const Ren::eResState desired_state,
                                const Ren::eStageBits stages, RenderPass &pass) {
    RpResource ret;
    ret.type = eRpResType::Texture;

    const uint16_t *ptex_index = name_to_texture_.Find(ref->name().c_str());
    if (!ptex_index) {
        RpAllocTex new_tex;
        new_tex.read_count = 0;
        new_tex.write_count = 0;
        new_tex.used_in_stages = Ren::eStageBits::None;
        new_tex.name = ref->name().c_str();
        new_tex.desc = ref->params;

        ret.index = textures_.emplace(new_tex);
        name_to_texture_[new_tex.name] = ret.index;
    } else {
        ret.index = *ptex_index;
    }

    RpAllocTex &tex = textures_[ret.index];
    tex.desc = ref->params;
    tex.ref = ref;
    ret._generation = tex._generation;
    ret.desired_state = desired_state;
    ret.stages = stages;

    ++tex.read_count;
    ++pass.ref_count_;

#ifndef NDEBUG
    for (size_t i = 0; i < pass.output_.size(); i++) {
        assert(pass.input_[i].type != eRpResType::Texture || pass.input_[i].index != ret.index);
    }
#endif

    pass.input_.push_back(ret);

    return ret;
}

RpResRef RpBuilder::WriteBuffer(const RpResRef handle, const Ren::eResState desired_state, const Ren::eStageBits stages,
                                RenderPass &pass) {
    assert(handle.type == eRpResType::Buffer);

    RpAllocBuf &buf = buffers_[handle.index];
    buf.written_in_passes.push_back({pass.index_, int16_t(pass.output_.size())});

    auto ret = RpResource{eRpResType::Buffer, buf._generation, desired_state, stages, handle.index};

    assert(buf.write_count == handle.write_count);
    ++buf.write_count;
    ++pass.ref_count_;

#ifndef NDEBUG
    for (size_t i = 0; i < pass.output_.size(); i++) {
        assert(pass.output_[i].type != eRpResType::Buffer || pass.output_[i].index != ret.index);
    }
#endif
    pass.output_.push_back(ret);

    ++ret.write_count;
    return ret;
}

RpResRef RpBuilder::WriteBuffer(const char *name, const RpBufDesc &desc, const Ren::eResState desired_state,
                                const Ren::eStageBits stages, RenderPass &pass) {
    RpResource ret;
    ret.type = eRpResType::Buffer;

    const uint16_t *pbuf_index = name_to_buffer_.Find(name);
    if (!pbuf_index) {
        RpAllocBuf new_buf;
        new_buf.read_count = 0;
        new_buf.write_count = 0;
        new_buf.used_in_stages = Ren::eStageBits::None;
        new_buf.name = name;
        new_buf.desc = desc;

        ret.index = buffers_.emplace(new_buf);
        name_to_buffer_[new_buf.name] = ret.index;
    } else {
        ret.index = *pbuf_index;
    }

    RpAllocBuf &buf = buffers_[ret.index];
    buf.desc = desc;
    buf.written_in_passes.push_back({pass.index_, int16_t(pass.output_.size())});
    ret._generation = buf._generation;
    ret.desired_state = desired_state;
    ret.stages = stages;

    ++buf.write_count;
    ++pass.ref_count_;

#ifndef NDEBUG
    for (size_t i = 0; i < pass.output_.size(); i++) {
        assert(pass.output_[i].type != eRpResType::Buffer || pass.output_[i].index != ret.index);
    }
#endif
    pass.output_.push_back(ret);

    ++ret.write_count;
    return ret;
}

RpResRef RpBuilder::WriteBuffer(const Ren::WeakBufferRef &ref, const Ren::eResState desired_state,
                                const Ren::eStageBits stages, RenderPass &pass) {
    RpResource ret;
    ret.type = eRpResType::Buffer;

    const uint16_t *pbuf_index = name_to_buffer_.Find(ref->name().c_str());
    if (!pbuf_index) {
        RpAllocBuf new_buf;
        new_buf.read_count = 0;
        new_buf.write_count = 0;
        new_buf.used_in_stages = Ren::eStageBits::None;
        new_buf.name = ref->name().c_str();
        new_buf.desc = RpBufDesc{ref->type(), ref->size()};

        ret.index = buffers_.emplace(new_buf);
        name_to_buffer_[new_buf.name] = ret.index;
    } else {
        ret.index = *pbuf_index;
    }

    RpAllocBuf &buf = buffers_[ret.index];
    assert(buf.desc.size == ref->size() && buf.desc.type == ref->type());
    buf.ref = ref;
    ret._generation = buf._generation;
    ret.desired_state = desired_state;
    ret.stages = stages;

    buf.written_in_passes.push_back({pass.index_, int16_t(pass.output_.size())});
    ++buf.write_count;
    ++pass.ref_count_;

#ifndef NDEBUG
    for (size_t i = 0; i < pass.output_.size(); i++) {
        assert(pass.output_[i].type != eRpResType::Buffer || pass.output_[i].index != ret.index);
    }
#endif
    pass.output_.push_back(ret);

    ++ret.write_count;
    return ret;
}

RpResRef RpBuilder::WriteTexture(const RpResRef handle, const Ren::eResState desired_state,
                                 const Ren::eStageBits stages, RenderPass &pass) {
    assert(handle.type == eRpResType::Texture);

    RpAllocTex &tex = textures_[handle.index];
    auto ret = RpResource{eRpResType::Texture, tex._generation, desired_state, stages, handle.index};

    assert(tex.write_count == handle.write_count);
    tex.written_in_passes.push_back({pass.index_, int16_t(pass.output_.size())});
    ++tex.write_count;
    ++pass.ref_count_;

#ifndef NDEBUG
    for (size_t i = 0; i < pass.output_.size(); i++) {
        assert(pass.output_[i].type != eRpResType::Texture || pass.output_[i].index != ret.index);
    }
#endif
    pass.output_.push_back(ret);

    ++ret.write_count;
    return ret;
}

RpResRef RpBuilder::WriteTexture(const char *name, const Ren::eResState desired_state, const Ren::eStageBits stages,
                                 RenderPass &pass) {
    const uint16_t *tex_index = name_to_texture_.Find(name);
    assert(tex_index && "Texture does not exist!");

    RpAllocTex &tex = textures_[*tex_index];
    auto ret = RpResource{eRpResType::Texture, tex._generation, desired_state, stages, *tex_index};

    tex.written_in_passes.push_back({pass.index_, int16_t(pass.output_.size())});
    ++tex.write_count;
    ++pass.ref_count_;

#ifndef NDEBUG
    for (size_t i = 0; i < pass.output_.size(); i++) {
        assert(pass.output_[i].type != eRpResType::Texture || pass.output_[i].index != ret.index);
    }
#endif
    pass.output_.push_back(ret);

    ++ret.write_count;
    return ret;
}

RpResRef RpBuilder::WriteTexture(const char *name, const Ren::Tex2DParams &p, const Ren::eResState desired_state,
                                 const Ren::eStageBits stages, RenderPass &pass) {
    RpResource ret;
    ret.type = eRpResType::Texture;

    const uint16_t *ptex_index = name_to_texture_.Find(name);
    if (!ptex_index) {
        RpAllocTex new_tex;
        new_tex.read_count = 0;
        new_tex.write_count = 0;
        new_tex.used_in_stages = Ren::eStageBits::None;
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
    ret.desired_state = desired_state;
    ret.stages = stages;

    tex.written_in_passes.push_back({pass.index_, int16_t(pass.output_.size())});
    ++tex.write_count;
    ++pass.ref_count_;

#ifndef NDEBUG
    for (size_t i = 0; i < pass.output_.size(); i++) {
        assert(pass.output_[i].type != eRpResType::Texture || pass.output_[i].index != ret.index);
    }
#endif
    pass.output_.push_back(ret);

    ++ret.write_count;
    return ret;
}

RpResRef RpBuilder::WriteTexture(const Ren::WeakTex2DRef &ref, const Ren::eResState desired_state,
                                 const Ren::eStageBits stages, RenderPass &pass, const int slot_index) {
    RpResource ret;
    ret.type = eRpResType::Texture;

    const uint16_t *ptex_index = name_to_texture_.Find(ref->name().c_str());
    if (!ptex_index) {
        RpAllocTex new_tex;
        new_tex.read_count = 0;
        new_tex.write_count = 0;
        new_tex.used_in_stages = Ren::eStageBits::None;
        new_tex.name = ref->name().c_str();
        new_tex.desc = ref->params;

        ret.index = textures_.emplace(new_tex);
        name_to_texture_[new_tex.name] = ret.index;
    } else {
        ret.index = *ptex_index;
    }

    RpAllocTex &tex = textures_[ret.index];
    tex.desc = ref->params;
    tex.ref = ref;
    ret._generation = tex._generation;
    ret.desired_state = desired_state;
    ret.stages = stages;

    tex.written_in_passes.push_back({pass.index_, int16_t(pass.output_.size())});
    ++tex.write_count;
    ++pass.ref_count_;

    if (slot_index == -1) {
#ifndef NDEBUG
        for (size_t i = 0; i < pass.output_.size(); i++) {
            assert(pass.output_[i].type != eRpResType::Texture || pass.output_[i].index != ret.index);
        }
#endif
        // Add new output
        pass.output_.push_back(ret);
    } else if (pass.output_[slot_index]) {
        // Replace existing output
        RpAllocTex &prev_tex = textures_[pass.output_[slot_index].index];
        --prev_tex.write_count;
        --pass.ref_count_;
        for (size_t i = 0; i < prev_tex.written_in_passes.size();) {
            if (prev_tex.written_in_passes[i].pass_index == pass.index_) {
                prev_tex.written_in_passes.erase(prev_tex.written_in_passes.begin() + i);
            } else {
                ++i;
            }
        }
        if (pass.output_[slot_index].index == ret.index) {
            --ret.write_count;
        }
        pass.output_[slot_index] = ret;
    }

    ++ret.write_count;
    return ret;
}

RpAllocBuf &RpBuilder::GetReadBuffer(const RpResRef handle) {
    assert(handle.type == eRpResType::Buffer);
    RpAllocBuf &buf = buffers_.at(handle.index);
    assert(buf.write_count == handle.write_count);
    ++buf.read_count;
    return buf;
}

RpAllocTex &RpBuilder::GetReadTexture(const RpResRef handle) {
    assert(handle.type == eRpResType::Texture);
    RpAllocTex &tex = textures_.at(handle.index);
    assert(tex.write_count == handle.write_count);
    // assert(tex.ref->resource_state == handle.desired_state);
    ++tex.read_count;
    return tex;
}

RpAllocBuf &RpBuilder::GetWriteBuffer(const RpResRef handle) {
    assert(handle.type == eRpResType::Buffer);
    RpAllocBuf &buf = buffers_.at(handle.index);
    assert(buf.write_count + 1 == handle.write_count);
    // assert(buf.ref->resource_state == handle.desired_state);
    ++buf.write_count;
    return buf;
}

RpAllocTex &RpBuilder::GetWriteTexture(const RpResRef handle) {
    assert(handle.type == eRpResType::Texture);
    RpAllocTex &tex = textures_.at(handle.index);
    assert(tex.write_count + 1 == handle.write_count);
    // assert(tex.ref->resource_state == handle.desired_state);
    ++tex.write_count;
    return tex;
}

void RpBuilder::AllocateNeededResources(RenderPass &pass) {
    for (size_t i = 0; i < pass.output_.size(); i++) {
        RpResource res = pass.output_[i];
        if (res.type == eRpResType::Buffer) {
            RpAllocBuf &buf = buffers_.at(res.index);
            if (!buf.ref || buf.desc.type != buf.ref->type() || buf.desc.size > buf.ref->size()) {
                buf.strong_ref = ctx_.LoadBuffer(buf.name.c_str(), buf.desc.type, buf.desc.size);
                buf.ref = buf.strong_ref;
            }
        } else if (res.type == eRpResType::Texture) {
            RpAllocTex &tex = textures_.at(res.index);
            if (!tex.ref || tex.desc != tex.ref->params) {
#ifndef NDEBUG
                if (tex.ref && tex.desc.usage != tex.ref->params.usage) {
                    ctx_.log()->Error("Conflicting usage flags detected: %s", tex.name.c_str());
                }
#endif
                Ren::eTexLoadStatus status;
                tex.strong_ref = ctx_.LoadTexture2D(tex.name.c_str(), tex.desc, ctx_.default_mem_allocs(), &status);
                tex.ref = tex.strong_ref;
                assert(status == Ren::eTexLoadStatus::CreatedDefault || status == Ren::eTexLoadStatus::Found ||
                       status == Ren::eTexLoadStatus::Reinitialized);
            }
        }
    }
}

void RpBuilder::Reset() {
    for (int i = int(render_passes_.size()) - 1; i >= 0; --i) {
        alloc_.destroy(render_passes_[i]);
    }
    render_passes_.clear();
    render_pass_data_.clear();
    alloc_.Reset();

    //
    // Reset resources
    //
    for (RpAllocBuf &buf : buffers_) {
        buf._generation = 0;
        buf.used_in_stages = Ren::eStageBits::None;
        buf.written_in_passes.clear();
        if (buf.ref) {
            buf.used_in_stages = Ren::StageBitsForState(buf.ref->resource_state);
        }
    }
    for (RpAllocTex &tex : textures_) {
        tex._generation = 0;
        tex.used_in_stages = Ren::eStageBits::None;
        tex.written_in_passes.clear();
        if (tex.ref) {
            tex.used_in_stages = Ren::StageBitsForState(tex.ref->resource_state);
        }
    }
    temp_samplers.clear();
}

int16_t RpBuilder::FindPreviousWrittenInPass(const RpResRef handle) {
    Ren::SmallVectorImpl<rp_write_pass_t> *written_in_passes = nullptr;
    if (handle.type == eRpResType::Buffer) {
        written_in_passes = &buffers_[handle.index].written_in_passes;
    } else if (handle.type == eRpResType::Texture) {
        written_in_passes = &textures_[handle.index].written_in_passes;
    }

    for (const rp_write_pass_t i : *written_in_passes) {
        RenderPass *pass = render_passes_[i.pass_index];
        assert(pass->output_[i.slot_index].type == handle.type && pass->output_[i.slot_index].index == handle.index);
        if (pass->output_[i.slot_index].write_count == handle.write_count - 1) {
            return i.pass_index;
        }
    }
    return -1;
}

bool RpBuilder::DependsOn_r(const int16_t dst_pass, const int16_t src_pass) {
    if (dst_pass == src_pass) {
        return true;
    }
    for (const int16_t dep : render_passes_[dst_pass]->depends_on_passes_) {
        if (DependsOn_r(dep, src_pass)) {
            return true;
        }
    }
    return false;
}

void RpBuilder::TraversePassDependencies(const RenderPass *pass, const int recursion_depth,
                                         std::vector<RenderPass *> &out_pass_stack) {
    assert(recursion_depth <= render_passes_.size());
    Ren::SmallVector<int16_t, 32> written_in_passes;
    for (size_t i = 0; i < pass->input_.size(); i++) {
        const int16_t prev_pass = FindPreviousWrittenInPass(pass->input_[i]);
        if (prev_pass != -1) {
            const auto it = std::find(std::begin(written_in_passes), std::end(written_in_passes), prev_pass);
            if (it == std::end(written_in_passes)) {
                written_in_passes.push_back(prev_pass);
            }
        }
    }

    for (size_t i = 0; i < pass->output_.size(); i++) {
        if (pass->output_[i].desired_state != Ren::eResState::RenderTarget &&
            pass->output_[i].desired_state != Ren::eResState::DepthWrite &&
            pass->output_[i].desired_state != Ren::eResState::UnorderedAccess) {
            continue;
        }

        const int16_t prev_pass = FindPreviousWrittenInPass(pass->output_[i]);
        if (prev_pass != -1) {
            const auto it = std::find(std::begin(written_in_passes), std::end(written_in_passes), prev_pass);
            if (it == std::end(written_in_passes)) {
                written_in_passes.push_back(prev_pass);
            }
        }
    }

    for (const int16_t i : written_in_passes) {
        RenderPass *_pass = render_passes_[i];
        assert(_pass != pass);
        const auto it =
            std::find(std::begin(pass->depends_on_passes_), std::end(pass->depends_on_passes_), _pass->index_);
        if (it == std::end(pass->depends_on_passes_)) {
            pass->depends_on_passes_.push_back(_pass->index_);
        }
        out_pass_stack.push_back(_pass);
        TraversePassDependencies(_pass, recursion_depth + 1, out_pass_stack);
    }
}

void RpBuilder::BuildResourceLinkedLists() {
    std::vector<RpResource *> all_resources;

    for (RenderPass *cur_pass : reordered_render_passes_) {
        for (size_t i = 0; i < cur_pass->input_.size(); i++) {
            RpResource *r = &cur_pass->input_[i];

            auto it = std::lower_bound(std::begin(all_resources), std::end(all_resources), r,
                                       [](const RpResource *lhs, const RpResource *rhs) {
                                           return RpResource::LessThanTypeAndIndex(*lhs, *rhs);
                                       });
            if (it != std::end(all_resources) && !RpResource::LessThanTypeAndIndex(*r, **it)) {
                (*it)->next_use = r;
                (*it) = r;
            } else {
                r->next_use = nullptr;
                all_resources.insert(it, r);
            }
        }

        for (size_t i = 0; i < cur_pass->output_.size(); i++) {
            RpResource *r = &cur_pass->output_[i];

            auto it = std::lower_bound(std::begin(all_resources), std::end(all_resources), r,
                                       [](const RpResource *lhs, const RpResource *rhs) {
                                           return RpResource::LessThanTypeAndIndex(*lhs, *rhs);
                                       });
            if (it != std::end(all_resources) && !RpResource::LessThanTypeAndIndex(*r, **it)) {
                (*it)->next_use = r;
                (*it) = r;
            } else {
                r->next_use = nullptr;
                all_resources.insert(it, r);
            }
        }
    }
}

void RpBuilder::Compile(const RpResRef backbuffer_sources[], int backbuffer_sources_count) {
#if 0 // reference-counted culling
    struct {
        RenderPass *owner;
        RpResRef resource;
    } stack[128] = {};
    uint32_t stack_size = 0;

    { // gather unreferenced resources
        for (RenderPass *cur_pass : render_passes_) {
            for (size_t i = 0; i < cur_pass->output_.size(); i++) {
                if (cur_pass->output_[i].read_count == 0) {
                    stack[stack_size++] = {cur_pass, cur_pass->output_[i]};
                }
            }
        }
    }

    while (stack_size) {
        auto el = stack[--stack_size];
        if (--el.owner->ref_count_ == 0) {
            for (size_t i = 0; i < el.owner->input_.size(); i++) {
                if (el.owner->input_[i].type == eRpResType::Buffer &&
                    --buffers_[el.owner->input_[i].index].read_count == 0) {
                    stack[stack_size++] = {el.owner, el.owner->input_[i]};
                } else if (el.owner->input_[i].type == eRpResType::Texture &&
                           --textures_[el.owner->input_[i].index].read_count == 0) {
                    stack[stack_size++] = {el.owner, el.owner->input_[i]};
                }
            }

            // TODO: remove pass from chain
        }
    }
#endif

    reordered_render_passes_.clear();
    reordered_render_passes_.reserve(render_passes_.size());

    if (backbuffer_sources_count) {
        //
        // Sorting and culling
        //
        Ren::SmallVector<RenderPass *, 32> written_in_passes;
        for (int i = 0; i < backbuffer_sources_count; ++i) {
            const int16_t prev_pass = FindPreviousWrittenInPass(backbuffer_sources[i]);
            if (prev_pass != -1) {
                written_in_passes.push_back(render_passes_[prev_pass]);
            }
        }

        reordered_render_passes_.assign(std::begin(written_in_passes), std::end(written_in_passes));

        for (const RenderPass *pass : written_in_passes) {
            TraversePassDependencies(pass, 0, reordered_render_passes_);
        }

        std::reverse(std::begin(reordered_render_passes_), std::end(reordered_render_passes_));

        int out_index = 0;
        for (int in_index = 0; in_index < int(reordered_render_passes_.size()); ++in_index) {
            if (!reordered_render_passes_[in_index]->visited_) {
                reordered_render_passes_[out_index++] = reordered_render_passes_[in_index];
                reordered_render_passes_[in_index]->visited_ = true;
            }
        }
        reordered_render_passes_.resize(out_index);

        if (!reordered_render_passes_.empty()) {
            std::vector<RenderPass *> scheduled_passes;
            scheduled_passes.reserve(reordered_render_passes_.size());

            // schedule the first pass
            scheduled_passes.push_back(reordered_render_passes_.front());
            reordered_render_passes_.erase(reordered_render_passes_.begin());

            while (!reordered_render_passes_.empty()) {
                int best_ovelap_score = -1;
                int best_candidate = 0;

                for (int i = 0; i < int(reordered_render_passes_.size()); ++i) {
                    int overlap_score = 0;

                    for (int j = int(scheduled_passes.size()) - 1; j >= 0; --j) {
                        if (DependsOn_r(reordered_render_passes_[i]->index_, scheduled_passes[j]->index_)) {
                            break;
                        }
                        ++overlap_score;
                    }

                    if (overlap_score <= best_ovelap_score) {
                        continue;
                    }

                    bool possible_candidate = true;
                    for (int j = 0; j < i; ++j) {
                        if (DependsOn_r(reordered_render_passes_[i]->index_, reordered_render_passes_[j]->index_)) {
                            possible_candidate = false;
                            break;
                        }
                    }

                    for (int j = 0; j < int(reordered_render_passes_.size()) && possible_candidate; ++j) {
                        if (j == i) {
                            continue;
                        }
                        for (const auto &output : reordered_render_passes_[i]->output_) {
                            for (const auto &input : reordered_render_passes_[j]->input_) {
                                if (output.type == input.type && output.index == input.index &&
                                    output.write_count >= input.write_count) {
                                    possible_candidate = false;
                                    break;
                                }
                            }
                            if (!possible_candidate) {
                                break;
                            }
                        }
                    }

                    if (possible_candidate) {
                        best_ovelap_score = overlap_score;
                        best_candidate = i;
                    }
                }

                scheduled_passes.push_back(reordered_render_passes_[best_candidate]);
                reordered_render_passes_.erase(reordered_render_passes_.begin() + best_candidate);
            }

            reordered_render_passes_ = std::move(scheduled_passes);
        }
    } else {
        // Use all passes as is
        reordered_render_passes_.assign(std::begin(render_passes_), std::end(render_passes_));
    }
}

void RpBuilder::Execute() {
    // Reset resources
    for (RpAllocBuf &buf : buffers_) {
        buf._generation = 0;
        buf.used_in_stages = Ren::eStageBits::None;
        if (buf.ref) {
            buf.used_in_stages = Ren::StageBitsForState(buf.ref->resource_state);
        }
    }
    for (RpAllocTex &tex : textures_) {
        tex._generation = 0;
        tex.used_in_stages = Ren::eStageBits::None;
        if (tex.ref) {
            tex.used_in_stages = Ren::StageBitsForState(tex.ref->resource_state);
        }
    }

    BuildResourceLinkedLists();

#if defined(USE_GL_RENDER)
    rast_state_.Apply();
#endif

    pass_timings_[ctx_.backend_frame()].clear();
    // Write timestamp at the beginning of execution
    const int query_beg = ctx_.WriteTimestamp(true);

    for (int i = 0; i < int(reordered_render_passes_.size()); ++i) {
        RenderPass &cur_pass = *reordered_render_passes_[i];

#if !defined(NDEBUG) && defined(USE_GL_RENDER)
        Ren::ResetGLState();
#endif

        Ren::DebugMarker _(ctx_.current_cmd_buf(), cur_pass.name());

        // Start timestamp
        pass_timing_t &pass_interval = pass_timings_[ctx_.backend_frame()].emplace_back();
        pass_interval.name = cur_pass.name();
        pass_interval.query_beg = ctx_.WriteTimestamp(true);

        AllocateNeededResources(cur_pass);
        InsertResourceTransitions(cur_pass);

        cur_pass.Execute(*this);

        // End timestamp
        pass_interval.query_end = ctx_.WriteTimestamp(false);
    }

    // Write timestamp at the end of execution
    pass_timing_t &initial_interval = pass_timings_[ctx_.backend_frame()].emplace_back();
    initial_interval.name = "GRAPH TOTAL";
    initial_interval.query_beg = query_beg;
    initial_interval.query_end = ctx_.WriteTimestamp(false);
}

void RpBuilder::InsertResourceTransitions(RenderPass &pass) {
    auto cmd_buf = reinterpret_cast<VkCommandBuffer>(ctx_.current_cmd_buf());

    Ren::SmallVector<Ren::TransitionInfo, 32> res_transitions;
    Ren::eStageBits src_stages = Ren::eStageBits::None;
    Ren::eStageBits dst_stages = Ren::eStageBits::None;

    for (size_t i = 0; i < pass.input_.size(); i++) {
        const RpResource &res = pass.input_[i];
        HandleResourceTransition(res, res_transitions, src_stages, dst_stages);
    }

    for (size_t i = 0; i < pass.output_.size(); i++) {
        const RpResource &res = pass.output_[i];
        HandleResourceTransition(res, res_transitions, src_stages, dst_stages);
    }

    Ren::TransitionResourceStates(cmd_buf, src_stages, dst_stages, res_transitions.cdata(),
                                  int(res_transitions.size()));
}

void RpBuilder::CheckResourceStates(RenderPass &pass) {
    for (size_t i = 0; i < pass.input_.size(); i++) {
        const RpResource &res = pass.input_[i];
        if (res.type == eRpResType::Buffer) {
            const RpAllocBuf &buf = buffers_[res.index];
            assert(buf.ref->resource_state == res.desired_state && "Buffer is in unexpected state!");
        } else if (res.type == eRpResType::Texture) {
            const RpAllocTex &tex = textures_[res.index];
            assert(tex.ref->resource_state == res.desired_state && "Texture is in unexpected state!");
        }
    }
    for (size_t i = 0; i < pass.output_.size(); i++) {
        const RpResource &res = pass.output_[i];
        if (res.type == eRpResType::Buffer) {
            const RpAllocBuf &buf = buffers_[res.index];
            assert(buf.ref->resource_state == res.desired_state && "Buffer is in unexpected state!");
        } else if (res.type == eRpResType::Texture) {
            const RpAllocTex &tex = textures_[res.index];
            assert(tex.ref->resource_state == res.desired_state && "Texture is in unexpected state!");
        }
    }
}

void RpBuilder::HandleResourceTransition(const RpResource &res,
                                         Ren::SmallVectorImpl<Ren::TransitionInfo> &res_transitions,
                                         Ren::eStageBits &src_stages, Ren::eStageBits &dst_stages) {
    for (const RpResource *next_res = res.next_use; next_res; next_res = next_res->next_use) {
        if (next_res->desired_state != res.desired_state ||
            next_res->desired_state == Ren::eResState::UnorderedAccess) {
            break;
        }
        dst_stages |= next_res->stages;
    }

    if (res.type == eRpResType::Buffer) {
        RpAllocBuf &buf = buffers_.at(res.index);
        if (buf.ref->resource_state != res.desired_state ||
            buf.ref->resource_state == Ren::eResState::UnorderedAccess) {
            src_stages |= buf.used_in_stages;
            dst_stages |= res.stages;
            buf.used_in_stages = Ren::eStageBits::None;
            res_transitions.emplace_back(buf.ref.get(), res.desired_state);
        }
        buf.used_in_stages |= res.stages;
    } else if (res.type == eRpResType::Texture) {
        RpAllocTex &tex = textures_.at(res.index);
        if (tex.ref->resource_state != res.desired_state ||
            tex.ref->resource_state == Ren::eResState::UnorderedAccess) {
            src_stages |= tex.used_in_stages;
            dst_stages |= res.stages;
            tex.used_in_stages = Ren::eStageBits::None;
            res_transitions.emplace_back(tex.ref.get(), res.desired_state);
        }
        tex.used_in_stages |= res.stages;
    }
}