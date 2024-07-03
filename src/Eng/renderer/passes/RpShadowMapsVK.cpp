#include "RpShadowMaps.h"

#include <Ren/Context.h>
#include <Ren/DebugMarker.h>
#include <Ren/DescriptorPool.h>
#include <Ren/RastState.h>
#include <Ren/VKCtx.h>

#include "../Renderer_Structs.h"
#include "../shaders/shadow_interface.h"

namespace RpSharedInternal {
uint32_t _draw_range(Ren::ApiContext *api_ctx, VkCommandBuffer cmd_buf, Ren::Span<const uint32_t> batch_indices,
                     Ren::Span<const Eng::BasicDrawBatch> batches, uint32_t i, const uint32_t mask, int *draws_count);
uint32_t _draw_range_ext(Ren::ApiContext *api_ctx, VkCommandBuffer cmd_buf, const Ren::Pipeline &pipeline,
                         Ren::Span<const uint32_t> batch_indices, Ren::Span<const Eng::BasicDrawBatch> batches,
                         uint32_t i, const uint32_t mask, const uint32_t materials_per_descriptor,
                         Ren::Span<const VkDescriptorSet> descr_sets, int *draws_count);
} // namespace RpSharedInternal

namespace RpShadowMapsInternal {
void _adjust_bias_and_viewport(Ren::ApiContext *api_ctx, VkCommandBuffer cmd_buf, const Eng::ShadowList &sh_list) {
    const VkViewport viewport = {
        float(sh_list.shadow_map_pos[0]),
        float((Eng::SHADOWMAP_RES / 2) - sh_list.shadow_map_pos[1] - sh_list.shadow_map_size[1]),
        float(sh_list.shadow_map_size[0]),
        float(sh_list.shadow_map_size[1]),
        0.0f,
        1.0f};
    api_ctx->vkCmdSetViewport(cmd_buf, 0, 1, &viewport);

    const VkRect2D scissor = {sh_list.scissor_test_pos[0],
                              (Eng::SHADOWMAP_RES / 2) - sh_list.scissor_test_pos[1] - sh_list.scissor_test_size[1],
                              uint32_t(sh_list.scissor_test_size[0]), uint32_t(sh_list.scissor_test_size[1])};
    api_ctx->vkCmdSetScissor(cmd_buf, 0, 1, &scissor);

    api_ctx->vkCmdSetDepthBias(cmd_buf, sh_list.bias[1], 0.0f, sh_list.bias[0]);
}

void _clear_region(Ren::ApiContext *api_ctx, VkCommandBuffer cmd_buf, const Eng::ShadowList &sh_list) {
    VkClearAttachment clear_att = {};
    clear_att.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    clear_att.clearValue.depthStencil.depth = 0.0f;

    VkClearRect clear_rect = {};
    clear_rect.rect.offset = {sh_list.scissor_test_pos[0],
                              (Eng::SHADOWMAP_RES / 2) - sh_list.scissor_test_pos[1] - sh_list.scissor_test_size[1]};
    clear_rect.rect.extent = {uint32_t(sh_list.scissor_test_size[0]), uint32_t(sh_list.scissor_test_size[1])};
    clear_rect.baseArrayLayer = 0;
    clear_rect.layerCount = 1;

    api_ctx->vkCmdClearAttachments(cmd_buf, 1, &clear_att, 1, &clear_rect);
}
} // namespace RpShadowMapsInternal

