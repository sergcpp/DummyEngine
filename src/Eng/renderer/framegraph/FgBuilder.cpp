#include "FgBuilder.h"

#include <Ren/Context.h>
#include <Ren/DebugMarker.h>
#include <Ren/TextureArray.h>
#include <optick/optick.h>

#include "FgNode.h"

namespace FgBuilderInternal {
const bool EnableTextureAliasing = true;
const bool EnableNodesReordering = true;

void insert_sorted(Ren::SmallVectorImpl<int16_t> &vec, const int16_t val) {
    const auto it = std::lower_bound(std::begin(vec), std::end(vec), val);
    if (it == std::end(vec) || val < (*it)) {
        vec.insert(it, val);
    }
}
} // namespace FgBuilderInternal

Ren::ILog *Eng::FgBuilder::log() { return ctx_.log(); }

Eng::FgNode &Eng::FgBuilder::AddNode(std::string_view name) {
    char *mem = alloc_.allocate(sizeof(FgNode) + alignof(FgNode));
    auto *new_rp = reinterpret_cast<FgNode *>(mem + alignof(FgNode) - (uintptr_t(mem) % alignof(FgNode)));
    alloc_.construct(new_rp, int(nodes_.size()), name, *this);
    nodes_.emplace_back(new_rp);
    return *nodes_.back();
}

Eng::FgNode *Eng::FgBuilder::FindNode(std::string_view name) {
    auto it = std::find_if(begin(nodes_), end(nodes_), [name](const FgNode *node) { return node->name() == name; });
    if (it != end(nodes_)) {
        return (*it);
    }
    return nullptr;
}

std::string Eng::FgBuilder::GetResourceDebugInfo(const FgResource &res) const {
    if (res.type == eFgResType::Texture) {
        if (textures_[res.index].external) {
            return "[Tex] " + textures_[res.index].name + " (ext)";
        } else {
            return "[Tex] " + textures_[res.index].name;
        }
    } else if (res.type == eFgResType::Buffer) {
        if (buffers_[res.index].external) {
            return "[Buf] " + buffers_[res.index].name + " (ext)";
        } else {
            return "[Buf] " + buffers_[res.index].name;
        }
    }
    return "";
}

Eng::FgResRef Eng::FgBuilder::ReadBuffer(const FgResRef handle, const Ren::eResState desired_state,
                                         const Ren::eStageBits stages, FgNode &node) {
    assert(handle.type == eFgResType::Buffer);

    FgAllocBuf &buf = buffers_[handle.index];
    const FgResource ret = {eFgResType::Buffer, buf._generation, desired_state, stages, handle.index};

    buf.read_in_nodes.push_back({node.index_, int16_t(node.input_.size())});
    assert(buf.write_count == handle.write_count);
    ++buf.read_count;

#ifndef NDEBUG
    // Ensure uniqueness
    for (const FgResource &r : node.input_) {
        assert(r.type != eFgResType::Buffer || r.index != ret.index);
    }
#endif
    node.input_.push_back(ret);

    return ret;
}

Eng::FgResRef Eng::FgBuilder::ReadBuffer(const Ren::WeakBufferRef &ref, const Ren::eResState desired_state,
                                         const Ren::eStageBits stages, FgNode &node, const int slot_index) {
    FgResource ret;
    ret.type = eFgResType::Buffer;

    const uint16_t *pbuf_index = name_to_buffer_.Find(ref->name());
    if (!pbuf_index) {
        FgAllocBuf new_buf;
        new_buf.read_count = 0;
        new_buf.write_count = 0;
        new_buf.used_in_stages = Ren::eStageBits::None;
        new_buf.name = ref->name().c_str();
        new_buf.desc = FgBufDesc{ref->type(), ref->size()};
        new_buf.external = true;

        ret.index = buffers_.emplace(new_buf);
        name_to_buffer_[new_buf.name] = ret.index;
    } else {
        ret.index = *pbuf_index;
    }

    FgAllocBuf &buf = buffers_[ret.index];
    assert(buf.desc.size <= ref->size() && buf.desc.type == ref->type());
    buf.ref = ref;
    ret._generation = buf._generation;
    ret.desired_state = desired_state;
    ret.stages = stages;

    buf.read_in_nodes.push_back({node.index_, int16_t(node.input_.size())});
    ++buf.read_count;

    if (slot_index == -1) {
#ifndef NDEBUG
        for (const FgResource &r : node.input_) {
            assert(r.type != eFgResType::Buffer || r.index != ret.index);
        }
#endif
        node.input_.push_back(ret);
    } else {
        // Replace existing input
        FgAllocBuf &prev_buf = buffers_[node.input_[slot_index].index];
        --prev_buf.write_count;
        for (size_t i = 0; i < prev_buf.written_in_nodes.size();) {
            if (prev_buf.written_in_nodes[i].node_index == node.index_) {
                prev_buf.written_in_nodes.erase(prev_buf.written_in_nodes.begin() + i);
            } else {
                ++i;
            }
        }
        if (node.input_[slot_index].index == ret.index) {
            --ret.write_count;
        }
        node.input_[slot_index] = ret;
    }

    return ret;
}

Eng::FgResRef Eng::FgBuilder::ReadBuffer(const Ren::WeakBufferRef &ref, const Ren::WeakTex1DRef &tbo,
                                         const Ren::eResState desired_state, const Ren::eStageBits stages,
                                         FgNode &node) {
    FgResource ret;
    ret.type = eFgResType::Buffer;

    const uint16_t *pbuf_index = name_to_buffer_.Find(ref->name());
    if (!pbuf_index) {
        FgAllocBuf new_buf;
        new_buf.read_count = 0;
        new_buf.write_count = 0;
        new_buf.used_in_stages = Ren::eStageBits::None;
        new_buf.name = ref->name().c_str();
        new_buf.desc = FgBufDesc{ref->type(), ref->size()};
        new_buf.external = true;

        ret.index = buffers_.emplace(new_buf);
        name_to_buffer_[new_buf.name] = ret.index;
    } else {
        ret.index = *pbuf_index;
    }

    FgAllocBuf &buf = buffers_[ret.index];
    assert(buf.desc.size <= ref->size() && buf.desc.type == ref->type());
    buf.ref = ref;
    buf.tbos[0] = tbo;
    ret._generation = buf._generation;
    ret.desired_state = desired_state;
    ret.stages = stages;

    buf.read_in_nodes.push_back({node.index_, int16_t(node.input_.size())});
    ++buf.read_count;

#ifndef NDEBUG
    for (const FgResource &r : node.input_) {
        assert(r.type != eFgResType::Buffer || r.index != ret.index);
    }
#endif
    node.input_.push_back(ret);

    return ret;
}

