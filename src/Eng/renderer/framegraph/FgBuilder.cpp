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

Eng::FgContext::FgContext(ShaderLoader &sh) : ctx_(sh.ren_ctx()), sh_(sh) {
    framebuffers_ = std::make_unique<FramebufferPool>();
}

Eng::FgContext::~FgContext() = default;

const Ren::StoragesRef &Eng::FgContext::storages() const { return ctx_.storages(); }

Ren::CommandBuffer Eng::FgContext::cmd_buf() const { return ctx_.current_cmd_buf(); }

Ren::ILog *Eng::FgContext::log() const { return ctx_.log(); }

Ren::DescrMultiPoolAlloc &Eng::FgContext::descr_alloc() const { return ctx_.default_descr_alloc(); }

int Eng::FgContext::backend_frame() const { return ctx_.backend_frame(); }

Ren::BufferROHandle Eng::FgContext::AccessROBuffer(const FgBufROHandle handle) const {
    return buffers_[handle].first.handle;
}

Ren::ImageROHandle Eng::FgContext::AccessROImage(const FgImgROHandle handle) const {
    return images_[handle].first.handle_to_use;
}

Ren::BufferHandle Eng::FgContext::AccessRWBuffer(const FgBufRWHandle handle) const {
    assert(buffers_.GetGeneration(handle.index) + 1 == handle.generation);
    buffers_.SetGeneration(handle.index, handle.generation);
    return buffers_[handle].first.handle;
}

Ren::ImageHandle Eng::FgContext::AccessRWImage(const FgImgRWHandle handle) const {
    assert(images_.GetGeneration(handle.index) + 1 == handle.generation);
    images_.SetGeneration(handle.index, handle.generation);
    return images_[handle].first.handle_to_use;
}

Ren::FramebufferHandle
Eng::FgContext::FindOrCreateFramebuffer(Ren::RenderPassROHandle render_pass, const Ren::FramebufferAttachment &depth,
                                        const Ren::FramebufferAttachment &stencil,
                                        Ren::Span<const Ren::FramebufferAttachment> color_attachments) const {
    return framebuffers_->FindOrCreate(ctx_, render_pass, depth, stencil, color_attachments);
}

Ren::FramebufferHandle
Eng::FgContext::FindOrCreateFramebuffer(Ren::RenderPassROHandle render_pass, Ren::ImageRWHandle depth,
                                        Ren::ImageRWHandle stencil,
                                        Ren::Span<const Ren::ImageRWHandle> color_attachments) const {
    return framebuffers_->FindOrCreate(ctx_, render_pass, depth, stencil, color_attachments);
}

Eng::FgBuilder::FgBuilder(Eng::ShaderLoader &sh, PrimDraw &prim_draw)
    : FgContext(sh), prim_draw_(prim_draw), alloc_buf_(new char[AllocBufSize]), alloc_(alloc_buf_.get(), AllocBufSize) {
    // 2D Image
    pi_clear_image_[0][int(Ren::eFormat::RGBA8)] = sh.FindOrCreatePipeline("internal/clear_image@RGBA8.comp.glsl");
    pi_clear_image_[0][int(Ren::eFormat::R32F)] = sh.FindOrCreatePipeline("internal/clear_image@R32F.comp.glsl");
    pi_clear_image_[0][int(Ren::eFormat::R16F)] = sh.FindOrCreatePipeline("internal/clear_image@R16F.comp.glsl");
    pi_clear_image_[0][int(Ren::eFormat::R8)] = sh.FindOrCreatePipeline("internal/clear_image@R8.comp.glsl");
    pi_clear_image_[0][int(Ren::eFormat::R32UI)] = sh.FindOrCreatePipeline("internal/clear_image@R32UI.comp.glsl");
    pi_clear_image_[0][int(Ren::eFormat::RG8)] = sh.FindOrCreatePipeline("internal/clear_image@RG8.comp.glsl");
    pi_clear_image_[0][int(Ren::eFormat::RG16F)] = sh.FindOrCreatePipeline("internal/clear_image@RG16F.comp.glsl");
    pi_clear_image_[0][int(Ren::eFormat::RG32F)] = sh.FindOrCreatePipeline("internal/clear_image@RG32F.comp.glsl");
    pi_clear_image_[0][int(Ren::eFormat::RG11F_B10F)] =
        sh.FindOrCreatePipeline("internal/clear_image@RG11F_B10F.comp.glsl");
    pi_clear_image_[0][int(Ren::eFormat::RGBA32F)] = sh.FindOrCreatePipeline("internal/clear_image@RGBA32F.comp.glsl");
    pi_clear_image_[0][int(Ren::eFormat::RGBA16F)] = sh.FindOrCreatePipeline("internal/clear_image@RGBA16F.comp.glsl");
    // 2D Image Array
    pi_clear_image_[1][int(Ren::eFormat::RGBA8)] =
        sh.FindOrCreatePipeline("internal/clear_image@ARRAY;RGBA8.comp.glsl");
    pi_clear_image_[1][int(Ren::eFormat::R32F)] = sh.FindOrCreatePipeline("internal/clear_image@ARRAY;R32F.comp.glsl");
    pi_clear_image_[1][int(Ren::eFormat::R16F)] = sh.FindOrCreatePipeline("internal/clear_image@ARRAY;R16F.comp.glsl");
    pi_clear_image_[1][int(Ren::eFormat::R8)] = sh.FindOrCreatePipeline("internal/clear_image@ARRAY;R8.comp.glsl");
    pi_clear_image_[1][int(Ren::eFormat::R32UI)] =
        sh.FindOrCreatePipeline("internal/clear_image@ARRAY;R32UI.comp.glsl");
    pi_clear_image_[1][int(Ren::eFormat::RG8)] = sh.FindOrCreatePipeline("internal/clear_image@ARRAY;RG8.comp.glsl");
    pi_clear_image_[1][int(Ren::eFormat::RG16F)] =
        sh.FindOrCreatePipeline("internal/clear_image@ARRAY;RG16F.comp.glsl");
    pi_clear_image_[1][int(Ren::eFormat::RG32F)] =
        sh.FindOrCreatePipeline("internal/clear_image@ARRAY;RG32F.comp.glsl");
    pi_clear_image_[1][int(Ren::eFormat::RG11F_B10F)] =
        sh.FindOrCreatePipeline("internal/clear_image@ARRAY;RG11F_B10F.comp.glsl");
    pi_clear_image_[1][int(Ren::eFormat::RGBA32F)] =
        sh.FindOrCreatePipeline("internal/clear_image@ARRAY;RGBA32F.comp.glsl");
    pi_clear_image_[1][int(Ren::eFormat::RGBA16F)] =
        sh.FindOrCreatePipeline("internal/clear_image@ARRAY;RGBA16F.comp.glsl");
    // 3D Image
    pi_clear_image_[2][int(Ren::eFormat::RGBA8)] = sh.FindOrCreatePipeline("internal/clear_image@_3D;RGBA8.comp.glsl");
    pi_clear_image_[2][int(Ren::eFormat::R32F)] = sh.FindOrCreatePipeline("internal/clear_image@_3D;R32F.comp.glsl");
    pi_clear_image_[2][int(Ren::eFormat::R16F)] = sh.FindOrCreatePipeline("internal/clear_image@_3D;R16F.comp.glsl");
    pi_clear_image_[2][int(Ren::eFormat::R8)] = sh.FindOrCreatePipeline("internal/clear_image@_3D;R8.comp.glsl");
    pi_clear_image_[2][int(Ren::eFormat::R32UI)] = sh.FindOrCreatePipeline("internal/clear_image@_3D;R32UI.comp.glsl");
    pi_clear_image_[2][int(Ren::eFormat::RG8)] = sh.FindOrCreatePipeline("internal/clear_image@_3D;RG8.comp.glsl");
    pi_clear_image_[2][int(Ren::eFormat::RG16F)] = sh.FindOrCreatePipeline("internal/clear_image@_3D;RG16F.comp.glsl");
    pi_clear_image_[2][int(Ren::eFormat::RG32F)] = sh.FindOrCreatePipeline("internal/clear_image@_3D;RG32F.comp.glsl");
    pi_clear_image_[2][int(Ren::eFormat::RG11F_B10F)] =
        sh.FindOrCreatePipeline("internal/clear_image@_3D;RG11F_B10F.comp.glsl");
    pi_clear_image_[2][int(Ren::eFormat::RGBA32F)] =
        sh.FindOrCreatePipeline("internal/clear_image@_3D;RGBA32F.comp.glsl");
    pi_clear_image_[2][int(Ren::eFormat::RGBA16F)] =
        sh.FindOrCreatePipeline("internal/clear_image@_3D;RGBA16F.comp.glsl");

    pi_clear_buffer_ = sh.FindOrCreatePipeline("internal/clear_buffer.comp.glsl");
}

