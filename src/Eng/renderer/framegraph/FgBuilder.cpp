#include "FgBuilder.h"

#include <Ren/Context.h>
#include <Ren/DebugMarker.h>
#include <optick/optick.h>

#include "../PrimDraw.h"
#include "../utils/ShaderLoader.h"
#include "FgNode.h"

#include "../renderer/shaders/clear_buffer_interface.h"
#include "../renderer/shaders/clear_image_interface.h"

namespace FgBuilderInternal {
extern const bool EnableResourceAliasing = true;
extern const bool EnableNodesReordering = false;

void insert_sorted(Ren::SmallVectorImpl<int16_t> &vec, const int16_t val) {
    const auto it = std::lower_bound(std::begin(vec), std::end(vec), val);
    if (it == std::end(vec) || val < (*it)) {
        vec.insert(it, val);
    }
}
} // namespace FgBuilderInternal

Eng::FgBuilder::FgBuilder(Ren::Context &ctx, Eng::ShaderLoader &sh, PrimDraw &prim_draw)
    : ctx_(ctx), sh_(sh), prim_draw_(prim_draw), alloc_buf_(new char[AllocBufSize]),
      alloc_(alloc_buf_.get(), AllocBufSize) {
    pi_clear_image_[int(Ren::eTexFormat::RGBA8)] = sh.LoadPipeline("internal/clear_image@RGBA8.comp.glsl");
    pi_clear_image_[int(Ren::eTexFormat::R32F)] = sh.LoadPipeline("internal/clear_image@R32F.comp.glsl");
    pi_clear_image_[int(Ren::eTexFormat::R16F)] = sh.LoadPipeline("internal/clear_image@R16F.comp.glsl");
    pi_clear_image_[int(Ren::eTexFormat::R8)] = sh.LoadPipeline("internal/clear_image@R8.comp.glsl");
    pi_clear_image_[int(Ren::eTexFormat::R32UI)] = sh.LoadPipeline("internal/clear_image@R32UI.comp.glsl");
    pi_clear_image_[int(Ren::eTexFormat::RG8)] = sh.LoadPipeline("internal/clear_image@RG8.comp.glsl");
    pi_clear_image_[int(Ren::eTexFormat::RG16F)] = sh.LoadPipeline("internal/clear_image@RG16F.comp.glsl");
    pi_clear_image_[int(Ren::eTexFormat::RG32F)] = sh.LoadPipeline("internal/clear_image@RG32F.comp.glsl");
    pi_clear_image_[int(Ren::eTexFormat::RG11F_B10F)] = sh.LoadPipeline("internal/clear_image@RG11F_B10F.comp.glsl");
    pi_clear_image_[int(Ren::eTexFormat::RGBA32F)] = sh.LoadPipeline("internal/clear_image@RGBA32F.comp.glsl");
    pi_clear_image_[int(Ren::eTexFormat::RGBA16F)] = sh.LoadPipeline("internal/clear_image@RGBA16F.comp.glsl");

    pi_clear_buffer_ = sh.LoadPipeline("internal/clear_buffer.comp.glsl");
}

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

void Eng::FgBuilder::GetResourceFrameLifetime(const FgAllocBuf &b, uint16_t out_lifetime[2][2]) const {
    out_lifetime[0][0] = out_lifetime[1][0] = uint16_t(b.lifetime.first_used_node());
    out_lifetime[0][1] = out_lifetime[1][1] = uint16_t(b.lifetime.last_used_node());
}

