#include "ExShadowColor.h"

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

namespace ExShadowColorInternal {
void _adjust_bias_and_viewport(const Ren::ApiContext &api, VkCommandBuffer cmd_buf, const Eng::shadow_list_t &sh_list) {
    const VkViewport viewport = {
        float(sh_list.shadow_map_pos[0]),
        float((Eng::SHADOWMAP_RES / 2) - sh_list.shadow_map_pos[1] - sh_list.shadow_map_size[1]),
        float(sh_list.shadow_map_size[0]),
        float(sh_list.shadow_map_size[1]),
        0.0f,
        1.0f};
    api.vkCmdSetViewport(cmd_buf, 0, 1, &viewport);

    const VkRect2D scissor = {{sh_list.scissor_test_pos[0],
                               (Eng::SHADOWMAP_RES / 2) - sh_list.scissor_test_pos[1] - sh_list.scissor_test_size[1]},
                              {uint32_t(sh_list.scissor_test_size[0]), uint32_t(sh_list.scissor_test_size[1])}};
    api.vkCmdSetScissor(cmd_buf, 0, 1, &scissor);

    api.vkCmdSetDepthBias(cmd_buf, sh_list.bias[1], 0.0f, sh_list.bias[0]);
}

void _clear_region(const Ren::ApiContext &api, VkCommandBuffer cmd_buf, const Eng::shadow_list_t &sh_list) {
    VkClearAttachment clear_att = {};
    clear_att.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    clear_att.clearValue.color = {{1.0f, 1.0f, 1.0f, 0.0f}};

    VkClearRect clear_rect = {};
    clear_rect.rect.offset = {sh_list.scissor_test_pos[0],
                              (Eng::SHADOWMAP_RES / 2) - sh_list.scissor_test_pos[1] - sh_list.scissor_test_size[1]};
    clear_rect.rect.extent = {uint32_t(sh_list.scissor_test_size[0]), uint32_t(sh_list.scissor_test_size[1])};
    clear_rect.baseArrayLayer = 0;
    clear_rect.layerCount = 1;

    api.vkCmdClearAttachments(cmd_buf, 1, &clear_att, 1, &clear_rect);
}
} // namespace ExShadowColorInternal

