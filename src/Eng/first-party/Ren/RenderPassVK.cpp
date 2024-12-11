#include "RenderPass.h"

#include "VKCtx.h"

#ifndef NDEBUG
#define VERBOSE_LOGGING
#endif

namespace Ren {
static_assert(int(eImageLayout::Undefined) == VK_IMAGE_LAYOUT_UNDEFINED, "!");
static_assert(int(eImageLayout::General) == VK_IMAGE_LAYOUT_GENERAL, "!");
static_assert(int(eImageLayout::ColorAttachmentOptimal) == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, "!");
static_assert(int(eImageLayout::DepthStencilAttachmentOptimal) == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
              "!");
static_assert(int(eImageLayout::DepthStencilReadOnlyOptimal) == VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL, "!");
static_assert(int(eImageLayout::ShaderReadOnlyOptimal) == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, "!");
static_assert(int(eImageLayout::TransferSrcOptimal) == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, "!");
static_assert(int(eImageLayout::TransferDstOptimal) == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, "!");

extern const VkAttachmentLoadOp vk_load_ops[] = {
    VK_ATTACHMENT_LOAD_OP_LOAD,      // Load
    VK_ATTACHMENT_LOAD_OP_CLEAR,     // Clear
    VK_ATTACHMENT_LOAD_OP_DONT_CARE, // DontCare
    VK_ATTACHMENT_LOAD_OP_NONE_EXT   // None
};
static_assert(std::size(vk_load_ops) == int(eLoadOp::_Count), "!");

extern const VkAttachmentStoreOp vk_store_ops[] = {
    VK_ATTACHMENT_STORE_OP_STORE,     // Store
    VK_ATTACHMENT_STORE_OP_DONT_CARE, // DontCare
    VK_ATTACHMENT_STORE_OP_NONE_EXT   // None
};
static_assert(std::size(vk_store_ops) == int(eStoreOp::_Count), "!");

// make sure we can simply cast these
static_assert(VkSampleCountFlagBits::VK_SAMPLE_COUNT_1_BIT == 1, "!");
static_assert(VkSampleCountFlagBits::VK_SAMPLE_COUNT_2_BIT == 2, "!");
static_assert(VkSampleCountFlagBits::VK_SAMPLE_COUNT_4_BIT == 4, "!");
static_assert(VkSampleCountFlagBits::VK_SAMPLE_COUNT_8_BIT == 8, "!");

VkFormat ToSRGBFormat(VkFormat format);
} // namespace Ren

Ren::RenderPass &Ren::RenderPass::operator=(RenderPass &&rhs) noexcept {
    if (this == &rhs) {
        return (*this);
    }

    Destroy();

    api_ctx_ = std::exchange(rhs.api_ctx_, nullptr);
    handle_ = std::exchange(rhs.handle_, {});
    color_rts = std::move(rhs.color_rts);
    depth_rt = std::exchange(rhs.depth_rt, {});

    return (*this);
}

