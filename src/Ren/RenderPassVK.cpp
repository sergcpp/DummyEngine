#include "RenderPass.h"

#include "VKCtx.h"

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

const VkAttachmentLoadOp vk_load_ops[] = {
    VK_ATTACHMENT_LOAD_OP_LOAD,     // Load
    VK_ATTACHMENT_LOAD_OP_CLEAR,    // Clear
    VK_ATTACHMENT_LOAD_OP_DONT_CARE // DontCare
};
static_assert((sizeof(vk_load_ops) / sizeof(vk_load_ops[0])) == int(eLoadOp::_Count), "!");

const VkAttachmentStoreOp vk_store_ops[] = {
    VK_ATTACHMENT_STORE_OP_STORE,    // Store
    VK_ATTACHMENT_STORE_OP_DONT_CARE // DontCare
};
static_assert((sizeof(vk_store_ops) / sizeof(vk_store_ops[0])) == int(eStoreOp::_Count), "!");

// make sure we can simply cast these
static_assert(VkSampleCountFlagBits::VK_SAMPLE_COUNT_1_BIT == 1, "!");
static_assert(VkSampleCountFlagBits::VK_SAMPLE_COUNT_2_BIT == 2, "!");
static_assert(VkSampleCountFlagBits::VK_SAMPLE_COUNT_4_BIT == 4, "!");
static_assert(VkSampleCountFlagBits::VK_SAMPLE_COUNT_8_BIT == 8, "!");
} // namespace Ren

Ren::RenderPass &Ren::RenderPass::operator=(RenderPass &&rhs) noexcept {
    if (this == &rhs) {
        return (*this);
    }

    Destroy();

    api_ctx_ = exchange(rhs.api_ctx_, nullptr);
    handle_ = exchange(rhs.handle_, {});
    color_rts = std::move(rhs.color_rts);
    depth_rt = exchange(rhs.depth_rt, {});

    return (*this);
}

bool Ren::RenderPass::Init(ApiContext *api_ctx, const RenderTargetInfo _color_rts[], int _color_rts_count,
                           RenderTargetInfo _depth_rt, ILog *log) {
    Destroy();

    SmallVector<VkAttachmentDescription, MaxRTAttachments> pass_attachments;
    VkAttachmentReference color_attachment_refs[MaxRTAttachments];
    for (int i = 0; i < MaxRTAttachments; i++) {
        color_attachment_refs[i] = {VK_ATTACHMENT_UNUSED, VK_IMAGE_LAYOUT_UNDEFINED};
    }
    VkAttachmentReference depth_attachment_ref = {VK_ATTACHMENT_UNUSED, VK_IMAGE_LAYOUT_UNDEFINED};

    color_rts.resize(_color_rts_count);
    depth_rt = {};

    for (int i = 0; i < _color_rts_count; ++i) {
        if (!_color_rts[i]) {
            continue;
        }

        const uint32_t att_index = uint32_t(pass_attachments.size());

        auto &att_desc = pass_attachments.emplace_back();
        att_desc.format = Ren::VKFormatFromTexFormat(_color_rts[i].format);
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

        color_attachment_refs[i].attachment = att_index;
        color_attachment_refs[i].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        color_rts[i] = _color_rts[i];
    }

    if (_depth_rt) {
        const uint32_t att_index = uint32_t(pass_attachments.size());

        auto &att_desc = pass_attachments.emplace_back();
        att_desc.format = Ren::VKFormatFromTexFormat(_depth_rt.format);
        att_desc.samples = VkSampleCountFlagBits(_depth_rt.samples);
        att_desc.loadOp = vk_load_ops[int(_depth_rt.load)];
        att_desc.storeOp = vk_store_ops[int(_depth_rt.store)];
        att_desc.stencilLoadOp = vk_load_ops[int(_depth_rt.stencil_load)];
        att_desc.stencilStoreOp = vk_store_ops[int(_depth_rt.stencil_store)];
        att_desc.initialLayout = VkImageLayout(_depth_rt.layout);
        att_desc.finalLayout = att_desc.initialLayout;

        depth_attachment_ref.attachment = att_index;
        depth_attachment_ref.layout = att_desc.initialLayout;

        depth_rt = _depth_rt;
    }

    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = _color_rts_count;
    subpass.pColorAttachments = color_attachment_refs;
    if (depth_attachment_ref.attachment != VK_ATTACHMENT_UNUSED) {
        subpass.pDepthStencilAttachment = &depth_attachment_ref;
    }

    VkRenderPassCreateInfo render_pass_create_info = {VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    render_pass_create_info.attachmentCount = uint32_t(pass_attachments.size());
    render_pass_create_info.pAttachments = pass_attachments.data();
    render_pass_create_info.subpassCount = 1;
    render_pass_create_info.pSubpasses = &subpass;

    const VkResult res = vkCreateRenderPass(api_ctx->device, &render_pass_create_info, nullptr, &handle_);
    if (res != VK_SUCCESS) {
        log->Error("Failed to create render pass!");
        return false;
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

bool Ren::RenderPass::Setup(ApiContext *api_ctx, const RenderTarget _color_rts[], const int _color_rts_count,
                            const RenderTarget _depth_rt, ILog *log) {
    if (_color_rts_count == color_rts.size() &&
        std::equal(
            _color_rts, _color_rts + _color_rts_count, color_rts.data(),
            [](const RenderTarget &rt, const RenderTargetInfo &i) { return (!rt.ref && !i) || (rt.ref && i == rt); }) &&
        ((!_depth_rt.ref && !depth_rt) || (_depth_rt.ref && depth_rt == _depth_rt))) {
        return true;
    }

    SmallVector<RenderTargetInfo, MaxRTAttachments> infos;
    for (int i = 0; i < _color_rts_count; ++i) {
        infos.emplace_back(_color_rts[i]);
    }

    return Init(api_ctx, infos.data(), _color_rts_count, RenderTargetInfo{_depth_rt}, log);
}

bool Ren::RenderPass::Setup(ApiContext *api_ctx, const RenderTargetInfo _color_rts[], int _color_rts_count,
                            RenderTargetInfo _depth_rt, ILog *log) {
    if (_color_rts_count == color_rts.size() &&
        std::equal(_color_rts, _color_rts + _color_rts_count, color_rts.data()) &&
        ((!_depth_rt && !depth_rt) || (_depth_rt && depth_rt == _depth_rt))) {
        return true;
    }
    return Init(api_ctx, _color_rts, _color_rts_count, _depth_rt, log);
}
