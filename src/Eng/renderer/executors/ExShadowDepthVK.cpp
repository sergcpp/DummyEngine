#include "ExShadowDepth.h"

#include <Ren/Context.h>
#include <Ren/DebugMarker.h>
#include <Ren/DescriptorPool.h>
#include <Ren/DrawCall.h>
#include <Ren/RastState.h>
#include <Ren/Vk/VKCtx.h>

#include "../Renderer_DrawList.h"
#include "../framegraph/FgBuilder.h"
#include "../shaders/shadow_interface.h"

namespace ExSharedInternal {
uint32_t _draw_range(const Ren::ApiContext &api, VkCommandBuffer cmd_buf, Ren::Span<const uint32_t> batch_indices,
                     Ren::Span<const Eng::basic_draw_batch_t> batches, uint32_t i, uint64_t mask, int *draws_count);
uint32_t _draw_range_ext(const Ren::ApiContext &api, VkCommandBuffer cmd_buf, const Ren::PipelineMain &pipeline,
                         Ren::Span<const uint32_t> batch_indices, Ren::Span<const Eng::basic_draw_batch_t> batches,
                         uint32_t i, uint64_t mask, uint32_t materials_per_descriptor,
                         Ren::Span<const VkDescriptorSet> descr_sets, int *draws_count);
} // namespace ExSharedInternal

namespace ExShadowDepthInternal {
void _adjust_bias_and_viewport(const Ren::ApiContext &api, VkCommandBuffer cmd_buf, const Eng::shadow_list_t &sh_list) {
    const VkViewport viewport = {
        float(sh_list.shadow_map_pos[0]),
        float(int(Eng::SHADOWMAP_RES / 2) - sh_list.shadow_map_pos[1] - sh_list.shadow_map_size[1]),
        float(sh_list.shadow_map_size[0]),
        float(sh_list.shadow_map_size[1]),
        0.0f,
        1.0f};
    api.vkCmdSetViewport(cmd_buf, 0, 1, &viewport);

    const VkRect2D scissor = {{sh_list.scissor_test_pos[0], int(Eng::SHADOWMAP_RES / 2) - sh_list.scissor_test_pos[1] -
                                                                sh_list.scissor_test_size[1]},
                              {uint32_t(sh_list.scissor_test_size[0]), uint32_t(sh_list.scissor_test_size[1])}};
    api.vkCmdSetScissor(cmd_buf, 0, 1, &scissor);

    api.vkCmdSetDepthBias(cmd_buf, sh_list.bias[1], 0.0f, sh_list.bias[0]);
}

void _clear_region(const Ren::ApiContext &api, VkCommandBuffer cmd_buf, const Eng::shadow_list_t &sh_list) {
    VkClearAttachment clear_att = {};
    clear_att.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    clear_att.clearValue.depthStencil.depth = 0.0f;

    VkClearRect clear_rect = {};
    clear_rect.rect.offset = {sh_list.scissor_test_pos[0],
                              int(Eng::SHADOWMAP_RES / 2) - sh_list.scissor_test_pos[1] - sh_list.scissor_test_size[1]};
    clear_rect.rect.extent = {uint32_t(sh_list.scissor_test_size[0]), uint32_t(sh_list.scissor_test_size[1])};
    clear_rect.baseArrayLayer = 0;
    clear_rect.layerCount = 1;

    api.vkCmdClearAttachments(cmd_buf, 1, &clear_att, 1, &clear_rect);
}
} // namespace ExShadowDepthInternal

