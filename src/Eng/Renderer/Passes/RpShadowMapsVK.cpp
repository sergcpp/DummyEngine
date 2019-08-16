#include "RpShadowMaps.h"

#include <Ren/RastState.h>

#include "../Renderer_Structs.h"

#include <Ren/Context.h>
#include <Ren/VKCtx.h>

#include "../assets/shaders/internal/shadow_interface.glsl"

namespace RpShadowMapsInternal {
void _adjust_bias_and_viewport(VkCommandBuffer cmd_buf, const ShadowList &sh_list) {
    const VkViewport viewport = {float(sh_list.shadow_map_pos[0]),
                                 float((REN_SHAD_RES / 2) - sh_list.shadow_map_pos[1] - sh_list.shadow_map_size[1]),
                                 float(sh_list.shadow_map_size[0]),
                                 float(sh_list.shadow_map_size[1]),
                                 0.0f,
                                 1.0f};
    vkCmdSetViewport(cmd_buf, 0, 1, &viewport);

    const VkRect2D scissor = {sh_list.scissor_test_pos[0],
                              (REN_SHAD_RES / 2) - sh_list.scissor_test_pos[1] - sh_list.scissor_test_size[1],
                              uint32_t(sh_list.scissor_test_size[0]), uint32_t(sh_list.scissor_test_size[1])};
    vkCmdSetScissor(cmd_buf, 0, 1, &scissor);

    vkCmdSetDepthBias(cmd_buf, sh_list.bias[1], 0.0f, sh_list.bias[0]);
}

void _clear_region(VkCommandBuffer cmd_buf, const ShadowList &sh_list) {
    VkClearAttachment clear_att = {};
    clear_att.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    clear_att.clearValue.depthStencil.depth = 1.0f;

    VkClearRect clear_rect = {};
    clear_rect.rect.offset = {sh_list.scissor_test_pos[0],
                              (REN_SHAD_RES / 2) - sh_list.scissor_test_pos[1] - sh_list.scissor_test_size[1]};
    clear_rect.rect.extent = {uint32_t(sh_list.scissor_test_size[0]), uint32_t(sh_list.scissor_test_size[1])};
    clear_rect.baseArrayLayer = 0;
    clear_rect.layerCount = 1;

    vkCmdClearAttachments(cmd_buf, 1, &clear_att, 1, &clear_rect);
}
} // namespace RpShadowMapsInternal