Eng::FgNode &Eng::FgBuilder::AddNode(const std::string_view name, const eFgQueueType queue) {
    char *mem = alloc_.allocate(sizeof(FgNode) + alignof(FgNode) - 1);
    auto *new_rp = reinterpret_cast<FgNode *>(mem + (alignof(FgNode) - uintptr_t(mem) % alignof(FgNode)) % alignof(FgNode));
    alloc_.construct(new_rp, name, int(nodes_.size()), queue, *this);
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
    if (res.type == eFgResType::Image) {
        const auto &[fgimg_main, fgimg_cold] = images_.GetUnsafe(res.opaque_handle.index);
        if (fgimg_cold.external) {
            return std::string("[Img] ") + fgimg_cold.name.c_str() + " (ext)";
        } else {
            return std::string("[Img] ") + fgimg_cold.name.c_str();
        }
    } else if (res.type == eFgResType::Buffer) {
        const auto &[fgbuf_main, fgbuf_cold] = buffers_.GetUnsafe(res.opaque_handle.index);
        if (fgbuf_cold.external) {
            return std::string("[Buf] ") + fgbuf_cold.name.c_str() + " (ext)";
        } else {
            return std::string("[Buf] ") + fgbuf_cold.name.c_str();
        }
    }
    return "";
}

void Eng::FgBuilder::GetResourceFrameLifetime(const FgAllocBufCold &b, uint16_t out_lifetime[2][2]) const {
    out_lifetime[0][0] = out_lifetime[1][0] = uint16_t(b.lifetime.first_used_node());
    out_lifetime[0][1] = out_lifetime[1][1] = uint16_t(b.lifetime.last_used_node() + 1);
}

void Eng::FgBuilder::GetResourceFrameLifetime(const FgAllocImgCold &i, uint16_t out_lifetime[2][2]) const {
    if (i.history_of == 0xffff && i.history_index == 0xffff) {
        // Non-history resource, same lifetime in both N and N+1 frames
        out_lifetime[0][0] = out_lifetime[1][0] = i.lifetime.first_used_node();
        out_lifetime[0][1] = out_lifetime[1][1] = i.lifetime.last_used_node() + 1;
    } else if (i.history_index != 0xffff) {
        // Frame N
        out_lifetime[0][0] = i.lifetime.first_used_node();
        out_lifetime[0][1] = uint16_t(reordered_nodes_.size());
        // Frame N+1
        const FgAllocImgCold &hist_img = images_.GetUnsafe(i.history_index).second;
        out_lifetime[1][0] = 0;
        out_lifetime[1][1] = hist_img.lifetime.last_used_node() + 1;
    } else {
        // Frame N
        assert(i.history_of != 0xffff);
        out_lifetime[0][0] = 0;
        out_lifetime[0][1] = i.lifetime.last_used_node() + 1;
        // Frame N+1
        const FgAllocImgCold &hist_img = images_.GetUnsafe(i.history_of).second;
        out_lifetime[1][0] = hist_img.lifetime.first_used_node();
        out_lifetime[1][1] = uint16_t(reordered_nodes_.size());
    }
}

Eng::FgBufROHandle Eng::FgBuilder::ReadBuffer(const FgBufROHandle handle, const Ren::eResState desired_state,
                                              const Ren::Bitmask<Ren::eStage> stages, FgNode &node,
                                              const int slot_index) {
    const auto &[fgbuf_main, fgbuf_cold] = buffers_[handle];

    const FgResource res = {eFgResType::Buffer, desired_state, Ren::Handle<void>{handle}, stages};

    fgbuf_cold.read_in_nodes.push_back({node.index_, int16_t(node.input_.size())});

    if (slot_index == -1) {
#ifndef NDEBUG
        for (const FgResource &r : node.input_) {
            assert(r.type != eFgResType::Buffer || r.opaque_handle != res.opaque_handle);
        }
#endif
        node.input_.push_back(res);
    } else {
        // Replace existing input
        const uint32_t buf_index = node.input_[slot_index].opaque_handle.index;
        const auto &[prev_buf_main, prev_buf_cold] = buffers_.GetUnsafe(buf_index);
        for (size_t i = 0; i < prev_buf_cold.read_in_nodes.size();) {
            if (prev_buf_cold.read_in_nodes[i].node_index == node.index_) {
                prev_buf_cold.read_in_nodes.erase(prev_buf_cold.read_in_nodes.begin() + i);
            } else {
                ++i;
            }
        }
        node.input_[slot_index] = res;
    }

    return handle;
}

Eng::FgImgROHandle Eng::FgBuilder::ReadImage(const FgImgROHandle handle, const Ren::eResState desired_state,
                                             const Ren::Bitmask<Ren::eStage> stages, FgNode &node) {
    const auto &[fgimg_main, fgimg_cold] = images_[handle];

    const FgResource res = {eFgResType::Image, desired_state, Ren::Handle<void>{handle}, stages};

    fgimg_cold.read_in_nodes.push_back({node.index_, int16_t(node.input_.size())});

#ifndef NDEBUG
    // Ensure uniqueness
    for (const FgResource &r : node.input_) {
        assert(r.type != eFgResType::Image || r.opaque_handle != res.opaque_handle);
    }
#endif
    node.input_.push_back(res);

    return handle;
}

Eng::FgImgROHandle Eng::FgBuilder::ReadHistoryImage(const FgImgROHandle handle, const Ren::eResState desired_state,
                                                    const Ren::Bitmask<Ren::eStage> stages, FgNode &node) {
    if (images_[handle].second.history_index == 0xffff) {
        // allocate new history image
        const FgImgRWHandle new_handle = images_.Emplace();

        const auto &[fgorig_main, fgorig_cold] = images_[handle];
        const auto &[fghist_main, fghist_cold] = images_[new_handle];

        fghist_cold.name = Ren::String{std::string(fgorig_cold.name) + " [Previous]"};
        fghist_cold.desc = fgorig_cold.desc;
        fghist_cold.history_of = handle.index;
        fgorig_cold.history_index = new_handle.index;

        name_to_image_.Insert(fghist_cold.name, new_handle.index);
    }

    const auto &[fgorig_main, fgorig_cold] = images_[handle];
    const auto &[fghist_main, fghist_cold] = images_.GetUnsafe(fgorig_cold.history_index);

    FgImgROHandle hist_handle = {fgorig_cold.history_index, images_.GetGeneration(fgorig_cold.history_index)};
    const FgResource res = {eFgResType::Image, desired_state, Ren::Handle<void>{hist_handle}, stages};

    fghist_cold.read_in_nodes.push_back({node.index_, int16_t(node.input_.size())});

#ifndef NDEBUG
    // Ensure uniqueness
    for (const FgResource &r : node.input_) {
        assert(r.type != eFgResType::Image || r.opaque_handle != res.opaque_handle);
    }
#endif
    node.input_.push_back(res);

    return hist_handle;
}

Eng::FgImgROHandle Eng::FgBuilder::ReadHistoryImage(std::string_view name, Ren::eResState desired_state,
                                                    Ren::Bitmask<Ren::eStage> stages, FgNode &node) {
    FgImgRWHandle handle;
    if (const uint16_t *p_index = name_to_image_.Find(name)) {
        handle = FgImgRWHandle{*p_index, images_.GetGeneration(*p_index)};
    }
    if (!handle) {
        handle = images_.Emplace();

        const auto &[fgimg_main, fgimg_cold] = images_[handle];
        fgimg_cold.name = Ren::String{name};
        // desc must be initialized later
        name_to_image_.Insert(fgimg_cold.name, handle.index);
    }
    return ReadHistoryImage(handle, desired_state, stages, node);
}

Eng::FgBufRWHandle Eng::FgBuilder::WriteBuffer(const std::string_view name, const FgBufDesc &desc,
                                               const Ren::eResState desired_state,
                                               const Ren::Bitmask<Ren::eStage> stages, FgNode &node) {
    assert(!name_to_buffer_.Find(name));

    Ren::String name_str{name};
    FgBufRWHandle handle = buffers_.Emplace();
    name_to_buffer_.Insert(name_str, handle.index);

    const auto &[fgbuf_main, fgbuf_cold] = buffers_[handle];

    fgbuf_cold.name = name_str;
    fgbuf_cold.desc = desc;

    const FgResource res = {eFgResType::Buffer, desired_state, Ren::Handle<void>{handle}, stages};

    fgbuf_cold.written_in_nodes.push_back({node.index_, int16_t(node.output_.size())});

    buffers_.SetGeneration(handle.index, handle.generation + 1);

#ifndef NDEBUG
    for (const FgResource &r : node.output_) {
        assert(r.type != eFgResType::Buffer || r.opaque_handle != res.opaque_handle);
    }
#endif
    node.output_.push_back(res);

    ++handle.generation;
    return handle;
}