bool Ren::RenderPass::Init(ApiContext *api_ctx, RenderTargetInfo _depth_rt, Span<const RenderTargetInfo> _color_rts,
                           ILog *log) {
    Destroy();

    SmallVector<VkAttachmentDescription, 4> pass_attachments;
    SmallVector<VkAttachmentReference, 4> color_attachment_refs(uint32_t(_color_rts.size()),
                                                                {VK_ATTACHMENT_UNUSED, VK_IMAGE_LAYOUT_UNDEFINED});
    VkAttachmentReference depth_attachment_ref = {VK_ATTACHMENT_UNUSED, VK_IMAGE_LAYOUT_UNDEFINED};

    color_rts.resize(uint32_t(_color_rts.size()));
    depth_rt = {};

    if (_depth_rt) {
        const auto att_index = uint32_t(pass_attachments.size());

        auto &att_desc = pass_attachments.emplace_back();
        att_desc.format = Ren::VKFormatFromTexFormat(_depth_rt.format);
        att_desc.samples = VkSampleCountFlagBits(_depth_rt.samples);
        att_desc.loadOp = vk_load_ops[int(_depth_rt.load)];
        if (att_desc.loadOp == VK_ATTACHMENT_LOAD_OP_NONE_EXT && !api_ctx->renderpass_loadstore_none_supported) {
            att_desc.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
        }
        att_desc.storeOp = vk_store_ops[int(_depth_rt.store)];
        if (att_desc.storeOp == VK_ATTACHMENT_STORE_OP_NONE_EXT && !api_ctx->renderpass_loadstore_none_supported) {
            att_desc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        }
        att_desc.stencilLoadOp = vk_load_ops[int(_depth_rt.stencil_load)];
        if (att_desc.stencilLoadOp == VK_ATTACHMENT_LOAD_OP_NONE_EXT && !api_ctx->renderpass_loadstore_none_supported) {
            att_desc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
        }
        att_desc.stencilStoreOp = vk_store_ops[int(_depth_rt.stencil_store)];
        if (att_desc.stencilStoreOp == VK_ATTACHMENT_STORE_OP_NONE_EXT &&
            !api_ctx->renderpass_loadstore_none_supported) {
            att_desc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;
        }
        att_desc.initialLayout = VkImageLayout(_depth_rt.layout);
        att_desc.finalLayout = att_desc.initialLayout;

        depth_attachment_ref.attachment = att_index;
        depth_attachment_ref.layout = att_desc.initialLayout;

        depth_rt = _depth_rt;
    }

    for (int i = 0; i < _color_rts.size(); ++i) {
        if (!_color_rts[i]) {
            continue;
        }

        const auto att_index = uint32_t(pass_attachments.size());

        auto &att_desc = pass_attachments.emplace_back();
        att_desc.format = VKFormatFromTexFormat(_color_rts[i].format);
        if (bool(_color_rts[i].flags & eTexFlagBits::SRGB)) {
            att_desc.format = ToSRGBFormat(att_desc.format);
        }
        att_desc.samples = VkSampleCountFlagBits(_color_rts[i].samples);
        if (VkImageLayout(_color_rts[i].layout) == VK_IMAGE_LAYOUT_UNDEFINED) {
            att_desc.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            att_desc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        } else {
            att_desc.loadOp = vk_load_ops[int(_color_rts[i].load)];
            att_desc.stencilLoadOp = vk_load_ops[int(_color_rts[i].load)];
        }
        att_desc.storeOp = vk_store_ops[int(_color_rts[i].store)];
        att_desc.stencilStoreOp = vk_store_ops[int(_color_rts[i].store)];
        att_desc.initialLayout = VkImageLayout(_color_rts[i].layout);
        att_desc.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        if (att_desc.loadOp == VK_ATTACHMENT_LOAD_OP_NONE_EXT && !api_ctx->renderpass_loadstore_none_supported) {
            att_desc.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
        }
        if (att_desc.stencilLoadOp == VK_ATTACHMENT_LOAD_OP_NONE_EXT && !api_ctx->renderpass_loadstore_none_supported) {
            att_desc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
        }
        if (att_desc.storeOp == VK_ATTACHMENT_STORE_OP_NONE && !api_ctx->renderpass_loadstore_none_supported) {
            att_desc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        }
        if (att_desc.stencilStoreOp == VK_ATTACHMENT_STORE_OP_NONE && !api_ctx->renderpass_loadstore_none_supported) {
            att_desc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;
        }

        color_attachment_refs[i].attachment = att_index;
        color_attachment_refs[i].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        color_rts[i] = _color_rts[i];
    }

    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = uint32_t(_color_rts.size());
    subpass.pColorAttachments = color_attachment_refs.data();
    if (depth_attachment_ref.attachment != VK_ATTACHMENT_UNUSED) {
        subpass.pDepthStencilAttachment = &depth_attachment_ref;
    }

    VkRenderPassCreateInfo render_pass_create_info = {VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    render_pass_create_info.attachmentCount = uint32_t(pass_attachments.size());
    render_pass_create_info.pAttachments = pass_attachments.data();
    render_pass_create_info.subpassCount = 1;
    render_pass_create_info.pSubpasses = &subpass;

    const VkResult res = api_ctx->vkCreateRenderPass(api_ctx->device, &render_pass_create_info, nullptr, &handle_);
    if (res != VK_SUCCESS) {
        log->Error("Failed to create render pass!");
        return false;
#ifdef VERBOSE_LOGGING
    } else {
        log->Info("RenderPass %p created", handle_);
#endif
    }

    api_ctx_ = api_ctx;
    return true;
}

void Ren::RenderPass::Destroy() {
    if (handle_ != VK_NULL_HANDLE) {
        api_ctx_->render_passes_to_destroy[api_ctx_->backend_frame].push_back(handle_);
        handle_ = VK_NULL_HANDLE;
    }
    color_rts.clear();
    depth_rt = {};
}

bool Ren::RenderPass::IsCompatibleWith(RenderTarget _depth_rt, Span<const RenderTarget> _color_rts) {
    return depth_rt == _depth_rt && Span<const RenderTargetInfo>(color_rts) == _color_rts;
}

bool Ren::RenderPass::Setup(ApiContext *api_ctx, const RenderTarget _depth_rt, Span<const RenderTarget> _color_rts,
                            ILog *log) {
    if (depth_rt == _depth_rt && Span<const RenderTargetInfo>(color_rts) == _color_rts) {
        return true;
    }

    SmallVector<RenderTargetInfo, 4> infos;
    for (int i = 0; i < _color_rts.size(); ++i) {
        infos.emplace_back(_color_rts[i]);
    }

    return Init(api_ctx, RenderTargetInfo{_depth_rt}, infos, log);
}

bool Ren::RenderPass::Setup(ApiContext *api_ctx, RenderTargetInfo _depth_rt, Span<const RenderTargetInfo> _color_rts,
                            ILog *log) {
    if (depth_rt == _depth_rt && Span<const RenderTargetInfo>(color_rts) == _color_rts) {
        return true;
    }
    return Init(api_ctx, _depth_rt, _color_rts, log);
}

#undef VERBOSE_LOGGING
