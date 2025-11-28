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

void Eng::PrimDraw::DrawPrim(Ren::CommandBuffer cmd_buf, const ePrim prim, const Ren::ProgramRef &p,
                             Ren::RenderTarget depth_rt, Ren::Span<const Ren::RenderTarget> color_rts,
                             const Ren::RastState &new_rast_state, Ren::RastState &applied_rast_state,
                             Ren::Span<const Ren::Binding> bindings, const void *uniform_data,
                             const int uniform_data_len, const int uniform_data_offset, const int instances) {
    using namespace PrimDrawInternal;

    Ren::ApiContext *api_ctx = ctx_->api_ctx();

    VkDescriptorSet descr_set = {};
    if (!p->descr_set_layouts().empty()) {
        VkDescriptorSetLayout descr_set_layout = p->descr_set_layouts()[0];
        descr_set = PrepareDescriptorSet(api_ctx, descr_set_layout, bindings, ctx_->default_descr_alloc(), ctx_->log());
    }

    { // transition resources if required
        VkPipelineStageFlags src_stages = 0, dst_stages = 0;

        Ren::SmallVector<VkImageMemoryBarrier, 16> img_barriers;
        Ren::SmallVector<VkBufferMemoryBarrier, 4> buf_barriers;

        for (const auto &b : bindings) {
            if (b.trg == Ren::eBindTarget::Tex || b.trg == Ren::eBindTarget::TexSampled) {
                assert(b.handle.img->resource_state == Ren::eResState::ShaderResource ||
                       b.handle.img->resource_state == Ren::eResState::DepthRead ||
                       b.handle.img->resource_state == Ren::eResState::StencilTestDepthFetch);
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
            ctx_->api_ctx()->vkCmdPipelineBarrier(cmd_buf, src_stages ? src_stages : VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                                  dst_stages, 0, 0, nullptr, buf_barriers.size(), buf_barriers.data(),
                                                  img_barriers.size(), img_barriers.data());
        }
    }

    const Ren::RenderPassRef rp = ctx_->LoadRenderPass(depth_rt, color_rts);
    const Ren::Framebuffer *fb =
        FindOrCreateFramebuffer(rp.get(), new_rast_state.depth.test_enabled ? depth_rt : Ren::RenderTarget{},
                                new_rast_state.stencil.enabled ? depth_rt : Ren::RenderTarget{}, color_rts);

    const Ren::VertexInputRef &vi = (prim == ePrim::Quad) ? fs_quad_vtx_input_ : sphere_vtx_input_;
    const Ren::PipelineRef pipeline = ctx_->LoadPipeline(new_rast_state, p, vi, rp, 0);
    if (pipeline.strong_refs() == 1) {
        // keep alive
        pipelines_.push_back(pipeline);
    }

    VkRenderPassBeginInfo render_pass_begin_info = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    render_pass_begin_info.renderPass = rp->vk_handle();
    render_pass_begin_info.framebuffer = fb->vk_handle();
    render_pass_begin_info.renderArea = {{0, 0}, {uint32_t(fb->w), uint32_t(fb->h)}};

    Ren::SmallVector<VkClearValue, 4> clear_values;
    if (depth_rt) {
        clear_values.push_back(VkClearValue{});
    }
    clear_values.resize(clear_values.size() + uint32_t(color_rts.size()), VkClearValue{});
    render_pass_begin_info.pClearValues = clear_values.cdata();
    render_pass_begin_info.clearValueCount = clear_values.size();

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

    if (descr_set) {
        api_ctx->vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->layout(), 0, 1, &descr_set,
                                         0, nullptr);
    }

    if (uniform_data) {
        api_ctx->vkCmdPushConstants(cmd_buf, pipeline->layout(), pipeline->prog()->pc_range(0).stageFlags,
                                    uniform_data_offset, uniform_data_len, uniform_data);
    }

    if (prim == ePrim::Quad) {
        pipeline->vtx_input()->BindBuffers(api_ctx, cmd_buf, quad_ndx_.offset, VK_INDEX_TYPE_UINT16);

        api_ctx->vkCmdDrawIndexed(cmd_buf, 6u, // index count
                                  instances,   // instance count
                                  0,           // first index
                                  0,           // vertex offset
                                  0);          // first instance
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

void Eng::PrimDraw::DrawPrim(const ePrim prim, const Ren::ProgramRef &p, Ren::RenderTarget depth_rt,
                             Ren::Span<const Ren::RenderTarget> color_rts, const Ren::RastState &new_rast_state,
                             Ren::RastState &applied_rast_state, Ren::Span<const Ren::Binding> bindings,
                             const void *uniform_data, const int uniform_data_len, const int uniform_data_offset,
                             const int instances) {
    Ren::ApiContext *api_ctx = ctx_->api_ctx();
    VkCommandBuffer cmd_buf = api_ctx->draw_cmd_buf[api_ctx->backend_frame];
    DrawPrim(cmd_buf, prim, p, depth_rt, color_rts, new_rast_state, applied_rast_state, bindings, uniform_data,
             uniform_data_len, uniform_data_offset, instances);
}

void Eng::PrimDraw::ClearTarget(Ren::CommandBuffer cmd_buf, Ren::RenderTarget depth_rt,
                                Ren::Span<const Ren::RenderTarget> color_rts) {
    const Ren::RenderPassRef rp = ctx_->LoadRenderPass(depth_rt, color_rts);
    const Ren::Framebuffer *fb = FindOrCreateFramebuffer(rp.get(), depth_rt, depth_rt, color_rts);

    VkRenderPassBeginInfo render_pass_begin_info = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    render_pass_begin_info.renderPass = rp->vk_handle();
    render_pass_begin_info.framebuffer = fb->vk_handle();
    render_pass_begin_info.renderArea = {{0, 0}, {uint32_t(fb->w), uint32_t(fb->h)}};

    Ren::SmallVector<VkClearValue, 4> clear_values;
    if (depth_rt) {
        clear_values.push_back(VkClearValue{});
    }
    clear_values.resize(clear_values.size() + uint32_t(color_rts.size()), VkClearValue{});
    render_pass_begin_info.pClearValues = clear_values.cdata();
    render_pass_begin_info.clearValueCount = clear_values.size();

    Ren::ApiContext *api_ctx = ctx_->api_ctx();
    api_ctx->vkCmdBeginRenderPass(cmd_buf, &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);

    // Do nothing

    api_ctx->vkCmdEndRenderPass(cmd_buf);
}

void Eng::PrimDraw::ClearTarget(Ren::RenderTarget depth_rt, Ren::Span<const Ren::RenderTarget> color_rts) {
    Ren::ApiContext *api_ctx = ctx_->api_ctx();
    VkCommandBuffer cmd_buf = api_ctx->draw_cmd_buf[api_ctx->backend_frame];
    ClearTarget(cmd_buf, depth_rt, color_rts);
}