Eng::FgBufRWHandle Eng::FgBuilder::WriteBuffer(FgBufRWHandle handle, const Ren::eResState desired_state,
                                               const Ren::Bitmask<Ren::eStage> stages, FgNode &node) {
    const auto &[fgbuf_main, fgbuf_cold] = buffers_[handle];

    const FgResource res = {eFgResType::Buffer, desired_state, Ren::Handle<void>{handle}, stages};

    fgbuf_cold.written_in_nodes.push_back({node.index_, int16_t(node.output_.size())});

    buffers_.SetGeneration(handle.index, handle.generation + 1);

#ifndef NDEBUG
    for (const FgResource &r : node.output_) {
        assert(r.type != eFgResType::Buffer || r.opaque_handle != res.opaque_handle);
    }
#endif
    node.output_.push_back(res);

    ++handle.generation;
    return handle;
}

Eng::FgImgRWHandle Eng::FgBuilder::WriteImage(std::string_view name, const FgImgDesc &desc,
                                              const Ren::eResState desired_state,
                                              const Ren::Bitmask<Ren::eStage> stages, FgNode &node) {
    FgImgRWHandle handle;
    if (const uint16_t *p_index = name_to_image_.Find(name)) {
        handle = FgImgRWHandle{*p_index, images_.GetGeneration(*p_index)};
    }
    if (!handle) {
        handle = images_.Emplace();

        const auto &[fgimg_main, fgimg_cold] = images_[handle];
        fgimg_cold.name = Ren::String{name};

        name_to_image_.Insert(fgimg_cold.name, handle.index);
    }

    const auto &[fgimg_main, fgimg_cold] = images_[handle];
    assert((fgimg_cold.desc.format == Ren::eFormat::Undefined || fgimg_cold.desc == desc) &&
           "Resource was already defined differently!");

    fgimg_cold.desc = desc;

    const FgResource res = {eFgResType::Image, desired_state, Ren::Handle<void>{handle}, stages};

    fgimg_cold.written_in_nodes.push_back({node.index_, int16_t(node.output_.size())});

    images_.SetGeneration(handle.index, handle.generation + 1);

#ifndef NDEBUG
    for (const FgResource &r : node.output_) {
        assert(r.type != eFgResType::Image || r.opaque_handle != res.opaque_handle);
    }
#endif
    node.output_.push_back(res);

    ++handle.generation;
    return handle;
}

Eng::FgImgRWHandle Eng::FgBuilder::WriteImage(FgImgRWHandle handle, const Ren::eResState desired_state,
                                              const Ren::Bitmask<Ren::eStage> stages, FgNode &node,
                                              const int slot_index) {
    const auto &[fgimg_main, fgimg_cold] = images_[handle];

    FgResource res = {eFgResType::Image, desired_state, Ren::Handle<void>{handle}, stages};

    fgimg_cold.written_in_nodes.push_back({node.index_, int16_t(node.output_.size())});

    images_.SetGeneration(handle.index, handle.generation + 1);

    if (slot_index == -1) {
#ifndef NDEBUG
        for (const FgResource &r : node.output_) {
            assert(r.type != eFgResType::Image || r.opaque_handle != res.opaque_handle);
        }
#endif
        node.output_.push_back(res);
    } else {
        assert(slot_index < int(node.output_.size()) && node.output_[slot_index]);
        // Replace existing output
        const uint32_t img_index = node.output_[slot_index].opaque_handle.index;
        const auto &[prev_img_main, prev_img_cold] = images_.GetUnsafe(img_index);
        images_.SetGeneration(img_index, images_.GetGeneration(img_index) - 1);
        for (size_t i = 0; i < prev_img_cold.written_in_nodes.size();) {
            if (prev_img_cold.written_in_nodes[i].node_index == node.index_) {
                prev_img_cold.written_in_nodes.erase(prev_img_cold.written_in_nodes.begin() + i);
            } else {
                ++i;
            }
        }
        if (node.output_[slot_index].opaque_handle.index == res.opaque_handle.index) {
            --res.opaque_handle.generation;
            --handle.generation;
        }
        node.output_[slot_index] = res;
    }

    ++handle.generation;
    return handle;
}

Eng::FgBufRWHandle Eng::FgBuilder::ImportResource(const Ren::BufferHandle _handle) {
    const auto &[buf_main, buf_cold] = ctx_.storages().buffers[_handle];

    FgBufRWHandle handle = FindBuffer(buf_cold.name);
    if (!handle) {
        handle = buffers_.Emplace();
        name_to_buffer_.Insert(buf_cold.name, handle.index);

        const auto &[fgbuf_main, fgbuf_cold] = buffers_[handle];

        fgbuf_cold.name = buf_cold.name;
        fgbuf_cold.desc = FgBufDesc{buf_cold.type, buf_cold.size};
        fgbuf_cold.external = true;
    }
    const auto &[fgbuf_main, fgbuf_cold] = buffers_[handle];
    assert(fgbuf_cold.desc.size == buf_cold.size && fgbuf_cold.desc.type == buf_cold.type);

    fgbuf_main.handle = _handle;

    return handle;
}

Eng::FgImgRWHandle Eng::FgBuilder::ImportResource(const Ren::ImageHandle _handle) {
    const auto &[img_main, img_cold] = ctx_.storages().images[_handle];

    FgImgRWHandle handle = FindImage(img_cold.name);
    if (!handle) {
        handle = images_.Emplace();

        const auto &[fgimg_main, fgimg_cold] = images_[handle];

        fgimg_cold.name = img_cold.name;
        fgimg_cold.desc = FgImgDesc{img_cold.params};
        fgimg_cold.external = true;

        name_to_image_.Insert(fgimg_cold.name, handle.index);
    }
    const auto &[fgimg_main, fgimg_cold] = images_[handle];
    assert(fgimg_cold.desc == FgImgDesc{img_cold.params});

    fgimg_main.handle_to_use = _handle;

    return handle;
}

void Eng::FgBuilder::AllocateNeededResources_Simple() {
    const Ren::StoragesRef &storages = ctx_.storages();
    for (auto it = std::begin(name_to_buffer_); it != std::end(name_to_buffer_); ++it) {
        const auto &[fgbuf_main, fgbuf_cold] = buffers_.GetUnsafe(it->val);
        if (fgbuf_cold.external || fgbuf_cold.alias_of != -1 || !fgbuf_cold.lifetime.is_used()) {
            continue;
        }
        assert(!fgbuf_main.handle);
        fgbuf_main.handle = ctx_.CreateBuffer(fgbuf_cold.name, fgbuf_cold.desc.type, fgbuf_cold.desc.size, 16,
                                              ctx_.default_mem_allocs());

        const auto &[buf_main, buf_cold] = storages.buffers[fgbuf_main.handle];
        for (int i = 0; i < int(fgbuf_cold.desc.views.size()); ++i) {
            const int view_index = Buffer_AddView(ctx_.api(), buf_main, buf_cold, fgbuf_cold.desc.views[i]);
            assert(view_index == i);
        }
    }
    for (auto it = std::begin(name_to_buffer_); it != std::end(name_to_buffer_); ++it) {
        const auto &[fgbuf_main, fgbuf_cold] = buffers_.GetUnsafe(it->val);
        if (fgbuf_cold.external || fgbuf_cold.alias_of == -1 || !fgbuf_cold.lifetime.is_used()) {
            continue;
        }
        const auto &[orig_fgbuf_main, orig_fgbuf_cold] = buffers_.GetUnsafe(fgbuf_cold.alias_of);
        assert(orig_fgbuf_cold.alias_of == -1);
        fgbuf_main.handle = orig_fgbuf_main.handle;
        ctx_.log()->Info("Buf %s will be alias of %s", fgbuf_cold.name.c_str(), orig_fgbuf_cold.name.c_str());
    }
    ///
    for (auto it = std::begin(name_to_image_); it != std::end(name_to_image_); ++it) {
        const auto &[fgimg_main, fgimg_cold] = images_.GetUnsafe(it->val);
        if (fgimg_cold.external || fgimg_cold.alias_of != -1 || !fgimg_cold.lifetime.is_used()) {
            continue;
        }

        if (fgimg_cold.history_index != 0xffff) {
            const auto &[hist_main, hist_cold] = images_.GetUnsafe(fgimg_cold.history_index);
            // combine usage flags
            fgimg_cold.desc.usage |= hist_cold.desc.usage;
            hist_cold.desc = fgimg_cold.desc;
        }
        if (fgimg_cold.history_of != 0xffff) {
            const auto &[hist_main, hist_cold] = images_.GetUnsafe(fgimg_cold.history_of);
            // combine usage flags
            fgimg_cold.desc.usage |= hist_cold.desc.usage;
            hist_cold.desc = fgimg_cold.desc;
        }

        assert(!fgimg_main.handle_to_use);
        ctx_.log()->Info("Alloc img %s (%ix%i %f MB)", fgimg_cold.name.c_str(), fgimg_cold.desc.w, fgimg_cold.desc.h,
                         float(GetDataLenBytes(fgimg_cold.desc)) * 0.000001f);

        fgimg_main.handle_to_own = ctx_.CreateImage(fgimg_cold.name, {}, fgimg_cold.desc, ctx_.default_mem_allocs());
        fgimg_main.handle_to_use = fgimg_main.handle_to_own;
        assert(fgimg_main.handle_to_use);

        for (int i = 0; i < int(fgimg_cold.desc.views.size()); ++i) {
            const auto &v = fgimg_cold.desc.views[i];
            [[maybe_unused]] const int view_index = ctx_.CreateImageView(
                fgimg_main.handle_to_own, v.format, v.mip_level, v.mip_count, v.base_layer, v.layer_count);
            assert(view_index == i || view_index == i + 1);
        }
    }
    for (auto it = std::begin(name_to_image_); it != std::end(name_to_image_); ++it) {
        const auto &[fgimg_main, fgimg_cold] = images_.GetUnsafe(it->val);
        if (fgimg_cold.external || fgimg_cold.alias_of == -1 || !fgimg_cold.lifetime.is_used()) {
            continue;
        }
        assert(!fgimg_main.handle_to_use);
        const auto &[orig_fgimg_main, orig_fgimg_cold] = images_.GetUnsafe(fgimg_cold.alias_of);
        assert(orig_fgimg_cold.alias_of == -1);
        fgimg_main.handle_to_use = orig_fgimg_main.handle_to_use;
        ctx_.log()->Info("Img %s will be alias of %s", fgimg_cold.name.c_str(), orig_fgimg_cold.name.c_str());
    }
}

