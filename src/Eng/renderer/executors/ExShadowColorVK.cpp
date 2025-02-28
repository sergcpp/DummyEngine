#include "ExShadowColor.h"

#include <Ren/Context.h>
#include <Ren/DebugMarker.h>
#include <Ren/DescriptorPool.h>
#include <Ren/DrawCall.h>
#include <Ren/RastState.h>
#include <Ren/VKCtx.h>

#include "../Renderer_Structs.h"
#include "../shaders/shadow_interface.h"

namespace ExSharedInternal {
uint32_t _draw_range(Ren::ApiContext *api_ctx, VkCommandBuffer cmd_buf, Ren::Span<const uint32_t> batch_indices,
                     Ren::Span<const Eng::BasicDrawBatch> batches, uint32_t i, uint64_t mask, int *draws_count);
uint32_t _draw_range_ext(Ren::ApiContext *api_ctx, VkCommandBuffer cmd_buf, const Ren::Pipeline &pipeline,
                         Ren::Span<const uint32_t> batch_indices, Ren::Span<const Eng::BasicDrawBatch> batches,
                         uint32_t i, uint64_t mask, uint32_t materials_per_descriptor,
                         Ren::Span<const VkDescriptorSet> descr_sets, int *draws_count);
} // namespace ExSharedInternal

namespace ExShadowColorInternal {
void _adjust_bias_and_viewport(Ren::ApiContext *api_ctx, VkCommandBuffer cmd_buf, const Eng::ShadowList &sh_list) {
    const VkViewport viewport = {
        float(sh_list.shadow_map_pos[0]),
        float((Eng::SHADOWMAP_RES / 2) - sh_list.shadow_map_pos[1] - sh_list.shadow_map_size[1]),
        float(sh_list.shadow_map_size[0]),
        float(sh_list.shadow_map_size[1]),
        0.0f,
        1.0f};
    api_ctx->vkCmdSetViewport(cmd_buf, 0, 1, &viewport);

    const VkRect2D scissor = {{sh_list.scissor_test_pos[0],
                               (Eng::SHADOWMAP_RES / 2) - sh_list.scissor_test_pos[1] - sh_list.scissor_test_size[1]},
                              {uint32_t(sh_list.scissor_test_size[0]), uint32_t(sh_list.scissor_test_size[1])}};
    api_ctx->vkCmdSetScissor(cmd_buf, 0, 1, &scissor);

    api_ctx->vkCmdSetDepthBias(cmd_buf, sh_list.bias[1], 0.0f, sh_list.bias[0]);
}

void _clear_region(Ren::ApiContext *api_ctx, VkCommandBuffer cmd_buf, const Eng::ShadowList &sh_list) {
    VkClearAttachment clear_att = {};
    clear_att.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    clear_att.clearValue.color = {1.0f, 1.0f, 1.0f, 0.0f};

    VkClearRect clear_rect = {};
    clear_rect.rect.offset = {sh_list.scissor_test_pos[0],
                              (Eng::SHADOWMAP_RES / 2) - sh_list.scissor_test_pos[1] - sh_list.scissor_test_size[1]};
    clear_rect.rect.extent = {uint32_t(sh_list.scissor_test_size[0]), uint32_t(sh_list.scissor_test_size[1])};
    clear_rect.baseArrayLayer = 0;
    clear_rect.layerCount = 1;

    api_ctx->vkCmdClearAttachments(cmd_buf, 1, &clear_att, 1, &clear_rect);
}
} // namespace ExShadowColorInternal