Eng::FgResRef Eng::FgBuilder::ReadTexture(const FgResRef handle, const Ren::eResState desired_state,
                                          const Ren::eStageBits stages, FgNode &node) {
    assert(handle.type == eFgResType::Texture);

    FgAllocTex &tex = textures_[handle.index];
    const FgResource ret = {eFgResType::Texture, tex._generation, desired_state, stages, handle.index};

    tex.read_in_nodes.push_back({node.index_, int16_t(node.input_.size())});
    // assert(tex.write_count == handle.write_count);
    ++tex.read_count;

#ifndef NDEBUG
    for (const FgResource &r : node.input_) {
        assert(r.type != eFgResType::Texture || r.index != ret.index);
    }
#endif
    node.input_.push_back(ret);

    return ret;
}

Eng::FgResRef Eng::FgBuilder::ReadTexture(std::string_view name, const Ren::eResState desired_state,
                                          const Ren::eStageBits stages, FgNode &node) {
    const uint16_t *tex_index = name_to_texture_.Find(name);
    assert(tex_index && "Texture does not exist!");

    FgAllocTex &tex = textures_[*tex_index];
    const FgResource ret = {eFgResType::Texture, tex._generation, desired_state, stages, *tex_index};

    tex.read_in_nodes.push_back({node.index_, int16_t(node.input_.size())});
    ++tex.read_count;

#ifndef NDEBUG
    for (const FgResource &r : node.input_) {
        assert(r.type != eFgResType::Texture || r.index != ret.index);
    }
#endif
    node.input_.push_back(ret);

    return ret;
}

Eng::FgResRef Eng::FgBuilder::ReadTexture(const Ren::WeakTex2DRef &ref, const Ren::eResState desired_state,
                                          const Ren::eStageBits stages, FgNode &node) {
    FgResource ret;
    ret.type = eFgResType::Texture;

    const uint16_t *ptex_index = name_to_texture_.Find(ref->name());
    if (!ptex_index) {
        FgAllocTex new_tex;
        new_tex.read_count = 0;
        new_tex.write_count = 0;
        new_tex.used_in_stages = Ren::eStageBits::None;
        new_tex.name = ref->name().c_str();
        new_tex.desc = ref->params;
        new_tex.external = true;

        ret.index = textures_.emplace(new_tex);
        name_to_texture_[new_tex.name] = ret.index;
    } else {
        ret.index = *ptex_index;
    }

    FgAllocTex &tex = textures_[ret.index];
    tex.desc = ref->params;
    tex.ref = ref;
    ret._generation = tex._generation;
    ret.desired_state = desired_state;
    ret.stages = stages;

    tex.read_in_nodes.push_back({node.index_, int16_t(node.input_.size())});
    ++tex.read_count;

#ifndef NDEBUG
    for (const FgResource &r : node.input_) {
        assert(r.type != eFgResType::Texture || r.index != ret.index);
    }
#endif

    node.input_.push_back(ret);

    return ret;
}

Eng::FgResRef Eng::FgBuilder::ReadTexture(const Ren::Texture2DArray *ref, Ren::eResState desired_state,
                                          Ren::eStageBits stages, FgNode &node) {
    FgResource ret;
    ret.type = eFgResType::Texture;

    const uint16_t *ptex_index = name_to_texture_.Find(ref->name());
    if (!ptex_index) {
        FgAllocTex new_tex;
        new_tex.read_count = 0;
        new_tex.write_count = 0;
        new_tex.used_in_stages = Ren::eStageBits::None;
        new_tex.name = ref->name();
        new_tex.external = true;

        ret.index = textures_.emplace(new_tex);
        name_to_texture_[new_tex.name] = ret.index;
    } else {
        ret.index = *ptex_index;
    }

    FgAllocTex &tex = textures_[ret.index];
    tex.arr = ref;
    ret._generation = tex._generation;
    ret.desired_state = desired_state;
    ret.stages = stages;

    tex.read_in_nodes.push_back({node.index_, int16_t(node.input_.size())});
    ++tex.read_count;

#ifndef NDEBUG
    for (const FgResource &r : node.input_) {
        assert(r.type != eFgResType::Texture || r.index != ret.index);
    }
#endif

    node.input_.push_back(ret);

    return ret;
}

Eng::FgResRef Eng::FgBuilder::ReadTexture(const Ren::Texture3D *ref, Ren::eResState desired_state,
                                          Ren::eStageBits stages, FgNode &node) {
    FgResource ret;
    ret.type = eFgResType::Texture;

    const uint16_t *ptex_index = name_to_texture_.Find(ref->name());
    if (!ptex_index) {
        FgAllocTex new_tex;
        new_tex.read_count = 0;
        new_tex.write_count = 0;
        new_tex.used_in_stages = Ren::eStageBits::None;
        new_tex.name = ref->name();
        new_tex.external = true;

        ret.index = textures_.emplace(new_tex);
        name_to_texture_[new_tex.name] = ret.index;
    } else {
        ret.index = *ptex_index;
    }

    FgAllocTex &tex = textures_[ret.index];
    tex.tex3d = ref;
    ret._generation = tex._generation;
    ret.desired_state = desired_state;
    ret.stages = stages;

    tex.read_in_nodes.push_back({node.index_, int16_t(node.input_.size())});
    ++tex.read_count;

#ifndef NDEBUG
    for (const FgResource &r : node.input_) {
        assert(r.type != eFgResType::Texture || r.index != ret.index);
    }
#endif

    node.input_.push_back(ret);

    return ret;
}