void Eng::FgBuilder::ClearResources_Simple() {
    const Ren::StoragesRef &storages = ctx_.storages();

    std::vector<Ren::BufferHandle> buffers_to_clear;
    std::vector<Ren::ImageHandle> images_to_clear;

    for (auto it = std::begin(name_to_buffer_); it != std::end(name_to_buffer_); ++it) {
        const auto &[fgbuf_main, fgbuf_cold] = buffers_.GetUnsafe(it->val);
        if (fgbuf_cold.external || !fgbuf_cold.lifetime.is_used()) {
            continue;
        }
        if (fgbuf_cold.alias_of == -1) {
            buffers_to_clear.push_back(fgbuf_main.handle);
        }
    }

    for (auto it = std::begin(name_to_image_); it != std::end(name_to_image_); ++it) {
        const auto &[fgimg_main, fgimg_cold] = images_.GetUnsafe(it->val);
        if (fgimg_cold.external || !fgimg_cold.lifetime.is_used()) {
            continue;
        }
        if (fgimg_main.handle_to_use && fgimg_cold.alias_of != -1) {
            images_to_clear.push_back(fgimg_main.handle_to_use);
        }
    }

    if (!buffers_to_clear.empty() || !images_to_clear.empty()) { // Clear resources
        Ren::CommandBuffer cmd_buf = ctx_.BegTempSingleTimeCommands();

        std::vector<Ren::TransitionInfo> transitions;
        transitions.reserve(images_to_clear.size() + buffers_to_clear.size());
        for (const Ren::BufferHandle b : buffers_to_clear) {
            transitions.emplace_back(b, Ren::eResState::CopyDst);
        }
        for (const Ren::ImageHandle i : images_to_clear) {
            const Ren::ImgParams &p = storages.images[i].second.params;
            if (p.usage & Ren::eImgUsage::Transfer) {
                transitions.emplace_back(i, Ren::eResState::CopyDst);
            } else if (p.usage & Ren::eImgUsage::Storage) {
                transitions.emplace_back(i, Ren::eResState::UnorderedAccess);
            } else if (p.usage & Ren::eImgUsage::RenderTarget) {
                if (Ren::IsDepthFormat(p.format)) {
                    transitions.emplace_back(i, Ren::eResState::DepthWrite);
                } else {
                    transitions.emplace_back(i, Ren::eResState::RenderTarget);
                }
            }
        }

        TransitionResourceStates(ctx_.api(), ctx_.storages(), cmd_buf, Ren::AllStages, Ren::AllStages, transitions);

        for (const Ren::BufferHandle b : buffers_to_clear) {
            const auto &[buf_main, buf_cold] = storages.buffers[b];
            if (buf_main.resource_state == Ren::eResState::CopyDst) {
                ClearBuffer_AsTransfer(b, cmd_buf);
            } else if (buf_main.resource_state == Ren::eResState::UnorderedAccess) {
                ClearBuffer_AsStorage(b, cmd_buf);
            } else if (buf_main.resource_state == Ren::eResState::BuildASWrite) {
                // NOTE: Skipped
            } else {
                assert(false);
            }
        }
        for (const Ren::ImageHandle i : images_to_clear) {
            const auto &[img_main, img_cold] = storages.images[i];
            if (img_main.resource_state == Ren::eResState::CopyDst) {
                ClearImage_AsTransfer(i, cmd_buf);
            } else if (img_main.resource_state == Ren::eResState::UnorderedAccess) {
                ClearImage_AsStorage(i, cmd_buf);
            } else if (img_main.resource_state == Ren::eResState::RenderTarget ||
                       img_main.resource_state == Ren::eResState::DepthWrite) {
                ClearImage_AsTarget(i, cmd_buf);
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

    for (auto &t : node_timings_) {
        t.clear();
    }

    for (auto it = std::begin(name_to_buffer_); it != std::end(name_to_buffer_);) {
        const auto &[fgbuf_main, fgbuf_cold] = buffers_.GetUnsafe(it->val);
        if (!fgbuf_cold.external && fgbuf_cold.alias_of == -1) {
            if (fgbuf_main.handle) {
                ctx_.ReleaseBuffer(fgbuf_main.handle);
            }
        }
        buffers_.EraseUnsafe(it->val);
        it = name_to_buffer_.erase(it);
    }
    assert(buffers_.empty());
    buffers_ = {}; // reset generation counters

    for (auto it = std::begin(name_to_image_); it != std::end(name_to_image_);) {
        const auto &[fgimg_main, fgimg_cold] = images_.GetUnsafe(it->val);
        if (!fgimg_cold.external && fgimg_cold.alias_of == -1) {
            if (fgimg_main.handle_to_own) {
                ctx_.ReleaseImage(fgimg_main.handle_to_own);
            }
        }
        images_.EraseUnsafe(it->val);
        it = name_to_image_.erase(it);
    }
    assert(images_.empty());
    images_ = {}; // reset generation counters

    framebuffers_->Clear(ctx_);

    ReleaseMemHeaps();
}

int16_t Eng::FgBuilder::FindPreviousWrittenInNode(const FgResource &res) {
    Ren::SmallVectorImpl<fg_node_slot_t> *written_in_nodes = nullptr;
    if (res.type == eFgResType::Buffer) {
        FgAllocBufCold &fgbuf_cold = buffers_.GetUnsafe(res.opaque_handle.index).second;
        written_in_nodes = &fgbuf_cold.written_in_nodes;
    } else if (res.type == eFgResType::Image) {
        FgAllocImgCold &fgimg_cold = images_.GetUnsafe(res.opaque_handle.index).second;
        written_in_nodes = &fgimg_cold.written_in_nodes;
    }

    assert(written_in_nodes);
    if (!written_in_nodes) {
        return -1;
    }

    for (const fg_node_slot_t i : *written_in_nodes) {
        const FgNode *node = nodes_[i.node_index];
        assert(node->output_[i.slot_index].type == res.type &&
               node->output_[i.slot_index].opaque_handle.index == res.opaque_handle.index);
        if (node->output_[i.slot_index].opaque_handle.generation == res.opaque_handle.generation - 1) {
            return i.node_index;
        }
    }
    return -1;
}

int16_t Eng::FgBuilder::FindPreviousWrittenInNode(const FgBufRWHandle res) {
    FgAllocBufCold &fgbuf_cold = buffers_[res].second;
    const Ren::SmallVectorImpl<fg_node_slot_t> &written_in_nodes = fgbuf_cold.written_in_nodes;

    for (const fg_node_slot_t i : written_in_nodes) {
        const FgNode *node = nodes_[i.node_index];
        assert(node->output_[i.slot_index].type == eFgResType::Buffer &&
               node->output_[i.slot_index].opaque_handle.index == res.index);
        if (node->output_[i.slot_index].opaque_handle.generation == res.generation - 1) {
            return i.node_index;
        }
    }
    return -1;
}

int16_t Eng::FgBuilder::FindPreviousWrittenInNode(const FgImgRWHandle res) {
    FgAllocImgCold &fgimg_cold = images_[res].second;
    const Ren::SmallVectorImpl<fg_node_slot_t> &written_in_nodes = fgimg_cold.written_in_nodes;

    for (const fg_node_slot_t i : written_in_nodes) {
        const FgNode *node = nodes_[i.node_index];
        assert(node->output_[i.slot_index].type == eFgResType::Image &&
               node->output_[i.slot_index].opaque_handle.index == res.index);
        if (node->output_[i.slot_index].opaque_handle.generation == res.generation - 1) {
            return i.node_index;
        }
    }
    return -1;
}

void Eng::FgBuilder::FindPreviousReadInNodes(const FgResource &res, Ren::SmallVectorImpl<int16_t> &out_nodes) {
    Ren::SmallVectorImpl<fg_node_slot_t> *read_in_nodes = nullptr;
    if (res.type == eFgResType::Buffer) {
        FgAllocBufCold &fgbuf_cold = buffers_.GetUnsafe(res.opaque_handle.index).second;
        read_in_nodes = &fgbuf_cold.read_in_nodes;
    } else if (res.type == eFgResType::Image) {
        FgAllocImgCold &fgimg_cold = images_.GetUnsafe(res.opaque_handle.index).second;
        read_in_nodes = &fgimg_cold.read_in_nodes;
    }

    assert(read_in_nodes);
    if (!read_in_nodes) {
        return;
    }

    for (const fg_node_slot_t i : *read_in_nodes) {
        const FgNode *node = nodes_[i.node_index];
        assert(node->input_[i.slot_index].type == res.type &&
               node->input_[i.slot_index].opaque_handle.index == res.opaque_handle.index);
        if (node->input_[i.slot_index].opaque_handle == res.opaque_handle) {
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
    for (auto it = std::begin(name_to_image_); it != std::end(name_to_image_); ++it) {
        const auto &[fgimg_main, fgimg_cold] = images_.GetUnsafe(it->val);
        if (fgimg_cold.history_of != 0xffffu) {
            assert(images_.GetUnsafe(fgimg_cold.history_of).second.history_index == it->val);
            fgimg_cold.desc = images_.GetUnsafe(fgimg_cold.history_of).second.desc;
        }
    }
    // Propagate usage flags
    for (FgNode *cur_node : reordered_nodes_) {
        for (const FgResource &r : cur_node->input_) {
            if (r.type == eFgResType::Buffer) {
            } else if (r.type == eFgResType::Image) {
                FgAllocImgCold &fgimg_cold = images_.GetUnsafe(r.opaque_handle.index).second;
                fgimg_cold.desc.usage |= Ren::ImgUsageFromState(r.desired_state);
            }
        }
        for (const FgResource &r : cur_node->output_) {
            if (r.type == eFgResType::Buffer) {
            } else if (r.type == eFgResType::Image) {
                FgAllocImgCold &fgimg_cold = images_.GetUnsafe(r.opaque_handle.index).second;
                fgimg_cold.desc.usage |= Ren::ImgUsageFromState(r.desired_state);
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
                FgAllocBufCold &fgbuf_cold = buffers_.GetUnsafe(res.opaque_handle.index).second;
                this_res = &fgbuf_cold;
            } else if (res.type == eFgResType::Image) {
                this_res = &images_.GetUnsafe(res.opaque_handle.index).second;
            }
            this_res->lifetime.first_read_node = std::min(this_res->lifetime.first_read_node, i);
            this_res->lifetime.last_read_node = std::max(this_res->lifetime.last_read_node, i);
        }
        for (const auto &res : node->output_) {
            FgAllocRes *this_res = nullptr;
            if (res.type == eFgResType::Buffer) {
                this_res = &buffers_.GetUnsafe(res.opaque_handle.index).second;
            } else if (res.type == eFgResType::Image) {
                this_res = &images_.GetUnsafe(res.opaque_handle.index).second;
            }
            this_res->lifetime.first_write_node = std::min(this_res->lifetime.first_write_node, i);
            this_res->lifetime.last_write_node = std::max(this_res->lifetime.last_write_node, i);
        }
    }

    if (!FgBuilderInternal::EnableResourceAliasing) {
        return;
    }

    img_alias_chains_.clear();
    buf_alias_chains_.clear();
    img_alias_chains_.resize(name_to_image_.capacity());
    buf_alias_chains_.resize(name_to_buffer_.capacity());

    { // Images
        std::vector<int> img_aliases(images_.capacity(), -1);
        for (auto it1 = std::begin(name_to_image_); it1 != std::end(name_to_image_); ++it1) {
            const auto &[img1_main, img1_cold] = images_.GetUnsafe(it1->val);
            if (img1_cold.external || img1_cold.history_index != 0xffff || img1_cold.history_of != 0xffff) {
                continue;
            }
            for (auto it2 = std::begin(name_to_image_); it2 != it1; ++it2) {
                const auto &[img2_main, img2_cold] = images_.GetUnsafe(it2->val);
                if (img2_cold.external || img2_cold.history_index != 0xffff || img2_cold.history_of != 0xffff ||
                    img_aliases[it2->val] != -1) {
                    continue;
                }
                if (img1_cold.desc == img2_cold.desc) {
                    bool disjoint = disjoint_lifetimes(img1_cold.lifetime, img2_cold.lifetime);
                    for (const int alias : img_alias_chains_[it2->val]) {
                        if (alias == it1->val) {
                            continue;
                        }
                        const fg_node_range_t &lifetime = images_.GetUnsafe(alias).second.lifetime;
                        disjoint &= disjoint_lifetimes(lifetime, img2_cold.lifetime);
                    }
                    if (disjoint) {
                        img_aliases[it1->val] = it2->val;
                        if (img_alias_chains_[it2->val].empty()) {
                            img_alias_chains_[it2->val].push_back(it2->val);
                        }
                        img_alias_chains_[it2->val].push_back(it1->val);
                        break;
                    }
                }
            }
        }
    }

    { // Buffers
        std::vector<int> buf_aliases(name_to_buffer_.capacity(), -1);
        for (auto it1 = std::begin(name_to_buffer_); it1 != std::end(name_to_buffer_); ++it1) {
            const auto &[buf1_main, buf1_cold] = buffers_.GetUnsafe(it1->val);
            if (buf1_cold.external) {
                continue;
            }
            for (auto it2 = std::begin(name_to_buffer_); it2 != it1; ++it2) {
                const auto &[buf2_main, buf2_cold] = buffers_.GetUnsafe(it2->val);
                if (buf2_cold.external || buf_aliases[it2->val] != -1) {
                    continue;
                }
                if (buf1_cold.desc.type == buf2_cold.desc.type && buf1_cold.desc.size == buf2_cold.desc.size &&
                    buf1_cold.desc.views == buf2_cold.desc.views) {
                    bool disjoint = disjoint_lifetimes(buf1_cold.lifetime, buf2_cold.lifetime);
                    for (const int alias : buf_alias_chains_[it2->val]) {
                        if (alias == it1->val) {
                            continue;
                        }
                        const fg_node_range_t &lifetime = buffers_.GetUnsafe(alias).second.lifetime;
                        disjoint &= disjoint_lifetimes(lifetime, buf2_cold.lifetime);
                    }
                    if (disjoint) {
                        buf_aliases[it1->val] = it2->val;
                        if (buf_alias_chains_[it2->val].empty()) {
                            buf_alias_chains_[it2->val].push_back(it2->val);
                        }
                        buf_alias_chains_[it2->val].push_back(it1->val);
                        break;
                    }
                }
            }
        }
    }

    for (int j = 0; j < int(img_alias_chains_.size()); ++j) {
        auto &chain = img_alias_chains_[j];
        if (chain.empty()) {
            continue;
        }

        std::sort(std::begin(chain), std::end(chain), [this](const int lhs, const int rhs) {
            return images_.GetUnsafe(lhs).second.lifetime.last_used_node() <
                   images_.GetUnsafe(rhs).second.lifetime.first_used_node();
        });

        FgAllocImgCold &first_img = images_.GetUnsafe(chain[0]).second;
        assert(first_img.alias_of == -1);

        for (int i = 1; i < int(chain.size()); ++i) {
            FgAllocImgCold &next_img = images_.GetUnsafe(chain[i]).second;
            next_img.alias_of = chain[0];
        }

        if (chain[0] != j) {
            img_alias_chains_[chain[0]] = std::move(chain);
        }
    }

    for (int j = 0; j < int(buf_alias_chains_.size()); ++j) {
        auto &chain = buf_alias_chains_[j];
        if (chain.empty()) {
            continue;
        }

        std::sort(std::begin(chain), std::end(chain), [this](const int lhs, const int rhs) {
            return buffers_.GetUnsafe(lhs).second.lifetime.last_used_node() <
                   buffers_.GetUnsafe(rhs).second.lifetime.first_used_node();
        });

        FgAllocBufCold &first_buf = buffers_.GetUnsafe(chain[0]).second;
        assert(first_buf.alias_of == -1);

        for (int i = 1; i < int(chain.size()); ++i) {
            FgAllocBufCold &next_buf = buffers_.GetUnsafe(chain[i]).second;
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
    all_resources.reserve(buffers_.size() + images_.size());

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
                if (r->type == eFgResType::Image) {
                    if (images_.GetUnsafe(r->opaque_handle.index).second.alias_of != -1) {
                        const auto &chain =
                            img_alias_chains_[images_.GetUnsafe(r->opaque_handle.index).second.alias_of];
                        const int unsafe_index = int(r->opaque_handle.index);
                        auto curr_it = std::find(std::begin(chain), std::end(chain), unsafe_index);
                        assert(curr_it != std::end(chain) && curr_it != std::begin(chain));

                        FgResource to_find;
                        to_find.type = eFgResType::Image;
                        to_find.opaque_handle.index = *--curr_it;

                        auto it2 =
                            std::lower_bound(begin(all_resources), end(all_resources), &to_find, resource_compare);
                        if (it2 != end(all_resources) && !FgResource::LessThanTypeAndIndex(to_find, **it2)) {
                            (*it2)->next_use = r;
                        }
                    }
                } else if (r->type == eFgResType::Buffer) {
                    if (buffers_.GetUnsafe(r->opaque_handle.index).second.alias_of != -1) {
                        const auto &chain =
                            buf_alias_chains_[buffers_.GetUnsafe(r->opaque_handle.index).second.alias_of];
                        const int unsafe_index = int(r->opaque_handle.index);
                        auto curr_it = std::find(std::begin(chain), std::end(chain), unsafe_index);
                        assert(curr_it != std::end(chain) && curr_it != std::begin(chain));

                        FgResource to_find;
                        to_find.type = eFgResType::Buffer;
                        to_find.opaque_handle.index = *--curr_it;

                        auto it2 =
                            std::lower_bound(begin(all_resources), end(all_resources), &to_find, resource_compare);
                        if (it2 != end(all_resources) && !FgResource::LessThanTypeAndIndex(to_find, **it2)) {
                            (*it2)->next_use = r;
                        }
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
                if (r->type == eFgResType::Image) {
                    if (images_.GetUnsafe(r->opaque_handle.index).second.alias_of != -1) {
                        const auto &chain =
                            img_alias_chains_[images_.GetUnsafe(r->opaque_handle.index).second.alias_of];
                        const int unsafe_index = int(r->opaque_handle.index);
                        auto curr_it = std::find(std::begin(chain), std::end(chain), unsafe_index);
                        assert(curr_it != std::end(chain) && curr_it != std::begin(chain));

                        FgResource to_find;
                        to_find.type = eFgResType::Image;
                        to_find.opaque_handle.index = *--curr_it;

                        auto it2 =
                            std::lower_bound(begin(all_resources), end(all_resources), &to_find, resource_compare);
                        if (it2 != end(all_resources) && !FgResource::LessThanTypeAndIndex(to_find, **it2)) {
                            (*it2)->next_use = r;
                        }
                    }
                } else if (r->type == eFgResType::Buffer) {
                    if (buffers_.GetUnsafe(r->opaque_handle.index).second.alias_of != -1) {
                        const auto &chain =
                            buf_alias_chains_[buffers_.GetUnsafe(r->opaque_handle.index).second.alias_of];
                        const int unsafe_index = int(r->opaque_handle.index);
                        auto curr_it = std::find(std::begin(chain), std::end(chain), unsafe_index);
                        assert(curr_it != std::end(chain) && curr_it != std::begin(chain));

                        FgResource to_find;
                        to_find.type = eFgResType::Buffer;
                        to_find.opaque_handle.index = *--curr_it;

                        auto it2 =
                            std::lower_bound(begin(all_resources), end(all_resources), &to_find, resource_compare);
                        if (it2 != end(all_resources) && !FgResource::LessThanTypeAndIndex(to_find, **it2)) {
                            (*it2)->next_use = r;
                        }
                    }
                }
                r->next_use = nullptr;
                all_resources.insert(it, r);
            }
        }
    }

    // Connect history resources across N and N+1 frames
    for (auto it = std::begin(name_to_image_); it != std::end(name_to_image_); ++it) {
        const auto &[fgimg_main, fgimg_cold] = images_.GetUnsafe(it->val);
        if (!fgimg_cold.lifetime.is_used() || (fgimg_cold.history_index == 0xffff && fgimg_cold.history_of == 0xffff)) {
            continue;
        }

        if (fgimg_cold.history_index != 0xffff) {
            const auto &[hist_main, hist_cold] = images_.GetUnsafe(fgimg_cold.history_index);
            if (!hist_main.handle_to_use) {
                continue;
            }
            FgNode *img_node = reordered_nodes_[fgimg_cold.lifetime.last_used_node()];
            FgResource *last_usage = img_node->FindUsageOf(eFgResType::Image, it->val);
            assert(last_usage);
            FgNode *hist_node = reordered_nodes_[hist_cold.lifetime.first_used_node()];
            FgResource *first_usage = hist_node->FindUsageOf(eFgResType::Image, fgimg_cold.history_index);
            assert(first_usage);
            last_usage->next_use = first_usage;
        } else if (fgimg_cold.history_of != 0xffff) {
            const auto &[hist_main, hist_cold] = images_.GetUnsafe(fgimg_cold.history_of);
            assert(hist_main.handle_to_use);

            FgNode *img_node = reordered_nodes_[fgimg_cold.lifetime.last_used_node()];
            FgResource *last_usage = img_node->FindUsageOf(eFgResType::Image, it->val);
            assert(last_usage);
            FgNode *hist_node = reordered_nodes_[hist_cold.lifetime.first_used_node()];
            FgResource *first_usage = hist_node->FindUsageOf(eFgResType::Image, fgimg_cold.history_of);
            assert(first_usage);
            last_usage->next_use = first_usage;
        }
    }
}

void Eng::FgBuilder::Compile(Ren::Span<const std::variant<FgBufRWHandle, FgImgRWHandle>> backbuffer_sources) {
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
            int16_t prev_node = -1;
            if (std::holds_alternative<FgBufRWHandle>(backbuffer_sources[i])) {
                prev_node = FindPreviousWrittenInNode(std::get<FgBufRWHandle>(backbuffer_sources[i]));
            } else if (std::holds_alternative<FgImgRWHandle>(backbuffer_sources[i])) {
                prev_node = FindPreviousWrittenInNode(std::get<FgImgRWHandle>(backbuffer_sources[i]));
            }

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
                const FgResource &res = node->output_[i];

                Ren::SmallVectorImpl<fg_node_slot_t> *read_in_nodes = nullptr;
                if (res.type == eFgResType::Buffer) {
                    read_in_nodes = &buffers_.GetUnsafe(res.opaque_handle.index).second.read_in_nodes;
                } else if (res.type == eFgResType::Image) {
                    read_in_nodes = &images_.GetUnsafe(res.opaque_handle.index).second.read_in_nodes;
                }

                for (const fg_node_slot_t slot : *read_in_nodes) {
                    const FgNode *_node = nodes_[slot.node_index];
                    // NOTE: write_count check is skipped because of history images
                    if (_node != node && _node->visited_ /*&&
                        res.write_count == _node->input_[slot.slot_index].write_count*/) {
                        has_consumers = true;
                        break;
                    }
                }

                if (has_consumers) {
                    break;
                }

                Ren::SmallVectorImpl<fg_node_slot_t> *written_in = nullptr;
                if (res.type == eFgResType::Buffer) {
                    written_in = &buffers_.GetUnsafe(res.opaque_handle.index).second.written_in_nodes;
                } else if (res.type == eFgResType::Image) {
                    written_in = &images_.GetUnsafe(res.opaque_handle.index).second.written_in_nodes;
                }

                for (const fg_node_slot_t slot : *written_in) {
                    const FgNode *_node = nodes_[slot.node_index];
                    if (res.type == eFgResType::Buffer) {
                        if (_node != node && _node->visited_ &&
                            res.opaque_handle.index == _node->output_[slot.slot_index].opaque_handle.index &&
                            res.opaque_handle.generation + 1 ==
                                _node->output_[slot.slot_index].opaque_handle.generation) {
                            has_consumers = true;
                            break;
                        }
                    } else {
                        if (_node != node && _node->visited_ &&
                            res.opaque_handle.index == _node->output_[slot.slot_index].opaque_handle.index &&
                            res.opaque_handle.generation + 1 ==
                                _node->output_[slot.slot_index].opaque_handle.generation) {
                            has_consumers = true;
                            break;
                        }
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
                                if (output.type == input.type &&
                                    output.opaque_handle.index == input.opaque_handle.index) {
                                    if (input.type == eFgResType::Buffer) {
                                        if (output.opaque_handle.generation >= input.opaque_handle.generation) {
                                            possible_candidate = false;
                                            break;
                                        }
                                    } else {
                                        if (output.opaque_handle.generation >= input.opaque_handle.generation) {
                                            possible_candidate = false;
                                            break;
                                        }
                                    }
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

    const Ren::StoragesRef &storages = ctx_.storages();

    ctx_.log()->Info("============================================================================");
    { // report buffers
        std::vector<uint32_t> not_handled_buffers;
        not_handled_buffers.reserve(buffers_.size());
        for (auto it = std::begin(name_to_buffer_); it != std::end(name_to_buffer_); ++it) {
            const auto &[fgbuf_main, fgbuf_cold] = buffers_.GetUnsafe(it->val);
            if (fgbuf_cold.alias_of != -1) {
                const auto &[orig_fgbuf_main, orig_fgbuf_cold] = buffers_.GetUnsafe(fgbuf_cold.alias_of);
                ctx_.log()->Info("Buf %-24.24s alias of %16s\t| %f MB", fgbuf_cold.name.c_str(),
                                 orig_fgbuf_cold.name.c_str(), 0.0f);
                continue;
            }
            if (!fgbuf_cold.external && fgbuf_main.handle) {
                const auto &[buf_main, buf_cold] = storages.buffers[fgbuf_main.handle];

                const Ren::MemAllocation &alloc = buf_cold.alloc;
                if (alloc.pool != 0xffff) {
                    ctx_.log()->Info("Buf %-24.24s \t\t\t\t\t| %f MB", buf_cold.name.c_str(),
                                     float(buf_cold.size) / (1024.0f * 1024.0f));
                } else {
                    ctx_.log()->Info("Buf %-24.24s (dedicated)\t\t\t| %f MB", buf_cold.name.c_str(),
                                     float(buf_cold.size) / (1024.0f * 1024.0f));
                }
            } else if (fgbuf_main.handle) {
                not_handled_buffers.push_back(it->val);
            }
        }
        ctx_.log()->Info("----------------------------------------------------------------------------");
        for (const uint32_t index : not_handled_buffers) {
            const auto &[fgbuf_main, fgbuf_cold] = buffers_.GetUnsafe(index);
            ctx_.log()->Info("Buf %-24.24s \t\t\t\t\t| %f MB", fgbuf_cold.name.c_str(),
                             float(fgbuf_cold.desc.size) / (1024.0f * 1024.0f));
        }
    }
    ctx_.log()->Info("============================================================================");
    { // report images
        std::vector<uint32_t> not_handled_images;
        not_handled_images.reserve(images_.size());
        for (auto it = std::begin(name_to_image_); it != std::end(name_to_image_); ++it) {
            const auto &[fgimg_main, fgimg_cold] = images_.GetUnsafe(it->val);
            if (fgimg_cold.alias_of != -1) {
                const auto &[orig_main, orig_cold] = images_.GetUnsafe(fgimg_cold.alias_of);
                ctx_.log()->Info("Img %-24.24s alias of %16s\t| %-f MB", fgimg_cold.name.c_str(),
                                 orig_cold.name.c_str(), float(GetDataLenBytes(fgimg_cold.desc)) / (1024.0f * 1024.0f));
                continue;
            }
            if (fgimg_main.handle_to_own) {
                ctx_.log()->Info("Img %-24.24s (%4ix%-4i)\t\t\t| %f MB", fgimg_cold.name.c_str(), fgimg_cold.desc.w,
                                 fgimg_cold.desc.h, float(GetDataLenBytes(fgimg_cold.desc)) / (1024.0f * 1024.0f));
            } else if (fgimg_main.handle_to_use) {
                not_handled_images.push_back(it->val);
            }
        }
        ctx_.log()->Info("----------------------------------------------------------------------------");
        for (const uint32_t index : not_handled_images) {
            const auto &[fgimg_main, fgimg_cold] = images_.GetUnsafe(index);
            ctx_.log()->Info("Img %-24.24s (%4ix%-4i)\t\t\t| %f MB", fgimg_cold.name.c_str(), fgimg_cold.desc.w,
                             fgimg_cold.desc.h, float(GetDataLenBytes(fgimg_cold.desc)) / (1024.0f * 1024.0f));
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

    Ren::DebugMarker exec_marker(ctx_.api(), ctx_.current_cmd_buf(), "Eng::FgBuilder::Execute");

    const Ren::StoragesRef &storages = ctx_.storages();

    // Swap history images
    for (auto it = std::begin(name_to_image_); it != std::end(name_to_image_); ++it) {
        const auto &[fgimg_main, fgimg_cold] = images_.GetUnsafe(it->val);
        if (fgimg_cold.history_index != 0xffff) {
            const auto &[fghist_main, fghist_cold] = images_.GetUnsafe(fgimg_cold.history_index);
            if (fghist_main.handle_to_use) {
                assert(fghist_cold.lifetime.is_used());
                std::swap(fgimg_main.handle_to_use, fghist_main.handle_to_use);
            }
        }
    }
    // Reset resources
    for (auto it = std::begin(name_to_buffer_); it != std::end(name_to_buffer_); ++it) {
        const auto &[fgbuf_main, fgbuf_cold] = buffers_.GetUnsafe(it->val);

        buffers_.SetGeneration(it->val, 0);
        fgbuf_cold.used_in_stages = {};
        if (fgbuf_main.handle) {
            const Ren::BufferMain &buf_main = storages.buffers[fgbuf_main.handle].first;
            fgbuf_cold.used_in_stages = StagesForState(buf_main.resource_state);
        }
    }
    for (auto it = std::begin(name_to_image_); it != std::end(name_to_image_); ++it) {
        const auto &[fgimg_main, fgimg_cold] = images_.GetUnsafe(it->val);

        images_.SetGeneration(it->val, 0);
        fgimg_cold.used_in_stages = {};
        if (fgimg_main.handle_to_use) {
            const Ren::ImageMain &img_main = storages.images[fgimg_main.handle_to_use].first;
            fgimg_cold.used_in_stages = StagesForState(img_main.resource_state);
        }
    }

    BuildResourceLinkedLists();

#if defined(REN_GL_BACKEND)
    rast_state_.Apply();
#endif

    node_timings_[ctx_.backend_frame()].clear();
    // Write timestamp at the beginning of execution
    const int query_beg = ctx_.WriteTimestamp(true);

    const bool DetailedTimestamps = (ctx_.validation_level() > 0);

    for (FgNode *cur_node : reordered_nodes_) {
        OPTICK_GPU_EVENT("Execute Node");
        OPTICK_TAG("Node Name", cur_node->name().data());

#if !defined(NDEBUG) && defined(REN_GL_BACKEND)
        Ren::ResetGLState();
#endif

        Ren::DebugMarker _(ctx_.api(), ctx_.current_cmd_buf(), cur_node->name());

        // Start timestamp
        node_timing_t &node_interval = node_timings_[ctx_.backend_frame()].emplace_back();
        node_interval.name = cur_node->name();
        if (DetailedTimestamps) {
            node_interval.query_beg = ctx_.WriteTimestamp(true);
        }

        InsertResourceTransitions(*cur_node);
        cur_node->Execute(*this);

        // End timestamp
        if (DetailedTimestamps) {
            node_interval.query_end = ctx_.WriteTimestamp(false);
        }
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
    Ren::Bitmask<Ren::eStage> src_stages, dst_stages;

    for (const FgResource &res : node.input_) {
        HandleResourceTransition(res, res_transitions, src_stages, dst_stages);
    }

    for (const FgResource &res : node.output_) {
        HandleResourceTransition(res, res_transitions, src_stages, dst_stages);
    }

    TransitionResourceStates(ctx_.api(), ctx_.storages(), ctx_.current_cmd_buf(), src_stages, dst_stages,
                             res_transitions);
}

void Eng::FgBuilder::CheckResourceStates(FgNode &node) {
    const Ren::StoragesRef &storages = ctx_.storages();
    for (const FgResource &res : node.input_) {
        if (res.type == eFgResType::Buffer) {
            const auto &[fgbuf_main, fgbuf_cold] = buffers_[FgBufROHandle{res.opaque_handle}];
            const Ren::BufferMain &buf_main = storages.buffers[fgbuf_main.handle].first;
            assert(buf_main.resource_state == res.desired_state && "Buffer is in unexpected state!");
        } else if (res.type == eFgResType::Image) {
            const auto &[fgimg_main, fgimg_cold] = images_[FgImgROHandle{res.opaque_handle}];
            const Ren::ImageMain &img_main = storages.images[fgimg_main.handle_to_use].first;
            assert(img_main.resource_state == res.desired_state && "Image is in unexpected state!");
        }
    }
    for (const FgResource &res : node.output_) {
        if (res.type == eFgResType::Buffer) {
            const auto &[fgbuf_main, fgbuf_cold] = buffers_[FgBufROHandle{res.opaque_handle}];
            const Ren::BufferMain &buf_main = storages.buffers[fgbuf_main.handle].first;
            assert(buf_main.resource_state == res.desired_state && "Buffer is in unexpected state!");
        } else if (res.type == eFgResType::Image) {
            const auto &[fgimg_main, fgimg_cold] = images_[FgImgROHandle{res.opaque_handle}];
            const Ren::ImageMain &img_main = storages.images[fgimg_main.handle_to_use].first;
            assert(img_main.resource_state == res.desired_state && "Image is in unexpected state!");
        }
    }
}

void Eng::FgBuilder::HandleResourceTransition(const FgResource &res,
                                              Ren::SmallVectorImpl<Ren::TransitionInfo> &res_transitions,
                                              Ren::Bitmask<Ren::eStage> &src_stages,
                                              Ren::Bitmask<Ren::eStage> &dst_stages) {
    const Ren::StoragesRef &storages = ctx_.storages();

    for (const FgResource *next_res = res.next_use; next_res; next_res = next_res->next_use) {
        if (next_res->desired_state != res.desired_state || IsRWState(next_res->desired_state)) {
            break;
        }
        dst_stages |= next_res->stages;
    }

    if (res.type == eFgResType::Buffer) {
        const FgAllocBufMain *fgbuf_main = &buffers_[FgBufROHandle{res.opaque_handle}].first;
        FgAllocBufCold *fgbuf_cold = &buffers_[FgBufRWHandle{res.opaque_handle}].second;

        if (fgbuf_cold->alias_of != -1) {
            fgbuf_main = &buffers_.GetUnsafe(fgbuf_cold->alias_of).first;
            fgbuf_cold = &buffers_.GetUnsafe(fgbuf_cold->alias_of).second;
            assert(fgbuf_cold->alias_of == -1);
        }

        const auto &[buf_main, buf_cold] = storages.buffers[fgbuf_main->handle];
        if (buf_main.resource_state == Ren::eResState::Undefined ||
            buf_main.resource_state == Ren::eResState::Discarded) {
            for (const FgResRef other : fgbuf_cold->overlaps_with) {
                if (other.type == eFgResType::Buffer) {
                    const auto &[other_main, other_cold] = buffers_.GetUnsafe(other.index);
                    src_stages |= other_cold.used_in_stages;
                    dst_stages |= other_cold.aliased_in_stages;

                    const auto &[otherbuf_main, otherbuf_cold] = storages.buffers[other_main.handle];
                    assert(otherbuf_main.resource_state != Ren::eResState::Discarded);
                    res_transitions.emplace_back(other_main.handle, Ren::eResState::Discarded);
                } else if (other.type == eFgResType::Image) {
                    const auto &[other_main, other_cold] = images_.GetUnsafe(other.index);
                    src_stages |= other_cold.used_in_stages;
                    dst_stages |= other_cold.aliased_in_stages;

                    const auto &[otherimg_main, otherimg_cold] = storages.images[other_main.handle_to_use];
                    assert(otherimg_main.resource_state != Ren::eResState::Discarded);
                    res_transitions.emplace_back(other_main.handle_to_use, Ren::eResState::Discarded);
                }
            }
        }

        if (buf_main.resource_state != res.desired_state || IsRWState(buf_main.resource_state)) {
            src_stages |= fgbuf_cold->used_in_stages;
            dst_stages |= res.stages;
            fgbuf_cold->used_in_stages = {};
            res_transitions.emplace_back(fgbuf_main->handle, res.desired_state);
        }

        fgbuf_cold->used_in_stages |= res.stages;
    } else if (res.type == eFgResType::Image) {
        const FgAllocImgMain *fgimg_main = &images_[FgImgROHandle{res.opaque_handle}].first;
        FgAllocImgCold *fgimg_cold = &images_[FgImgRWHandle{res.opaque_handle}].second;

        if (fgimg_cold->alias_of != -1) {
            fgimg_main = &images_.GetUnsafe(fgimg_cold->alias_of).first;
            fgimg_cold = &images_.GetUnsafe(fgimg_cold->alias_of).second;
            assert(fgimg_cold->alias_of == -1);
        }

        const auto &[img_main, img_cold] = storages.images[fgimg_main->handle_to_use];
        if (img_main.resource_state == Ren::eResState::Undefined ||
            img_main.resource_state == Ren::eResState::Discarded) {
            for (const FgResRef other : fgimg_cold->overlaps_with) {
                if (other.type == eFgResType::Buffer) {
                    const auto &[other_main, other_cold] = buffers_.GetUnsafe(other.index);
                    src_stages |= other_cold.used_in_stages;
                    dst_stages |= other_cold.aliased_in_stages;

                    const auto &[otherbuf_main, otherbuf_cold] = storages.buffers[other_main.handle];
                    assert(otherbuf_main.resource_state != Ren::eResState::Discarded);
                    res_transitions.emplace_back(other_main.handle, Ren::eResState::Discarded);
                } else if (other.type == eFgResType::Image) {
                    const auto &[other_main, other_cold] = images_.GetUnsafe(other.index);
                    src_stages |= other_cold.used_in_stages;
                    dst_stages |= other_cold.aliased_in_stages;

                    const auto &[otherimg_main, otherimg_cold] = storages.images[other_main.handle_to_use];
                    assert(otherimg_main.resource_state != Ren::eResState::Discarded);
                    res_transitions.emplace_back(other_main.handle_to_use, Ren::eResState::Discarded);
                }
            }
        }

        if (img_main.resource_state != res.desired_state || IsRWState(img_main.resource_state)) {
            src_stages |= fgimg_cold->used_in_stages;
            dst_stages |= res.stages;
            fgimg_cold->used_in_stages = {};
            res_transitions.emplace_back(fgimg_main->handle_to_use, res.desired_state);
        }

        fgimg_cold->used_in_stages |= res.stages;
    }
}

void Eng::FgBuilder::ClearBuffer_AsTransfer(const Ren::BufferHandle buf, Ren::CommandBuffer cmd_buf) {
    const auto &[buf_main, buf_cold] = ctx_.storages().buffers[buf];
    Buffer_Fill(ctx_.api(), buf_main, 0, buf_cold.size, 0, cmd_buf);
}

void Eng::FgBuilder::ClearBuffer_AsStorage(const Ren::BufferHandle buf, Ren::CommandBuffer cmd_buf) {
    const Ren::Binding bindings[] = {{Ren::eBindTarget::SBufRW, ClearBuffer::OUT_BUF_SLOT, buf}};

    const auto &[buf_main, buf_cold] = ctx_.storages().buffers[buf];
    assert((buf_cold.size % 4) == 0);

    const Ren::Vec3u grp_count = Ren::Vec3u{Ren::DivCeil<uint32_t>(buf_cold.size / 4, ClearBuffer::GRP_SIZE_X), 1u, 1u};

    ClearBuffer::Params uniform_params;
    uniform_params.data_len = (buf_cold.size / 4);

    DispatchCompute(cmd_buf, pi_clear_buffer_, ctx_.storages(), grp_count, bindings, &uniform_params,
                    sizeof(ClearBuffer::Params), ctx_.default_descr_alloc(), ctx_.log());
}

void Eng::FgBuilder::ClearImage_AsTransfer(const Ren::ImageHandle img, Ren::CommandBuffer cmd_buf) {
    // NOTE: we can not really use anything other than zero due to aliasing
    ctx_.CmdClearImage(img, {}, cmd_buf);
}

void Eng::FgBuilder::ClearImage_AsStorage(const Ren::ImageHandle img, Ren::CommandBuffer cmd_buf) {
    const Ren::ImgParams &p = ctx_.storages().images[img].second.params;
    const Ren::PipelineHandle pi = (p.flags & Ren::eImgFlags::Array) ? pi_clear_image_[1][int(p.format)]
                                                                     : (p.d != 0 ? pi_clear_image_[2][int(p.format)]
                                                                                 : pi_clear_image_[0][int(p.format)]);
    assert(pi && "No clear pipeline registered for this image format");

    const Ren::Binding bindings[] = {{Ren::eBindTarget::ImageRW, ClearImage::OUT_IMG_SLOT, img}};

    const Ren::Vec3u grp_count =
        Ren::Vec3u{Ren::DivCeil<uint32_t>(p.w, ClearImage::GRP_SIZE_X),
                   Ren::DivCeil<uint32_t>(p.h, ClearImage::GRP_SIZE_Y), std::max<uint32_t>(p.d, 1)};

    DispatchCompute(cmd_buf, pi, ctx_.storages(), grp_count, bindings, nullptr, 0, ctx_.default_descr_alloc(),
                    ctx_.log());
}

void Eng::FgBuilder::ClearImage_AsTarget(const Ren::ImageHandle img, Ren::CommandBuffer cmd_buf) {
    const Ren::ImgParams &p = ctx_.storages().images[img].second.params;

    Ren::RenderTarget depth_target;
    Ren::SmallVector<Ren::RenderTarget, 1> color_target;

    if (IsDepthFormat(p.format)) {
        depth_target = {img, Ren::eLoadOp::Clear, Ren::eStoreOp::Store};
    } else {
        color_target.emplace_back(img, Ren::eLoadOp::Clear, Ren::eStoreOp::Store);
    }

    prim_draw_.ClearTarget(cmd_buf, depth_target, color_target, framebuffers_.get());
}