void Eng::ExShadowColor::DrawShadowMaps(FgBuilder &builder) {
    using namespace ExSharedInternal;
    using namespace ExShadowColorInternal;

    using BDB = BasicDrawBatch;

    FgAllocBuf &unif_shared_data_buf = builder.GetReadBuffer(shared_data_buf_);
    FgAllocBuf &instances_buf = builder.GetReadBuffer(instances_buf_);
    FgAllocBuf &instance_indices_buf = builder.GetReadBuffer(instance_indices_buf_);
    FgAllocBuf &materials_buf = builder.GetReadBuffer(materials_buf_);
    FgAllocTex &noise_tex = builder.GetReadTexture(noise_tex_);

    Ren::Context &ctx = builder.ctx();
    Ren::ApiContext *api_ctx = ctx.api_ctx();

    VkCommandBuffer cmd_buf = api_ctx->draw_cmd_buf[api_ctx->backend_frame];

    VkDescriptorSetLayout simple_descr_set_layout = pi_solid_[0]->prog()->descr_set_layouts()[0];
    VkDescriptorSet simple_descr_sets[2];
    { // allocate descriptor sets
        const Ren::Binding bindings[] = {{Ren::eBindTarget::UTBuf, BIND_INST_BUF, *instances_buf.tbos[0]},
                                         {Ren::eBindTarget::SBufRO, BIND_INST_NDX_BUF, *instance_indices_buf.ref},
                                         {Ren::eBindTarget::SBufRO, BIND_MATERIALS_BUF, *materials_buf.ref}};
        simple_descr_sets[0] =
            PrepareDescriptorSet(api_ctx, simple_descr_set_layout, bindings, ctx.default_descr_alloc(), ctx.log());
        simple_descr_sets[1] = bindless_tex_->textures_descr_sets[0];
    }

    /*VkDescriptorSetLayout vege_descr_set_layout = pi_vege_solid_->prog()->descr_set_layouts()[0];
    VkDescriptorSet vege_descr_sets[2];
    { // allocate descriptor sets
        const Ren::Binding bindings[] = {{Ren::eBindTarget::UBuf, BIND_UB_SHARED_DATA_BUF, *unif_shared_data_buf.ref},
                                         {Ren::eBindTarget::UTBuf, BIND_INST_BUF, *instances_buf.tbos[0]},
                                         {Ren::eBindTarget::SBufRO, BIND_INST_NDX_BUF, *instance_indices_buf.ref},
                                         {Ren::eBindTarget::SBufRO, BIND_MATERIALS_BUF, *materials_buf.ref},
                                         {Ren::eBindTarget::Tex2DSampled, BIND_NOISE_TEX, *noise_tex.ref}};
        vege_descr_sets[0] =
            PrepareDescriptorSet(api_ctx, vege_descr_set_layout, bindings, ctx.default_descr_alloc(), ctx.log());
        vege_descr_sets[1] = bindless_tex_->textures_descr_sets[0];
    }*/

    bool region_cleared[MAX_SHADOWMAPS_TOTAL] = {};
    [[maybe_unused]] int draw_calls_count = 0;

    const uint32_t materials_per_descriptor = api_ctx->max_combined_image_samplers / MAX_TEX_PER_MATERIAL;

    VkRenderPassBeginInfo rp_begin_info = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    rp_begin_info.renderPass = pi_solid_[0]->render_pass()->vk_handle();
    rp_begin_info.framebuffer = shadow_fb_.vk_handle();
    rp_begin_info.renderArea = {{0, 0}, {uint32_t(w_), uint32_t(h_)}};
    api_ctx->vkCmdBeginRenderPass(cmd_buf, &rp_begin_info, VK_SUBPASS_CONTENTS_INLINE);

    Ren::SmallVector<uint32_t, 32> batch_points((*p_list_)->shadow_lists.count, 0);

    { // opaque objects
        Ren::DebugMarker _(api_ctx, ctx.current_cmd_buf(), "STATIC-SOLID");

        api_ctx->vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_solid_[0]->layout(), 0, 2,
                                         simple_descr_sets, 0, nullptr);

        pi_solid_[0]->vtx_input()->BindBuffers(api_ctx, cmd_buf, 0, VK_INDEX_TYPE_UINT32);

        static const uint64_t BitFlags[] = {BDB::BitAlphaBlend, BDB::BitAlphaBlend | BDB::BitBackSided,
                                            BDB::BitAlphaBlend | BDB::BitTwoSided};
        for (int pi = 0; pi < 3; ++pi) {
            api_ctx->vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_solid_[pi]->handle());
            for (int i = 0; i < int((*p_list_)->shadow_lists.count); ++i) {
                const ShadowList &sh_list = (*p_list_)->shadow_lists.data[i];
                if (!sh_list.dirty && sh_list.alpha_blend_start_index == -1) {
                    continue;
                }

                _adjust_bias_and_viewport(api_ctx, cmd_buf, sh_list);

                if (!std::exchange(region_cleared[i], true)) {
                    _clear_region(api_ctx, cmd_buf, sh_list);
                }

                if (sh_list.alpha_blend_start_index == -1) {
                    continue;
                }

                Shadow::Params uniform_params = {};
                uniform_params.g_shadow_view_proj_mat = (*p_list_)->shadow_regions.data[i].clip_from_world;
                api_ctx->vkCmdPushConstants(cmd_buf, pi_solid_[pi]->layout(), VK_SHADER_STAGE_VERTEX_BIT, 0,
                                            sizeof(Shadow::Params), &uniform_params);

                Ren::Span<const uint32_t> batch_indices = {
                    (*p_list_)->shadow_batch_indices.data() + sh_list.alpha_blend_start_index,
                    sh_list.shadow_batch_count - (sh_list.alpha_blend_start_index - sh_list.shadow_batch_start)};

                uint32_t j = batch_points[i];
                j = _draw_range_ext(api_ctx, cmd_buf, *pi_alpha_[pi], batch_indices, (*p_list_)->shadow_batches, j,
                                    BitFlags[pi], materials_per_descriptor, bindless_tex_->textures_descr_sets,
                                    &draw_calls_count);
                batch_points[i] = j;
            }
        }
    }

    { // alpha-tested objects
        Ren::DebugMarker _(api_ctx, ctx.current_cmd_buf(), "STATIC-ALPHA");

        api_ctx->vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_alpha_[0]->layout(), 0, 2,
                                         simple_descr_sets, 0, nullptr);

        pi_alpha_[0]->vtx_input()->BindBuffers(api_ctx, cmd_buf, 0, VK_INDEX_TYPE_UINT32);

        static const uint64_t BitFlags[] = {BDB::BitAlphaBlend | BDB::BitAlphaTest,
                                            BDB::BitAlphaBlend | BDB::BitAlphaTest | BDB::BitBackSided,
                                            BDB::BitAlphaBlend | BDB::BitAlphaTest | BDB::BitTwoSided};
        for (int pi = 0; pi < 3; ++pi) {
            api_ctx->vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_alpha_[pi]->handle());
            for (int i = 0; i < int((*p_list_)->shadow_lists.count); ++i) {
                const ShadowList &sh_list = (*p_list_)->shadow_lists.data[i];
                if (!sh_list.dirty && sh_list.alpha_blend_start_index == -1) {
                    continue;
                }

                _adjust_bias_and_viewport(api_ctx, cmd_buf, sh_list);

                if (!std::exchange(region_cleared[i], true)) {
                    _clear_region(api_ctx, cmd_buf, sh_list);
                }

                if (sh_list.alpha_blend_start_index == -1) {
                    continue;
                }

                Shadow::Params uniform_params = {};
                uniform_params.g_shadow_view_proj_mat = (*p_list_)->shadow_regions.data[i].clip_from_world;
                api_ctx->vkCmdPushConstants(cmd_buf, pi_alpha_[pi]->layout(), VK_SHADER_STAGE_VERTEX_BIT, 0,
                                            sizeof(Shadow::Params), &uniform_params);

                Ren::Span<const uint32_t> batch_indices = {
                    (*p_list_)->shadow_batch_indices.data() + sh_list.alpha_blend_start_index,
                    sh_list.shadow_batch_count - (sh_list.alpha_blend_start_index - sh_list.shadow_batch_start)};

                uint32_t j = batch_points[i];
                j = _draw_range_ext(api_ctx, cmd_buf, *pi_alpha_[pi], batch_indices, (*p_list_)->shadow_batches, j,
                                    BitFlags[pi], materials_per_descriptor, bindless_tex_->textures_descr_sets,
                                    &draw_calls_count);
                batch_points[i] = j;
            }
        }
    }

    api_ctx->vkCmdEndRenderPass(cmd_buf);
}
