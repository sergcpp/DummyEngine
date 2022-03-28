#include "GraphBuilder.h"

#include <Ren/Context.h>
#include <Ren/DebugMarker.h>

#include "RenderPass.h"

Ren::ILog *RpBuilder::log() { return ctx_.log(); }

RenderPass &RpBuilder::AddPass(const char *name) {
    auto *new_rp = reinterpret_cast<RenderPass *>(alloc_.allocate(sizeof(RenderPass)));
    alloc_.construct(new_rp, name, *this);
    render_passes_.emplace_back(new_rp);
    return *render_passes_.back();
}

RpResource RpBuilder::ReadBuffer(const RpResource handle, const Ren::eResState desired_state,
                                 const Ren::eStageBits stages, RenderPass &pass) {
    assert(handle.type == eRpResType::Buffer);

    RpAllocBuf &buf = buffers_[handle.index];
    const RpResource ret = {eRpResType::Buffer, buf._generation, desired_state, stages, handle.index};

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

RpResource RpBuilder::ReadBuffer(const Ren::WeakBufferRef &ref, const Ren::eResState desired_state,
                                 const Ren::eStageBits stages, RenderPass &pass) {
    RpResource ret;
    ret.type = eRpResType::Buffer;

    const uint32_t *pbuf_index = name_to_buffer_.Find(ref->name().c_str());
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

RpResource RpBuilder::ReadTexture(const RpResource handle, const Ren::eResState desired_state,
                                  const Ren::eStageBits stages, RenderPass &pass) {
    assert(handle.type == eRpResType::Texture);

    RpAllocTex &tex = textures_[handle.index];
    const RpResource ret = {eRpResType::Texture, tex._generation, desired_state, stages, handle.index};

    ++tex.read_count;

#ifndef NDEBUG
    for (size_t i = 0; i < pass.input_.size(); i++) {
        assert(pass.input_[i].type != eRpResType::Texture || pass.input_[i].index != ret.index);
    }
#endif
    pass.input_.push_back(ret);

    return ret;
}

RpResource RpBuilder::ReadTexture(const char *name, const Ren::eResState desired_state, const Ren::eStageBits stages,
                                  RenderPass &pass) {
    const uint32_t *tex_index = name_to_texture_.Find(name);
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

RpResource RpBuilder::ReadTexture(const Ren::WeakTex2DRef &ref, const Ren::eResState desired_state,
                                  const Ren::eStageBits stages, RenderPass &pass) {
    RpResource ret;
    ret.type = eRpResType::Texture;

    const uint32_t *ptex_index = name_to_texture_.Find(ref->name().c_str());
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

RpResource RpBuilder::WriteBuffer(const RpResource handle, const Ren::eResState desired_state,
                                  const Ren::eStageBits stages, RenderPass &pass) {
    assert(handle.type == eRpResType::Buffer);

    RpAllocBuf &buf = buffers_[handle.index];
    const auto ret = RpResource{eRpResType::Buffer, buf._generation, desired_state, stages, handle.index};

    ++buf.write_count;
    ++pass.ref_count_;

#ifndef NDEBUG
    for (size_t i = 0; i < pass.output_.size(); i++) {
        assert(pass.output_[i].type != eRpResType::Buffer || pass.output_[i].index != ret.index);
    }
#endif
    pass.output_.push_back(ret);

    return ret;
}

RpResource RpBuilder::WriteBuffer(const char *name, const RpBufDesc &desc, const Ren::eResState desired_state,
                                  const Ren::eStageBits stages, RenderPass &pass) {
    RpResource ret;
    ret.type = eRpResType::Buffer;

    const uint32_t *pbuf_index = name_to_buffer_.Find(name);
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

    return ret;
}

RpResource RpBuilder::WriteBuffer(const Ren::WeakBufferRef &ref, const Ren::eResState desired_state,
                                  const Ren::eStageBits stages, RenderPass &pass) {
    RpResource ret;
    ret.type = eRpResType::Buffer;

    const uint32_t *pbuf_index = name_to_buffer_.Find(ref->name().c_str());
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

    ++buf.write_count;
    ++pass.ref_count_;

#ifndef NDEBUG
    for (size_t i = 0; i < pass.output_.size(); i++) {
        assert(pass.output_[i].type != eRpResType::Buffer || pass.output_[i].index != ret.index);
    }
#endif
    pass.output_.push_back(ret);

    return ret;
}

RpResource RpBuilder::WriteTexture(const RpResource handle, const Ren::eResState desired_state,
                                   const Ren::eStageBits stages, RenderPass &pass) {
    assert(handle.type == eRpResType::Texture);

    RpAllocTex &tex = textures_[handle.index];
    const auto ret = RpResource{eRpResType::Texture, tex._generation, desired_state, stages, handle.index};

    ++tex.write_count;
    ++pass.ref_count_;

#ifndef NDEBUG
    for (size_t i = 0; i < pass.output_.size(); i++) {
        assert(pass.output_[i].type != eRpResType::Texture || pass.output_[i].index != ret.index);
    }
#endif
    pass.output_.push_back(ret);

    return ret;
}

RpResource RpBuilder::WriteTexture(const char *name, const Ren::eResState desired_state, const Ren::eStageBits stages,
                                   RenderPass &pass) {
    const uint32_t *tex_index = name_to_texture_.Find(name);
    assert(tex_index && "Texture does not exist!");

    RpAllocTex &tex = textures_[*tex_index];
    const auto ret = RpResource{eRpResType::Texture, tex._generation, desired_state, stages, *tex_index};

    ++tex.write_count;
    ++pass.ref_count_;

#ifndef NDEBUG
    for (size_t i = 0; i < pass.output_.size(); i++) {
        assert(pass.output_[i].type != eRpResType::Texture || pass.output_[i].index != ret.index);
    }
#endif
    pass.output_.push_back(ret);

    return ret;
}

RpResource RpBuilder::WriteTexture(const char *name, const Ren::Tex2DParams &p, const Ren::eResState desired_state,
                                   const Ren::eStageBits stages, RenderPass &pass) {
    RpResource ret;
    ret.type = eRpResType::Texture;

    const uint32_t *ptex_index = name_to_texture_.Find(name);
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

    ++tex.write_count;
    ++pass.ref_count_;

#ifndef NDEBUG
    for (size_t i = 0; i < pass.output_.size(); i++) {
        assert(pass.output_[i].type != eRpResType::Texture || pass.output_[i].index != ret.index);
    }
#endif
    pass.output_.push_back(ret);

    return ret;
}

RpResource RpBuilder::WriteTexture(const Ren::WeakTex2DRef &ref, const Ren::eResState desired_state,
                                   const Ren::eStageBits stages, RenderPass &pass) {
    RpResource ret;
    ret.type = eRpResType::Texture;

    const uint32_t *ptex_index = name_to_texture_.Find(ref->name().c_str());
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

    ++tex.write_count;
    ++pass.ref_count_;

#ifndef NDEBUG
    for (size_t i = 0; i < pass.output_.size(); i++) {
        assert(pass.output_[i].type != eRpResType::Texture || pass.output_[i].index != ret.index);
    }
#endif

    pass.output_.push_back(ret);

    return ret;
}

RpAllocBuf &RpBuilder::GetReadBuffer(const RpResource &handle) {
    assert(handle.type == eRpResType::Buffer);
    RpAllocBuf &buf = buffers_.at(handle.index);
    assert(buf._generation == handle._generation);
    ++buf.read_count;
    return buf;
}

RpAllocTex &RpBuilder::GetReadTexture(const RpResource &handle) {
    assert(handle.type == eRpResType::Texture);
    RpAllocTex &tex = textures_.at(handle.index);
    assert(tex._generation == handle._generation);
    assert(tex.ref->resource_state == handle.desired_state);
    ++tex.read_count;
    return tex;
}

RpAllocBuf &RpBuilder::GetWriteBuffer(const RpResource &handle) {
    assert(handle.type == eRpResType::Buffer);
    RpAllocBuf &buf = buffers_.at(handle.index);
    assert(buf._generation == handle._generation);
    assert(buf.ref->resource_state == handle.desired_state);
    ++buf.write_count;
    return buf;
}

RpAllocTex &RpBuilder::GetWriteTexture(const RpResource &handle) {
    assert(handle.type == eRpResType::Texture);
    RpAllocTex &tex = textures_.at(handle.index);
    assert(tex._generation == handle._generation);
    assert(tex.ref->resource_state == handle.desired_state);
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
    temp_samplers.clear();
}

void RpBuilder::Compile() {
    struct {
        RenderPass *owner;
        RpResource resource;
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

    { // build resource linked lists
        std::vector<RpResource *> all_resources;

        for (RenderPass *cur_pass : render_passes_) {
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

#if defined(USE_GL_RENDER)
    rast_state_.Apply();
#endif

    pass_timings_[ctx_.backend_frame()].clear();
    // Write timestamp at the beginning of execution
    const int query_beg = ctx_.WriteTimestamp(true);

    for (int i = 0; i < int(render_passes_.size()); ++i) {
        RenderPass &cur_pass = *render_passes_[i];

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
        cur_pass.input_.clear();
        cur_pass.output_.clear();

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