void Eng::ExShadowDepth::DrawShadowMaps(const FgContext &fg, const Ren::ImageRWHandle shadow_depth) {
    using namespace ExSharedInternal;
    using namespace ExShadowDepthInternal;

    using BDB = basic_draw_batch_t;

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

    const Ren::PipelineMain *pi_solid_main[3] = {&storages.pipelines[pi_solid_[0]].first,
                                                 &storages.pipelines[pi_solid_[1]].first,
                                                 &storages.pipelines[pi_solid_[2]].first};
    const Ren::ProgramMain &pr_solid0_main = storages.programs[pi_solid_main[0]->prog].first;

    VkDescriptorSetLayout simple_descr_set_layout = pr_solid0_main.descr_set_layouts[0];
    VkDescriptorSet simple_descr_sets[2];
    { // allocate descriptor sets
        const Ren::Binding bindings[] = {{Ren::eBindTarget::UTBuf, BIND_INST_BUF, instances},
                                         {Ren::eBindTarget::SBufRO, BIND_INST_NDX_BUF, instance_indices},
                                         {Ren::eBindTarget::SBufRO, BIND_MATERIALS_BUF, materials}};
        simple_descr_sets[0] =
            PrepareDescriptorSet(api, storages, simple_descr_set_layout, bindings, fg.descr_alloc(), fg.log());
        simple_descr_sets[1] = bindless_tex_->textures_descr_sets[0];
    }

    const Ren::PipelineMain &pi_vege_main = storages.pipelines[pi_vege_solid_].first;
    const Ren::ProgramMain &pr_vege_main = storages.programs[pi_vege_main.prog].first;

    VkDescriptorSetLayout vege_descr_set_layout = pr_vege_main.descr_set_layouts[0];
    VkDescriptorSet vege_descr_sets[2];
    { // allocate descriptor sets
        const Ren::Binding bindings[] = {{Ren::eBindTarget::UBuf, BIND_UB_SHARED_DATA_BUF, unif_shared_data},
                                         {Ren::eBindTarget::UTBuf, BIND_INST_BUF, instances},
                                         {Ren::eBindTarget::SBufRO, BIND_INST_NDX_BUF, instance_indices},
                                         {Ren::eBindTarget::SBufRO, BIND_MATERIALS_BUF, materials},
                                         {Ren::eBindTarget::TexSampled, BIND_NOISE_TEX, noise}};
        vege_descr_sets[0] =
            PrepareDescriptorSet(api, storages, vege_descr_set_layout, bindings, fg.descr_alloc(), fg.log());
        vege_descr_sets[1] = bindless_tex_->textures_descr_sets[0];
    }

    bool region_cleared[MAX_SHADOWMAPS_TOTAL] = {};
    [[maybe_unused]] int draw_calls_count = 0;

    const uint32_t materials_per_descriptor = api.max_combined_image_samplers / MAX_TEX_PER_MATERIAL;

    const Ren::RenderPass &rp = storages.render_passes[pi_solid_main[0]->render_pass];

    const Ren::FramebufferHandle fb_main =
        fg.FindOrCreateFramebuffer(pi_solid_main[0]->render_pass, shadow_depth, {}, {});

    VkRenderPassBeginInfo rp_begin_info = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    rp_begin_info.renderPass = rp.handle;
    rp_begin_info.framebuffer = storages.framebuffers[fb_main].first.handle;
    rp_begin_info.renderArea = {{0, 0}, {uint32_t(w_), uint32_t(h_)}};
    api.vkCmdBeginRenderPass(cmd_buf, &rp_begin_info, VK_SUBPASS_CONTENTS_INLINE);

    Ren::SmallVector<uint32_t, 32> batch_points((*p_list_)->shadow_lists.count, 0);

    { // opaque objects
        Ren::DebugMarker _(api, fg.cmd_buf(), "STATIC-SOLID");

        api.vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_solid_main[0]->layout, 0, 1,
                                    simple_descr_sets, 0, nullptr);

        const Ren::VertexInput &vtx_input = storages.vtx_inputs[pi_solid_main[0]->vtx_input];
        VertexInput_BindBuffers(api, vtx_input, storages.buffers, attrib_bufs, ndx_buf, cmd_buf, 0,
                                VK_INDEX_TYPE_UINT32);

        static const uint64_t BitFlags[] = {0, BDB::BitBackSided, BDB::BitTwoSided};
        for (int pi = 0; pi < 3; ++pi) {
            api.vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_solid_main[pi]->pipeline);
            for (int i = 0; i < int((*p_list_)->shadow_lists.count); ++i) {
                const shadow_list_t &sh_list = (*p_list_)->shadow_lists.data[i];
                if (!sh_list.dirty && !sh_list.shadow_batch_count) {
                    continue;
                }

                _adjust_bias_and_viewport(api, cmd_buf, sh_list);

                if (!std::exchange(region_cleared[i], true)) {
                    _clear_region(api, cmd_buf, sh_list);
                }

                Shadow::Params uniform_params = {};
                uniform_params.g_shadow_view_proj_mat = (*p_list_)->shadow_regions.data[i].clip_from_world;
                api.vkCmdPushConstants(cmd_buf, pi_solid_main[pi]->layout, VK_SHADER_STAGE_VERTEX_BIT, 0,
                                       sizeof(Shadow::Params), &uniform_params);

                Ren::Span<const uint32_t> batch_indices = {
                    (*p_list_)->shadow_batch_indices.data() + sh_list.shadow_batch_start, sh_list.shadow_batch_count};

                uint32_t j = batch_points[i];
                j = _draw_range(api, cmd_buf, batch_indices, (*p_list_)->shadow_batches, j, BitFlags[pi],
                                &draw_calls_count);
                batch_points[i] = j;
            }
        }
    }

    /*{ // draw opaque vegetation
        Ren::DebugMarker _mm(api, fg.cmd_buf(), "VEGE-SOLID");

        api.vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_vege_solid_.handle());
        api.vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_vege_solid_.layout(), 0, 2,
                                         vege_descr_sets, 0, nullptr);
        uint32_t bound_descr_id = 0;

        vi_depth_pass_vege_solid_->BindBuffers(api, cmd_buf, 0, VK_INDEX_TYPE_UINT32);

        for (int i = 0; i < int((*p_list_)->shadow_lists.count); ++i) {
            const shadow_list_t &sh_list = (*p_list_)->shadow_lists.data[i];
            if (!sh_list.dirty && !sh_list.shadow_batch_count) {
                continue;
            }

            _adjust_bias_and_viewport(api, cmd_buf, sh_list);

            if (!std::exchange(region_cleared[i], true)) {
                _clear_region(api, cmd_buf, sh_list);
            }

            Shadow::Params uniform_params = {};
            uniform_params.g_shadow_view_proj_mat = (*p_list_)->shadow_regions.data[i].clip_from_world;
            api.vkCmdPushConstants(cmd_buf, pi_solid_[0].layout(), VK_SHADER_STAGE_VERTEX_BIT, 0,
                                        sizeof(Shadow::Params), &uniform_params);

            uint32_t j = batch_points[i];
            for (; j < sh_list.shadow_batch_start + sh_list.shadow_batch_count; j++) {
                const auto &batch = (*p_list_)->shadow_batches[(*p_list_)->shadow_batch_indices[j]];
                if (!batch.instance_count || batch.alpha_test_bit || batch.type_bits != basic_draw_batch_t::TypeVege) {
                    continue;
                }

                const uint32_t descr_id = batch.material_index / materials_per_descriptor;
                if (descr_id != bound_descr_id) {
                    api.vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_vege_solid_.layout(),
                                                     1, 1, &bindless_tex_->textures_descr_sets[descr_id], 0, nullptr);
                    bound_descr_id = descr_id;
                }

                api.vkCmdDrawIndexed(cmd_buf, batch.indices_count, // index count
                                          batch.instance_count,         // instance count
                                          batch.indices_offset,         // first index
                                          batch.base_vertex,            // vertex offset
                                          batch.instance_start);        // first instance
                ++draw_calls_count;
            }
            batch_points[i] = j;
        }
    }*/

    const Ren::PipelineMain *pi_alpha_main[3] = {&storages.pipelines[pi_alpha_[0]].first,
                                                 &storages.pipelines[pi_alpha_[1]].first,
                                                 &storages.pipelines[pi_alpha_[2]].first};

    { // alpha-tested objects
        Ren::DebugMarker _(api, fg.cmd_buf(), "STATIC-ALPHA");

        api.vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_alpha_main[0]->layout, 0, 2,
                                    simple_descr_sets, 0, nullptr);

        const Ren::VertexInput &vtx_input = storages.vtx_inputs[pi_alpha_main[0]->vtx_input];
        VertexInput_BindBuffers(api, vtx_input, storages.buffers, attrib_bufs, ndx_buf, cmd_buf, 0,
                                VK_INDEX_TYPE_UINT32);

        static const uint64_t BitFlags[] = {BDB::BitAlphaTest, BDB::BitAlphaTest | BDB::BitBackSided,
                                            BDB::BitAlphaTest | BDB::BitTwoSided};
        for (int pi = 0; pi < 3; ++pi) {
            api.vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_alpha_main[pi]->pipeline);
            for (int i = 0; i < int((*p_list_)->shadow_lists.count); ++i) {
                const shadow_list_t &sh_list = (*p_list_)->shadow_lists.data[i];
                if (!sh_list.dirty && !sh_list.shadow_batch_count) {
                    continue;
                }

                _adjust_bias_and_viewport(api, cmd_buf, sh_list);

                if (!std::exchange(region_cleared[i], true)) {
                    _clear_region(api, cmd_buf, sh_list);
                }

                Shadow::Params uniform_params = {};
                uniform_params.g_shadow_view_proj_mat = (*p_list_)->shadow_regions.data[i].clip_from_world;
                api.vkCmdPushConstants(cmd_buf, pi_alpha_main[pi]->layout, VK_SHADER_STAGE_VERTEX_BIT, 0,
                                       sizeof(Shadow::Params), &uniform_params);

                Ren::Span<const uint32_t> batch_indices = {
                    (*p_list_)->shadow_batch_indices.data() + sh_list.shadow_batch_start, sh_list.shadow_batch_count};

                uint32_t j = batch_points[i];
                j = _draw_range_ext(api, cmd_buf, *pi_alpha_main[pi], batch_indices, (*p_list_)->shadow_batches, j,
                                    BitFlags[pi], materials_per_descriptor, bindless_tex_->textures_descr_sets,
                                    &draw_calls_count);
                batch_points[i] = j;
            }
        }
    }

    /*{ // alpha-tested vegetation
        Ren::DebugMarker _mm(api, fg.cmd_buf(), "VEGE-ALPHA");

        api.vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_vege_alpha_.handle());
        api.vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_vege_alpha_.layout(), 0, 2,
                                         vege_descr_sets, 0, nullptr);
        uint32_t bound_descr_id = 0;

        vi_depth_pass_alpha_->BindBuffers(api, cmd_buf, 0, VK_INDEX_TYPE_UINT32);

        for (int i = 0; i < int((*p_list_)->shadow_lists.count); ++i) {
            const shadow_list_t &sh_list = (*p_list_)->shadow_lists.data[i];
            if (!sh_list.dirty && !sh_list.shadow_batch_count) {
                continue;
            }

            _adjust_bias_and_viewport(api, cmd_buf, sh_list);

            if (!std::exchange(region_cleared[i], true)) {
                _clear_region(api, cmd_buf, sh_list);
            }

            Shadow::Params uniform_params = {};
            uniform_params.g_shadow_view_proj_mat = (*p_list_)->shadow_regions.data[i].clip_from_world;
            api.vkCmdPushConstants(cmd_buf, pi_solid_[0].layout(), VK_SHADER_STAGE_VERTEX_BIT, 0,
                                        sizeof(Shadow::Params), &uniform_params);

            for (uint32_t j = sh_list.shadow_batch_start; j < sh_list.shadow_batch_start + sh_list.shadow_batch_count;
                 j++) {
                const auto &batch = (*p_list_)->shadow_batches[(*p_list_)->shadow_batch_indices[j]];
                if (!batch.instance_count || !batch.alpha_test_bit || batch.type_bits != basic_draw_batch_t::TypeVege) {
                    continue;
                }

                const uint32_t descr_id = batch.material_index / materials_per_descriptor;
                if (descr_id != bound_descr_id) {
                    api.vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_vege_alpha_.layout(),
                                                     1, 1, &bindless_tex_->textures_descr_sets[descr_id], 0, nullptr);
                    bound_descr_id = descr_id;
                }

                api.vkCmdDrawIndexed(cmd_buf, batch.indices_count, // index count
                                          batch.instance_count,         // instance count
                                          batch.indices_offset,         // first index
                                          batch.base_vertex,            // vertex offset
                                          batch.instance_start);        // first instance
                ++draw_calls_count;
            }
        }
    }*/

    api.vkCmdEndRenderPass(cmd_buf);
}
