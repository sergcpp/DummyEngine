#include "ExDepthFill.h"

#include <Ren/Context.h>
#include <Ren/DebugMarker.h>
#include <Ren/DrawCall.h>
#include <Ren/Vk/VKCtx.h>
#include <Ren/utils/Span.h>

#include "../Renderer_DrawList.h"
#include "../framegraph/FgBuilder.h"

namespace ExSharedInternal {
uint32_t _draw_range(const Ren::ApiContext &api, VkCommandBuffer cmd_buf, Ren::Span<const uint32_t> batch_indices,
                     Ren::Span<const Eng::basic_draw_batch_t> batches, uint32_t i, const uint64_t mask,
                     int *draws_count) {
    for (; i < batch_indices.size(); i++) {
        const auto &batch = batches[batch_indices[i]];
        if ((batch.sort_key & Eng::basic_draw_batch_t::FlagBits) != mask) {
            break;
        }

        if (!batch.instance_count) {
            continue;
        }

        api.vkCmdDrawIndexed(cmd_buf, batch.indices_count, // index count
                             batch.instance_count,         // instance count
                             batch.indices_offset,         // first index
                             batch.base_vertex,            // vertex offset
                             batch.instance_start);        // first instance
        ++(*draws_count);
    }
    return i;
}

uint32_t _draw_range_ext(const Ren::ApiContext &api, VkCommandBuffer cmd_buf, const Ren::PipelineMain &pipeline,
                         Ren::Span<const uint32_t> batch_indices, Ren::Span<const Eng::basic_draw_batch_t> batches,
                         uint32_t i, const uint64_t mask, const uint32_t materials_per_descriptor,
                         Ren::Span<const VkDescriptorSet> descr_sets, int *draws_count) {
    uint32_t bound_descr_id = 0;
    for (; i < batch_indices.size(); i++) {
        const auto &batch = batches[batch_indices[i]];
        if ((batch.sort_key & Eng::basic_draw_batch_t::FlagBits) != mask) {
            break;
        }

        if (!batch.instance_count) {
            continue;
        }

        const uint32_t descr_id = batch.material_index / materials_per_descriptor;
        if (descr_id != bound_descr_id) {
            api.vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.layout, 1, 1,
                                        &descr_sets[descr_id], 0, nullptr);
            bound_descr_id = descr_id;
        }

        api.vkCmdDrawIndexed(cmd_buf, batch.indices_count, // index count
                             batch.instance_count,         // instance count
                             batch.indices_offset,         // first index
                             batch.base_vertex,            // vertex offset
                             batch.instance_start);        // first instance
        ++(*draws_count);
    }
    return i;
}

uint32_t _skip_range(Ren::Span<const uint32_t> batch_indices, Ren::Span<const Eng::basic_draw_batch_t> batches,
                     uint32_t i, uint64_t mask);
} // namespace ExSharedInternal