void RpShadowMaps::DrawShadowMaps(RpBuilder &builder, RpAllocTex &shadowmap_tex) {
    using namespace RpShadowMapsInternal;

    RpAllocBuf &unif_shared_data_buf = builder.GetReadBuffer(shared_data_buf_);
    RpAllocBuf &instances_buf = builder.GetReadBuffer(instances_buf_);
    RpAllocBuf &materials_buf = builder.GetReadBuffer(materials_buf_);
    RpAllocTex &noise_tex = builder.GetReadTexture(noise_tex_);

    Ren::Context &ctx = builder.ctx();
    Ren::ApiContext *api_ctx = ctx.api_ctx();

    VkCommandBuffer cmd_buf = api_ctx->draw_cmd_buf[api_ctx->backend_frame];

    VkDescriptorSetLayout simple_descr_set_layout = pi_solid_.prog()->descr_set_layouts()[0];
    VkDescriptorSet simple_descr_sets[2];
    simple_descr_sets[0] = ctx.default_descr_alloc()->Alloc(0 /* img_count */, 0 /* ubuf_count */, 1 /* sbuf_count */,
                                                            1 /* tbuf_count */, simple_descr_set_layout);
    simple_descr_sets[1] = (*bindless_tex_->textures_descr_sets)[0];

    { // update descriptor set
        const VkBufferView instances_buf_view = instances_buf.tbos[0]->view();
        const VkDescriptorBufferInfo mat_buf_info = {materials_buf.ref->handle().buf, 0, VK_WHOLE_SIZE};

        VkWriteDescriptorSet descr_writes[2] = {};

        descr_writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descr_writes[0].dstSet = simple_descr_sets[0];
        descr_writes[0].dstBinding = REN_INST_BUF_SLOT;
        descr_writes[0].dstArrayElement = 0;
        descr_writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
        descr_writes[0].descriptorCount = 1;
        descr_writes[0].pTexelBufferView = &instances_buf_view;

        descr_writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descr_writes[1].dstSet = simple_descr_sets[0];
        descr_writes[1].dstBinding = REN_MATERIALS_SLOT;
        descr_writes[1].dstArrayElement = 0;
        descr_writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descr_writes[1].descriptorCount = 1;
        descr_writes[1].pBufferInfo = &mat_buf_info;

        vkUpdateDescriptorSets(api_ctx->device, 2, descr_writes, 0, nullptr);
    }

    VkDescriptorSetLayout vege_descr_set_layout = pi_vege_solid_.prog()->descr_set_layouts()[0];
    VkDescriptorSet vege_descr_sets[2];
    vege_descr_sets[0] = ctx.default_descr_alloc()->Alloc(1 /* img_count */, 0 /* ubuf_count */, 1 /* sbuf_count */,
                                                          1 /* tbuf_count */, vege_descr_set_layout);
    vege_descr_sets[1] = (*bindless_tex_->textures_descr_sets)[0];

    { // update descriptor set
        const VkDescriptorBufferInfo ubuf_info = {unif_shared_data_buf.ref->handle().buf, 0, VK_WHOLE_SIZE};
        const VkBufferView instances_buf_view = instances_buf.tbos[0]->view();
        const VkDescriptorBufferInfo mat_buf_info = {materials_buf.ref->handle().buf, 0, VK_WHOLE_SIZE};
        const VkDescriptorImageInfo img_info = noise_tex.ref->vk_desc_image_info();

        VkWriteDescriptorSet descr_writes[4] = {};

        descr_writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descr_writes[0].dstSet = vege_descr_sets[0];
        descr_writes[0].dstBinding = REN_UB_SHARED_DATA_LOC;
        descr_writes[0].dstArrayElement = 0;
        descr_writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descr_writes[0].descriptorCount = 1;
        descr_writes[0].pBufferInfo = &ubuf_info;

        descr_writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descr_writes[1].dstSet = vege_descr_sets[0];
        descr_writes[1].dstBinding = REN_INST_BUF_SLOT;
        descr_writes[1].dstArrayElement = 0;
        descr_writes[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
        descr_writes[1].descriptorCount = 1;
        descr_writes[1].pTexelBufferView = &instances_buf_view;

        descr_writes[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descr_writes[2].dstSet = vege_descr_sets[0];
        descr_writes[2].dstBinding = REN_NOISE_TEX_SLOT;
        descr_writes[2].dstArrayElement = 0;
        descr_writes[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descr_writes[2].descriptorCount = 1;
        descr_writes[2].pImageInfo = &img_info;

        descr_writes[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descr_writes[3].dstSet = vege_descr_sets[0];
        descr_writes[3].dstBinding = REN_MATERIALS_SLOT;
        descr_writes[3].dstArrayElement = 0;
        descr_writes[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descr_writes[3].descriptorCount = 1;
        descr_writes[3].pBufferInfo = &mat_buf_info;

        vkUpdateDescriptorSets(api_ctx->device, 4, descr_writes, 0, nullptr);
    }

    bool region_cleared[REN_MAX_SHADOWMAPS_TOTAL] = {};
    int draw_calls_count = 0;

    const uint32_t materials_per_descriptor = api_ctx->max_combined_image_samplers / REN_MAX_TEX_PER_MATERIAL;

    {
        VkRenderPassBeginInfo rp_begin_info = {};
        rp_begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        rp_begin_info.renderPass = rp_depth_only_.handle();
        rp_begin_info.framebuffer = shadow_fb_.handle();
        rp_begin_info.renderArea = {0, 0, uint32_t(shadowmap_tex.ref->params.w), uint32_t(shadowmap_tex.ref->params.h)};
        vkCmdBeginRenderPass(cmd_buf, &rp_begin_info, VK_SUBPASS_CONTENTS_INLINE);

        { // opaque objects
            vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_solid_.handle());
            vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_solid_.layout(), 0, 1,
                                    simple_descr_sets, 0, nullptr);

            vi_depth_pass_solid_.BindBuffers(cmd_buf, 0, VK_INDEX_TYPE_UINT32);

            for (int i = 0; i < int(shadow_lists_.count); i++) {
                const ShadowList &sh_list = shadow_lists_.data[i];
                if (!sh_list.shadow_batch_count) {
                    continue;
                }

                _adjust_bias_and_viewport(cmd_buf, sh_list);

                if (!region_cleared[i]) {
                    _clear_region(cmd_buf, sh_list);
                    region_cleared[i] = true;
                }

                Shadow::Params uniform_params = {};
                uniform_params.uShadowViewProjMatrix = shadow_regions_.data[i].clip_from_world;
                vkCmdPushConstants(cmd_buf, pi_solid_.layout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(Shadow::Params),
                                   &uniform_params);

                for (uint32_t j = sh_list.shadow_batch_start;
                     j < sh_list.shadow_batch_start + sh_list.shadow_batch_count; j++) {
                    const DepthDrawBatch &batch = shadow_batches_.data[shadow_batch_indices_.data[j]];
                    if (!batch.instance_count || batch.alpha_test_bit || batch.type_bits == DepthDrawBatch::TypeVege) {
                        continue;
                    }

                    vkCmdPushConstants(cmd_buf, pi_solid_.layout(), VK_SHADER_STAGE_VERTEX_BIT,
                                       offsetof(Shadow::Params, uInstanceIndices),
                                       batch.instance_count * sizeof(Ren::Vec2i), &batch.instance_indices[0][0]);

                    vkCmdDrawIndexed(cmd_buf, batch.indices_count, // index count
                                     batch.instance_count,         // instance count
                                     batch.indices_offset,         // first index
                                     batch.base_vertex,            // vertex offset
                                     0);                           // first instance
                    ++draw_calls_count;
                }
            }
        }

        { // draw opaque vegetation
            vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_vege_solid_.handle());
            vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_vege_solid_.layout(), 0, 1,
                                    vege_descr_sets, 0, nullptr);

            vi_depth_pass_vege_solid_.BindBuffers(cmd_buf, 0, VK_INDEX_TYPE_UINT32);

            for (int i = 0; i < int(shadow_lists_.count); i++) {
                const ShadowList &sh_list = shadow_lists_.data[i];
                if (!sh_list.shadow_batch_count) {
                    continue;
                }

                _adjust_bias_and_viewport(cmd_buf, sh_list);

                if (!region_cleared[i]) {
                    _clear_region(cmd_buf, sh_list);
                    region_cleared[i] = true;
                }

                Shadow::Params uniform_params = {};
                uniform_params.uShadowViewProjMatrix = shadow_regions_.data[i].clip_from_world;
                vkCmdPushConstants(cmd_buf, pi_solid_.layout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(Shadow::Params),
                                   &uniform_params);

                for (uint32_t j = sh_list.shadow_batch_start;
                     j < sh_list.shadow_batch_start + sh_list.shadow_batch_count; j++) {
                    const DepthDrawBatch &batch = shadow_batches_.data[shadow_batch_indices_.data[j]];
                    if (!batch.instance_count || batch.alpha_test_bit || batch.type_bits != DepthDrawBatch::TypeVege) {
                        continue;
                    }

                    vkCmdPushConstants(cmd_buf, pi_vege_solid_.layout(), VK_SHADER_STAGE_VERTEX_BIT,
                                       offsetof(Shadow::Params, uInstanceIndices),
                                       batch.instance_count * sizeof(Ren::Vec2i), &batch.instance_indices[0][0]);

                    vkCmdDrawIndexed(cmd_buf, batch.indices_count, // index count
                                     batch.instance_count,         // instance count
                                     batch.indices_offset,         // first index
                                     batch.base_vertex,            // vertex offset
                                     0);                           // first instance
                    ++draw_calls_count;
                }
            }
        }

        { // transparent (alpha-tested) objects
            vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_transp_.handle());
            vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_transp_.layout(), 0, 2,
                                    simple_descr_sets, 0, nullptr);
            uint32_t bound_descr_id = 0;

            vi_depth_pass_transp_.BindBuffers(cmd_buf, 0, VK_INDEX_TYPE_UINT32);

            for (int i = 0; i < int(shadow_lists_.count); i++) {
                const ShadowList &sh_list = shadow_lists_.data[i];
                if (!sh_list.shadow_batch_count) {
                    continue;
                }

                _adjust_bias_and_viewport(cmd_buf, sh_list);

                if (!region_cleared[i]) {
                    _clear_region(cmd_buf, sh_list);
                    region_cleared[i] = true;
                }

                Shadow::Params uniform_params = {};
                uniform_params.uShadowViewProjMatrix = shadow_regions_.data[i].clip_from_world;
                vkCmdPushConstants(cmd_buf, pi_solid_.layout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(Shadow::Params),
                                   &uniform_params);

                for (uint32_t j = sh_list.shadow_batch_start;
                     j < sh_list.shadow_batch_start + sh_list.shadow_batch_count; j++) {
                    const DepthDrawBatch &batch = shadow_batches_.data[shadow_batch_indices_.data[j]];
                    if (!batch.instance_count || !batch.alpha_test_bit || batch.type_bits == DepthDrawBatch::TypeVege) {
                        continue;
                    }

                    const uint32_t descr_id = batch.instance_indices[0][1] / materials_per_descriptor;
                    if (descr_id != bound_descr_id) {
                        vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_transp_.layout(), 1, 1,
                                                &(*bindless_tex_->textures_descr_sets)[descr_id], 0, nullptr);
                        bound_descr_id = descr_id;
                    }

                    vkCmdPushConstants(cmd_buf, pi_transp_.layout(), VK_SHADER_STAGE_VERTEX_BIT,
                                       offsetof(Shadow::Params, uInstanceIndices),
                                       batch.instance_count * sizeof(Ren::Vec2i), &batch.instance_indices[0][0]);
                    vkCmdDrawIndexed(cmd_buf, batch.indices_count, // index count
                                     batch.instance_count,         // instance count
                                     batch.indices_offset,         // first index
                                     batch.base_vertex,            // vertex offset
                                     0);                           // first instance
                    ++draw_calls_count;
                }
            }
        }

        { // transparent (alpha-tested) vegetation
            vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_vege_transp_.handle());
            vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_vege_transp_.layout(), 0, 2,
                                    vege_descr_sets, 0, nullptr);
            uint32_t bound_descr_id = 0;

            vi_depth_pass_transp_.BindBuffers(cmd_buf, 0, VK_INDEX_TYPE_UINT32);

            for (int i = 0; i < int(shadow_lists_.count); i++) {
                const ShadowList &sh_list = shadow_lists_.data[i];
                if (!sh_list.shadow_batch_count) {
                    continue;
                }

                _adjust_bias_and_viewport(cmd_buf, sh_list);

                if (!region_cleared[i]) {
                    _clear_region(cmd_buf, sh_list);
                    region_cleared[i] = true;
                }

                Shadow::Params uniform_params = {};
                uniform_params.uShadowViewProjMatrix = shadow_regions_.data[i].clip_from_world;
                vkCmdPushConstants(cmd_buf, pi_solid_.layout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(Shadow::Params),
                                   &uniform_params);

                for (uint32_t j = sh_list.shadow_batch_start;
                     j < sh_list.shadow_batch_start + sh_list.shadow_batch_count; j++) {
                    const DepthDrawBatch &batch = shadow_batches_.data[shadow_batch_indices_.data[j]];
                    if (!batch.instance_count || !batch.alpha_test_bit || batch.type_bits != DepthDrawBatch::TypeVege) {
                        continue;
                    }

                    const uint32_t descr_id = batch.instance_indices[0][1] / materials_per_descriptor;
                    if (descr_id != bound_descr_id) {
                        vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_vege_transp_.layout(), 1,
                                                1, &(*bindless_tex_->textures_descr_sets)[descr_id], 0, nullptr);
                        bound_descr_id = descr_id;
                    }

                    vkCmdPushConstants(cmd_buf, pi_vege_transp_.layout(), VK_SHADER_STAGE_VERTEX_BIT,
                                       offsetof(Shadow::Params, uInstanceIndices),
                                       batch.instance_count * sizeof(Ren::Vec2i), &batch.instance_indices[0][0]);
                    vkCmdDrawIndexed(cmd_buf, batch.indices_count, // index count
                                     batch.instance_count,         // instance count
                                     batch.indices_offset,         // first index
                                     batch.base_vertex,            // vertex offset
                                     0);                           // first instance
                    ++draw_calls_count;
                }
            }
        }

        vkCmdEndRenderPass(cmd_buf);
    }

    (void)draw_calls_count;
}