void Eng::RpShadowMaps::DrawShadowMaps(RpBuilder &builder, RpAllocTex &shadowmap_tex) {
    using namespace RpSharedInternal;
    using namespace RpShadowMapsInternal;

    using BDB = BasicDrawBatch;

    RpAllocBuf &unif_shared_data_buf = builder.GetReadBuffer(shared_data_buf_);
    RpAllocBuf &instances_buf = builder.GetReadBuffer(instances_buf_);
    RpAllocBuf &instance_indices_buf = builder.GetReadBuffer(instance_indices_buf_);
    RpAllocBuf &materials_buf = builder.GetReadBuffer(materials_buf_);
    RpAllocTex &noise_tex = builder.GetReadTexture(noise_tex_);

    Ren::Context &ctx = builder.ctx();
    Ren::ApiContext *api_ctx = ctx.api_ctx();

    VkCommandBuffer cmd_buf = api_ctx->draw_cmd_buf[api_ctx->backend_frame];

    VkDescriptorSetLayout simple_descr_set_layout = pi_solid_[0].prog()->descr_set_layouts()[0];
    VkDescriptorSet simple_descr_sets[2];
    { // allocate descriptor sets
        Ren::DescrSizes descr_sizes;
        descr_sizes.sbuf_count = 1;
        descr_sizes.tbuf_count = 1;

        simple_descr_sets[0] = ctx.default_descr_alloc()->Alloc(descr_sizes, simple_descr_set_layout);
        simple_descr_sets[1] = bindless_tex_->textures_descr_sets[0];
    }

    { // update descriptor set
        const VkBufferView instances_buf_view = instances_buf.tbos[0]->view();
        const VkDescriptorBufferInfo instance_indices_buf_info = {instance_indices_buf.ref->vk_handle(), 0,
                                                                  VK_WHOLE_SIZE};
        const VkDescriptorBufferInfo mat_buf_info = {materials_buf.ref->vk_handle(), 0, VK_WHOLE_SIZE};

        VkWriteDescriptorSet descr_writes[3];
        descr_writes[0] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        descr_writes[0].dstSet = simple_descr_sets[0];
        descr_writes[0].dstBinding = BIND_INST_BUF;
        descr_writes[0].dstArrayElement = 0;
        descr_writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
        descr_writes[0].descriptorCount = 1;
        descr_writes[0].pTexelBufferView = &instances_buf_view;

        descr_writes[1] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        descr_writes[1].dstSet = simple_descr_sets[0];
        descr_writes[1].dstBinding = BIND_INST_NDX_BUF;
        descr_writes[1].dstArrayElement = 0;
        descr_writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descr_writes[1].descriptorCount = 1;
        descr_writes[1].pBufferInfo = &instance_indices_buf_info;

        descr_writes[2] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        descr_writes[2].dstSet = simple_descr_sets[0];
        descr_writes[2].dstBinding = BIND_MATERIALS_BUF;
        descr_writes[2].dstArrayElement = 0;
        descr_writes[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descr_writes[2].descriptorCount = 1;
        descr_writes[2].pBufferInfo = &mat_buf_info;

        api_ctx->vkUpdateDescriptorSets(api_ctx->device, uint32_t(std::size(descr_writes)), descr_writes, 0, nullptr);
    }

    VkDescriptorSetLayout vege_descr_set_layout = pi_vege_solid_.prog()->descr_set_layouts()[0];
    VkDescriptorSet vege_descr_sets[2];
    { // allocate descriptor sets
        Ren::DescrSizes descr_sizes;
        descr_sizes.img_sampler_count = 1;
        descr_sizes.sbuf_count = 1;
        descr_sizes.tbuf_count = 1;

        vege_descr_sets[0] = ctx.default_descr_alloc()->Alloc(descr_sizes, vege_descr_set_layout);
        vege_descr_sets[1] = bindless_tex_->textures_descr_sets[0];
    }

    { // update descriptor set
        const VkDescriptorBufferInfo ubuf_info = {unif_shared_data_buf.ref->vk_handle(), 0, VK_WHOLE_SIZE};
        const VkBufferView instances_buf_view = instances_buf.tbos[0]->view();
        const VkDescriptorBufferInfo instance_indices_buf_info = {instance_indices_buf.ref->vk_handle(), 0,
                                                                  VK_WHOLE_SIZE};
        const VkDescriptorBufferInfo mat_buf_info = {materials_buf.ref->vk_handle(), 0, VK_WHOLE_SIZE};
        const VkDescriptorImageInfo img_info = noise_tex.ref->vk_desc_image_info();

        VkWriteDescriptorSet descr_writes[5];
        descr_writes[0] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        descr_writes[0].dstSet = vege_descr_sets[0];
        descr_writes[0].dstBinding = BIND_UB_SHARED_DATA_BUF;
        descr_writes[0].dstArrayElement = 0;
        descr_writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descr_writes[0].descriptorCount = 1;
        descr_writes[0].pBufferInfo = &ubuf_info;

        descr_writes[1] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        descr_writes[1].dstSet = vege_descr_sets[0];
        descr_writes[1].dstBinding = BIND_INST_BUF;
        descr_writes[1].dstArrayElement = 0;
        descr_writes[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
        descr_writes[1].descriptorCount = 1;
        descr_writes[1].pTexelBufferView = &instances_buf_view;

        descr_writes[2] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        descr_writes[2].dstSet = vege_descr_sets[0];
        descr_writes[2].dstBinding = BIND_INST_NDX_BUF;
        descr_writes[2].dstArrayElement = 0;
        descr_writes[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descr_writes[2].descriptorCount = 1;
        descr_writes[2].pBufferInfo = &instance_indices_buf_info;

        descr_writes[3] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        descr_writes[3].dstSet = vege_descr_sets[0];
        descr_writes[3].dstBinding = BIND_NOISE_TEX;
        descr_writes[3].dstArrayElement = 0;
        descr_writes[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descr_writes[3].descriptorCount = 1;
        descr_writes[3].pImageInfo = &img_info;

        descr_writes[4] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        descr_writes[4].dstSet = vege_descr_sets[0];
        descr_writes[4].dstBinding = BIND_MATERIALS_BUF;
        descr_writes[4].dstArrayElement = 0;
        descr_writes[4].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descr_writes[4].descriptorCount = 1;
        descr_writes[4].pBufferInfo = &mat_buf_info;

        api_ctx->vkUpdateDescriptorSets(api_ctx->device, uint32_t(std::size(descr_writes)), descr_writes, 0, nullptr);
    }

    bool region_cleared[MAX_SHADOWMAPS_TOTAL] = {};
    [[maybe_unused]] int draw_calls_count = 0;

    const uint32_t materials_per_descriptor = api_ctx->max_combined_image_samplers / MAX_TEX_PER_MATERIAL;

    VkRenderPassBeginInfo rp_begin_info = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    rp_begin_info.renderPass = rp_depth_only_.handle();
    rp_begin_info.framebuffer = shadow_fb_.handle();
    rp_begin_info.renderArea = {0, 0, uint32_t(shadowmap_tex.ref->params.w), uint32_t(shadowmap_tex.ref->params.h)};
    api_ctx->vkCmdBeginRenderPass(cmd_buf, &rp_begin_info, VK_SUBPASS_CONTENTS_INLINE);

    Ren::SmallVector<uint32_t, 32> batch_points((*p_list_)->shadow_lists.count, 0);

    { // opaque objects
        Ren::DebugMarker _(api_ctx, ctx.current_cmd_buf(), "STATIC-SOLID");

        api_ctx->vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_solid_[0].layout(), 0, 1,
                                         simple_descr_sets, 0, nullptr);

        vi_depth_pass_solid_.BindBuffers(api_ctx, cmd_buf, 0, VK_INDEX_TYPE_UINT32);

        const uint32_t BitFlags[] = {0, BDB::BitBackSided, BDB::BitTwoSided};
        for (int pi = 0; pi < 3; ++pi) {
            api_ctx->vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_solid_[pi].handle());
            for (int i = 0; i < int((*p_list_)->shadow_lists.count); i++) {
                const ShadowList &sh_list = (*p_list_)->shadow_lists.data[i];
                if (!sh_list.shadow_batch_count) {
                    continue;
                }

                _adjust_bias_and_viewport(api_ctx, cmd_buf, sh_list);

                if (!region_cleared[i]) {
                    _clear_region(api_ctx, cmd_buf, sh_list);
                    region_cleared[i] = true;
                }

                Shadow::Params uniform_params = {};
                uniform_params.g_shadow_view_proj_mat = (*p_list_)->shadow_regions.data[i].clip_from_world;
                api_ctx->vkCmdPushConstants(cmd_buf, pi_solid_[pi].layout(), VK_SHADER_STAGE_VERTEX_BIT, 0,
                                            sizeof(Shadow::Params), &uniform_params);

                Ren::Span<const uint32_t> batch_indices = {
                    (*p_list_)->shadow_batch_indices.data() + sh_list.shadow_batch_start, sh_list.shadow_batch_count};

                uint32_t j = batch_points[i];
                j = _draw_range(api_ctx, cmd_buf, batch_indices, (*p_list_)->shadow_batches, j, BitFlags[pi],
                                &draw_calls_count);
                batch_points[i] = j;
            }
        }
    }

    /*{ // draw opaque vegetation
        Ren::DebugMarker _mm(api_ctx, ctx.current_cmd_buf(), "VEGE-SOLID");

        api_ctx->vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_vege_solid_.handle());
        api_ctx->vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_vege_solid_.layout(), 0, 2,
                                         vege_descr_sets, 0, nullptr);
        uint32_t bound_descr_id = 0;

        vi_depth_pass_vege_solid_.BindBuffers(api_ctx, cmd_buf, 0, VK_INDEX_TYPE_UINT32);

        for (int i = 0; i < int((*p_list_)->shadow_lists.count); i++) {
            const ShadowList &sh_list = (*p_list_)->shadow_lists.data[i];
            if (!sh_list.shadow_batch_count) {
                continue;
            }

            _adjust_bias_and_viewport(api_ctx, cmd_buf, sh_list);

            if (!region_cleared[i]) {
                _clear_region(api_ctx, cmd_buf, sh_list);
                region_cleared[i] = true;
            }

            Shadow::Params uniform_params = {};
            uniform_params.g_shadow_view_proj_mat = (*p_list_)->shadow_regions.data[i].clip_from_world;
            api_ctx->vkCmdPushConstants(cmd_buf, pi_solid_[0].layout(), VK_SHADER_STAGE_VERTEX_BIT, 0,
                                        sizeof(Shadow::Params), &uniform_params);

            uint32_t j = batch_points[i];
            for (; j < sh_list.shadow_batch_start + sh_list.shadow_batch_count; j++) {
                const auto &batch = (*p_list_)->shadow_batches[(*p_list_)->shadow_batch_indices[j]];
                if (!batch.instance_count || batch.alpha_test_bit || batch.type_bits != BasicDrawBatch::TypeVege) {
                    continue;
                }

                const uint32_t descr_id = batch.material_index / materials_per_descriptor;
                if (descr_id != bound_descr_id) {
                    api_ctx->vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_vege_solid_.layout(),
                                                     1, 1, &bindless_tex_->textures_descr_sets[descr_id], 0, nullptr);
                    bound_descr_id = descr_id;
                }

                api_ctx->vkCmdDrawIndexed(cmd_buf, batch.indices_count, // index count
                                          batch.instance_count,         // instance count
                                          batch.indices_offset,         // first index
                                          batch.base_vertex,            // vertex offset
                                          batch.instance_start);        // first instance
                ++draw_calls_count;
            }
            batch_points[i] = j;
        }
    }*/

    { // transparent (alpha-tested) objects
        Ren::DebugMarker _(api_ctx, ctx.current_cmd_buf(), "STATIC-ALPHA");

        api_ctx->vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_transp_[0].layout(), 0, 2,
                                         simple_descr_sets, 0, nullptr);
        uint32_t bound_descr_id = 0;

        vi_depth_pass_transp_.BindBuffers(api_ctx, cmd_buf, 0, VK_INDEX_TYPE_UINT32);

        const uint32_t BitFlags[] = {BDB::BitAlphaTest, BDB::BitAlphaTest | BDB::BitBackSided,
                                     BDB::BitAlphaTest | BDB::BitTwoSided};
        for (int pi = 0; pi < 3; ++pi) {
            api_ctx->vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_transp_[pi].handle());
            for (int i = 0; i < int((*p_list_)->shadow_lists.count); i++) {
                const ShadowList &sh_list = (*p_list_)->shadow_lists.data[i];
                if (!sh_list.shadow_batch_count) {
                    continue;
                }

                _adjust_bias_and_viewport(api_ctx, cmd_buf, sh_list);

                if (!region_cleared[i]) {
                    _clear_region(api_ctx, cmd_buf, sh_list);
                    region_cleared[i] = true;
                }

                Shadow::Params uniform_params = {};
                uniform_params.g_shadow_view_proj_mat = (*p_list_)->shadow_regions.data[i].clip_from_world;
                api_ctx->vkCmdPushConstants(cmd_buf, pi_transp_[pi].layout(), VK_SHADER_STAGE_VERTEX_BIT, 0,
                                            sizeof(Shadow::Params), &uniform_params);

                Ren::Span<const uint32_t> batch_indices = {
                    (*p_list_)->shadow_batch_indices.data() + sh_list.shadow_batch_start, sh_list.shadow_batch_count};

                uint32_t j = batch_points[i];
                j = _draw_range_ext(api_ctx, cmd_buf, pi_transp_[pi], batch_indices, (*p_list_)->shadow_batches, j,
                                    BitFlags[pi], materials_per_descriptor, bindless_tex_->textures_descr_sets,
                                    &draw_calls_count);
                batch_points[i] = j;
            }
        }
    }

    /*{ // transparent (alpha-tested) vegetation
        Ren::DebugMarker _mm(api_ctx, ctx.current_cmd_buf(), "VEGE-ALPHA");

        api_ctx->vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_vege_transp_.handle());
        api_ctx->vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_vege_transp_.layout(), 0, 2,
                                         vege_descr_sets, 0, nullptr);
        uint32_t bound_descr_id = 0;

        vi_depth_pass_transp_.BindBuffers(api_ctx, cmd_buf, 0, VK_INDEX_TYPE_UINT32);

        for (int i = 0; i < int((*p_list_)->shadow_lists.count); i++) {
            const ShadowList &sh_list = (*p_list_)->shadow_lists.data[i];
            if (!sh_list.shadow_batch_count) {
                continue;
            }

            _adjust_bias_and_viewport(api_ctx, cmd_buf, sh_list);

            if (!region_cleared[i]) {
                _clear_region(api_ctx, cmd_buf, sh_list);
                region_cleared[i] = true;
            }

            Shadow::Params uniform_params = {};
            uniform_params.g_shadow_view_proj_mat = (*p_list_)->shadow_regions.data[i].clip_from_world;
            api_ctx->vkCmdPushConstants(cmd_buf, pi_solid_[0].layout(), VK_SHADER_STAGE_VERTEX_BIT, 0,
                                        sizeof(Shadow::Params), &uniform_params);

            for (uint32_t j = sh_list.shadow_batch_start; j < sh_list.shadow_batch_start + sh_list.shadow_batch_count;
                 j++) {
                const auto &batch = (*p_list_)->shadow_batches[(*p_list_)->shadow_batch_indices[j]];
                if (!batch.instance_count || !batch.alpha_test_bit || batch.type_bits != BasicDrawBatch::TypeVege) {
                    continue;
                }

                const uint32_t descr_id = batch.material_index / materials_per_descriptor;
                if (descr_id != bound_descr_id) {
                    api_ctx->vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_vege_transp_.layout(),
                                                     1, 1, &bindless_tex_->textures_descr_sets[descr_id], 0, nullptr);
                    bound_descr_id = descr_id;
                }

                api_ctx->vkCmdDrawIndexed(cmd_buf, batch.indices_count, // index count
                                          batch.instance_count,         // instance count
                                          batch.indices_offset,         // first index
                                          batch.base_vertex,            // vertex offset
                                          batch.instance_start);        // first instance
                ++draw_calls_count;
            }
        }
    }*/

    api_ctx->vkCmdEndRenderPass(cmd_buf);
}
