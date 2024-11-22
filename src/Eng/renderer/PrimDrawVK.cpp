#include "PrimDraw.h"

#include <Ren/Context.h>
#include <Ren/Framebuffer.h>
#include <Ren/ProbeStorage.h>
#include <Ren/VKCtx.h>

namespace Ren {
extern const VkAttachmentLoadOp vk_load_ops[];
extern const VkAttachmentStoreOp vk_store_ops[];
} // namespace Ren

namespace PrimDrawInternal {
extern const int SphereIndicesCount;
} // namespace PrimDrawInternal

Eng::PrimDraw::~PrimDraw() {}

void Eng::PrimDraw::Reset() {}

void Eng::PrimDraw::DrawPrim(const ePrim prim, const Ren::ProgramRef &p, Ren::Span<const Ren::RenderTarget> color_rts,
                             Ren::RenderTarget depth_rt, const Ren::RastState &new_rast_state,
                             Ren::RastState &applied_rast_state, Ren::Span<const Ren::Binding> bindings,
                             const void *uniform_data, const int uniform_data_len, const int uniform_data_offset,
                             const int instances) {
    using namespace PrimDrawInternal;

    Ren::ApiContext *api_ctx = ctx_->api_ctx();

    VkDescriptorSetLayout descr_set_layout = p->descr_set_layouts()[0];
    VkDescriptorSet descr_set =
        PrepareDescriptorSet(api_ctx, descr_set_layout, bindings, ctx_->default_descr_alloc(), ctx_->log());

    VkCommandBuffer cmd_buf = api_ctx->draw_cmd_buf[api_ctx->backend_frame];

    { // transition resources if required
        VkPipelineStageFlags src_stages = 0, dst_stages = 0;

        Ren::SmallVector<VkImageMemoryBarrier, 16> img_barriers;
        Ren::SmallVector<VkBufferMemoryBarrier, 4> buf_barriers;

        for (const auto &b : bindings) {
            if (b.trg == Ren::eBindTarget::Tex2DSampled) {
                assert(b.handle.tex->resource_state == Ren::eResState::ShaderResource ||
                       b.handle.tex->resource_state == Ren::eResState::DepthRead ||
                       b.handle.tex->resource_state == Ren::eResState::StencilTestDepthFetch);
            }
        }

        for (const auto &rt : color_rts) {
            if (rt.ref->resource_state != Ren::eResState::RenderTarget) {
                auto &new_barrier = img_barriers.emplace_back();
                new_barrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
                new_barrier.srcAccessMask = Ren::VKAccessFlagsForState(rt.ref->resource_state);
                new_barrier.dstAccessMask = Ren::VKAccessFlagsForState(Ren::eResState::RenderTarget);
                new_barrier.oldLayout = VkImageLayout(Ren::VKImageLayoutForState(rt.ref->resource_state));
                new_barrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                new_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                new_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                new_barrier.image = rt.ref->handle().img;
                new_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                new_barrier.subresourceRange.baseMipLevel = 0;
                new_barrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
                new_barrier.subresourceRange.baseArrayLayer = 0;
                new_barrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

                src_stages |= Ren::VKPipelineStagesForState(rt.ref->resource_state);
                dst_stages |= Ren::VKPipelineStagesForState(Ren::eResState::RenderTarget);

                rt.ref->resource_state = Ren::eResState::RenderTarget;
            }
        }

        if (depth_rt.ref && depth_rt.ref->resource_state != Ren::eResState::DepthWrite &&
            depth_rt.ref->resource_state != Ren::eResState::DepthRead &&
            depth_rt.ref->resource_state != Ren::eResState::StencilTestDepthFetch) {
            auto &new_barrier = img_barriers.emplace_back();
            new_barrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
            new_barrier.srcAccessMask = Ren::VKAccessFlagsForState(depth_rt.ref->resource_state);
            new_barrier.dstAccessMask = Ren::VKAccessFlagsForState(Ren::eResState::DepthWrite);
            new_barrier.oldLayout = VkImageLayout(Ren::VKImageLayoutForState(depth_rt.ref->resource_state));
            new_barrier.newLayout = VkImageLayout(Ren::VKImageLayoutForState(Ren::eResState::DepthWrite));
            new_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            new_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            new_barrier.image = depth_rt.ref->handle().img;
            new_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
            new_barrier.subresourceRange.baseMipLevel = 0;
            new_barrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
            new_barrier.subresourceRange.baseArrayLayer = 0;
            new_barrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

            src_stages |= Ren::VKPipelineStagesForState(depth_rt.ref->resource_state);
            dst_stages |= Ren::VKPipelineStagesForState(Ren::eResState::DepthWrite);

            depth_rt.ref->resource_state = Ren::eResState::DepthWrite;
        }

        if (ctx_->default_vertex_buf1()->resource_state != Ren::eResState::VertexBuffer) {
            auto &new_barrier = buf_barriers.emplace_back();
            new_barrier = {VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
            new_barrier.srcAccessMask = Ren::VKAccessFlagsForState(ctx_->default_vertex_buf1()->resource_state);
            new_barrier.dstAccessMask = Ren::VKAccessFlagsForState(Ren::eResState::VertexBuffer);
            new_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            new_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            new_barrier.buffer = ctx_->default_vertex_buf1()->vk_handle();
            new_barrier.offset = VkDeviceSize{0};
            new_barrier.size = VkDeviceSize{ctx_->default_vertex_buf1()->size()};

            src_stages |= Ren::VKPipelineStagesForState(ctx_->default_vertex_buf1()->resource_state);
            dst_stages |= Ren::VKPipelineStagesForState(Ren::eResState::VertexBuffer);

            ctx_->default_vertex_buf1()->resource_state = Ren::eResState::VertexBuffer;
        }

        if (ctx_->default_vertex_buf2()->resource_state != Ren::eResState::VertexBuffer) {
            auto &new_barrier = buf_barriers.emplace_back();
            new_barrier = {VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
            new_barrier.srcAccessMask = Ren::VKAccessFlagsForState(ctx_->default_vertex_buf2()->resource_state);
            new_barrier.dstAccessMask = Ren::VKAccessFlagsForState(Ren::eResState::VertexBuffer);
            new_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            new_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            new_barrier.buffer = ctx_->default_vertex_buf2()->vk_handle();
            new_barrier.offset = VkDeviceSize{0};
            new_barrier.size = VkDeviceSize{ctx_->default_vertex_buf2()->size()};

            src_stages |= Ren::VKPipelineStagesForState(ctx_->default_vertex_buf2()->resource_state);
            dst_stages |= Ren::VKPipelineStagesForState(Ren::eResState::VertexBuffer);

            ctx_->default_vertex_buf2()->resource_state = Ren::eResState::VertexBuffer;
        }

        if (ctx_->default_indices_buf()->resource_state != Ren::eResState::IndexBuffer) {
            auto &new_barrier = buf_barriers.emplace_back();
            new_barrier = {VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
            new_barrier.srcAccessMask = Ren::VKAccessFlagsForState(ctx_->default_indices_buf()->resource_state);
            new_barrier.dstAccessMask = Ren::VKAccessFlagsForState(Ren::eResState::IndexBuffer);
            new_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            new_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            new_barrier.buffer = ctx_->default_indices_buf()->vk_handle();
            new_barrier.offset = VkDeviceSize{0};
            new_barrier.size = VkDeviceSize{ctx_->default_indices_buf()->size()};

            src_stages |= Ren::VKPipelineStagesForState(ctx_->default_indices_buf()->resource_state);
            dst_stages |= Ren::VKPipelineStagesForState(Ren::eResState::IndexBuffer);

            ctx_->default_indices_buf()->resource_state = Ren::eResState::IndexBuffer;
        }

        src_stages &= ctx_->api_ctx()->supported_stages_mask;
        dst_stages &= ctx_->api_ctx()->supported_stages_mask;

        if (!img_barriers.empty() || !buf_barriers.empty()) {
            ctx_->api_ctx()->vkCmdPipelineBarrier(
                cmd_buf, src_stages ? src_stages : VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, dst_stages, 0, 0, nullptr,
                uint32_t(buf_barriers.size()), buf_barriers.data(), uint32_t(img_barriers.size()), img_barriers.data());
        }
    }

    if (ctx_->capabilities.dynamic_rendering) {
        const Ren::Pipeline *pipeline = FindOrCreatePipeline(prim, p, nullptr, color_rts, depth_rt, &new_rast_state);
        assert(pipeline);

        Ren::SmallVector<VkRenderingAttachmentInfoKHR, 4> color_attachments;
        for (const auto &rt : color_rts) {
            auto &col_att = color_attachments.emplace_back();
            col_att = {VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR};
            col_att.imageView = rt.ref->handle().views[0];
            col_att.imageLayout = VkImageLayout(VKImageLayoutForState(rt.ref->resource_state));
            col_att.loadOp = Ren::vk_load_ops[int(rt.load)];
            col_att.storeOp = Ren::vk_store_ops[int(rt.store)];
        }

        VkRenderingAttachmentInfoKHR depth_attachment = {VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR};
        if (depth_rt) {
            depth_attachment.imageView = depth_rt.ref->handle().views[0];
            depth_attachment.imageLayout = VkImageLayout(VKImageLayoutForState(depth_rt.ref->resource_state));
            depth_attachment.loadOp = Ren::vk_load_ops[int(depth_rt.load)];
            depth_attachment.storeOp = Ren::vk_store_ops[int(depth_rt.store)];
        }

        VkRenderingInfoKHR render_info = {VK_STRUCTURE_TYPE_RENDERING_INFO_KHR};
        render_info.renderArea.offset.x = new_rast_state.viewport[0];
        render_info.renderArea.offset.y = new_rast_state.viewport[1];
        render_info.renderArea.extent.width = new_rast_state.viewport[2];
        render_info.renderArea.extent.height = new_rast_state.viewport[3];
        render_info.layerCount = 1;
        render_info.colorAttachmentCount = uint32_t(color_attachments.size());
        render_info.pColorAttachments = color_attachments.data();
        render_info.pDepthAttachment = depth_rt ? &depth_attachment : nullptr;
        render_info.pStencilAttachment = new_rast_state.stencil.enabled ? &depth_attachment : nullptr;

        api_ctx->vkCmdBeginRenderingKHR(cmd_buf, &render_info);

        api_ctx->vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->handle());

        const VkViewport viewport = {0.0f, 0.0f, float(new_rast_state.viewport[2]), float(new_rast_state.viewport[3]),
                                     0.0f, 1.0f};
        api_ctx->vkCmdSetViewport(cmd_buf, 0, 1, &viewport);

        const VkRect2D scissor = {{0, 0}, {uint32_t(new_rast_state.viewport[2]), uint32_t(new_rast_state.viewport[3])}};
        api_ctx->vkCmdSetScissor(cmd_buf, 0, 1, &scissor);

        api_ctx->vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->layout(), 0, 1, &descr_set,
                                         0, nullptr);

        if (uniform_data) {
            api_ctx->vkCmdPushConstants(cmd_buf, pipeline->layout(), pipeline->prog()->pc_ranges()[0].stageFlags,
                                        uniform_data_offset, uniform_data_len, uniform_data);
        }

        if (prim == ePrim::Quad) {
            pipeline->vtx_input()->BindBuffers(api_ctx, cmd_buf, quad_ndx_.offset, VK_INDEX_TYPE_UINT16);

            api_ctx->vkCmdDrawIndexed(cmd_buf, uint32_t(6), // index count
                                      instances,            // instance count
                                      0,                    // first index
                                      0,                    // vertex offset
                                      0);                   // first instance
        } else if (prim == ePrim::Sphere) {
            pipeline->vtx_input()->BindBuffers(api_ctx, cmd_buf, sphere_ndx_.offset, VK_INDEX_TYPE_UINT16);

            api_ctx->vkCmdDrawIndexed(cmd_buf, uint32_t(SphereIndicesCount), // index count
                                      instances,                             // instance count
                                      0,                                     // first index
                                      0,                                     // vertex offset
                                      0);                                    // first instance
        }

        api_ctx->vkCmdEndRenderingKHR(cmd_buf);
    } else {
        const Ren::RenderPass *rp = FindOrCreateRenderPass(color_rts, depth_rt);
        const Ren::Framebuffer *fb =
            FindOrCreateFramebuffer(rp, color_rts, new_rast_state.depth.test_enabled ? depth_rt : Ren::RenderTarget{},
                                    new_rast_state.stencil.enabled ? depth_rt : Ren::RenderTarget{});
        const Ren::Pipeline *pipeline = FindOrCreatePipeline(prim, p, rp, {}, {}, &new_rast_state);

        VkRenderPassBeginInfo render_pass_begin_info = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
        render_pass_begin_info.renderPass = rp->handle();
        render_pass_begin_info.framebuffer = fb->handle();
        render_pass_begin_info.renderArea = {{0, 0}, {uint32_t(fb->w), uint32_t(fb->h)}};

        api_ctx->vkCmdBeginRenderPass(cmd_buf, &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);
        api_ctx->vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->handle());

        const VkViewport viewport = {0.0f, 0.0f, float(new_rast_state.viewport[2]), float(new_rast_state.viewport[3]),
                                     0.0f, 1.0f};
        api_ctx->vkCmdSetViewport(cmd_buf, 0, 1, &viewport);

        VkRect2D scissor = {{0, 0}, {uint32_t(new_rast_state.viewport[2]), uint32_t(new_rast_state.viewport[3])}};
        if (new_rast_state.scissor.enabled) {
            scissor = VkRect2D{{new_rast_state.scissor.rect[0], new_rast_state.scissor.rect[1]},
                               {uint32_t(new_rast_state.scissor.rect[2]), uint32_t(new_rast_state.scissor.rect[3])}};
        }
        api_ctx->vkCmdSetScissor(cmd_buf, 0, 1, &scissor);

        api_ctx->vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->layout(), 0, 1, &descr_set,
                                         0, nullptr);

        if (uniform_data) {
            api_ctx->vkCmdPushConstants(cmd_buf, pipeline->layout(), pipeline->prog()->pc_ranges()[0].stageFlags,
                                        uniform_data_offset, uniform_data_len, uniform_data);
        }

        if (prim == ePrim::Quad) {
            pipeline->vtx_input()->BindBuffers(api_ctx, cmd_buf, quad_ndx_.offset, VK_INDEX_TYPE_UINT16);

            api_ctx->vkCmdDrawIndexed(cmd_buf, uint32_t(6), // index count
                                      instances,            // instance count
                                      0,                    // first index
                                      0,                    // vertex offset
                                      0);                   // first instance
        } else if (prim == ePrim::Sphere) {
            pipeline->vtx_input()->BindBuffers(api_ctx, cmd_buf, sphere_ndx_.offset, VK_INDEX_TYPE_UINT16);

            api_ctx->vkCmdDrawIndexed(cmd_buf, uint32_t(SphereIndicesCount), // index count
                                      instances,                             // instance count
                                      0,                                     // first index
                                      0,                                     // vertex offset
                                      0);                                    // first instance
        }

        api_ctx->vkCmdEndRenderPass(cmd_buf);
    }
}