void Eng::ExDepthFill::DrawDepth(const FgContext &fg, const Ren::ImageRWHandle depth,
                                 const Ren::ImageRWHandle velocity) {
    using namespace ExSharedInternal;

    const Ren::BufferROHandle attrib_bufs[] = {fg.AccessROBuffer(args_->vtx_buf1), fg.AccessROBuffer(args_->vtx_buf2)};
    const Ren::BufferROHandle ndx_buf = fg.AccessROBuffer(args_->ndx_buf);

    const Ren::BufferROHandle unif_shared_data = fg.AccessROBuffer(args_->shared_data);
    const Ren::BufferROHandle instances = fg.AccessROBuffer(args_->instances);
    const Ren::BufferROHandle instance_indices = fg.AccessROBuffer(args_->instance_indices);
    const Ren::BufferROHandle materials = fg.AccessROBuffer(args_->materials);
    const Ren::ImageROHandle noise = fg.AccessROImage(args_->noise);

    const Ren::ApiContext &api = fg.ren_ctx().api();
    const Ren::StoragesRef &storages = fg.storages();

    VkCommandBuffer cmd_buf = fg.cmd_buf();
    //
    // Setup viewport
    //
    const VkViewport viewport = {0.0f, 0.0f, float(view_state_->ren_res[0]), float(view_state_->ren_res[1]),
                                 0.0f, 1.0f};
    api.vkCmdSetViewport(cmd_buf, 0, 1, &viewport);
    const VkRect2D scissor = {{0, 0}, {uint32_t(view_state_->ren_res[0]), uint32_t(view_state_->ren_res[1])}};
    api.vkCmdSetScissor(cmd_buf, 0, 1, &scissor);

    const uint32_t materials_per_descriptor = api.max_combined_image_samplers / MAX_TEX_PER_MATERIAL;

    static backend_info_t _dummy = {};
    uint32_t i = 0;

    using BDB = basic_draw_batch_t;

    const Ren::PipelineMain *pi_static_solid_main[3] = {&storages.pipelines[pi_static_solid_[0]].first,
                                                        &storages.pipelines[pi_static_solid_[1]].first,
                                                        &storages.pipelines[pi_static_solid_[2]].first};
    const Ren::ProgramMain &pr_static_solid0_main = storages.programs[pi_static_solid_main[0]->prog].first;

    const Ren::PipelineMain *pi_vege_static_solid_main[2] = {&storages.pipelines[pi_vege_static_solid_[0]].first,
                                                             &storages.pipelines[pi_vege_static_solid_[1]].first};
    const Ren::ProgramMain &pr_vege_static_solid0_main =
        storages.programs[pi_vege_static_solid_main[0]->prog].first;

    //
    // Prepare descriptor sets
    //

    VkDescriptorSetLayout simple_descr_set_layout = pr_static_solid0_main.descr_set_layouts[0];
    VkDescriptorSet simple_descr_sets[2];
    { // allocate descriptors
        const Ren::Binding bindings[] = {{Ren::eBindTarget::UBuf, BIND_UB_SHARED_DATA_BUF, unif_shared_data},
                                         {Ren::eBindTarget::UTBuf, BIND_INST_BUF, instances},
                                         {Ren::eBindTarget::SBufRO, BIND_INST_NDX_BUF, instance_indices},
                                         {Ren::eBindTarget::SBufRO, BIND_MATERIALS_BUF, materials}};
        simple_descr_sets[0] =
            PrepareDescriptorSet(api, storages, simple_descr_set_layout, bindings, fg.descr_alloc(), fg.log());
        simple_descr_sets[1] = bindless_tex_->textures_descr_sets[0];
    }

    VkDescriptorSetLayout vege_descr_set_layout = pr_vege_static_solid0_main.descr_set_layouts[0];
    VkDescriptorSet vege_descr_sets[2];
    { // allocate descriptors
        const Ren::Binding bindings[] = {{Ren::eBindTarget::UBuf, BIND_UB_SHARED_DATA_BUF, unif_shared_data},
                                         {Ren::eBindTarget::UTBuf, BIND_INST_BUF, instances},
                                         {Ren::eBindTarget::SBufRO, BIND_INST_NDX_BUF, instance_indices},
                                         {Ren::eBindTarget::SBufRO, BIND_MATERIALS_BUF, materials},
                                         {Ren::eBindTarget::TexSampled, BIND_NOISE_TEX, noise}};
        vege_descr_sets[0] =
            PrepareDescriptorSet(api, storages, vege_descr_set_layout, bindings, fg.descr_alloc(), fg.log());
        vege_descr_sets[1] = bindless_tex_->textures_descr_sets[0];
    }

    const Ren::Span<const basic_draw_batch_t> zfill_batches = {(*p_list_)->basic_batches};
    const Ren::Span<const uint32_t> zfill_batch_indices = {(*p_list_)->basic_batch_indices};

    int draws_count = 0;

    { // solid meshes
        Ren::DebugMarker _m(api, cmd_buf, "STATIC-SOLID-SIMPLE");
        const int rp_index = (args_->clear_depth && !draws_count) ? 0 : 1;

        const Ren::FramebufferHandle fb = fg.FindOrCreateFramebuffer(rp_depth_only_[rp_index], depth, depth, {});

        VkClearValue clear_value = {};
        clear_value.depthStencil.depth = 0.0f;
        clear_value.depthStencil.stencil = 0;

        VkRenderPassBeginInfo rp_begin_info = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
        rp_begin_info.renderPass = storages.render_passes[rp_depth_only_[rp_index]].handle;
        rp_begin_info.framebuffer = storages.framebuffers[fb].first.handle;
        rp_begin_info.renderArea = {{0, 0}, {uint32_t(view_state_->ren_res[0]), uint32_t(view_state_->ren_res[1])}};
        rp_begin_info.pClearValues = &clear_value;
        rp_begin_info.clearValueCount = 1;
        api.vkCmdBeginRenderPass(cmd_buf, &rp_begin_info, VK_SUBPASS_CONTENTS_INLINE);

        const Ren::VertexInput &vtx_input = storages.vtx_inputs[pi_static_solid_main[0]->vtx_input];
        VertexInput_BindBuffers(api, vtx_input, storages.buffers, attrib_bufs, ndx_buf, cmd_buf, 0,
                                VK_INDEX_TYPE_UINT32);

        { // one-sided
            Ren::DebugMarker _mm(api, cmd_buf, "ONE-SIDED");
            api.vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_static_solid_main[0]->pipeline);
            api.vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_static_solid_main[0]->layout, 0, 1,
                                        simple_descr_sets, 0, nullptr);
            i = _draw_range(api, cmd_buf, zfill_batch_indices, zfill_batches, i, 0u, &draws_count);
            api.vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_static_solid_main[1]->pipeline);
            i = _draw_range(api, cmd_buf, zfill_batch_indices, zfill_batches, i, BDB::BitBackSided, &draws_count);
        }

        { // two-sided
            Ren::DebugMarker _mm(api, cmd_buf, "TWO-SIDED");
            api.vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_static_solid_main[2]->pipeline);
            api.vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_static_solid_main[2]->layout, 0, 1,
                                        simple_descr_sets, 0, nullptr);
            i = _draw_range(api, cmd_buf, zfill_batch_indices, zfill_batches, i, BDB::BitTwoSided, &draws_count);
        }

        api.vkCmdEndRenderPass(cmd_buf);
    }

    const Ren::PipelineMain *pi_moving_solid_main[3] = {&storages.pipelines[pi_moving_solid_[0]].first,
                                                        &storages.pipelines[pi_moving_solid_[1]].first,
                                                        &storages.pipelines[pi_moving_solid_[2]].first};

    { // moving solid meshes (depth and velocity)
        Ren::DebugMarker _m(api, cmd_buf, "STATIC-SOLID-MOVING");
        const int rp_index = (args_->clear_depth && !draws_count) ? 0 : 1;

        const Ren::ImageRWHandle velocity_target[] = {velocity};
        const Ren::FramebufferHandle fb =
            fg.FindOrCreateFramebuffer(rp_depth_velocity_[rp_index], depth, depth, velocity_target);

        VkClearValue clear_value = {};
        clear_value.depthStencil.depth = 0.0f;
        clear_value.depthStencil.stencil = 0;

        VkRenderPassBeginInfo rp_begin_info = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
        rp_begin_info.renderArea = {{0, 0}, {uint32_t(view_state_->ren_res[0]), uint32_t(view_state_->ren_res[1])}};
        rp_begin_info.pClearValues = &clear_value;
        rp_begin_info.clearValueCount = 1;
        rp_begin_info.renderPass = storages.render_passes[rp_depth_velocity_[rp_index]].handle;
        rp_begin_info.framebuffer = storages.framebuffers[fb].first.handle;

        api.vkCmdBeginRenderPass(cmd_buf, &rp_begin_info, VK_SUBPASS_CONTENTS_INLINE);

        const Ren::VertexInput &vtx_input = storages.vtx_inputs[pi_moving_solid_main[0]->vtx_input];
        VertexInput_BindBuffers(api, vtx_input, storages.buffers, attrib_bufs, ndx_buf, cmd_buf, 0,
                                VK_INDEX_TYPE_UINT32);

        { // one-sided
            Ren::DebugMarker _mm(api, fg.cmd_buf(), "ONE-SIDED");
            api.vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_moving_solid_main[0]->pipeline);
            api.vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_moving_solid_main[0]->layout, 0, 1,
                                        simple_descr_sets, 0, nullptr);
            i = _draw_range(api, cmd_buf, zfill_batch_indices, zfill_batches, i, BDB::BitMoving, &draws_count);
            api.vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_moving_solid_main[1]->pipeline);
            api.vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_moving_solid_main[1]->layout, 0, 1,
                                        simple_descr_sets, 0, nullptr);
            i = _draw_range(api, cmd_buf, zfill_batch_indices, zfill_batches, i, BDB::BitMoving | BDB::BitBackSided,
                            &draws_count);
        }

        { // two-sided
            Ren::DebugMarker _mm(api, fg.cmd_buf(), "TWO-SIDED");
            api.vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_moving_solid_main[2]->pipeline);
            api.vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_moving_solid_main[2]->layout, 0, 1,
                                        simple_descr_sets, 0, nullptr);
            i = _draw_range(api, cmd_buf, zfill_batch_indices, zfill_batches, i, BDB::BitMoving | BDB::BitTwoSided,
                            &draws_count);
        }

        api.vkCmdEndRenderPass(cmd_buf);
    }

    const Ren::PipelineMain *pi_static_transp_main[3] = {&storages.pipelines[pi_static_transp_[0]].first,
                                                         &storages.pipelines[pi_static_transp_[1]].first,
                                                         &storages.pipelines[pi_static_transp_[2]].first};

    { // simple alpha-tested meshes (depth only)
        Ren::DebugMarker _m(api, fg.cmd_buf(), "STATIC-ALPHA-SIMPLE");
        const int rp_index = (args_->clear_depth && !draws_count) ? 0 : 1;

        const Ren::FramebufferHandle fb = fg.FindOrCreateFramebuffer(rp_depth_only_[rp_index], depth, depth, {});

        VkClearValue clear_value = {};
        clear_value.depthStencil.depth = 0.0f;
        clear_value.depthStencil.stencil = 0;

        VkRenderPassBeginInfo rp_begin_info = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
        rp_begin_info.renderPass = storages.render_passes[rp_depth_only_[rp_index]].handle;
        rp_begin_info.framebuffer = storages.framebuffers[fb].first.handle;
        rp_begin_info.renderArea = {{0, 0}, {uint32_t(view_state_->ren_res[0]), uint32_t(view_state_->ren_res[1])}};
        rp_begin_info.pClearValues = &clear_value;
        rp_begin_info.clearValueCount = 1;
        api.vkCmdBeginRenderPass(cmd_buf, &rp_begin_info, VK_SUBPASS_CONTENTS_INLINE);

        const Ren::VertexInput &vtx_input = storages.vtx_inputs[pi_static_transp_main[0]->vtx_input];
        VertexInput_BindBuffers(api, vtx_input, storages.buffers, attrib_bufs, ndx_buf, cmd_buf, 0,
                                VK_INDEX_TYPE_UINT32);

        { // one-sided
            Ren::DebugMarker _mm(api, fg.cmd_buf(), "ONE-SIDED");
            api.vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_static_transp_main[0]->pipeline);
            api.vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_static_transp_main[0]->layout, 0,
                                        2, simple_descr_sets, 0, nullptr);
            i = _draw_range_ext(api, cmd_buf, *pi_static_transp_main[0], zfill_batch_indices, zfill_batches, i,
                                BDB::BitAlphaTest, materials_per_descriptor, bindless_tex_->textures_descr_sets,
                                &draws_count);
            api.vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_static_transp_main[1]->pipeline);
            i = _draw_range_ext(api, cmd_buf, *pi_static_transp_main[0], zfill_batch_indices, zfill_batches, i,
                                BDB::BitAlphaTest | BDB::BitBackSided, materials_per_descriptor,
                                bindless_tex_->textures_descr_sets, &draws_count);
        }

        { // two-sided
            Ren::DebugMarker _mm(api, fg.cmd_buf(), "TWO-SIDED");
            api.vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_static_transp_main[2]->pipeline);
            api.vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_static_transp_main[2]->layout, 0,
                                        2, simple_descr_sets, 0, nullptr);
            i = _draw_range_ext(api, cmd_buf, *pi_static_transp_main[2], zfill_batch_indices, zfill_batches, i,
                                BDB::BitAlphaTest | BDB::BitTwoSided, materials_per_descriptor,
                                bindless_tex_->textures_descr_sets, &draws_count);
        }

        api.vkCmdEndRenderPass(cmd_buf);
    }

    const Ren::PipelineMain *pi_moving_transp_main[3] = {&storages.pipelines[pi_moving_transp_[0]].first,
                                                         &storages.pipelines[pi_moving_transp_[1]].first,
                                                         &storages.pipelines[pi_moving_transp_[2]].first};

    { // moving alpha-tested meshes (depth and velocity)
        Ren::DebugMarker _m(api, fg.cmd_buf(), "STATIC-ALPHA-MOVING");
        const int rp_index = (args_->clear_depth && !draws_count) ? 0 : 1;

        const Ren::ImageRWHandle velocity_target[] = {velocity};
        const Ren::FramebufferHandle fb =
            fg.FindOrCreateFramebuffer(rp_depth_velocity_[rp_index], depth, depth, velocity_target);

        VkClearValue clear_value = {};
        clear_value.depthStencil.depth = 0.0f;
        clear_value.depthStencil.stencil = 0;

        VkRenderPassBeginInfo rp_begin_info = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
        rp_begin_info.renderArea = {{0, 0}, {uint32_t(view_state_->ren_res[0]), uint32_t(view_state_->ren_res[1])}};
        rp_begin_info.pClearValues = &clear_value;
        rp_begin_info.clearValueCount = 1;
        rp_begin_info.renderPass = storages.render_passes[rp_depth_velocity_[rp_index]].handle;
        rp_begin_info.framebuffer = storages.framebuffers[fb].first.handle;

        api.vkCmdBeginRenderPass(cmd_buf, &rp_begin_info, VK_SUBPASS_CONTENTS_INLINE);

        const Ren::VertexInput &vtx_input = storages.vtx_inputs[pi_moving_transp_main[0]->vtx_input];
        VertexInput_BindBuffers(api, vtx_input, storages.buffers, attrib_bufs, ndx_buf, cmd_buf, 0,
                                VK_INDEX_TYPE_UINT32);

        { // one-sided
            Ren::DebugMarker _mm(api, fg.cmd_buf(), "ONE-SIDED");
            api.vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_moving_transp_main[0]->pipeline);
            api.vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_moving_transp_main[0]->layout, 0,
                                        2, simple_descr_sets, 0, nullptr);
            i = _draw_range_ext(api, cmd_buf, *pi_moving_transp_main[0], zfill_batch_indices, zfill_batches, i,
                                BDB::BitAlphaTest | BDB::BitMoving, materials_per_descriptor,
                                bindless_tex_->textures_descr_sets, &draws_count);
            api.vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_moving_transp_main[1]->pipeline);
            api.vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_moving_transp_main[1]->layout, 0,
                                        2, simple_descr_sets, 0, nullptr);
            i = _draw_range_ext(api, cmd_buf, *pi_moving_transp_main[1], zfill_batch_indices, zfill_batches, i,
                                BDB::BitAlphaTest | BDB::BitMoving | BDB::BitBackSided, materials_per_descriptor,
                                bindless_tex_->textures_descr_sets, &draws_count);
        }

        { // two-sided
            Ren::DebugMarker _mm(api, fg.cmd_buf(), "TWO-SIDED");
            api.vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_moving_transp_main[2]->pipeline);
            api.vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_moving_transp_main[2]->layout, 0,
                                        2, simple_descr_sets, 0, nullptr);
            i = _draw_range_ext(api, cmd_buf, *pi_moving_transp_main[2], zfill_batch_indices, zfill_batches, i,
                                BDB::BitAlphaTest | BDB::BitMoving | BDB::BitTwoSided, materials_per_descriptor,
                                bindless_tex_->textures_descr_sets, &draws_count);
        }

        api.vkCmdEndRenderPass(cmd_buf);
    }

    { // static solid vegetation
        Ren::DebugMarker _m(api, cmd_buf, "VEGE-SOLID-SIMPLE");
        const int rp_index = (args_->clear_depth && !draws_count) ? 0 : 1;

        const Ren::FramebufferHandle fb = fg.FindOrCreateFramebuffer(rp_depth_only_[rp_index], depth, depth, {});

        VkClearValue clear_value = {};
        clear_value.depthStencil.depth = 0.0f;
        clear_value.depthStencil.stencil = 0;

        VkRenderPassBeginInfo rp_begin_info = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
        rp_begin_info.renderArea = {{0, 0}, {uint32_t(view_state_->ren_res[0]), uint32_t(view_state_->ren_res[1])}};
        rp_begin_info.pClearValues = &clear_value;
        rp_begin_info.clearValueCount = 1;
        rp_begin_info.renderPass = storages.render_passes[rp_depth_only_[rp_index]].handle;
        rp_begin_info.framebuffer = storages.framebuffers[fb].first.handle;

        api.vkCmdBeginRenderPass(cmd_buf, &rp_begin_info, VK_SUBPASS_CONTENTS_INLINE);

        const Ren::VertexInput &vtx_input = storages.vtx_inputs[pi_vege_static_solid_main[0]->vtx_input];
        VertexInput_BindBuffers(api, vtx_input, storages.buffers, attrib_bufs, ndx_buf, cmd_buf, 0,
                                VK_INDEX_TYPE_UINT32);

        { // one-sided
            Ren::DebugMarker _mm(api, cmd_buf, "ONE-SIDED");
            api.vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_vege_static_solid_main[0]->pipeline);
            api.vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_vege_static_solid_main[0]->layout,
                                        0, 2, vege_descr_sets, 0, nullptr);
            i = _draw_range(api, cmd_buf, zfill_batch_indices, zfill_batches, i, BDB::BitsVege, &draws_count);
        }

        { // two-sided
            Ren::DebugMarker _mm(api, cmd_buf, "TWO-SIDED");
            api.vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_vege_static_solid_main[1]->pipeline);
            api.vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_vege_static_solid_main[1]->layout,
                                        0, 2, vege_descr_sets, 0, nullptr);
            i = _draw_range(api, cmd_buf, zfill_batch_indices, zfill_batches, i, BDB::BitsVege | BDB::BitTwoSided,
                            &draws_count);
        }

        api.vkCmdEndRenderPass(cmd_buf);
    }

    const Ren::PipelineMain *pi_vege_moving_solid_main[2] = {&storages.pipelines[pi_vege_moving_solid_[0]].first,
                                                             &storages.pipelines[pi_vege_moving_solid_[1]].first};

    { // moving solid vegetation (depth and velocity)
        Ren::DebugMarker _m(api, fg.cmd_buf(), "VEGE-SOLID-MOVING");
        const int rp_index = (args_->clear_depth && !draws_count) ? 0 : 1;

        const Ren::ImageRWHandle velocity_target[] = {velocity};
        const Ren::FramebufferHandle fb =
            fg.FindOrCreateFramebuffer(rp_depth_velocity_[rp_index], depth, depth, velocity_target);

        VkClearValue clear_value = {};
        clear_value.depthStencil.depth = 0.0f;
        clear_value.depthStencil.stencil = 0;

        VkRenderPassBeginInfo rp_begin_info = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
        rp_begin_info.renderArea = {{0, 0}, {uint32_t(view_state_->ren_res[0]), uint32_t(view_state_->ren_res[1])}};
        rp_begin_info.pClearValues = &clear_value;
        rp_begin_info.clearValueCount = 1;
        rp_begin_info.renderPass = storages.render_passes[rp_depth_velocity_[rp_index]].handle;
        rp_begin_info.framebuffer = storages.framebuffers[fb].first.handle;

        api.vkCmdBeginRenderPass(cmd_buf, &rp_begin_info, VK_SUBPASS_CONTENTS_INLINE);

        const Ren::VertexInput &vtx_input = storages.vtx_inputs[pi_vege_moving_solid_main[0]->vtx_input];
        VertexInput_BindBuffers(api, vtx_input, storages.buffers, attrib_bufs, ndx_buf, cmd_buf, 0,
                                VK_INDEX_TYPE_UINT32);

        { // one-sided
            Ren::DebugMarker _mm(api, fg.cmd_buf(), "ONE-SIDED");
            api.vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_vege_moving_solid_main[0]->pipeline);
            api.vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_vege_moving_solid_main[0]->layout,
                                        0, 1, vege_descr_sets, 0, nullptr);
            i = _draw_range(api, cmd_buf, zfill_batch_indices, zfill_batches, i, BDB::BitsVege | BDB::BitMoving,
                            &draws_count);
        }

        { // two-sided
            Ren::DebugMarker _mm(api, fg.cmd_buf(), "TWO-SIDED");
            api.vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_vege_moving_solid_main[1]->pipeline);
            api.vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_vege_moving_solid_main[1]->layout,
                                        0, 1, vege_descr_sets, 0, nullptr);
            i = _draw_range(api, cmd_buf, zfill_batch_indices, zfill_batches, i,
                            BDB::BitsVege | BDB::BitMoving | BDB::BitTwoSided, &draws_count);
        }

        api.vkCmdEndRenderPass(cmd_buf);
    }

    const Ren::PipelineMain *pi_vege_static_transp_main[2] = {&storages.pipelines[pi_vege_static_transp_[0]].first,
                                                              &storages.pipelines[pi_vege_static_transp_[1]].first};

    { // static alpha-tested vegetation (depth and velocity)
        Ren::DebugMarker _m(api, fg.cmd_buf(), "VEGE-ALPHA-SIMPLE");
        const int rp_index = (args_->clear_depth && !draws_count) ? 0 : 1;

        const Ren::ImageRWHandle velocity_target[] = {velocity};
        const Ren::FramebufferHandle fb =
            fg.FindOrCreateFramebuffer(rp_depth_velocity_[rp_index], depth, depth, velocity_target);

        VkClearValue clear_value = {};
        clear_value.depthStencil.depth = 0.0f;
        clear_value.depthStencil.stencil = 0;

        VkRenderPassBeginInfo rp_begin_info = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
        rp_begin_info.renderArea = {{0, 0}, {uint32_t(view_state_->ren_res[0]), uint32_t(view_state_->ren_res[1])}};
        rp_begin_info.pClearValues = &clear_value;
        rp_begin_info.clearValueCount = 1;
        rp_begin_info.renderPass = storages.render_passes[rp_depth_velocity_[rp_index]].handle;
        rp_begin_info.framebuffer = storages.framebuffers[fb].first.handle;

        api.vkCmdBeginRenderPass(cmd_buf, &rp_begin_info, VK_SUBPASS_CONTENTS_INLINE);

        const Ren::VertexInput &vtx_input = storages.vtx_inputs[pi_vege_static_transp_main[0]->vtx_input];
        VertexInput_BindBuffers(api, vtx_input, storages.buffers, attrib_bufs, ndx_buf, cmd_buf, 0,
                                VK_INDEX_TYPE_UINT32);

        { // one-sided
            Ren::DebugMarker _mm(api, fg.cmd_buf(), "ONE-SIDED");
            api.vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_vege_static_transp_main[0]->pipeline);
            api.vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_vege_static_transp_main[0]->layout,
                                        0, 2, vege_descr_sets, 0, nullptr);
            i = _draw_range_ext(api, cmd_buf, *pi_vege_static_transp_main[0], zfill_batch_indices, zfill_batches, i,
                                BDB::BitsVege | BDB::BitAlphaTest, materials_per_descriptor,
                                bindless_tex_->textures_descr_sets, &draws_count);
        }

        { // two-sided
            Ren::DebugMarker _mm(api, fg.cmd_buf(), "TWO-SIDED");
            api.vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_vege_static_transp_main[1]->pipeline);
            api.vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_vege_static_transp_main[1]->layout,
                                        0, 2, vege_descr_sets, 0, nullptr);
            i = _draw_range_ext(api, cmd_buf, *pi_vege_static_transp_main[1], zfill_batch_indices, zfill_batches, i,
                                BDB::BitsVege | BDB::BitAlphaTest | BDB::BitTwoSided, materials_per_descriptor,
                                bindless_tex_->textures_descr_sets, &draws_count);
        }

        api.vkCmdEndRenderPass(cmd_buf);
    }

    const Ren::PipelineMain *pi_vege_moving_transp_main[2] = {&storages.pipelines[pi_vege_moving_transp_[0]].first,
                                                              &storages.pipelines[pi_vege_moving_transp_[1]].first};

    { // moving alpha-tested vegetation (depth and velocity)
        Ren::DebugMarker _m(api, fg.cmd_buf(), "VEGE-ALPHA-MOVING");
        const int rp_index = (args_->clear_depth && !draws_count) ? 0 : 1;

        const Ren::ImageRWHandle velocity_target[] = {velocity};
        const Ren::FramebufferHandle fb =
            fg.FindOrCreateFramebuffer(rp_depth_velocity_[rp_index], depth, depth, velocity_target);

        VkClearValue clear_value = {};
        clear_value.depthStencil.depth = 0.0f;
        clear_value.depthStencil.stencil = 0;

        VkRenderPassBeginInfo rp_begin_info = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
        rp_begin_info.renderArea = {{0, 0}, {uint32_t(view_state_->ren_res[0]), uint32_t(view_state_->ren_res[1])}};
        rp_begin_info.pClearValues = &clear_value;
        rp_begin_info.clearValueCount = 1;
        rp_begin_info.renderPass = storages.render_passes[rp_depth_velocity_[rp_index]].handle;
        rp_begin_info.framebuffer = storages.framebuffers[fb].first.handle;

        api.vkCmdBeginRenderPass(cmd_buf, &rp_begin_info, VK_SUBPASS_CONTENTS_INLINE);

        const Ren::VertexInput &vtx_input = storages.vtx_inputs[pi_vege_moving_transp_main[0]->vtx_input];
        VertexInput_BindBuffers(api, vtx_input, storages.buffers, attrib_bufs, ndx_buf, cmd_buf, 0,
                                VK_INDEX_TYPE_UINT32);

        { // one-sided
            Ren::DebugMarker _mm(api, fg.cmd_buf(), "ONE-SIDED");
            api.vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_vege_moving_transp_main[0]->pipeline);
            api.vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_vege_moving_transp_main[0]->layout,
                                        0, 2, vege_descr_sets, 0, nullptr);
            i = _draw_range_ext(api, cmd_buf, *pi_vege_moving_transp_main[0], zfill_batch_indices, zfill_batches, i,
                                BDB::BitsVege | BDB::BitAlphaTest | BDB::BitMoving, materials_per_descriptor,
                                bindless_tex_->textures_descr_sets, &draws_count);
        }

        { // two-sided
            Ren::DebugMarker _mm(api, fg.cmd_buf(), "TWO-SIDED");
            api.vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_vege_moving_transp_main[1]->pipeline);
            api.vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_vege_moving_transp_main[1]->layout,
                                        0, 2, vege_descr_sets, 0, nullptr);
            i = _draw_range_ext(api, cmd_buf, *pi_vege_moving_transp_main[1], zfill_batch_indices, zfill_batches, i,
                                BDB::BitsVege | BDB::BitAlphaTest | BDB::BitMoving | BDB::BitTwoSided,
                                materials_per_descriptor, bindless_tex_->textures_descr_sets, &draws_count);
        }

        api.vkCmdEndRenderPass(cmd_buf);
    }

    const Ren::PipelineMain *pi_skin_static_solid_main[2] = {&storages.pipelines[pi_skin_static_solid_[0]].first,
                                                             &storages.pipelines[pi_skin_static_solid_[1]].first};

    { // solid skinned meshes (depth and velocity)
        Ren::DebugMarker _m(api, fg.cmd_buf(), "SKIN-SOLID-SIMPLE");
        const int rp_index = (args_->clear_depth && !draws_count) ? 0 : 1;

        const Ren::ImageRWHandle velocity_target[] = {velocity};
        const Ren::FramebufferHandle fb =
            fg.FindOrCreateFramebuffer(rp_depth_velocity_[rp_index], depth, depth, velocity_target);

        VkClearValue clear_value = {};
        clear_value.depthStencil.depth = 0.0f;
        clear_value.depthStencil.stencil = 0;

        VkRenderPassBeginInfo rp_begin_info = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
        rp_begin_info.renderArea = {{0, 0}, {uint32_t(view_state_->ren_res[0]), uint32_t(view_state_->ren_res[1])}};
        rp_begin_info.pClearValues = &clear_value;
        rp_begin_info.clearValueCount = 1;
        rp_begin_info.renderPass = storages.render_passes[rp_depth_velocity_[rp_index]].handle;
        rp_begin_info.framebuffer = storages.framebuffers[fb].first.handle;

        api.vkCmdBeginRenderPass(cmd_buf, &rp_begin_info, VK_SUBPASS_CONTENTS_INLINE);

        const Ren::VertexInput &vtx_input = storages.vtx_inputs[pi_skin_static_solid_main[0]->vtx_input];
        VertexInput_BindBuffers(api, vtx_input, storages.buffers, attrib_bufs, ndx_buf, cmd_buf, 0,
                                VK_INDEX_TYPE_UINT32);

        { // one-sided
            Ren::DebugMarker _mm(api, fg.cmd_buf(), "ONE-SIDED");
            api.vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_skin_static_solid_main[0]->pipeline);
            api.vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_skin_static_solid_main[0]->layout,
                                        0, 1, simple_descr_sets, 0, nullptr);
            i = _draw_range(api, cmd_buf, zfill_batch_indices, zfill_batches, i, BDB::BitsSkinned, &draws_count);
        }

        { // two-sided
            Ren::DebugMarker _mm(api, fg.cmd_buf(), "TWO-SIDED");
            api.vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_skin_static_solid_main[1]->pipeline);
            api.vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_skin_static_solid_main[1]->layout,
                                        0, 1, simple_descr_sets, 0, nullptr);
            i = _draw_range(api, cmd_buf, zfill_batch_indices, zfill_batches, i, BDB::BitsSkinned | BDB::BitTwoSided,
                            &draws_count);
        }

        api.vkCmdEndRenderPass(cmd_buf);
    }

    const Ren::PipelineMain *pi_skin_moving_solid_main[2] = {&storages.pipelines[pi_skin_moving_solid_[0]].first,
                                                             &storages.pipelines[pi_skin_moving_solid_[1]].first};

    { // moving solid skinned (depth and velocity)
        Ren::DebugMarker _m(api, fg.cmd_buf(), "SKIN-SOLID-MOVING");
        const int rp_index = (args_->clear_depth && !draws_count) ? 0 : 1;

        const Ren::ImageRWHandle velocity_target[] = {velocity};
        const Ren::FramebufferHandle fb =
            fg.FindOrCreateFramebuffer(rp_depth_velocity_[rp_index], depth, depth, velocity_target);

        VkClearValue clear_value = {};
        clear_value.depthStencil.depth = 0.0f;
        clear_value.depthStencil.stencil = 0;

        VkRenderPassBeginInfo rp_begin_info = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
        rp_begin_info.renderArea = {{0, 0}, {uint32_t(view_state_->ren_res[0]), uint32_t(view_state_->ren_res[1])}};
        rp_begin_info.pClearValues = &clear_value;
        rp_begin_info.clearValueCount = 1;
        rp_begin_info.renderPass = storages.render_passes[rp_depth_velocity_[rp_index]].handle;
        rp_begin_info.framebuffer = storages.framebuffers[fb].first.handle;

        api.vkCmdBeginRenderPass(cmd_buf, &rp_begin_info, VK_SUBPASS_CONTENTS_INLINE);

        const Ren::VertexInput &vtx_input = storages.vtx_inputs[pi_skin_moving_solid_main[0]->vtx_input];
        VertexInput_BindBuffers(api, vtx_input, storages.buffers, attrib_bufs, ndx_buf, cmd_buf, 0,
                                VK_INDEX_TYPE_UINT32);

        { // one-sided
            Ren::DebugMarker _mm(api, fg.cmd_buf(), "ONE-SIDED");
            api.vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_skin_moving_solid_main[0]->pipeline);
            api.vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_skin_moving_solid_main[0]->layout,
                                        0, 1, simple_descr_sets, 0, nullptr);
            i = _draw_range(api, cmd_buf, zfill_batch_indices, zfill_batches, i, BDB::BitsSkinned | BDB::BitMoving,
                            &draws_count);
        }

        { // two-sided
            Ren::DebugMarker _mm(api, fg.cmd_buf(), "TWO-SIDED");
            api.vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_skin_moving_solid_main[1]->pipeline);
            api.vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_skin_moving_solid_main[1]->layout,
                                        0, 1, simple_descr_sets, 0, nullptr);
            i = _draw_range(api, cmd_buf, zfill_batch_indices, zfill_batches, i,
                            BDB::BitsSkinned | BDB::BitMoving | BDB::BitTwoSided, &draws_count);
        }

        api.vkCmdEndRenderPass(cmd_buf);
    }

    const Ren::PipelineMain *pi_skin_static_transp_main[2] = {&storages.pipelines[pi_skin_static_transp_[0]].first,
                                                              &storages.pipelines[pi_skin_static_transp_[1]].first};

    { // static alpha-tested skinned (depth and velocity)
        Ren::DebugMarker _m(api, fg.cmd_buf(), "SKIN-ALPHA-SIMPLE");
        const int rp_index = (args_->clear_depth && !draws_count) ? 0 : 1;

        const Ren::ImageRWHandle velocity_target[] = {velocity};
        const Ren::FramebufferHandle fb =
            fg.FindOrCreateFramebuffer(rp_depth_velocity_[rp_index], depth, depth, velocity_target);

        VkClearValue clear_value = {};
        clear_value.depthStencil.depth = 0.0f;
        clear_value.depthStencil.stencil = 0;

        VkRenderPassBeginInfo rp_begin_info = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
        rp_begin_info.renderArea = {{0, 0}, {uint32_t(view_state_->ren_res[0]), uint32_t(view_state_->ren_res[1])}};
        rp_begin_info.pClearValues = &clear_value;
        rp_begin_info.clearValueCount = 1;
        rp_begin_info.renderPass = storages.render_passes[rp_depth_velocity_[rp_index]].handle;
        rp_begin_info.framebuffer = storages.framebuffers[fb].first.handle;

        api.vkCmdBeginRenderPass(cmd_buf, &rp_begin_info, VK_SUBPASS_CONTENTS_INLINE);

        const Ren::VertexInput &vtx_input = storages.vtx_inputs[pi_skin_static_transp_main[0]->vtx_input];
        VertexInput_BindBuffers(api, vtx_input, storages.buffers, attrib_bufs, ndx_buf, cmd_buf, 0,
                                VK_INDEX_TYPE_UINT32);

        { // one-sided
            Ren::DebugMarker _mm(api, fg.cmd_buf(), "ONE-SIDED");
            api.vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_skin_static_transp_main[0]->pipeline);
            api.vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_skin_static_transp_main[0]->layout,
                                        0, 2, simple_descr_sets, 0, nullptr);
            i = _draw_range_ext(api, cmd_buf, *pi_skin_static_transp_main[0], zfill_batch_indices, zfill_batches, i,
                                BDB::BitsSkinned | BDB::BitAlphaTest, materials_per_descriptor,
                                bindless_tex_->textures_descr_sets, &draws_count);
        }

        { // two-sided
            Ren::DebugMarker _mm(api, fg.cmd_buf(), "TWO-SIDED");
            api.vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_skin_static_transp_main[1]->pipeline);
            api.vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_skin_static_transp_main[1]->layout,
                                        0, 2, simple_descr_sets, 0, nullptr);
            i = _draw_range_ext(api, cmd_buf, *pi_skin_static_transp_main[1], zfill_batch_indices, zfill_batches, i,
                                BDB::BitsSkinned | BDB::BitAlphaTest | BDB::BitTwoSided, materials_per_descriptor,
                                bindless_tex_->textures_descr_sets, &draws_count);
        }

        api.vkCmdEndRenderPass(cmd_buf);
    }

    const Ren::PipelineMain *pi_skin_moving_transp_main[2] = {&storages.pipelines[pi_skin_moving_transp_[0]].first,
                                                              &storages.pipelines[pi_skin_moving_transp_[1]].first};

    { // moving alpha-tested skinned (depth and velocity)
        Ren::DebugMarker _m(api, fg.cmd_buf(), "SKIN-ALPHA-MOVING");
        const int rp_index = (args_->clear_depth && !draws_count) ? 0 : 1;

        const Ren::ImageRWHandle velocity_target[] = {velocity};
        const Ren::FramebufferHandle fb =
            fg.FindOrCreateFramebuffer(rp_depth_velocity_[rp_index], depth, depth, velocity_target);

        VkClearValue clear_value = {};
        clear_value.depthStencil.depth = 0.0f;
        clear_value.depthStencil.stencil = 0;

        VkRenderPassBeginInfo rp_begin_info = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
        rp_begin_info.renderArea = {{0, 0}, {uint32_t(view_state_->ren_res[0]), uint32_t(view_state_->ren_res[1])}};
        rp_begin_info.pClearValues = &clear_value;
        rp_begin_info.clearValueCount = 1;
        rp_begin_info.renderPass = storages.render_passes[rp_depth_velocity_[rp_index]].handle;
        rp_begin_info.framebuffer = storages.framebuffers[fb].first.handle;

        api.vkCmdBeginRenderPass(cmd_buf, &rp_begin_info, VK_SUBPASS_CONTENTS_INLINE);

        const Ren::VertexInput &vtx_input = storages.vtx_inputs[pi_skin_moving_transp_main[0]->vtx_input];
        VertexInput_BindBuffers(api, vtx_input, storages.buffers, attrib_bufs, ndx_buf, cmd_buf, 0,
                                VK_INDEX_TYPE_UINT32);

        { // one-sided
            Ren::DebugMarker _mm(api, fg.cmd_buf(), "ONE-SIDED");
            api.vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_skin_moving_transp_main[0]->pipeline);
            api.vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_skin_moving_transp_main[0]->layout,
                                        0, 2, simple_descr_sets, 0, nullptr);
            i = _draw_range_ext(api, cmd_buf, *pi_skin_moving_transp_main[0], zfill_batch_indices, zfill_batches, i,
                                BDB::BitsSkinned | BDB::BitAlphaTest | BDB::BitMoving, materials_per_descriptor,
                                bindless_tex_->textures_descr_sets, &draws_count);
        }

        { // two-sided
            Ren::DebugMarker _mm(api, fg.cmd_buf(), "TWO-SIDED");
            api.vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_skin_moving_transp_main[1]->pipeline);
            api.vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_skin_moving_transp_main[1]->layout,
                                        0, 2, simple_descr_sets, 0, nullptr);
            i = _draw_range_ext(api, cmd_buf, *pi_skin_moving_transp_main[1], zfill_batch_indices, zfill_batches, i,
                                BDB::BitsSkinned | BDB::BitAlphaTest | BDB::BitMoving | BDB::BitTwoSided,
                                materials_per_descriptor, bindless_tex_->textures_descr_sets, &draws_count);
        }

        api.vkCmdEndRenderPass(cmd_buf);
    }
}