void Eng::ExShadowColor::DrawShadowMaps(const FgContext &fg, const Ren::ImageRWHandle shadow_depth,
                                        const Ren::ImageRWHandle shadow_color) {
    using namespace ExSharedInternal;
    using namespace ExShadowColorInternal;

    using BDB = basic_draw_batch_t;

    const Ren::BufferROHandle attrib_bufs[] = {fg.AccessROBuffer(args_->vtx_buf1), fg.AccessROBuffer(args_->vtx_buf2)};
    const Ren::BufferROHandle ndx_buf = fg.AccessROBuffer(args_->ndx_buf);

    [[maybe_unused]] const Ren::BufferROHandle unif_shared_data = fg.AccessROBuffer(args_->shared_data);
    const Ren::BufferROHandle instances = fg.AccessROBuffer(args_->instances);
    const Ren::BufferROHandle instance_indices = fg.AccessROBuffer(args_->instance_indices);
    const Ren::BufferROHandle materials = fg.AccessROBuffer(args_->materials);
    [[maybe_unused]] const Ren::ImageROHandle noise = fg.AccessROImage(args_->noise);

    const Ren::ApiContext &api = fg.ren_ctx().api();
    const Ren::StoragesRef &storages = fg.storages();

    const Ren::PipelineMain *pi_solid_main[3] = {&storages.pipelines[pi_solid_[0]].first,
                                                 &storages.pipelines[pi_solid_[1]].first,
                                                 &storages.pipelines[pi_solid_[2]].first};
    const Ren::ProgramMain &pr_solid0_main = storages.programs[pi_solid_main[0]->prog].first;

    VkCommandBuffer cmd_buf = fg.cmd_buf();

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

    /*VkDescriptorSetLayout vege_descr_set_layout = pi_vege_solid_->prog()->descr_set_layouts()[0];
    VkDescriptorSet vege_descr_sets[2];
    { // allocate descriptor sets
        const Ren::Binding bindings[] = {{Ren::eBindTarget::UBuf, BIND_UB_SHARED_DATA_BUF, *unif_shared_data_buf.ref},
                                         {Ren::eBindTarget::UTBuf, BIND_INST_BUF, *instances_buf.ref},
                                         {Ren::eBindTarget::SBufRO, BIND_INST_NDX_BUF, instance_indices_buf},
                                         {Ren::eBindTarget::SBufRO, BIND_MATERIALS_BUF, *materials_buf.ref},
                                         {Ren::eBindTarget::TexSampled, BIND_NOISE_TEX, *noise_tex.ref}};
        vege_descr_sets[0] =
            PrepareDescriptorSet(*api, vege_descr_set_layout, bindings, fg.descr_alloc(), ctx.log());
        vege_descr_sets[1] = bindless_tex_->textures_descr_sets[0];
    }*/

    bool region_cleared[MAX_SHADOWMAPS_TOTAL] = {};
    [[maybe_unused]] int draw_calls_count = 0;

    const uint32_t materials_per_descriptor = api.max_combined_image_samplers / MAX_TEX_PER_MATERIAL;

    const Ren::RenderPass &rp = storages.render_passes[pi_solid_main[0]->render_pass];

    const Ren::ImageRWHandle color_targets[] = {shadow_color};
    const Ren::FramebufferHandle fb_main =
        fg.FindOrCreateFramebuffer(pi_solid_main[0]->render_pass, shadow_depth, {}, color_targets);

    VkRenderPassBeginInfo rp_begin_info = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    rp_begin_info.renderPass = rp.handle;
    rp_begin_info.framebuffer = storages.framebuffers[fb_main].first.handle;
    rp_begin_info.renderArea = {{0, 0}, {uint32_t(w_), uint32_t(h_)}};
    api.vkCmdBeginRenderPass(cmd_buf, &rp_begin_info, VK_SUBPASS_CONTENTS_INLINE);

    Ren::SmallVector<uint32_t, 32> batch_points((*p_list_)->shadow_lists.count, 0);

    { // opaque objects
        Ren::DebugMarker _(api, fg.cmd_buf(), "STATIC-SOLID");

        api.vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_solid_main[0]->layout, 0, 2,
                                    simple_descr_sets, 0, nullptr);

        const Ren::VertexInput &vtx_input = storages.vtx_inputs[pi_solid_main[0]->vtx_input];
        VertexInput_BindBuffers(api, vtx_input, storages.buffers, attrib_bufs, ndx_buf, cmd_buf, 0,
                                VK_INDEX_TYPE_UINT32);

        static const uint64_t BitFlags[] = {BDB::BitAlphaBlend, BDB::BitAlphaBlend | BDB::BitBackSided,
                                            BDB::BitAlphaBlend | BDB::BitTwoSided};
        for (int pi = 0; pi < 3; ++pi) {
            api.vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_solid_main[pi]->pipeline);
            for (int i = 0; i < int((*p_list_)->shadow_lists.count); ++i) {
                const shadow_list_t &sh_list = (*p_list_)->shadow_lists.data[i];
                if (!sh_list.dirty && sh_list.alpha_blend_start_index == -1) {
                    continue;
                }

                _adjust_bias_and_viewport(api, cmd_buf, sh_list);

                if (!std::exchange(region_cleared[i], true)) {
                    _clear_region(api, cmd_buf, sh_list);
                }

                if (sh_list.alpha_blend_start_index == -1) {
                    continue;
                }

                Shadow::Params uniform_params = {};
                uniform_params.g_shadow_view_proj_mat = (*p_list_)->shadow_regions.data[i].clip_from_world;
                api.vkCmdPushConstants(cmd_buf, pi_solid_main[pi]->layout, VK_SHADER_STAGE_VERTEX_BIT, 0,
                                       sizeof(Shadow::Params), &uniform_params);

                Ren::Span<const uint32_t> batch_indices = {
                    (*p_list_)->shadow_batch_indices.data() + sh_list.alpha_blend_start_index,
                    sh_list.shadow_batch_count - (sh_list.alpha_blend_start_index - sh_list.shadow_batch_start)};

                uint32_t j = batch_points[i];
                j = _draw_range_ext(api, cmd_buf, *pi_solid_main[pi], batch_indices, (*p_list_)->shadow_batches, j,
                                    BitFlags[pi], materials_per_descriptor, bindless_tex_->textures_descr_sets,
                                    &draw_calls_count);
                batch_points[i] = j;
            }
        }
    }

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

        static const uint64_t BitFlags[] = {BDB::BitAlphaBlend | BDB::BitAlphaTest,
                                            BDB::BitAlphaBlend | BDB::BitAlphaTest | BDB::BitBackSided,
                                            BDB::BitAlphaBlend | BDB::BitAlphaTest | BDB::BitTwoSided};
        for (int pi = 0; pi < 3; ++pi) {
            api.vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_alpha_main[pi]->pipeline);
            for (int i = 0; i < int((*p_list_)->shadow_lists.count); ++i) {
                const shadow_list_t &sh_list = (*p_list_)->shadow_lists.data[i];
                if (!sh_list.dirty && sh_list.alpha_blend_start_index == -1) {
                    continue;
                }

                _adjust_bias_and_viewport(api, cmd_buf, sh_list);

                if (!std::exchange(region_cleared[i], true)) {
                    _clear_region(api, cmd_buf, sh_list);
                }

                if (sh_list.alpha_blend_start_index == -1) {
                    continue;
                }

                Shadow::Params uniform_params = {};
                uniform_params.g_shadow_view_proj_mat = (*p_list_)->shadow_regions.data[i].clip_from_world;
                api.vkCmdPushConstants(cmd_buf, pi_alpha_main[pi]->layout, VK_SHADER_STAGE_VERTEX_BIT, 0,
                                       sizeof(Shadow::Params), &uniform_params);

                Ren::Span<const uint32_t> batch_indices = {
                    (*p_list_)->shadow_batch_indices.data() + sh_list.alpha_blend_start_index,
                    sh_list.shadow_batch_count - (sh_list.alpha_blend_start_index - sh_list.shadow_batch_start)};

                uint32_t j = batch_points[i];
                j = _draw_range_ext(api, cmd_buf, *pi_alpha_main[pi], batch_indices, (*p_list_)->shadow_batches, j,
                                    BitFlags[pi], materials_per_descriptor, bindless_tex_->textures_descr_sets,
                                    &draw_calls_count);
                batch_points[i] = j;
            }
        }
    }

    api.vkCmdEndRenderPass(cmd_buf);
}