Eng::FgResRef Eng::FgBuilder::ReadHistoryTexture(const FgResRef handle, const Ren::eResState desired_state,
                                                 const Ren::eStageBits stages, FgNode &node) {
    assert(handle.type == eFgResType::Texture);

    FgAllocTex *orig_tex = &textures_[handle.index];
    if (orig_tex->history_index == -1) {
        // allocate new history texture
        FgAllocTex new_tex;
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
    FgAllocTex &tex = textures_[orig_tex->history_index];

    const FgResource ret = {eFgResType::Texture, tex._generation, desired_state, stages,
                            uint16_t(orig_tex->history_index)};

    tex.desc = orig_tex->desc;
    tex.read_in_nodes.push_back({node.index_, int16_t(node.input_.size())});
    // assert(tex.write_count == handle.write_count);
    ++tex.read_count;

#ifndef NDEBUG
    for (const FgResource &r : node.input_) {
        assert(r.type != eFgResType::Texture || r.index != ret.index);
    }
#endif
    node.input_.push_back(ret);

    return ret;
}

Eng::FgResRef Eng::FgBuilder::ReadHistoryTexture(std::string_view name, Ren::eResState desired_state,
                                                 Ren::eStageBits stages, FgNode &node) {
    FgResRef ret;
    ret.type = eFgResType::Texture;

    const uint16_t *ptex_index = name_to_texture_.Find(name);
    if (!ptex_index) {
        FgAllocTex new_tex;
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

    return ReadHistoryTexture(ret, desired_state, stages, node);
}

Eng::FgResRef Eng::FgBuilder::WriteBuffer(const FgResRef handle, const Ren::eResState desired_state,
                                          const Ren::eStageBits stages, FgNode &node) {
    assert(handle.type == eFgResType::Buffer);

    FgAllocBuf &buf = buffers_[handle.index];
    auto ret = FgResource{eFgResType::Buffer, buf._generation, desired_state, stages, handle.index};

    buf.written_in_nodes.push_back({node.index_, int16_t(node.output_.size())});
    assert(buf.write_count == handle.write_count);
    ++buf.write_count;

#ifndef NDEBUG
    for (const FgResource &r : node.output_) {
        assert(r.type != eFgResType::Buffer || r.index != ret.index);
    }
#endif
    node.output_.push_back(ret);

    ++ret.write_count;
    return ret;
}

Eng::FgResRef Eng::FgBuilder::WriteBuffer(std::string_view name, const FgBufDesc &desc,
                                          const Ren::eResState desired_state, const Ren::eStageBits stages,
                                          FgNode &node) {
    FgResource ret;
    ret.type = eFgResType::Buffer;

    const uint16_t *pbuf_index = name_to_buffer_.Find(name);
    if (!pbuf_index) {
        FgAllocBuf new_buf;
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

    FgAllocBuf &buf = buffers_[ret.index];
    buf.desc = desc;
    ret._generation = buf._generation;
    ret.desired_state = desired_state;
    ret.stages = stages;

    buf.written_in_nodes.push_back({node.index_, int16_t(node.output_.size())});
    ++buf.write_count;

#ifndef NDEBUG
    for (const FgResource &r : node.output_) {
        assert(r.type != eFgResType::Buffer || r.index != ret.index);
    }
#endif
    node.output_.push_back(ret);

    ++ret.write_count;
    return ret;
}

Eng::FgResRef Eng::FgBuilder::WriteBuffer(const Ren::WeakBufferRef &ref, const Ren::eResState desired_state,
                                          const Ren::eStageBits stages, FgNode &node) {
    FgResource ret;
    ret.type = eFgResType::Buffer;

    const uint16_t *pbuf_index = name_to_buffer_.Find(ref->name());
    if (!pbuf_index) {
        FgAllocBuf new_buf;
        new_buf.read_count = 0;
        new_buf.write_count = 0;
        new_buf.used_in_stages = Ren::eStageBits::None;
        new_buf.name = ref->name().c_str();
        new_buf.desc = FgBufDesc{ref->type(), ref->size()};
        new_buf.external = true;

        ret.index = buffers_.emplace(new_buf);
        name_to_buffer_[new_buf.name] = ret.index;
    } else {
        ret.index = *pbuf_index;
    }

    FgAllocBuf &buf = buffers_[ret.index];
    assert(buf.desc.size <= ref->size() && buf.desc.type == ref->type());
    buf.ref = ref;
    ret._generation = buf._generation;
    ret.desired_state = desired_state;
    ret.stages = stages;

    buf.written_in_nodes.push_back({node.index_, int16_t(node.output_.size())});
    ++buf.write_count;

#ifndef NDEBUG
    for (const FgResource &r : node.output_) {
        assert(r.type != eFgResType::Buffer || r.index != ret.index);
    }
#endif
    node.output_.push_back(ret);

    ++ret.write_count;
    return ret;
}

Eng::FgResRef Eng::FgBuilder::WriteTexture(const FgResRef handle, const Ren::eResState desired_state,
                                           const Ren::eStageBits stages, FgNode &node) {
    assert(handle.type == eFgResType::Texture);

    FgAllocTex &tex = textures_[handle.index];
    auto ret = FgResource{eFgResType::Texture, tex._generation, desired_state, stages, handle.index};

    assert(tex.write_count == handle.write_count);
    tex.written_in_nodes.push_back({node.index_, int16_t(node.output_.size())});
    ++tex.write_count;

#ifndef NDEBUG
    for (const FgResource &r : node.output_) {
        assert(r.type != eFgResType::Texture || r.index != ret.index);
    }
#endif
    node.output_.push_back(ret);

    ++ret.write_count;
    return ret;
}

Eng::FgResRef Eng::FgBuilder::WriteTexture(std::string_view name, const Ren::eResState desired_state,
                                           const Ren::eStageBits stages, FgNode &node) {
    const uint16_t *tex_index = name_to_texture_.Find(name);
    assert(tex_index && "Texture does not exist!");

    FgAllocTex &tex = textures_[*tex_index];
    auto ret = FgResource{eFgResType::Texture, tex._generation, desired_state, stages, *tex_index};

    tex.written_in_nodes.push_back({node.index_, int16_t(node.output_.size())});
    ++tex.write_count;

#ifndef NDEBUG
    for (const FgResource &r : node.output_) {
        assert(r.type != eFgResType::Texture || r.index != ret.index);
    }
#endif
    node.output_.push_back(ret);

    ++ret.write_count;
    return ret;
}

Eng::FgResRef Eng::FgBuilder::WriteTexture(std::string_view name, const Ren::Tex2DParams &p,
                                           const Ren::eResState desired_state, const Ren::eStageBits stages,
                                           FgNode &node) {
    FgResource ret;
    ret.type = eFgResType::Texture;

    const uint16_t *ptex_index = name_to_texture_.Find(name);
    if (!ptex_index) {
        FgAllocTex new_tex;
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

    FgAllocTex &tex = textures_[ret.index];
    assert(tex.desc.format == Ren::eTexFormat::Undefined || tex.desc.format == p.format);
    tex.desc = p;

    ret._generation = tex._generation;
    ret.desired_state = desired_state;
    ret.stages = stages;

    tex.written_in_nodes.push_back({node.index_, int16_t(node.output_.size())});
    ++tex.write_count;

#ifndef NDEBUG
    for (const FgResource &r : node.output_) {
        assert(r.type != eFgResType::Texture || r.index != ret.index);
    }
#endif
    node.output_.push_back(ret);

    ++ret.write_count;
    return ret;
}

Eng::FgResRef Eng::FgBuilder::WriteTexture(const Ren::WeakTex2DRef &ref, const Ren::eResState desired_state,
                                           const Ren::eStageBits stages, FgNode &node, const int slot_index) {
    FgResource ret;
    ret.type = eFgResType::Texture;

    const uint16_t *ptex_index = name_to_texture_.Find(ref->name());
    if (!ptex_index) {
        FgAllocTex new_tex;
        new_tex.read_count = 0;
        new_tex.write_count = 0;
        new_tex.used_in_stages = Ren::eStageBits::None;
        new_tex.name = ref->name().c_str();
        new_tex.desc = ref->params;
        new_tex.external = true;

        ret.index = textures_.emplace(new_tex);
        name_to_texture_[new_tex.name] = ret.index;
    } else {
        ret.index = *ptex_index;
    }

    FgAllocTex &tex = textures_[ret.index];
    tex.desc = ref->params;
    tex.ref = ref;
    ret._generation = tex._generation;
    ret.desired_state = desired_state;
    ret.stages = stages;

    tex.written_in_nodes.push_back({node.index_, int16_t(node.output_.size())});
    ++tex.write_count;

    if (slot_index == -1) {
#ifndef NDEBUG
        for (const FgResource &r : node.output_) {
            assert(r.type != eFgResType::Texture || r.index != ret.index);
        }
#endif
        // Add new output
        node.output_.push_back(ret);
    } else if (slot_index < node.output_.size() && node.output_[slot_index]) {
        // Replace existing output
        FgAllocTex &prev_tex = textures_[node.output_[slot_index].index];
        --prev_tex.write_count;
        for (size_t i = 0; i < prev_tex.written_in_nodes.size();) {
            if (prev_tex.written_in_nodes[i].node_index == node.index_) {
                prev_tex.written_in_nodes.erase(prev_tex.written_in_nodes.begin() + i);
            } else {
                ++i;
            }
        }
        if (node.output_[slot_index].index == ret.index) {
            --ret.write_count;
        }
        node.output_[slot_index] = ret;
    }

    ++ret.write_count;
    return ret;
}

Eng::FgResRef Eng::FgBuilder::WriteTexture(const Ren::Texture2DArray *ref, const Ren::eResState desired_state,
                                           const Ren::eStageBits stages, FgNode &node) {
    FgResource ret;
    ret.type = eFgResType::Texture;

    const uint16_t *ptex_index = name_to_texture_.Find(ref->name());
    if (!ptex_index) {
        FgAllocTex new_tex;
        new_tex.read_count = 0;
        new_tex.write_count = 0;
        new_tex.used_in_stages = Ren::eStageBits::None;
        new_tex.name = ref->name();
        // new_tex.desc = ref->params;
        new_tex.external = true;

        ret.index = textures_.emplace(new_tex);
        name_to_texture_[new_tex.name] = ret.index;
    } else {
        ret.index = *ptex_index;
    }

    FgAllocTex &tex = textures_[ret.index];
    // tex.desc = ref->params;
    tex.arr = ref;
    ret._generation = tex._generation;
    ret.desired_state = desired_state;
    ret.stages = stages;

    tex.written_in_nodes.push_back({node.index_, int16_t(node.output_.size())});
    ++tex.write_count;

#ifndef NDEBUG
    for (const FgResource &r : node.output_) {
        assert(r.type != eFgResType::Texture || r.index != ret.index);
    }
#endif
    // Add new output
    node.output_.push_back(ret);

    ++ret.write_count;
    return ret;
}

Eng::FgResRef Eng::FgBuilder::MakeTextureResource(const Ren::WeakTex2DRef &ref) {
    FgResource ret;
    ret.type = eFgResType::Texture;

    const uint16_t *ptex_index = name_to_texture_.Find(ref->name());
    if (!ptex_index) {
        FgAllocTex new_tex;
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

    FgAllocTex &tex = textures_[ret.index];
    tex.desc = ref->params;
    tex.ref = ref;
    ret._generation = tex._generation;

    return ret;
}

Eng::FgAllocBuf &Eng::FgBuilder::GetReadBuffer(const FgResRef handle) {
    assert(handle.type == eFgResType::Buffer);
    FgAllocBuf &buf = buffers_.at(handle.index);
    assert(buf.write_count == handle.write_count);
    ++buf.read_count;
    return buf;
}

Eng::FgAllocTex &Eng::FgBuilder::GetReadTexture(const FgResRef handle) {
    assert(handle.type == eFgResType::Texture);
    FgAllocTex &tex = textures_.at(handle.index);
    assert(tex.write_count == handle.write_count);
    // assert(tex.ref->resource_state == handle.desired_state);
    ++tex.read_count;
    return tex;
}

Eng::FgAllocBuf &Eng::FgBuilder::GetWriteBuffer(const FgResRef handle) {
    assert(handle.type == eFgResType::Buffer);
    FgAllocBuf &buf = buffers_.at(handle.index);
    assert(buf.write_count + 1 == handle.write_count);
    // assert(buf.ref->resource_state == handle.desired_state);
    ++buf.write_count;
    return buf;
}

Eng::FgAllocTex &Eng::FgBuilder::GetWriteTexture(const FgResRef handle) {
    assert(handle.type == eFgResType::Texture);
    FgAllocTex &tex = textures_.at(handle.index);
    assert(tex.write_count + 1 == handle.write_count);
    // assert(tex.ref->resource_state == handle.desired_state);
    ++tex.write_count;
    return tex;
}

void Eng::FgBuilder::AllocateNeededResources(FgNode &node) {
    std::vector<Ren::Tex2DRef> textures_to_clear;

    for (const FgResource res : node.output_) {
        if (res.type == eFgResType::Buffer) {
            FgAllocBuf &buf = buffers_.at(res.index);
            if (!buf.ref || buf.desc.type != buf.ref->type() || buf.desc.size > buf.ref->size()) {
                const uint32_t size_before = buf.ref ? buf.ref->size() : 0;
                buf.strong_ref = ctx_.LoadBuffer(buf.name, buf.desc.type, buf.desc.size);
                if (buf.ref) {
                    ctx_.log()->Info("Reinit buf %s (%u bytes -> %u bytes)", buf.name.c_str(), size_before,
                                     buf.ref->size());
                }
                buf.ref = buf.strong_ref;
            }
        } else if (res.type == eFgResType::Texture) {
            FgAllocTex &tex = textures_.at(res.index);
            if (tex.external) {
                continue;
            }
            if (tex.history_index != -1) {
                FgAllocTex &hist_tex = textures_.at(tex.history_index);
                // combine usage flags
                tex.desc.usage |= hist_tex.desc.usage;
                hist_tex.desc = tex.desc;

                tex.ref = tex.strong_ref;
                hist_tex.ref = hist_tex.strong_ref;

                // Needed to clear the image initially
                hist_tex.desc.usage |= Ren::eTexUsageBits::Transfer;

                if (!hist_tex.ref || hist_tex.desc != hist_tex.ref->params) {
                    if (hist_tex.ref) {
                        const uint32_t mem_before = EstimateMemory(hist_tex.ref->params);
                        const uint32_t mem_after = EstimateMemory(hist_tex.desc);
                        ctx_.log()->Info("Reinit tex %s (%ix%i %f MB -> %ix%i %f MB)", hist_tex.name.c_str(),
                                         hist_tex.ref->params.w, hist_tex.ref->params.h, float(mem_before) * 0.000001f,
                                         hist_tex.desc.w, hist_tex.desc.h, float(mem_after) * 0.000001f);
                    } else {
                        ctx_.log()->Info("Alloc tex %s (%ix%i %f MB)", hist_tex.name.c_str(), hist_tex.desc.w,
                                         hist_tex.desc.h, float(EstimateMemory(hist_tex.desc)) * 0.000001f);
                    }
                    Ren::eTexLoadStatus status;
                    hist_tex.strong_ref =
                        ctx_.LoadTexture2D(hist_tex.name, hist_tex.desc, ctx_.default_mem_allocs(), &status);

                    hist_tex.ref = hist_tex.strong_ref;
                    assert(status == Ren::eTexLoadStatus::CreatedDefault || status == Ren::eTexLoadStatus::Found ||
                           status == Ren::eTexLoadStatus::Reinitialized);
                }

                textures_to_clear.emplace_back(hist_tex.strong_ref);
            }

            // Needed to clear the image initially
            tex.desc.usage |= Ren::eTexUsageBits::Transfer;

            if (tex.alias_of != -1) {
                const FgAllocTex &orig_tex = textures_.at(tex.alias_of);
                assert(orig_tex.alias_of == -1);
                tex.ref = orig_tex.ref;
                tex.strong_ref = {};
                ctx_.log()->Info("Tex %s will be alias of %s", tex.name.c_str(), orig_tex.name.c_str());
            } else if (!tex.ref || tex.desc != tex.ref->params) {
                if (tex.ref) {
                    const uint32_t mem_before = EstimateMemory(tex.ref->params);
                    const uint32_t mem_after = EstimateMemory(tex.desc);
                    ctx_.log()->Info("Reinit tex %s (%ix%i %f MB -> %ix%i %f MB)", tex.name.c_str(), tex.ref->params.w,
                                     tex.ref->params.h, float(mem_before) * 0.000001f, tex.desc.w, tex.desc.h,
                                     float(mem_after) * 0.000001f);
                } else {
                    ctx_.log()->Info("Alloc tex %s (%ix%i %f MB)", tex.name.c_str(), tex.desc.w, tex.desc.h,
                                     float(EstimateMemory(tex.desc)) * 0.000001f);
                }
                Ren::eTexLoadStatus status;
                tex.strong_ref = ctx_.LoadTexture2D(tex.name, tex.desc, ctx_.default_mem_allocs(), &status);
                tex.ref = tex.strong_ref;
                assert(status == Ren::eTexLoadStatus::CreatedDefault || status == Ren::eTexLoadStatus::Found ||
                       status == Ren::eTexLoadStatus::Reinitialized);

                // TODO: this is redundant!
                textures_to_clear.emplace_back(tex.strong_ref);
            }
        }
    }

    if (!textures_to_clear.empty()) { // Clear textures
        Ren::CommandBuffer cmd_buf = ctx_.BegTempSingleTimeCommands();

        std::vector<Ren::TransitionInfo> transitions;
        transitions.reserve(textures_to_clear.size());
        for (const Ren::Tex2DRef &t : textures_to_clear) {
            transitions.emplace_back(t.get(), Ren::eResState::CopyDst);
        }
        TransitionResourceStates(ctx_.api_ctx(), cmd_buf, Ren::AllStages, Ren::AllStages, transitions);

        for (Ren::Tex2DRef &t : textures_to_clear) {
            const float rgba[4] = {
                float(t->params.fallback_color[0]) / 255.0f, float(t->params.fallback_color[1]) / 255.0f,
                float(t->params.fallback_color[2]) / 255.0f, 0.0f /*float(t->params.fallback_color[3]) / 255.0f*/};
            Ren::ClearImage(*t, rgba, cmd_buf);
        }

        ctx_.EndTempSingleTimeCommands(cmd_buf);
    }
}

void Eng::FgBuilder::Reset() {
    for (int i = int(nodes_.size()) - 1; i >= 0; --i) {
        alloc_.destroy(nodes_[i]);
    }
    nodes_.clear();
    nodes_data_.clear();
    alloc_.Reset();

    name_to_buffer_.clear();
    name_to_texture_.clear();

    for (auto &t : node_timings_) {
        t.clear();
    }

    buffers_.clear();
    textures_.clear();

    temp_samplers.clear();
}

int16_t Eng::FgBuilder::FindPreviousWrittenInNode(const FgResRef handle) {
    Ren::SmallVectorImpl<fg_write_node_t> *written_in_nodes = nullptr;
    if (handle.type == eFgResType::Buffer) {
        written_in_nodes = &buffers_[handle.index].written_in_nodes;
    } else if (handle.type == eFgResType::Texture) {
        written_in_nodes = &textures_[handle.index].written_in_nodes;
    }

    for (const fg_write_node_t i : *written_in_nodes) {
        const FgNode *node = nodes_[i.node_index];
        assert(node->output_[i.slot_index].type == handle.type && node->output_[i.slot_index].index == handle.index);
        if (node->output_[i.slot_index].write_count == handle.write_count - 1) {
            return i.node_index;
        }
    }
    return -1;
}

void Eng::FgBuilder::FindPreviousReadInNodes(const FgResRef handle, Ren::SmallVectorImpl<int16_t> &out_nodes) {
    Ren::SmallVectorImpl<fg_write_node_t> *read_in_nodes = nullptr;
    if (handle.type == eFgResType::Buffer) {
        read_in_nodes = &buffers_[handle.index].read_in_nodes;
    } else if (handle.type == eFgResType::Texture) {
        read_in_nodes = &textures_[handle.index].read_in_nodes;
    }

    for (const fg_write_node_t i : *read_in_nodes) {
        const FgNode *node = nodes_[i.node_index];
        assert(node->input_[i.slot_index].type == handle.type && node->input_[i.slot_index].index == handle.index);
        if (node->input_[i.slot_index].write_count == handle.write_count) {
            const auto it = std::lower_bound(std::begin(out_nodes), std::end(out_nodes), i.node_index);
            if (it == std::end(out_nodes) || i.node_index < (*it)) {
                out_nodes.insert(it, i.node_index);
            }
        }
    }
}

bool Eng::FgBuilder::DependsOn_r(const int16_t dst_node, const int16_t src_node) {
    if (dst_node == src_node) {
        return true;
    }
    for (const int16_t dep : nodes_[dst_node]->depends_on_nodes_) {
        if (DependsOn_r(dep, src_node)) {
            return true;
        }
    }
    return false;
}

void Eng::FgBuilder::TraverseNodeDependencies_r(FgNode *node, const int recursion_depth,
                                                std::vector<FgNode *> &out_node_stack) {
    using namespace FgBuilderInternal;
    assert(recursion_depth <= nodes_.size());
    node->visited_ = true;

    Ren::SmallVector<int16_t, 32> previous_nodes;
    for (size_t i = 0; i < node->input_.size(); ++i) {
        // Resource reads are unordered, so we only interesed in write here
        const int16_t prev_node = FindPreviousWrittenInNode(node->input_[i]);
        if (prev_node != -1) {
            const auto it = std::lower_bound(std::begin(previous_nodes), std::end(previous_nodes), prev_node);
            if (it == std::end(previous_nodes) || prev_node < (*it)) {
                previous_nodes.insert(it, prev_node);
            }
            insert_sorted(node->depends_on_nodes_, prev_node);
        }
    }

    for (size_t i = 0; i < node->output_.size(); ++i) {
        if (node->output_[i].desired_state != Ren::eResState::RenderTarget &&
            node->output_[i].desired_state != Ren::eResState::DepthWrite &&
            node->output_[i].desired_state != Ren::eResState::UnorderedAccess &&
            node->output_[i].desired_state != Ren::eResState::CopyDst) {
            continue;
        }

        const size_t before = previous_nodes.size();
        FindPreviousReadInNodes(node->output_[i], previous_nodes);

        // Reads have priority over writes
        if (previous_nodes.size() == before) {
            const int16_t prev_node = FindPreviousWrittenInNode(node->output_[i]);
            if (prev_node != -1) {
                const auto it = std::lower_bound(std::begin(previous_nodes), std::end(previous_nodes), prev_node);
                if (it == std::end(previous_nodes) || prev_node < (*it)) {
                    previous_nodes.insert(it, prev_node);
                }
                insert_sorted(node->depends_on_nodes_, prev_node);
            }
        }
    }

    for (const int16_t i : previous_nodes) {
        FgNode *_node = nodes_[i];
        assert(_node != node);

        if (!_node->visited_) {
            TraverseNodeDependencies_r(_node, recursion_depth + 1, out_node_stack);
        }
    }

    out_node_stack.push_back(node);
}

void Eng::FgBuilder::PrepareAllocResources() {
    std::vector<bool> visited_buffers(buffers_.capacity(), false);
    std::vector<bool> visited_textures(textures_.capacity(), false);

    for (FgNode *cur_node : reordered_nodes_) {
        for (const FgResource &r : cur_node->input_) {
            if (r.type == eFgResType::Buffer) {
                visited_buffers[r.index] = true;
            } else if (r.type == eFgResType::Texture) {
                FgAllocTex &tex = textures_[r.index];
                tex.desc.usage |= Ren::TexUsageFromState(r.desired_state);
                visited_textures[r.index] = true;
            }
        }
        for (const FgResource &r : cur_node->output_) {
            if (r.type == eFgResType::Buffer) {
                visited_buffers[r.index] = true;
            } else if (r.type == eFgResType::Texture) {
                FgAllocTex &tex = textures_[r.index];
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

void Eng::FgBuilder::BuildAliases() {
    auto disjoint_lifetimes = [](const fg_range_t &r1, const fg_range_t &r2) -> bool {
        if (!r1.is_used() || !r2.is_used() || !r1.can_alias() || !r2.can_alias()) {
            return false;
        }
        return r1.last_used_node() < r2.first_used_node() || r2.last_used_node() < r1.first_used_node();
    };

    // Gather node ranges
    for (int i = 0; i < int(reordered_nodes_.size()); ++i) {
        const FgNode *node = reordered_nodes_[i];
        for (const auto &res : node->input_) {
            if (res.type == eFgResType::Texture) {
                fg_range_t &lifetime = textures_[res.index].lifetime;
                lifetime.first_read_node = std::min(lifetime.first_read_node, i);
                lifetime.last_read_node = std::max(lifetime.last_read_node, i);
            } else if (res.type == eFgResType::Buffer) {
                fg_range_t &lifetime = buffers_[res.index].lifetime;
                lifetime.first_read_node = std::min(lifetime.first_read_node, i);
                lifetime.last_read_node = std::max(lifetime.last_read_node, i);
            }
        }
        for (const auto &res : node->output_) {
            if (res.type == eFgResType::Texture) {
                fg_range_t &lifetime = textures_[res.index].lifetime;
                lifetime.first_write_node = std::min(lifetime.first_write_node, i);
                lifetime.last_write_node = std::max(lifetime.last_write_node, i);
            } else if (res.type == eFgResType::Buffer) {
                fg_range_t &lifetime = buffers_[res.index].lifetime;
                lifetime.first_write_node = std::min(lifetime.first_write_node, i);
                lifetime.last_write_node = std::max(lifetime.last_write_node, i);
            }
        }
    }

    alias_chains_.clear();
    alias_chains_.resize(textures_.size());

    std::vector<int> aliases(textures_.size(), -1);
    for (auto i = textures_.begin(); i != textures_.end(); ++i) {
        const FgAllocTex &tex1 = *i;
        if (tex1.external || tex1.history_index != -1 || tex1.history_of != -1) {
            continue;
        }

        for (auto j = textures_.begin(); j < i; ++j) {
            const FgAllocTex &tex2 = *j;
            if (tex2.external || tex2.history_index != -1 || tex2.history_of != -1 || aliases[j.index()] != -1) {
                continue;
            }

            if (tex1.desc.format == tex2.desc.format && tex1.desc.w == tex2.desc.w && tex1.desc.h == tex2.desc.h &&
                tex1.desc.mip_count == tex2.desc.mip_count) {
                bool disjoint = disjoint_lifetimes(tex1.lifetime, tex2.lifetime);
                for (const int alias : alias_chains_[j.index()]) {
                    if (alias == i.index()) {
                        continue;
                    }
                    const fg_range_t &range = textures_[alias].lifetime;
                    disjoint &= disjoint_lifetimes(range, tex2.lifetime);
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

        std::sort(begin(chain), end(chain), [this](const int lhs, const int rhs) {
            return textures_[lhs].lifetime.last_used_node() < textures_[rhs].lifetime.first_used_node();
        });

        FgAllocTex &first_tex = textures_[chain[0]];
        assert(first_tex.alias_of == -1);

        for (int i = 1; i < int(chain.size()); ++i) {
            FgAllocTex &next_tex = textures_[chain[i]];
            next_tex.alias_of = chain[0];
            // propagate usage
            first_tex.desc.usage |= next_tex.desc.usage;
        }

        if (chain[0] != j) {
            alias_chains_[chain[0]] = std::move(chain);
        }
    }
}

void Eng::FgBuilder::BuildResourceLinkedLists() {
    OPTICK_EVENT();
    std::vector<FgResource *> all_resources;

    auto resource_compare = [](const FgResource *lhs, const FgResource *rhs) {
        return FgResource::LessThanTypeAndIndex(*lhs, *rhs);
    };

    for (FgNode *cur_node : reordered_nodes_) {
        for (size_t i = 0; i < cur_node->input_.size(); i++) {
            FgResource *r = &cur_node->input_[i];

            auto it = std::lower_bound(begin(all_resources), end(all_resources), r, resource_compare);
            if (it != end(all_resources) && !FgResource::LessThanTypeAndIndex(*r, **it)) {
                (*it)->next_use = r;
                (*it) = r;
            } else {
                if (r->type == eFgResType::Texture && textures_[r->index].alias_of != -1) {
                    const auto &chain = alias_chains_[textures_[r->index].alias_of];
                    auto curr_it = std::find(begin(chain), end(chain), r->index);
                    assert(curr_it != end(chain) && curr_it != begin(chain));

                    FgResource to_find;
                    to_find.type = eFgResType::Texture;
                    to_find.index = *--curr_it;

                    auto it2 = std::lower_bound(begin(all_resources), end(all_resources), &to_find, resource_compare);
                    if (it2 != end(all_resources) && !FgResource::LessThanTypeAndIndex(to_find, **it2)) {
                        (*it2)->next_use = r;
                    }
                }
                r->next_use = nullptr;
                all_resources.insert(it, r);
            }
        }

        for (size_t i = 0; i < cur_node->output_.size(); i++) {
            FgResource *r = &cur_node->output_[i];

            auto it = std::lower_bound(begin(all_resources), end(all_resources), r, resource_compare);
            if (it != end(all_resources) && !FgResource::LessThanTypeAndIndex(*r, **it)) {
                (*it)->next_use = r;
                (*it) = r;
            } else {
                if (r->type == eFgResType::Texture && textures_[r->index].alias_of != -1) {
                    const auto &chain = alias_chains_[textures_[r->index].alias_of];
                    auto curr_it = std::find(begin(chain), end(chain), r->index);
                    assert(curr_it != end(chain) && curr_it != begin(chain));

                    FgResource to_find;
                    to_find.type = eFgResType::Texture;
                    to_find.index = *--curr_it;

                    auto it2 = std::lower_bound(begin(all_resources), end(all_resources), &to_find, resource_compare);
                    if (it2 != end(all_resources) && !FgResource::LessThanTypeAndIndex(to_find, **it2)) {
                        (*it2)->next_use = r;
                    }
                }
                r->next_use = nullptr;
                all_resources.insert(it, r);
            }
        }
    }
}

void Eng::FgBuilder::Compile(Ren::Span<const FgResRef> backbuffer_sources) {
    OPTICK_EVENT();

    reordered_nodes_.clear();
    reordered_nodes_.reserve(nodes_.size());

    if (!backbuffer_sources.empty()) {
        //
        // Sorting and culling
        //
        Ren::SmallVector<FgNode *, 32> written_in_nodes;
        for (int i = 0; i < int(backbuffer_sources.size()); ++i) {
            const int16_t prev_node = FindPreviousWrittenInNode(backbuffer_sources[i]);
            if (prev_node != -1) {
                written_in_nodes.push_back(nodes_[prev_node]);
            }
        }

        for (FgNode *node : written_in_nodes) {
            TraverseNodeDependencies_r(node, 0, reordered_nodes_);
        }

        if (FgBuilderInternal::EnableNodesReordering && !reordered_nodes_.empty()) {
            std::vector<FgNode *> scheduled_nodes;
            scheduled_nodes.reserve(reordered_nodes_.size());

            // schedule the first node
            scheduled_nodes.push_back(reordered_nodes_.front());
            reordered_nodes_.erase(reordered_nodes_.begin());

            while (!reordered_nodes_.empty()) {
                int best_ovelap_score = -1;
                int best_candidate = 0;

                for (int i = 0; i < int(reordered_nodes_.size()); ++i) {
                    int overlap_score = 0;

                    for (int j = int(scheduled_nodes.size()) - 1; j >= 0; --j) {
                        if (DependsOn_r(reordered_nodes_[i]->index_, scheduled_nodes[j]->index_)) {
                            break;
                        }
                        ++overlap_score;
                    }

                    if (overlap_score <= best_ovelap_score) {
                        continue;
                    }

                    bool possible_candidate = true;
                    for (int j = 0; j < i; ++j) {
                        if (DependsOn_r(reordered_nodes_[i]->index_, reordered_nodes_[j]->index_)) {
                            possible_candidate = false;
                            break;
                        }
                    }

                    for (int j = 0; j < int(reordered_nodes_.size()) && possible_candidate; ++j) {
                        if (j == i) {
                            continue;
                        }
                        for (const auto &output : reordered_nodes_[i]->output_) {
                            for (const auto &input : reordered_nodes_[j]->input_) {
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

                scheduled_nodes.push_back(reordered_nodes_[best_candidate]);
                reordered_nodes_.erase(reordered_nodes_.begin() + best_candidate);
            }

            reordered_nodes_ = std::move(scheduled_nodes);
        }
    } else {
        // Use all nodes as is
        reordered_nodes_.assign(begin(nodes_), end(nodes_));
    }

    PrepareAllocResources();
    if (FgBuilderInternal::EnableTextureAliasing) {
        BuildAliases();
    }

    for (FgNode *node : reordered_nodes_) {
        // Must be allocated in order of node execution (because of how aliasing works)
        AllocateNeededResources(*node);
    }

#if 0
    ctx_.log()->Info("======================================================================");
    uint32_t total_buffer_memory = 0;
    for (const FgAllocBuf &buf : buffers_) {
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
    for (const FgAllocTex &tex : textures_) {
        if (tex.alias_of != -1) {
            const FgAllocTex &orig_tex = textures_[tex.alias_of];
            ctx_.log()->Info("Tex %-24.24s alias of %12s\t\t| %-f MB", tex.name.c_str(), orig_tex.name.c_str(),
                             float(total_texture_memory) * 0.000001f);
            continue;
        }
        if (tex.strong_ref) {
            total_texture_memory += EstimateMemory(tex.strong_ref->params);
            ctx_.log()->Info("Tex %-24.24s (%4ix%-4i %f MB)\t\t| %f MB", tex.name.c_str(), tex.desc.w, tex.desc.h,
                             float(EstimateMemory(tex.ref->params)) * 0.000001f,
                             float(total_texture_memory) * 0.000001f);
        } else if (tex.ref) {
            not_handled_textures.push_back(tex.ref);
        }
    }
    ctx_.log()->Info("----------------------------------------------------------------------");
    ctx_.log()->Info("Graph owned texture memory:\t\t\t\t\t| %f MB", float(total_texture_memory) * 0.000001f);
    ctx_.log()->Info("----------------------------------------------------------------------");
    for (const auto &ref : not_handled_textures) {
        total_texture_memory += EstimateMemory(ref->params);
        ctx_.log()->Info("Tex %-24.24s (%4ix%-4i %f MB)\t\t| %f MB", ref->name().c_str(), ref->params.w, ref->params.h,
                         float(EstimateMemory(ref->params)) * 0.000001f, float(total_texture_memory) * 0.000001f);
    }
    ctx_.log()->Info("----------------------------------------------------------------------");
    ctx_.log()->Info("Total graph texture memory:\t\t\t\t\t| %f MB", float(total_texture_memory) * 0.000001f);
    ctx_.log()->Info("======================================================================");
#endif
}

void Eng::FgBuilder::Execute() {
    OPTICK_EVENT();

    // Swap history images
    for (FgAllocTex &tex : textures_) {
        if (tex.history_index != -1) {
            auto &hist_tex = textures_.at(tex.history_index);
            std::swap(tex.ref, hist_tex.ref);
        }
    }
    // Reset resources
    for (FgAllocBuf &buf : buffers_) {
        buf._generation = 0;
        buf.used_in_stages = Ren::eStageBits::None;
        if (buf.ref) {
            buf.used_in_stages = StageBitsForState(buf.ref->resource_state);
        }
    }
    for (FgAllocTex &tex : textures_) {
        tex._generation = 0;
        tex.used_in_stages = Ren::eStageBits::None;
        if (tex.ref) {
            tex.used_in_stages = StageBitsForState(tex.ref->resource_state);
            // Needed to clear the texture initially
            tex.used_in_stages |= Ren::eStageBits::Transfer;
        } else if (tex.arr) {
            tex.used_in_stages = StageBitsForState(tex.arr->resource_state);
        }
    }

    BuildResourceLinkedLists();

#if defined(USE_GL_RENDER)
    rast_state_.Apply();
#endif

    node_timings_[ctx_.backend_frame()].clear();
    // Write timestamp at the beginning of execution
    const int query_beg = ctx_.WriteTimestamp(true);

    for (FgNode *cur_node : reordered_nodes_) {
        OPTICK_GPU_EVENT("Execute Node");
        OPTICK_TAG("Node Name", cur_node->name().data());

#if !defined(NDEBUG) && defined(USE_GL_RENDER)
        Ren::ResetGLState();
#endif

        Ren::DebugMarker _(ctx_.api_ctx(), ctx_.current_cmd_buf(), cur_node->name());

        // Start timestamp
        node_timing_t &node_interval = node_timings_[ctx_.backend_frame()].emplace_back();
        node_interval.name = cur_node->name();
        node_interval.query_beg = ctx_.WriteTimestamp(true);

        InsertResourceTransitions(*cur_node);

        cur_node->Execute(*this);

        // End timestamp
        node_interval.query_end = ctx_.WriteTimestamp(false);
    }

    // Write timestamp at the end of execution
    node_timing_t &initial_interval = node_timings_[ctx_.backend_frame()].emplace_back();
    initial_interval.name = "GRAPH TOTAL";
    initial_interval.query_beg = query_beg;
    initial_interval.query_end = ctx_.WriteTimestamp(false);
}

void Eng::FgBuilder::InsertResourceTransitions(FgNode &node) {
    OPTICK_GPU_EVENT("InsertResourceTransitions");
    auto cmd_buf = reinterpret_cast<VkCommandBuffer>(ctx_.current_cmd_buf());

    Ren::SmallVector<Ren::TransitionInfo, 32> res_transitions;
    Ren::eStageBits src_stages = Ren::eStageBits::None;
    Ren::eStageBits dst_stages = Ren::eStageBits::None;

    for (const FgResource &res : node.input_) {
        HandleResourceTransition(res, res_transitions, src_stages, dst_stages);
    }

    for (const FgResource &res : node.output_) {
        HandleResourceTransition(res, res_transitions, src_stages, dst_stages);
    }

    TransitionResourceStates(ctx_.api_ctx(), cmd_buf, src_stages, dst_stages, res_transitions);
}

void Eng::FgBuilder::CheckResourceStates(FgNode &node) {
    for (const FgResource &res : node.input_) {
        if (res.type == eFgResType::Buffer) {
            const FgAllocBuf &buf = buffers_[res.index];
            assert(buf.ref->resource_state == res.desired_state && "Buffer is in unexpected state!");
        } else if (res.type == eFgResType::Texture) {
            const FgAllocTex &tex = textures_[res.index];
            assert(tex.ref->resource_state == res.desired_state && "Texture is in unexpected state!");
        }
    }
    for (const FgResource &res : node.output_) {
        if (res.type == eFgResType::Buffer) {
            const FgAllocBuf &buf = buffers_[res.index];
            assert(buf.ref->resource_state == res.desired_state && "Buffer is in unexpected state!");
        } else if (res.type == eFgResType::Texture) {
            const FgAllocTex &tex = textures_[res.index];
            assert(tex.ref->resource_state == res.desired_state && "Texture is in unexpected state!");
        }
    }
}

void Eng::FgBuilder::HandleResourceTransition(const FgResource &res,
                                              Ren::SmallVectorImpl<Ren::TransitionInfo> &res_transitions,
                                              Ren::eStageBits &src_stages, Ren::eStageBits &dst_stages) {
    for (const FgResource *next_res = res.next_use; next_res; next_res = next_res->next_use) {
        if (next_res->desired_state != res.desired_state ||
            next_res->desired_state == Ren::eResState::UnorderedAccess ||
            next_res->desired_state == Ren::eResState::CopyDst) {
            break;
        }
        dst_stages |= next_res->stages;
    }

    if (res.type == eFgResType::Buffer) {
        FgAllocBuf &buf = buffers_.at(res.index);
        if (buf.ref->resource_state != res.desired_state ||
            buf.ref->resource_state == Ren::eResState::UnorderedAccess ||
            buf.ref->resource_state == Ren::eResState::CopyDst) {
            src_stages |= buf.used_in_stages;
            dst_stages |= res.stages;
            buf.used_in_stages = Ren::eStageBits::None;
            res_transitions.emplace_back(buf.ref.get(), res.desired_state);
        }
        buf.used_in_stages |= res.stages;
    } else if (res.type == eFgResType::Texture) {
        FgAllocTex *tex = &textures_.at(res.index);

        if (tex->alias_of != -1) {
            tex = &textures_.at(tex->alias_of);
            assert(tex->alias_of == -1);
        }

        if (tex->tex3d) {
            if (tex->tex3d->resource_state != res.desired_state ||
                tex->tex3d->resource_state == Ren::eResState::UnorderedAccess ||
                tex->tex3d->resource_state == Ren::eResState::CopyDst) {
                src_stages |= tex->used_in_stages;
                dst_stages |= res.stages;
                tex->used_in_stages = Ren::eStageBits::None;
                res_transitions.emplace_back(tex->tex3d, res.desired_state);
            }
        } else if (tex->arr) {
            if (tex->arr->resource_state != res.desired_state ||
                tex->arr->resource_state == Ren::eResState::UnorderedAccess ||
                tex->arr->resource_state == Ren::eResState::CopyDst) {
                src_stages |= tex->used_in_stages;
                dst_stages |= res.stages;
                tex->used_in_stages = Ren::eStageBits::None;
                res_transitions.emplace_back(tex->arr, res.desired_state);
            }
        } else {
            if (tex->ref->resource_state != res.desired_state ||
                tex->ref->resource_state == Ren::eResState::UnorderedAccess ||
                tex->ref->resource_state == Ren::eResState::CopyDst) {
                src_stages |= tex->used_in_stages;
                dst_stages |= res.stages;
                tex->used_in_stages = Ren::eStageBits::None;
                res_transitions.emplace_back(tex->ref.get(), res.desired_state);
            }
        }
        tex->used_in_stages |= res.stages;
    }
}