const Ren::RenderPass *Eng::PrimDraw::FindOrCreateRenderPass(Ren::Span<const Ren::RenderTarget> color_targets,
                                                             Ren::RenderTarget depth_target) {
    // TODO: binary search
    for (size_t i = 0; i < render_passes_.size(); ++i) {
        if (render_passes_[i].IsCompatibleWith(color_targets, depth_target)) {
            return &render_passes_[i];
        }
    }

    Ren::ApiContext *api_ctx = ctx_->api_ctx();
    Ren::RenderPass &new_render_pass = render_passes_.emplace_back();

    if (!new_render_pass.Setup(api_ctx, color_targets, depth_target, ctx_->log())) {
        ctx_->log()->Error("Failed to setup renderpass!");
        render_passes_.pop_back();
        return nullptr;
    }

    return &new_render_pass;
}

const Ren::Pipeline *Eng::PrimDraw::FindOrCreatePipeline(const ePrim prim, Ren::ProgramRef p, const Ren::RenderPass *rp,
                                                         Ren::Span<const Ren::RenderTarget> color_targets,
                                                         const Ren::RenderTarget depth_target,
                                                         const Ren::RastState *rs) {
    // TODO: binary search
    for (size_t i = 0; i < pipelines_[int(prim)].size(); ++i) {
        if (pipelines_[int(prim)][i].prog() == p && pipelines_[int(prim)][i].render_pass() == rp &&
            pipelines_[int(prim)][i].rast_state() == *rs) {

            bool formats_match = (color_targets.size() == pipelines_[int(prim)][i].color_formats().size());
            for (int j = 0; j < color_targets.size() && formats_match; ++j) {
                formats_match &= color_targets[j].ref->params.format == pipelines_[int(prim)][i].color_formats()[j];
            }

            if (depth_target) {
                formats_match &= (depth_target.ref->params.format == pipelines_[int(prim)][i].depth_format());
            }

            return &pipelines_[int(prim)][i];
        }
    }

    Ren::ApiContext *api_ctx = ctx_->api_ctx();
    Ren::Pipeline &new_pipeline = pipelines_[int(prim)].emplace_back();

    const Ren::VertexInput *vtx_input = (prim == ePrim::Quad) ? &fs_quad_vtx_input_ : &sphere_vtx_input_;

    if (rp) {
        if (!new_pipeline.Init(api_ctx, *rs, std::move(p), vtx_input, rp, 0, ctx_->log())) {
            ctx_->log()->Error("Failed to initialize pipeline!");
            pipelines_[int(prim)].pop_back();
            return nullptr;
        }
    } else {
        if (!new_pipeline.Init(api_ctx, *rs, std::move(p), vtx_input, color_targets, depth_target, 0, ctx_->log())) {
            ctx_->log()->Error("Failed to initialize pipeline!");
            pipelines_[int(prim)].pop_back();
            return nullptr;
        }
    }

    return &new_pipeline;
}