void Eng::FgBuilder::GetResourceFrameLifetime(const FgAllocTex &t, uint16_t out_lifetime[2][2]) const {
    if (t.history_of == -1 && t.history_index == -1) {
        // Non-history resource, same lifetime in both N and N+1 frames
        out_lifetime[0][0] = out_lifetime[1][0] = t.lifetime.first_used_node();
        out_lifetime[0][1] = out_lifetime[1][1] = t.lifetime.last_used_node() + 1;
    } else if (t.history_index != -1) {
        // Frame N
        out_lifetime[0][0] = t.lifetime.first_used_node();
        out_lifetime[0][1] = uint16_t(reordered_nodes_.size());
        // Frame N+1
        const FgAllocTex &hist_tex = textures_[t.history_index];
        out_lifetime[1][0] = 0;
        out_lifetime[1][1] = hist_tex.lifetime.last_used_node() + 1;
    } else {
        // Frame N
        assert(t.history_of != -1);
        out_lifetime[0][0] = 0;
        out_lifetime[0][1] = t.lifetime.last_used_node() + 1;
        // Frame N+1
        const FgAllocTex &hist_tex = textures_[t.history_of];
        out_lifetime[1][0] = hist_tex.lifetime.first_used_node();
        out_lifetime[1][1] = uint16_t(reordered_nodes_.size());
    }
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

Eng::FgResRef Eng::FgBuilder::ReadBuffer(const Ren::WeakBufRef &ref, const Ren::eResState desired_state,
                                         const Ren::eStageBits stages, FgNode &node, const int slot_index) {
    FgResource ret;
    ret.type = eFgResType::Buffer;

    const uint16_t *pbuf_index = name_to_buffer_.Find(ref->name());
    if (!pbuf_index) {
        FgAllocBuf new_buf;
        new_buf.name = ref->name().c_str();
        new_buf.desc = FgBufDesc{ref->type(), ref->size()};
        new_buf.external = true;

        ret.index = buffers_.emplace(new_buf);
        name_to_buffer_[new_buf.name] = ret.index;
    } else {
        ret.index = *pbuf_index;
    }

    FgAllocBuf &buf = buffers_[ret.index];
    assert(buf.desc.size == ref->size() && buf.desc.type == ref->type());
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

Eng::FgResRef Eng::FgBuilder::ReadTexture(const Ren::WeakTexRef &ref, const Ren::eResState desired_state,
                                          const Ren::eStageBits stages, FgNode &node) {
    FgResource ret;
    ret.type = eFgResType::Texture;

    const uint16_t *ptex_index = name_to_texture_.Find(ref->name());
    if (!ptex_index) {
        FgAllocTex new_tex;
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

Eng::FgResRef Eng::FgBuilder::ReadHistoryTexture(const FgResRef handle, const Ren::eResState desired_state,
                                                 const Ren::eStageBits stages, FgNode &node) {
    assert(handle.type == eFgResType::Texture);

    FgAllocTex *orig_tex = &textures_[handle.index];
    if (orig_tex->history_index == -1) {
        // allocate new history texture
        FgAllocTex new_tex;
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

Eng::FgResRef Eng::FgBuilder::WriteBuffer(const Ren::WeakBufRef &ref, const Ren::eResState desired_state,
                                          const Ren::eStageBits stages, FgNode &node) {
    FgResource ret;
    ret.type = eFgResType::Buffer;

    const uint16_t *pbuf_index = name_to_buffer_.Find(ref->name());
    if (!pbuf_index) {
        FgAllocBuf new_buf;
        new_buf.name = ref->name().c_str();
        new_buf.desc = FgBufDesc{ref->type(), ref->size()};
        new_buf.external = true;

        ret.index = buffers_.emplace(new_buf);
        name_to_buffer_[new_buf.name] = ret.index;
    } else {
        ret.index = *pbuf_index;
    }

    FgAllocBuf &buf = buffers_[ret.index];
    assert(buf.desc.size == ref->size() && buf.desc.type == ref->type());
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

Eng::FgResRef Eng::FgBuilder::WriteTexture(std::string_view name, const Ren::TexParams &p,
                                           const Ren::eResState desired_state, const Ren::eStageBits stages,
                                           FgNode &node) {
    FgResource ret;
    ret.type = eFgResType::Texture;

    const uint16_t *ptex_index = name_to_texture_.Find(name);
    if (!ptex_index) {
        FgAllocTex new_tex;
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

Eng::FgResRef Eng::FgBuilder::WriteTexture(const Ren::WeakTexRef &ref, const Ren::eResState desired_state,
                                           const Ren::eStageBits stages, FgNode &node, const int slot_index) {
    FgResource ret;
    ret.type = eFgResType::Texture;

    const uint16_t *ptex_index = name_to_texture_.Find(ref->name());
    if (!ptex_index) {
        FgAllocTex new_tex;
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
    } else if (slot_index < int(node.output_.size()) && node.output_[slot_index]) {
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

Eng::FgResRef Eng::FgBuilder::MakeTextureResource(const Ren::WeakTexRef &ref) {
    FgResource ret;
    ret.type = eFgResType::Texture;

    const uint16_t *ptex_index = name_to_texture_.Find(ref->name());
    if (!ptex_index) {
        FgAllocTex new_tex;
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

void Eng::FgBuilder::AllocateNeededResources_Simple() {
    for (auto it = std::begin(buffers_); it != std::end(buffers_); ++it) {
        FgAllocBuf &buf = *it;
        if (buf.external || buf.alias_of != -1 || !buf.lifetime.is_used()) {
            continue;
        }
        assert(!buf.ref);
        buf.strong_ref = ctx_.LoadBuffer(buf.name, buf.desc.type, buf.desc.size, 16, ctx_.default_mem_allocs());
        buf.ref = buf.strong_ref;
        for (int i = 0; i < int(buf.desc.views.size()); ++i) {
            const int view_index = buf.ref->AddBufferView(buf.desc.views[i]);
            assert(view_index == i);
        }
    }
    for (auto it = std::begin(buffers_); it != std::end(buffers_); ++it) {
        FgAllocBuf &buf = *it;
        if (buf.external || buf.alias_of == -1 || !buf.lifetime.is_used()) {
            continue;
        }
        const FgAllocBuf &orig_buf = buffers_.at(buf.alias_of);
        assert(orig_buf.alias_of == -1);
        buf.ref = orig_buf.ref;
        buf.strong_ref = {};
        ctx_.log()->Info("Buf %s will be alias of %s", buf.name.c_str(), orig_buf.name.c_str());
    }
    for (auto it = std::begin(textures_); it != std::end(textures_); ++it) {
        FgAllocTex &tex = *it;
        if (tex.external || tex.alias_of != -1 || !tex.lifetime.is_used()) {
            continue;
        }

        if (tex.history_index != -1) {
            FgAllocTex &hist_tex = textures_.at(tex.history_index);
            // combine usage flags
            tex.desc.usage |= hist_tex.desc.usage;
            hist_tex.desc = tex.desc;
        }
        if (tex.history_of != -1) {
            FgAllocTex &hist_tex = textures_.at(tex.history_of);
            // combine usage flags
            tex.desc.usage |= hist_tex.desc.usage;
            hist_tex.desc = tex.desc;
        }

        assert(!tex.ref);
        ctx_.log()->Info("Alloc tex %s (%ix%i %f MB)", tex.name.c_str(), tex.desc.w, tex.desc.h,
                         float(GetDataLenBytes(tex.desc)) * 0.000001f);
        Ren::eTexLoadStatus status;
        tex.strong_ref = ctx_.LoadTexture(tex.name, tex.desc, ctx_.default_mem_allocs(), &status);
        tex.ref = tex.strong_ref;
        assert(status == Ren::eTexLoadStatus::CreatedDefault || status == Ren::eTexLoadStatus::Found ||
               status == Ren::eTexLoadStatus::Reinitialized);
    }
    for (auto it = std::begin(textures_); it != std::end(textures_); ++it) {
        FgAllocTex &tex = *it;
        if (tex.external || tex.alias_of == -1 || !tex.lifetime.is_used()) {
            continue;
        }
        assert(!tex.ref);
        const FgAllocTex &orig_tex = textures_.at(tex.alias_of);
        assert(orig_tex.alias_of == -1);
        tex.ref = orig_tex.ref;
        tex.strong_ref = {};
        ctx_.log()->Info("Tex %s will be alias of %s", tex.name.c_str(), orig_tex.name.c_str());
    }
}

void Eng::FgBuilder::ClearResources_Simple() {
    std::vector<Ren::BufRef> buffers_to_clear;
    std::vector<Ren::TexRef> textures_to_clear;

    for (const FgAllocBuf &b : buffers_) {
        if (b.external || !b.lifetime.is_used()) {
            continue;
        }
        if (b.strong_ref && b.alias_of == -1) {
            buffers_to_clear.push_back(b.strong_ref);
        }
    }

    for (const FgAllocTex &t : textures_) {
        if (t.external || !t.lifetime.is_used()) {
            continue;
        }
        if (t.strong_ref && t.alias_of == -1) {
            textures_to_clear.push_back(t.strong_ref);
        }
    }

    if (!textures_to_clear.empty() || !buffers_to_clear.empty()) { // Clear resources
        Ren::CommandBuffer cmd_buf = ctx_.BegTempSingleTimeCommands();

        std::vector<Ren::TransitionInfo> transitions;
        transitions.reserve(textures_to_clear.size() + buffers_to_clear.size());
        for (const Ren::TexRef &t : textures_to_clear) {
            const Ren::TexParams p = t->params;
            if (p.usage & Ren::eTexUsage::Transfer) {
                transitions.emplace_back(t.get(), Ren::eResState::CopyDst);
            } else if (p.usage & Ren::eTexUsage::Storage) {
                transitions.emplace_back(t.get(), Ren::eResState::UnorderedAccess);
            } else if (p.usage & Ren::eTexUsage::RenderTarget) {
                if (Ren::IsDepthFormat(t->params.format)) {
                    transitions.emplace_back(t.get(), Ren::eResState::DepthWrite);
                } else {
                    transitions.emplace_back(t.get(), Ren::eResState::RenderTarget);
                }
            }
        }
        for (const Ren::BufRef &b : buffers_to_clear) {
            transitions.emplace_back(b.get(), Ren::eResState::CopyDst);
        }
        TransitionResourceStates(ctx_.api_ctx(), cmd_buf, Ren::AllStages, Ren::AllStages, transitions);

        for (Ren::TexRef &t : textures_to_clear) {
            if (t->resource_state == Ren::eResState::CopyDst) {
                ClearImage_AsTransfer(t, cmd_buf);
            } else if (t->resource_state == Ren::eResState::UnorderedAccess) {
                ClearImage_AsStorage(t, cmd_buf);
            } else if (t->resource_state == Ren::eResState::RenderTarget ||
                       t->resource_state == Ren::eResState::DepthWrite) {
                ClearImage_AsTarget(t, cmd_buf);
            } else {
                assert(false);
            }
        }
        for (Ren::BufRef &b : buffers_to_clear) {
            if (b->resource_state == Ren::eResState::CopyDst) {
                ClearBuffer_AsTransfer(b, cmd_buf);
            } else if (b->resource_state == Ren::eResState::UnorderedAccess) {
                ClearBuffer_AsStorage(b, cmd_buf);
            } else if (b->resource_state == Ren::eResState::BuildASWrite) {
                // NOTE: Skipped
            } else {
                assert(false);
            }
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

    ReleaseMemHeaps();
}

int16_t Eng::FgBuilder::FindPreviousWrittenInNode(const FgResRef handle) {
    Ren::SmallVectorImpl<fg_node_slot_t> *written_in_nodes = nullptr;
    if (handle.type == eFgResType::Buffer) {
        written_in_nodes = &buffers_[handle.index].written_in_nodes;
    } else if (handle.type == eFgResType::Texture) {
        written_in_nodes = &textures_[handle.index].written_in_nodes;
    }

    assert(written_in_nodes);
    if (!written_in_nodes) {
        return -1;
    }

    for (const fg_node_slot_t i : *written_in_nodes) {
        const FgNode *node = nodes_[i.node_index];
        assert(node->output_[i.slot_index].type == handle.type && node->output_[i.slot_index].index == handle.index);
        if (node->output_[i.slot_index].write_count == handle.write_count - 1) {
            return i.node_index;
        }
    }
    return -1;
}

void Eng::FgBuilder::FindPreviousReadInNodes(const FgResRef handle, Ren::SmallVectorImpl<int16_t> &out_nodes) {
    Ren::SmallVectorImpl<fg_node_slot_t> *read_in_nodes = nullptr;
    if (handle.type == eFgResType::Buffer) {
        read_in_nodes = &buffers_[handle.index].read_in_nodes;
    } else if (handle.type == eFgResType::Texture) {
        read_in_nodes = &textures_[handle.index].read_in_nodes;
    }

    assert(read_in_nodes);
    if (!read_in_nodes) {
        return;
    }

    for (const fg_node_slot_t i : *read_in_nodes) {
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
    // Initialize history res description (they are allowed to be declared before actual resource)
    for (auto it = std::begin(textures_); it != std::end(textures_); ++it) {
        FgAllocTex &tex = *it;
        if (tex.history_of != -1) {
            assert(textures_[tex.history_of].history_index == it.index());
            tex.desc = textures_[tex.history_of].desc;
        }
    }
    // Propagate usage flags
    for (FgNode *cur_node : reordered_nodes_) {
        for (const FgResource &r : cur_node->input_) {
            if (r.type == eFgResType::Buffer) {
            } else if (r.type == eFgResType::Texture) {
                FgAllocTex &tex = textures_[r.index];
                tex.desc.usage |= Ren::TexUsageFromState(r.desired_state);
            }
        }
        for (const FgResource &r : cur_node->output_) {
            if (r.type == eFgResType::Buffer) {
            } else if (r.type == eFgResType::Texture) {
                FgAllocTex &tex = textures_[r.index];
                tex.desc.usage |= Ren::TexUsageFromState(r.desired_state);
            }
        }
    }
}

void Eng::FgBuilder::PrepareResourceLifetimes() {
    auto disjoint_lifetimes = [](const fg_node_range_t &r1, const fg_node_range_t &r2) -> bool {
        if (!r1.is_used() || !r2.is_used() || !r1.can_alias() || !r2.can_alias()) {
            return false;
        }
        return r1.last_used_node() < r2.first_used_node() || r2.last_used_node() < r1.first_used_node();
    };

    // Gather lifetimes
    for (int16_t i = 0; i < int16_t(reordered_nodes_.size()); ++i) {
        const FgNode *node = reordered_nodes_[i];
        for (const auto &res : node->input_) {
            FgAllocRes *this_res = nullptr;
            if (res.type == eFgResType::Buffer) {
                this_res = &buffers_.at(res.index);
            } else /*if (res.type == eFgResType::Texture)*/ {
                assert(res.type == eFgResType::Texture);
                this_res = &textures_.at(res.index);
            }
            this_res->lifetime.first_read_node = std::min(this_res->lifetime.first_read_node, i);
            this_res->lifetime.last_read_node = std::max(this_res->lifetime.last_read_node, i);
        }
        for (const auto &res : node->output_) {
            FgAllocRes *this_res = nullptr;
            if (res.type == eFgResType::Buffer) {
                this_res = &buffers_.at(res.index);
            } else /*if (res.type == eFgResType::Texture)*/ {
                assert(res.type == eFgResType::Texture);
                this_res = &textures_.at(res.index);
            }
            this_res->lifetime.first_write_node = std::min(this_res->lifetime.first_write_node, i);
            this_res->lifetime.last_write_node = std::max(this_res->lifetime.last_write_node, i);
        }
    }

    if (!FgBuilderInternal::EnableResourceAliasing) {
        return;
    }

    tex_alias_chains_.clear();
    buf_alias_chains_.clear();
    tex_alias_chains_.resize(textures_.capacity());
    buf_alias_chains_.resize(buffers_.capacity());

    std::vector<int> tex_aliases(textures_.capacity(), -1);
    for (auto i = textures_.begin(); i != textures_.end(); ++i) {
        const FgAllocTex &tex1 = *i;
        if (tex1.external || tex1.history_index != -1 || tex1.history_of != -1) {
            continue;
        }
        for (auto j = textures_.begin(); j < i; ++j) {
            const FgAllocTex &tex2 = *j;
            if (tex2.external || tex2.history_index != -1 || tex2.history_of != -1 || tex_aliases[j.index()] != -1) {
                continue;
            }
            if (tex1.desc.format == tex2.desc.format && tex1.desc.w == tex2.desc.w && tex1.desc.h == tex2.desc.h &&
                tex1.desc.mip_count == tex2.desc.mip_count) {
                bool disjoint = disjoint_lifetimes(tex1.lifetime, tex2.lifetime);
                for (const int alias : tex_alias_chains_[j.index()]) {
                    if (alias == i.index()) {
                        continue;
                    }
                    const fg_node_range_t &lifetime = textures_[alias].lifetime;
                    disjoint &= disjoint_lifetimes(lifetime, tex2.lifetime);
                }
                if (disjoint) {
                    tex_aliases[i.index()] = j.index();
                    if (tex_alias_chains_[j.index()].empty()) {
                        tex_alias_chains_[j.index()].push_back(j.index());
                    }
                    tex_alias_chains_[j.index()].push_back(i.index());
                    break;
                }
            }
        }
    }

    std::vector<int> buf_aliases(buffers_.capacity(), -1);
    for (auto i = buffers_.begin(); i != buffers_.end(); ++i) {
        const FgAllocBuf &buf1 = *i;
        if (buf1.external) {
            continue;
        }
        for (auto j = buffers_.begin(); j < i; ++j) {
            const FgAllocBuf &buf2 = *j;
            if (buf2.external || buf_aliases[j.index()] != -1) {
                continue;
            }
            if (buf1.desc.type == buf2.desc.type && buf1.desc.size == buf2.desc.size &&
                buf1.desc.views == buf2.desc.views) {
                bool disjoint = disjoint_lifetimes(buf1.lifetime, buf2.lifetime);
                for (const int alias : buf_alias_chains_[j.index()]) {
                    if (alias == i.index()) {
                        continue;
                    }
                    const fg_node_range_t &lifetime = buffers_[alias].lifetime;
                    disjoint &= disjoint_lifetimes(lifetime, buf2.lifetime);
                }
                if (disjoint) {
                    buf_aliases[i.index()] = j.index();
                    if (buf_alias_chains_[j.index()].empty()) {
                        buf_alias_chains_[j.index()].push_back(j.index());
                    }
                    buf_alias_chains_[j.index()].push_back(i.index());
                    break;
                }
            }
        }
    }

    for (int j = 0; j < int(tex_alias_chains_.size()); ++j) {
        auto &chain = tex_alias_chains_[j];
        if (chain.empty()) {
            continue;
        }

        std::sort(std::begin(chain), std::end(chain), [this](const int lhs, const int rhs) {
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
            tex_alias_chains_[chain[0]] = std::move(chain);
        }
    }

    for (int j = 0; j < int(buf_alias_chains_.size()); ++j) {
        auto &chain = buf_alias_chains_[j];
        if (chain.empty()) {
            continue;
        }

        std::sort(std::begin(chain), std::end(chain), [this](const int lhs, const int rhs) {
            return buffers_[lhs].lifetime.last_used_node() < buffers_[rhs].lifetime.first_used_node();
        });

        FgAllocBuf &first_buf = buffers_[chain[0]];
        assert(first_buf.alias_of == -1);

        for (int i = 1; i < int(chain.size()); ++i) {
            FgAllocBuf &next_buf = buffers_[chain[i]];
            next_buf.alias_of = chain[0];
        }

        if (chain[0] != j) {
            buf_alias_chains_[chain[0]] = std::move(chain);
        }
    }
}

void Eng::FgBuilder::BuildResourceLinkedLists() {
    OPTICK_EVENT();
    std::vector<FgResource *> all_resources;
    all_resources.reserve(buffers_.size() + textures_.size());

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
                    const auto &chain = tex_alias_chains_[textures_[r->index].alias_of];
                    auto curr_it = std::find(std::begin(chain), std::end(chain), r->index);
                    assert(curr_it != std::end(chain) && curr_it != std::begin(chain));

                    FgResource to_find;
                    to_find.type = eFgResType::Texture;
                    to_find.index = *--curr_it;

                    auto it2 = std::lower_bound(std::begin(all_resources), std::end(all_resources), &to_find,
                                                resource_compare);
                    if (it2 != std::end(all_resources) && !FgResource::LessThanTypeAndIndex(to_find, **it2)) {
                        (*it2)->next_use = r;
                    }
                } else if (r->type == eFgResType::Buffer && buffers_[r->index].alias_of != -1) {
                    const auto &chain = buf_alias_chains_[buffers_[r->index].alias_of];
                    auto curr_it = std::find(std::begin(chain), std::end(chain), r->index);
                    assert(curr_it != std::end(chain) && curr_it != std::begin(chain));

                    FgResource to_find;
                    to_find.type = eFgResType::Buffer;
                    to_find.index = *--curr_it;

                    auto it2 = std::lower_bound(std::begin(all_resources), std::end(all_resources), &to_find,
                                                resource_compare);
                    if (it2 != std::end(all_resources) && !FgResource::LessThanTypeAndIndex(to_find, **it2)) {
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
                    const auto &chain = tex_alias_chains_[textures_[r->index].alias_of];
                    auto curr_it = std::find(std::begin(chain), std::end(chain), r->index);
                    assert(curr_it != std::end(chain) && curr_it != std::begin(chain));

                    FgResource to_find;
                    to_find.type = eFgResType::Texture;
                    to_find.index = *--curr_it;

                    auto it2 = std::lower_bound(begin(all_resources), end(all_resources), &to_find, resource_compare);
                    if (it2 != end(all_resources) && !FgResource::LessThanTypeAndIndex(to_find, **it2)) {
                        (*it2)->next_use = r;
                    }
                } else if (r->type == eFgResType::Buffer && buffers_[r->index].alias_of != -1) {
                    const auto &chain = buf_alias_chains_[buffers_[r->index].alias_of];
                    auto curr_it = std::find(std::begin(chain), std::end(chain), r->index);
                    assert(curr_it != std::end(chain) && curr_it != std::begin(chain));

                    FgResource to_find;
                    to_find.type = eFgResType::Buffer;
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

    // Connect history resources across N and N+1 frames
    for (auto it = std::begin(textures_); it != std::end(textures_); ++it) {
        FgAllocTex &tex = *it;
        if (!tex.lifetime.is_used() || (tex.history_index == -1 && tex.history_of == -1)) {
            continue;
        }

        if (tex.history_index != -1) {
            auto &hist_tex = textures_.at(tex.history_index);
            if (!hist_tex.ref) {
                continue;
            }
            if (hist_tex.ref) {
                FgNode *tex_node = reordered_nodes_[tex.lifetime.last_used_node()];
                FgResource *last_usage = tex_node->FindUsageOf(eFgResType::Texture, it.index());
                assert(last_usage);
                FgNode *hist_node = reordered_nodes_[hist_tex.lifetime.first_used_node()];
                FgResource *first_usage = hist_node->FindUsageOf(eFgResType::Texture, tex.history_index);
                assert(first_usage);
                last_usage->next_use = first_usage;
            }
        } else if (tex.history_of != -1) {
            auto &hist_tex = textures_.at(tex.history_of);
            assert(hist_tex.ref);

            FgNode *tex_node = reordered_nodes_[tex.lifetime.last_used_node()];
            FgResource *last_usage = tex_node->FindUsageOf(eFgResType::Texture, it.index());
            assert(last_usage);
            FgNode *hist_node = reordered_nodes_[hist_tex.lifetime.first_used_node()];
            FgResource *first_usage = hist_node->FindUsageOf(eFgResType::Texture, tex.history_of);
            assert(first_usage);
            last_usage->next_use = first_usage;
        }
    }
}

void Eng::FgBuilder::Compile(Ren::Span<const FgResRef> backbuffer_sources) {
    using namespace FgBuilderInternal;
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

        for (auto it = rbegin(reordered_nodes_); it != rend(reordered_nodes_); ++it) {
            FgNode *node = *it;
            bool has_consumers =
                std::find(std::begin(written_in_nodes), std::end(written_in_nodes), node) != std::end(written_in_nodes);
            for (size_t i = 0; i < node->output_.size() && !has_consumers; ++i) {
                FgResRef handle = node->output_[i];

                Ren::SmallVectorImpl<fg_node_slot_t> *read_in_nodes = nullptr;
                if (handle.type == eFgResType::Buffer) {
                    read_in_nodes = &buffers_[handle.index].read_in_nodes;
                } else if (handle.type == eFgResType::Texture) {
                    read_in_nodes = &textures_[handle.index].read_in_nodes;
                }

                for (const fg_node_slot_t slot : *read_in_nodes) {
                    const FgNode *_node = nodes_[slot.node_index];
                    if (_node != node && _node->visited_) {
                        has_consumers = true;
                        break;
                    }
                }

                if (has_consumers) {
                    break;
                }

                Ren::SmallVectorImpl<fg_node_slot_t> *written_in = nullptr;
                if (handle.type == eFgResType::Buffer) {
                    written_in = &buffers_[handle.index].written_in_nodes;
                } else if (handle.type == eFgResType::Texture) {
                    written_in = &textures_[handle.index].written_in_nodes;
                }

                for (const fg_node_slot_t slot : *written_in) {
                    const FgNode *_node = nodes_[slot.node_index];
                    if (_node != node && node->visited_) {
                        has_consumers = true;
                        break;
                    }
                }
            }

            if (!has_consumers) {
                node->visited_ = false;
            }
        }

        reordered_nodes_.erase(std::remove_if(begin(reordered_nodes_), end(reordered_nodes_),
                                              [](FgNode *node) { return !node->visited_; }),
                               end(reordered_nodes_));

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
    PrepareResourceLifetimes();
    if (ctx_.capabilities.memory_heaps) {
        // Memory-level aliasing
        const bool success = AllocateNeededResources_MemHeaps();
        if (success) {
            ClearResources_MemHeaps();
        } else {
            // Fallback to separate per-resource allocation
            AllocateNeededResources_Simple();
            ClearResources_Simple();
        }
    } else {
        // Simple per-resource aliasing
        AllocateNeededResources_Simple();
        ClearResources_Simple();
    }

    ctx_.log()->Info("============================================================================");
    { // report buffers
        std::vector<Ren::WeakBufRef> not_handled_buffers;
        not_handled_buffers.reserve(buffers_.size());
        for (const FgAllocBuf &buf : buffers_) {
            if (buf.alias_of != -1) {
                const FgAllocBuf &orig_buf = buffers_[buf.alias_of];
                ctx_.log()->Info("Buf %-24.24s alias of %16s\t| %f MB", buf.name.c_str(), orig_buf.name.c_str(), 0.0f);
                continue;
            }
            if (buf.strong_ref) {
                const Ren::MemAllocation &alloc = buf.strong_ref->mem_alloc();
                if (alloc.pool != 0xffff) {
                    ctx_.log()->Info("Buf %-24.24s \t\t\t\t\t| %f MB", buf.name.c_str(),
                                     float(alloc.block) / (1024.0f * 1024.0f));
                } else {
                    ctx_.log()->Info("Buf %-24.24s (dedicated)\t\t\t| %f MB", buf.name.c_str(),
                                     float(buf.strong_ref->size()) / (1024.0f * 1024.0f));
                }
            } else if (buf.ref) {
                not_handled_buffers.push_back(buf.ref);
            }
        }
        ctx_.log()->Info("----------------------------------------------------------------------------");
        for (const auto &ref : not_handled_buffers) {
            ctx_.log()->Info("Buf %-24.24s \t\t\t\t\t| %f MB", ref->name().c_str(),
                             float(ref->size()) / (1024.0f * 1024.0f));
        }
    }
    ctx_.log()->Info("============================================================================");
    { // report textures
        std::vector<Ren::WeakTexRef> not_handled_textures;
        not_handled_textures.reserve(textures_.size());
        for (const FgAllocTex &tex : textures_) {
            if (tex.alias_of != -1) {
                const FgAllocTex &orig_tex = textures_[tex.alias_of];
                ctx_.log()->Info("Tex %-24.24s alias of %16s\t| %-f MB", tex.name.c_str(), orig_tex.name.c_str(),
                                 float(GetDataLenBytes(tex.ref->params)) / (1024.0f * 1024.0f));
                continue;
            }
            if (tex.strong_ref) {
                ctx_.log()->Info("Tex %-24.24s (%4ix%-4i)\t\t\t| %f MB", tex.name.c_str(), tex.desc.w, tex.desc.h,
                                 float(GetDataLenBytes(tex.ref->params)) / (1024.0f * 1024.0f));
            } else if (tex.ref) {
                not_handled_textures.push_back(tex.ref);
            }
        }
        ctx_.log()->Info("----------------------------------------------------------------------------");
        for (const auto &ref : not_handled_textures) {
            ctx_.log()->Info("Tex %-24.24s (%4ix%-4i)\t\t\t| %f MB", ref->name().c_str(), ref->params.w, ref->params.h,
                             float(GetDataLenBytes(ref->params)) / (1024.0f * 1024.0f));
        }
    }
    ctx_.log()->Info("============================================================================");
    if (!memory_heaps_.empty()) {
        for (int i = 0; i < int(memory_heaps_.size()); ++i) {
            ctx_.log()->Info("Heap[%i] %p\t\t\t\t\t| %f MB", i, memory_heaps_[i].mem,
                             float(memory_heaps_[i].size) / (1024.0f * 1024.0f));
        }
        ctx_.log()->Info("============================================================================");
    }
}

void Eng::FgBuilder::Execute() {
    OPTICK_EVENT();

    Ren::DebugMarker exec_marker(ctx_.api_ctx(), ctx_.current_cmd_buf(), "Eng::FgBuilder::Execute");

    // Swap history images
    for (FgAllocTex &tex : textures_) {
        if (tex.history_index != -1) {
            auto &hist_tex = textures_.at(tex.history_index);
            if (hist_tex.ref) {
                assert(hist_tex.lifetime.is_used());
                std::swap(tex.ref, hist_tex.ref);
            }
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
        }
    }

    BuildResourceLinkedLists();

#if defined(REN_GL_BACKEND)
    rast_state_.Apply();
#endif

    node_timings_[ctx_.backend_frame()].clear();
    // Write timestamp at the beginning of execution
    const int query_beg = ctx_.WriteTimestamp(true);

    for (FgNode *cur_node : reordered_nodes_) {
        OPTICK_GPU_EVENT("Execute Node");
        OPTICK_TAG("Node Name", cur_node->name().data());

#if !defined(NDEBUG) && defined(REN_GL_BACKEND)
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

    Ren::SmallVector<Ren::TransitionInfo, 32> res_transitions;
    Ren::eStageBits src_stages = Ren::eStageBits::None;
    Ren::eStageBits dst_stages = Ren::eStageBits::None;

    for (const FgResource &res : node.input_) {
        HandleResourceTransition(res, res_transitions, src_stages, dst_stages);
    }

    for (const FgResource &res : node.output_) {
        HandleResourceTransition(res, res_transitions, src_stages, dst_stages);
    }

    TransitionResourceStates(ctx_.api_ctx(), ctx_.current_cmd_buf(), src_stages, dst_stages, res_transitions);
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
        if (next_res->desired_state != res.desired_state || IsRWState(next_res->desired_state)) {
            break;
        }
        dst_stages |= next_res->stages;
    }

    if (res.type == eFgResType::Buffer) {
        FgAllocBuf *buf = &buffers_.at(res.index);

        if (buf->alias_of != -1) {
            buf = &buffers_.at(buf->alias_of);
            assert(buf->alias_of == -1);
        }

        if (buf->ref->resource_state == Ren::eResState::Undefined ||
            buf->ref->resource_state == Ren::eResState::Discarded) {
            for (const FgResRef other : buf->overlaps_with) {
                if (other.type == eFgResType::Buffer) {
                    FgAllocBuf *other_buf = &buffers_.at(other.index);
                    src_stages |= other_buf->used_in_stages;
                    dst_stages |= other_buf->aliased_in_stages;
                    assert(other_buf->ref->resource_state != Ren::eResState::Discarded);
                    res_transitions.emplace_back(other_buf->ref.get(), Ren::eResState::Discarded);
                } else if (other.type == eFgResType::Texture) {
                    FgAllocTex *other_tex = &textures_.at(other.index);
                    src_stages |= other_tex->used_in_stages;
                    dst_stages |= other_tex->aliased_in_stages;
                    assert(other_tex->ref->resource_state != Ren::eResState::Discarded);
                    res_transitions.emplace_back(other_tex->ref.get(), Ren::eResState::Discarded);
                }
            }
        }

        if (buf->ref->resource_state != res.desired_state || IsRWState(buf->ref->resource_state)) {
            src_stages |= buf->used_in_stages;
            dst_stages |= res.stages;
            buf->used_in_stages = Ren::eStageBits::None;
            res_transitions.emplace_back(buf->ref.get(), res.desired_state);
        }

        buf->used_in_stages |= res.stages;
    } else if (res.type == eFgResType::Texture) {
        FgAllocTex *tex = &textures_.at(res.index);

        if (tex->alias_of != -1) {
            tex = &textures_.at(tex->alias_of);
            assert(tex->alias_of == -1);
        }

        if (tex->ref->resource_state == Ren::eResState::Undefined ||
            tex->ref->resource_state == Ren::eResState::Discarded) {
            for (const FgResRef other : tex->overlaps_with) {
                if (other.type == eFgResType::Buffer) {
                    FgAllocBuf *other_buf = &buffers_.at(other.index);
                    src_stages |= other_buf->used_in_stages;
                    dst_stages |= other_buf->aliased_in_stages;
                    assert(other_buf->ref->resource_state != Ren::eResState::Discarded);
                    res_transitions.emplace_back(other_buf->ref.get(), Ren::eResState::Discarded);
                } else if (other.type == eFgResType::Texture) {
                    FgAllocTex *other_tex = &textures_.at(other.index);
                    src_stages |= other_tex->used_in_stages;
                    dst_stages |= other_tex->aliased_in_stages;
                    assert(other_tex->ref->resource_state != Ren::eResState::Discarded);
                    res_transitions.emplace_back(other_tex->ref.get(), Ren::eResState::Discarded);
                }
            }
        }

        if (tex->ref->resource_state != res.desired_state || IsRWState(tex->ref->resource_state)) {
            src_stages |= tex->used_in_stages;
            dst_stages |= res.stages;
            tex->used_in_stages = Ren::eStageBits::None;
            res_transitions.emplace_back(tex->ref.get(), res.desired_state);
        }

        tex->used_in_stages |= res.stages;
    }
}

void Eng::FgBuilder::ClearBuffer_AsTransfer(Ren::BufRef &buf, Ren::CommandBuffer cmd_buf) {
    buf->Fill(0, buf->size(), 0, cmd_buf);
}

void Eng::FgBuilder::ClearBuffer_AsStorage(Ren::BufRef &buf, Ren::CommandBuffer cmd_buf) {
    const Ren::Binding bindings[] = {{Ren::eBindTarget::SBufRW, ClearBuffer::OUT_BUF_SLOT, *buf}};

    assert((buf->size() % 4) == 0);

    const Ren::Vec3u grp_count = Ren::Vec3u{
        ((buf->size() / 4) + ClearBuffer::LOCAL_GROUP_SIZE_X - 1u) / ClearBuffer::LOCAL_GROUP_SIZE_X, 1u, 1u};

    ClearBuffer::Params uniform_params;
    uniform_params.data_len = (buf->size() / 4);

    Ren::DispatchCompute(cmd_buf, *pi_clear_buffer_, grp_count, bindings, &uniform_params, sizeof(ClearBuffer::Params),
                         ctx_.default_descr_alloc(), ctx_.log());
}

void Eng::FgBuilder::ClearImage_AsTransfer(Ren::TexRef &tex, Ren::CommandBuffer cmd_buf) {
    // NOTE: we can not really use anything other than zero due to aliasing
    static const float rgba[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    Ren::ClearImage(*tex, rgba, cmd_buf);
}

void Eng::FgBuilder::ClearImage_AsStorage(Ren::TexRef &tex, Ren::CommandBuffer cmd_buf) {
    const Ren::TexParams &p = tex->params;

    const Ren::PipelineRef &pi = pi_clear_image_[int(p.format)];
    assert(pi);

    const Ren::Binding bindings[] = {{Ren::eBindTarget::Image2D, ClearImage::OUT_IMG_SLOT, *tex}};

    const Ren::Vec3u grp_count =
        Ren::Vec3u{(p.w + ClearImage::LOCAL_GROUP_SIZE_X - 1u) / ClearImage::LOCAL_GROUP_SIZE_X,
                   (p.h + ClearImage::LOCAL_GROUP_SIZE_Y - 1u) / ClearImage::LOCAL_GROUP_SIZE_Y, 1u};

    Ren::DispatchCompute(cmd_buf, *pi, grp_count, bindings, nullptr, 0, ctx_.default_descr_alloc(), ctx_.log());
}

void Eng::FgBuilder::ClearImage_AsTarget(Ren::TexRef &tex, Ren::CommandBuffer cmd_buf) {
    const Ren::TexParams &p = tex->params;

    Ren::RenderTarget depth_target;
    Ren::SmallVector<Ren::RenderTarget, 1> color_target;

    if (Ren::IsDepthFormat(p.format)) {
        depth_target = {tex, Ren::eLoadOp::Clear, Ren::eStoreOp::Store};
    } else {
        color_target.emplace_back(tex, Ren::eLoadOp::Clear, Ren::eStoreOp::Store);
    }

    prim_draw_.ClearTarget(cmd_buf, depth_target, color_target);
}
