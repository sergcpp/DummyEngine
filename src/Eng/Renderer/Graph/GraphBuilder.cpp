#include "GraphBuilder.h"

#include <Ren/Context.h>
#include <Ren/DebugMarker.h>
#include <optick/optick.h>

#include "SubPass.h"

namespace GraphBuilderInternal {
const bool EnableTextureAliasing = true;
}

Ren::ILog *RpBuilder::log() { return ctx_.log(); }

RpSubpass &RpBuilder::AddPass(const char *name) {
    auto *new_rp = reinterpret_cast<RpSubpass *>(alloc_.allocate(sizeof(RpSubpass)));
    alloc_.construct(new_rp, int(subpasses_.size()), name, *this);
    subpasses_.emplace_back(new_rp);
    return *subpasses_.back();
}

RpSubpass *RpBuilder::FindPass(const char *name) {
    auto it = std::find_if(std::begin(subpasses_), std::end(subpasses_),
                           [name](const RpSubpass *pass) { return strcmp(pass->name(), name) == 0; });
    if (it != std::end(subpasses_)) {
        return (*it);
    }
    return nullptr;
}

RpResRef RpBuilder::ReadBuffer(const RpResRef handle, const Ren::eResState desired_state, const Ren::eStageBits stages,
                               RpSubpass &pass) {
    assert(handle.type == eRpResType::Buffer);

    RpAllocBuf &buf = buffers_[handle.index];
    const RpResource ret = {eRpResType::Buffer, buf._generation, desired_state, stages, handle.index};

    buf.read_in_passes.push_back({pass.index_, int16_t(pass.input_.size())});
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
                               const Ren::eStageBits stages, RpSubpass &pass) {
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

    buf.read_in_passes.push_back({pass.index_, int16_t(pass.input_.size())});
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
                                RpSubpass &pass) {
    assert(handle.type == eRpResType::Texture);

    RpAllocTex &tex = textures_[handle.index];
    const RpResource ret = {eRpResType::Texture, tex._generation, desired_state, stages, handle.index};

    tex.read_in_passes.push_back({pass.index_, int16_t(pass.input_.size())});
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
                                RpSubpass &pass) {
    const uint16_t *tex_index = name_to_texture_.Find(name);
    assert(tex_index && "Texture does not exist!");

    RpAllocTex &tex = textures_[*tex_index];
    const RpResource ret = {eRpResType::Texture, tex._generation, desired_state, stages, *tex_index};

    tex.read_in_passes.push_back({pass.index_, int16_t(pass.input_.size())});
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
                                const Ren::eStageBits stages, RpSubpass &pass) {
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

    tex.read_in_passes.push_back({pass.index_, int16_t(pass.input_.size())});
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

RpResRef RpBuilder::ReadHistoryTexture(const RpResRef handle, const Ren::eResState desired_state,
                                       const Ren::eStageBits stages, RpSubpass &pass) {
    assert(handle.type == eRpResType::Texture);

    RpAllocTex *orig_tex = &textures_[handle.index];
    if (orig_tex->history_index == -1) {
        // allocate new history texture
        RpAllocTex new_tex;
        new_tex.read_count = 0;
        new_tex.write_count = 0;
        new_tex.used_in_stages = Ren::eStageBits::None;
        new_tex.name = orig_tex->name + " [Previous]";
        new_tex.desc = orig_tex->desc;
        new_tex.history_of = handle.index;

        const uint16_t new_index = textures_.emplace(new_tex);
        name_to_texture_[new_tex.name] = new_index;

        orig_tex = &textures_[handle.index];
        orig_tex->history_index = new_index;
    }
    RpAllocTex &tex = textures_[orig_tex->history_index];

    const RpResource ret = {eRpResType::Texture, tex._generation, desired_state, stages,
                            uint16_t(orig_tex->history_index)};

    tex.desc = orig_tex->desc;
    tex.read_in_passes.push_back({pass.index_, int16_t(pass.input_.size())});
    // assert(tex.write_count == handle.write_count);
    ++tex.read_count;

#ifndef NDEBUG
    for (size_t i = 0; i < pass.input_.size(); i++) {
        assert(pass.input_[i].type != eRpResType::Texture || pass.input_[i].index != ret.index);
    }
#endif
    pass.input_.push_back(ret);

    return ret;
}

RpResRef RpBuilder::ReadHistoryTexture(const char *name, Ren::eResState desired_state, Ren::eStageBits stages,
                                       RpSubpass &pass) {
    RpResRef ret;
    ret.type = eRpResType::Texture;

    const uint16_t *ptex_index = name_to_texture_.Find(name);
    if (!ptex_index) {
        RpAllocTex new_tex;
        new_tex.read_count = 0;
        new_tex.write_count = 0;
        new_tex.used_in_stages = Ren::eStageBits::None;
        new_tex.name = name;
        // desc must be initialized later

        ret.index = textures_.emplace(new_tex);
        name_to_texture_[new_tex.name] = ret.index;
    } else {
        ret.index = *ptex_index;
    }

    return ReadHistoryTexture(ret, desired_state, stages, pass);
}

RpResRef RpBuilder::WriteBuffer(const RpResRef handle, const Ren::eResState desired_state, const Ren::eStageBits stages,
                                RpSubpass &pass) {
    assert(handle.type == eRpResType::Buffer);

    RpAllocBuf &buf = buffers_[handle.index];
    auto ret = RpResource{eRpResType::Buffer, buf._generation, desired_state, stages, handle.index};

    buf.written_in_passes.push_back({pass.index_, int16_t(pass.output_.size())});
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
                                const Ren::eStageBits stages, RpSubpass &pass) {
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

RpResRef RpBuilder::WriteBuffer(const Ren::WeakBufferRef &ref, const Ren::eResState desired_state,
                                const Ren::eStageBits stages, RpSubpass &pass) {
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
                                 const Ren::eStageBits stages, RpSubpass &pass) {
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
                                 RpSubpass &pass) {
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
                                 const Ren::eStageBits stages, RpSubpass &pass) {
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
    assert(tex.desc.format == Ren::eTexFormat::Undefined || tex.desc.format == p.format);
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
                                 const Ren::eStageBits stages, RpSubpass &pass, const int slot_index) {
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

RpResRef RpBuilder::MakeTextureResource(const Ren::WeakTex2DRef &ref) {
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

void RpBuilder::AllocateNeededResources(RpSubpass &pass) {
    for (size_t i = 0; i < pass.output_.size(); i++) {
        RpResource res = pass.output_[i];
        if (res.type == eRpResType::Buffer) {
            RpAllocBuf &buf = buffers_.at(res.index);
            if (!buf.ref || buf.desc.type != buf.ref->type() || buf.desc.size > buf.ref->size()) {
                const uint32_t size_before = buf.ref ? buf.ref->size() : 0;
                buf.strong_ref = ctx_.LoadBuffer(buf.name.c_str(), buf.desc.type, buf.desc.size);
                if (buf.ref) {
                    ctx_.log()->Info("Reinit buf %s (%u bytes -> %u bytes)", buf.name.c_str(), size_before,
                                     buf.ref->size());
                }
                buf.ref = buf.strong_ref;
            }
        } else if (res.type == eRpResType::Texture) {
            RpAllocTex &tex = textures_.at(res.index);
            if (tex.history_index != -1) {
                RpAllocTex &hist_tex = textures_.at(tex.history_index);
                // combine usage flags
                tex.desc.usage |= hist_tex.desc.usage;
                hist_tex.desc = tex.desc;

                tex.ref = tex.strong_ref;
                hist_tex.ref = hist_tex.strong_ref;

                if (!hist_tex.ref || hist_tex.desc != hist_tex.ref->params) {
                    if (hist_tex.ref) {
                        const uint32_t mem_before = Ren::EstimateMemory(hist_tex.ref->params);
                        const uint32_t mem_after = Ren::EstimateMemory(hist_tex.desc);
                        ctx_.log()->Info("Reinit tex %s (%ix%i %f MB -> %ix%i %f MB)", hist_tex.name.c_str(),
                                         hist_tex.ref->params.w, hist_tex.ref->params.h, float(mem_before) * 0.000001f,
                                         hist_tex.desc.w, hist_tex.desc.h, float(mem_after) * 0.000001f);
                    } else {
                        ctx_.log()->Info("Alloc tex %s (%ix%i %f MB)", hist_tex.name.c_str(), hist_tex.desc.w,
                                         hist_tex.desc.h, float(Ren::EstimateMemory(hist_tex.desc)) * 0.000001f);
                    }
                    Ren::eTexLoadStatus status;
                    hist_tex.strong_ref =
                        ctx_.LoadTexture2D(hist_tex.name.c_str(), hist_tex.desc, ctx_.default_mem_allocs(), &status);
                    hist_tex.ref = hist_tex.strong_ref;
                    assert(status == Ren::eTexLoadStatus::CreatedDefault || status == Ren::eTexLoadStatus::Found ||
                           status == Ren::eTexLoadStatus::Reinitialized);
                }
            }

            if (tex.alias_of != -1) {
                const RpAllocTex &orig_tex = textures_.at(tex.alias_of);
                assert(orig_tex.alias_of == -1);
                tex.ref = orig_tex.ref;
                tex.strong_ref = {};
                ctx_.log()->Info("Tex %s will be alias of %s", tex.name.c_str(), orig_tex.name.c_str());
            } else if (!tex.ref || tex.desc != tex.ref->params) {
                if (tex.ref) {
                    const uint32_t mem_before = Ren::EstimateMemory(tex.ref->params);
                    const uint32_t mem_after = Ren::EstimateMemory(tex.desc);
                    ctx_.log()->Info("Reinit tex %s (%ix%i %f MB -> %ix%i %f MB)", tex.name.c_str(), tex.ref->params.w,
                                     tex.ref->params.h, float(mem_before) * 0.000001f, tex.desc.w, tex.desc.h,
                                     float(mem_after) * 0.000001f);
                } else {
                    ctx_.log()->Info("Alloc tex %s (%ix%i %f MB)", tex.name.c_str(), tex.desc.w, tex.desc.h,
                                     float(Ren::EstimateMemory(tex.desc)) * 0.000001f);
                }
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
    for (int i = int(subpasses_.size()) - 1; i >= 0; --i) {
        alloc_.destroy(subpasses_[i]);
    }
    subpasses_.clear();
    subpass_data_.clear();
    alloc_.Reset();

    //
    // Reset resources
    //
    for (RpAllocBuf &buf : buffers_) {
        buf._generation = 0;
        buf.used_in_stages = Ren::eStageBits::None;
        buf.read_in_passes.clear();
        buf.written_in_passes.clear();
        if (buf.ref) {
            buf.used_in_stages = Ren::StageBitsForState(buf.ref->resource_state);
        }
    }
    for (RpAllocTex &tex : textures_) {
        tex._generation = 0;
        tex.alias_of = -1;
        tex.desc.format = Ren::eTexFormat::Undefined;
        tex.desc.usage = {}; // gather usage flags again
        tex.used_in_stages = Ren::eStageBits::None;
        tex.read_in_passes.clear();
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
        RpSubpass *pass = subpasses_[i.pass_index];
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
    for (const int16_t dep : subpasses_[dst_pass]->depends_on_passes_) {
        if (DependsOn_r(dep, src_pass)) {
            return true;
        }
    }
    return false;
}

void RpBuilder::TraversePassDependencies_r(const RpSubpass *pass, const int recursion_depth,
                                           std::vector<RpSubpass *> &out_pass_stack) {
    assert(recursion_depth <= subpasses_.size());
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
        RpSubpass *_pass = subpasses_[i];
        assert(_pass != pass);
        const auto it =
            std::find(std::begin(pass->depends_on_passes_), std::end(pass->depends_on_passes_), _pass->index_);
        if (it == std::end(pass->depends_on_passes_)) {
            pass->depends_on_passes_.push_back(_pass->index_);
        }
        out_pass_stack.push_back(_pass);
        TraversePassDependencies_r(_pass, recursion_depth + 1, out_pass_stack);
    }
}

void RpBuilder::PrepareAllocResources() {
    std::vector<bool> visited_buffers(buffers_.capacity(), false);
    std::vector<bool> visited_textures(textures_.capacity(), false);

    for (RpSubpass *cur_pass : reordered_subpasses_) {
        for (size_t i = 0; i < cur_pass->input_.size(); ++i) {
            const RpResource &r = cur_pass->input_[i];
            if (r.type == eRpResType::Buffer) {
                visited_buffers[r.index] = true;
            } else if (r.type == eRpResType::Texture) {
                RpAllocTex &tex = textures_[r.index];
                tex.desc.usage |= Ren::TexUsageFromState(r.desired_state);
                visited_textures[r.index] = true;
            }
        }
        for (size_t i = 0; i < cur_pass->output_.size(); ++i) {
            const RpResource &r = cur_pass->output_[i];
            if (r.type == eRpResType::Buffer) {
                visited_buffers[r.index] = true;
            } else if (r.type == eRpResType::Texture) {
                RpAllocTex &tex = textures_[r.index];
                tex.desc.usage |= Ren::TexUsageFromState(r.desired_state);
                visited_textures[r.index] = true;
            }
        }
    }

    // Release unused resources
    for (auto it = buffers_.begin(); it != buffers_.end(); ++it) {
        if (!visited_buffers[it.index()]) {
            it->ref = it->strong_ref = {};
        }
    }
    for (auto it = textures_.begin(); it != textures_.end(); ++it) {
        if (!visited_textures[it.index()] && (it->history_of == -1 || !visited_textures[it->history_of]) &&
            (it->history_index == -1 || !visited_textures[it->history_index])) {
            it->ref = it->strong_ref = {};
        }
    }
}

void RpBuilder::BuildRenderPasses() {
    auto should_merge = [](const RpSubpass *prev, const RpSubpass *next) {
        // TODO: merge similar renderpasses
        return false;
    };

    render_passes_.clear();
    render_passes_.reserve(reordered_subpasses_.size());

    for (int beg = 0; beg < int(reordered_subpasses_.size());) {
        int end = beg + 1;
        for (; end < int(reordered_subpasses_.size()); ++end) {
            if (!should_merge(reordered_subpasses_[beg], reordered_subpasses_[end])) {
                break;
            }
        }

        for (int i = beg; i < end; ++i) {
            reordered_subpasses_[i]->actual_pass_index_ = int16_t(render_passes_.size());
        }

        render_passes_.emplace_back();
        auto &new_pass = render_passes_.back();
        new_pass.subpass_beg = beg;
        new_pass.subpass_end = end;

        beg = end;
    }
}

void RpBuilder::BuildTransientResources() {
    for (RpAllocTex &tex : textures_) {
        tex.transient = (tex.history_index == -1 && tex.history_of == -1);
    }

    std::vector<int> actual_pass_used(textures_.size(), -1);
    for (auto it = textures_.begin(); it != textures_.end(); ++it) {
        RpAllocTex &tex = *it;
        for (const rp_write_pass_t &p : tex.written_in_passes) {
            const int actual_pass_index = subpasses_[p.pass_index]->actual_pass_index_;
            if (actual_pass_used[it.index()] != -1 && actual_pass_used[it.index()] != actual_pass_index) {
                tex.transient = false;
                break;
            }
            actual_pass_used[it.index()] = actual_pass_index;
        }
    }
    // TODO: actually use this information
}

void RpBuilder::BuildAliases() {
    struct range_t {
        int first_write_pass = std::numeric_limits<int>::max();
        int last_write_pass = -1;
        int first_read_pass = std::numeric_limits<int>::max();
        int last_read_pass = -1;

        bool has_writer() const { return first_write_pass <= last_write_pass; }
        bool has_reader() const { return first_read_pass <= last_read_pass; }
        bool is_used() const { return has_writer() || has_reader(); }

        bool can_alias() const {
            if (has_reader() && has_writer() && first_read_pass <= first_write_pass) {
                return false;
            }
            return true;
        }

        int last_used_pass() const {
            int last_pass = 0;
            if (has_writer()) {
                last_pass = std::max(last_pass, last_write_pass);
            }
            if (has_reader()) {
                last_pass = std::max(last_pass, last_read_pass);
            }
            return last_pass;
        }

        int first_used_pass() const {
            int first_pass = std::numeric_limits<int>::max();
            if (has_writer()) {
                first_pass = std::min(first_pass, first_write_pass);
            }
            if (has_reader()) {
                first_pass = std::min(first_pass, first_read_pass);
            }
            return first_pass;
        }
    };

    auto disjoint_lifetimes = [](const range_t &r1, const range_t &r2) -> bool {
        if (!r1.is_used() || !r2.is_used() || !r1.can_alias() || !r2.can_alias()) {
            return false;
        }
        return r1.last_used_pass() < r2.first_used_pass() || r2.last_used_pass() < r1.first_used_pass();
    };

    std::vector<range_t> ranges(textures_.size());

    // Gather pass ranges
    for (int i = 0; i < int(reordered_subpasses_.size()); ++i) {
        const RpSubpass *subpass = reordered_subpasses_[i];
        for (const auto &res : subpass->input_) {
            if (res.type == eRpResType::Texture) {
                range_t &range = ranges[res.index];
                range.first_read_pass = std::min(range.first_read_pass, i);
                range.last_read_pass = std::max(range.last_read_pass, i);
            }
        }
        for (const auto &res : subpass->output_) {
            if (res.type == eRpResType::Texture) {
                range_t &range = ranges[res.index];
                range.first_write_pass = std::min(range.first_write_pass, i);
                range.last_write_pass = std::max(range.last_write_pass, i);
            }
        }
    }

    alias_chains_.clear();
    alias_chains_.resize(textures_.size());

    std::vector<int> aliases(textures_.size(), -1);

    for (auto i = textures_.begin(); i != textures_.end(); ++i) {
        RpAllocTex &tex1 = *i;
        const range_t &range1 = ranges[i.index()];

        if (tex1.history_index != -1 || tex1.history_of != -1) {
            continue;
        }

        for (auto j = textures_.begin(); j < i; ++j) {
            RpAllocTex &tex2 = *j;
            const range_t &range2 = ranges[j.index()];

            if (tex2.history_index != -1 || tex2.history_of != -1 || aliases[j.index()] != -1) {
                continue;
            }

            if (tex1.desc.format == tex2.desc.format && tex1.desc.w == tex2.desc.w && tex1.desc.h == tex2.desc.h &&
                tex1.desc.mip_count == tex2.desc.mip_count) {
                bool disjoint = disjoint_lifetimes(range1, range2);
                for (const int alias : alias_chains_[j.index()]) {
                    if (alias == i.index()) {
                        continue;
                    }
                    const range_t &range = ranges[alias];
                    disjoint &= disjoint_lifetimes(range, range2);
                }
                if (disjoint) {
                    aliases[i.index()] = j.index();
                    if (alias_chains_[j.index()].empty()) {
                        alias_chains_[j.index()].push_back(j.index());
                    }
                    alias_chains_[j.index()].push_back(i.index());
                    break;
                }
            }
        }
    }

    for (int j = 0; j < int(alias_chains_.size()); ++j) {
        auto &chain = alias_chains_[j];
        if (chain.empty()) {
            continue;
        }

        std::sort(std::begin(chain), std::end(chain), [&](const int lhs, const int rhs) {
            return ranges[lhs].last_used_pass() < ranges[rhs].first_used_pass();
        });

        RpAllocTex &first_tex = textures_[chain[0]];
        assert(first_tex.alias_of == -1);

        for (int i = 1; i < int(chain.size()); ++i) {
            RpAllocTex &next_tex = textures_[chain[i]];
            next_tex.alias_of = chain[0];
            // propagate usage
            first_tex.desc.usage |= next_tex.desc.usage;
        }

        if (chain[0] != j) {
            alias_chains_[chain[0]] = std::move(chain);
        }
    }
}

void RpBuilder::BuildResourceLinkedLists() {
    OPTICK_EVENT();
    std::vector<RpResource *> all_resources;

    auto resource_compare = [](const RpResource *lhs, const RpResource *rhs) {
        return RpResource::LessThanTypeAndIndex(*lhs, *rhs);
    };

    for (RpSubpass *cur_pass : reordered_subpasses_) {
        for (size_t i = 0; i < cur_pass->input_.size(); i++) {
            RpResource *r = &cur_pass->input_[i];

            auto it = std::lower_bound(std::begin(all_resources), std::end(all_resources), r, resource_compare);
            if (it != std::end(all_resources) && !RpResource::LessThanTypeAndIndex(*r, **it)) {
                (*it)->next_use = r;
                (*it) = r;
            } else {
                if (r->type == eRpResType::Texture && textures_[r->index].alias_of != -1) {
                    const auto &chain = alias_chains_[textures_[r->index].alias_of];
                    auto curr_it = std::find(std::begin(chain), std::end(chain), r->index);
                    assert(curr_it != std::end(chain) && curr_it != std::begin(chain));

                    RpResource to_find;
                    to_find.type = eRpResType::Texture;
                    to_find.index = *--curr_it;

                    auto it2 = std::lower_bound(std::begin(all_resources), std::end(all_resources), &to_find,
                                                resource_compare);
                    if (it2 != std::end(all_resources) && !RpResource::LessThanTypeAndIndex(to_find, **it2)) {
                        (*it2)->next_use = r;
                    }
                }
                r->next_use = nullptr;
                all_resources.insert(it, r);
            }
        }

        for (size_t i = 0; i < cur_pass->output_.size(); i++) {
            RpResource *r = &cur_pass->output_[i];

            auto it = std::lower_bound(std::begin(all_resources), std::end(all_resources), r, resource_compare);
            if (it != std::end(all_resources) && !RpResource::LessThanTypeAndIndex(*r, **it)) {
                (*it)->next_use = r;
                (*it) = r;
            } else {
                if (r->type == eRpResType::Texture && textures_[r->index].alias_of != -1) {
                    const auto &chain = alias_chains_[textures_[r->index].alias_of];
                    auto curr_it = std::find(std::begin(chain), std::end(chain), r->index);
                    assert(curr_it != std::end(chain) && curr_it != std::begin(chain));

                    RpResource to_find;
                    to_find.type = eRpResType::Texture;
                    to_find.index = *--curr_it;

                    auto it2 = std::lower_bound(std::begin(all_resources), std::end(all_resources), &to_find,
                                                resource_compare);
                    if (it2 != std::end(all_resources) && !RpResource::LessThanTypeAndIndex(to_find, **it2)) {
                        (*it2)->next_use = r;
                    }
                }
                r->next_use = nullptr;
                all_resources.insert(it, r);
            }
        }
    }
}

void RpBuilder::Compile(const RpResRef backbuffer_sources[], int backbuffer_sources_count) {
    OPTICK_EVENT();

#if 0 // reference-counted culling
    struct {
        RenderPass *owner;
        RpResRef resource;
    } stack[128] = {};
    uint32_t stack_size = 0;

    { // gather unreferenced resources
        for (RenderPass *cur_pass : subpasses_) {
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

    reordered_subpasses_.clear();
    reordered_subpasses_.reserve(subpasses_.size());

    if (backbuffer_sources_count) {
        //
        // Sorting and culling
        //
        Ren::SmallVector<RpSubpass *, 32> written_in_passes;
        for (int i = 0; i < backbuffer_sources_count; ++i) {
            const int16_t prev_pass = FindPreviousWrittenInPass(backbuffer_sources[i]);
            if (prev_pass != -1) {
                written_in_passes.push_back(subpasses_[prev_pass]);
            }
        }

        reordered_subpasses_.assign(std::begin(written_in_passes), std::end(written_in_passes));

        for (const RpSubpass *pass : written_in_passes) {
            TraversePassDependencies_r(pass, 0, reordered_subpasses_);
        }

        std::reverse(std::begin(reordered_subpasses_), std::end(reordered_subpasses_));

        int out_index = 0;
        for (int in_index = 0; in_index < int(reordered_subpasses_.size()); ++in_index) {
            if (!reordered_subpasses_[in_index]->visited_) {
                reordered_subpasses_[out_index++] = reordered_subpasses_[in_index];
                reordered_subpasses_[in_index]->visited_ = true;
            }
        }
        reordered_subpasses_.resize(out_index);

        if (!reordered_subpasses_.empty()) {
            std::vector<RpSubpass *> scheduled_passes;
            scheduled_passes.reserve(reordered_subpasses_.size());

            // schedule the first pass
            scheduled_passes.push_back(reordered_subpasses_.front());
            reordered_subpasses_.erase(reordered_subpasses_.begin());

            while (!reordered_subpasses_.empty()) {
                int best_ovelap_score = -1;
                int best_candidate = 0;

                for (int i = 0; i < int(reordered_subpasses_.size()); ++i) {
                    int overlap_score = 0;

                    for (int j = int(scheduled_passes.size()) - 1; j >= 0; --j) {
                        if (DependsOn_r(reordered_subpasses_[i]->index_, scheduled_passes[j]->index_)) {
                            break;
                        }
                        ++overlap_score;
                    }

                    if (overlap_score <= best_ovelap_score) {
                        continue;
                    }

                    bool possible_candidate = true;
                    for (int j = 0; j < i; ++j) {
                        if (DependsOn_r(reordered_subpasses_[i]->index_, reordered_subpasses_[j]->index_)) {
                            possible_candidate = false;
                            break;
                        }
                    }

                    for (int j = 0; j < int(reordered_subpasses_.size()) && possible_candidate; ++j) {
                        if (j == i) {
                            continue;
                        }
                        for (const auto &output : reordered_subpasses_[i]->output_) {
                            for (const auto &input : reordered_subpasses_[j]->input_) {
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

                scheduled_passes.push_back(reordered_subpasses_[best_candidate]);
                reordered_subpasses_.erase(reordered_subpasses_.begin() + best_candidate);
            }

            reordered_subpasses_ = std::move(scheduled_passes);
        }
    } else {
        // Use all passes as is
        reordered_subpasses_.assign(std::begin(subpasses_), std::end(subpasses_));
    }

    BuildTransientResources();
    PrepareAllocResources();
    if (GraphBuilderInternal::EnableTextureAliasing) {
        BuildAliases();
    }
    BuildRenderPasses();

    for (RpSubpass *subpass : reordered_subpasses_) {
        // Must be allocated in order of pass execution (because of how aliasing works)
        AllocateNeededResources(*subpass);
    }

#if 0
    ctx_.log()->Info("======================================================================");
    uint32_t total_buffer_memory = 0;
    for (const RpAllocBuf &buf : buffers_) {
        if (buf.strong_ref) {
            ctx_.log()->Info("Buf %16s (%f MB)\t| %f MB", buf.name.c_str(), float(buf.strong_ref->size()) * 0.000001f,
                             float(total_buffer_memory) * 0.000001f);
            total_buffer_memory += buf.strong_ref->size();
        }
    }
    ctx_.log()->Info("----------------------------------------------------------------------");
    ctx_.log()->Info("Total graph buffer memory: %f MB", float(total_buffer_memory) * 0.000001f);
    ctx_.log()->Info("======================================================================");
#endif
#if 1
    ctx_.log()->Info("======================================================================");
    std::vector<Ren::WeakTex2DRef> not_handled_textures;
    not_handled_textures.reserve(textures_.size());
    uint32_t total_texture_memory = 0;
    for (const RpAllocTex &tex : textures_) {
        if (tex.alias_of != -1) {
            const RpAllocTex &orig_tex = textures_[tex.alias_of];
            ctx_.log()->Info("Tex %-24.24s alias of %12s\t\t| %-f MB", tex.name.c_str(), orig_tex.name.c_str(),
                             float(total_texture_memory) * 0.000001f);
            continue;
        }
        if (tex.strong_ref) {
            total_texture_memory += Ren::EstimateMemory(tex.strong_ref->params);
            ctx_.log()->Info("Tex %-24.24s (%4ix%-4i %f MB)\t\t| %f MB", tex.name.c_str(), tex.desc.w, tex.desc.h,
                             float(Ren::EstimateMemory(tex.ref->params)) * 0.000001f,
                             float(total_texture_memory) * 0.000001f);
        } else if (tex.ref) {
            not_handled_textures.push_back(tex.ref);
        }
    }
    ctx_.log()->Info("----------------------------------------------------------------------");
    ctx_.log()->Info("Graph owned texture memory:\t\t\t\t\t| %f MB", float(total_texture_memory) * 0.000001f);
    ctx_.log()->Info("----------------------------------------------------------------------");
    for (const auto &ref : not_handled_textures) {
        total_texture_memory += Ren::EstimateMemory(ref->params);
        ctx_.log()->Info("Tex %-24.24s (%4ix%-4i %f MB)\t\t| %f MB", ref->name().c_str(), ref->params.w, ref->params.h,
                         float(Ren::EstimateMemory(ref->params)) * 0.000001f, float(total_texture_memory) * 0.000001f);
    }
    ctx_.log()->Info("----------------------------------------------------------------------");
    ctx_.log()->Info("Total graph texture memory:\t\t\t\t\t| %f MB", float(total_texture_memory) * 0.000001f);
    ctx_.log()->Info("======================================================================");
#endif
}

void RpBuilder::Execute() {
    OPTICK_EVENT();

    // Swap history images
    for (RpAllocTex &tex : textures_) {
        if (tex.history_index != -1) {
            auto &hist_tex = textures_.at(tex.history_index);
            std::swap(tex.ref, hist_tex.ref);
        }
    }
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

    for (int j = 0; j < int(render_passes_.size()); ++j) {
        for (int i = render_passes_[j].subpass_beg; i < render_passes_[j].subpass_end; ++i) {
            RpSubpass &cur_pass = *reordered_subpasses_[i];
            OPTICK_GPU_EVENT("Execute Pass");
            OPTICK_TAG("Pass Name", cur_pass.name());

#if !defined(NDEBUG) && defined(USE_GL_RENDER)
            Ren::ResetGLState();
#endif

            Ren::DebugMarker _(ctx_.current_cmd_buf(), cur_pass.name());

            // Start timestamp
            pass_timing_t &pass_interval = pass_timings_[ctx_.backend_frame()].emplace_back();
            pass_interval.name = cur_pass.name();
            pass_interval.query_beg = ctx_.WriteTimestamp(true);

            InsertResourceTransitions(cur_pass);

            cur_pass.Execute(*this);

            // End timestamp
            pass_interval.query_end = ctx_.WriteTimestamp(false);
        }
    }

    // Write timestamp at the end of execution
    pass_timing_t &initial_interval = pass_timings_[ctx_.backend_frame()].emplace_back();
    initial_interval.name = "GRAPH TOTAL";
    initial_interval.query_beg = query_beg;
    initial_interval.query_end = ctx_.WriteTimestamp(false);
}

void RpBuilder::InsertResourceTransitions(RpSubpass &pass) {
    OPTICK_GPU_EVENT("InsertResourceTransitions");
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

    Ren::TransitionResourceStates(cmd_buf, src_stages, dst_stages, res_transitions);
}

void RpBuilder::CheckResourceStates(RpSubpass &pass) {
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
        RpAllocTex *tex = &textures_.at(res.index);

        if (tex->alias_of != -1) {
            tex = &textures_.at(tex->alias_of);
            assert(tex->alias_of == -1);
        }

        if (tex->ref->resource_state != res.desired_state ||
            tex->ref->resource_state == Ren::eResState::UnorderedAccess) {
            src_stages |= tex->used_in_stages;
            dst_stages |= res.stages;
            tex->used_in_stages = Ren::eStageBits::None;
            res_transitions.emplace_back(tex->ref.get(), res.desired_state);
        }
        tex->used_in_stages |= res.stages;
    }
}