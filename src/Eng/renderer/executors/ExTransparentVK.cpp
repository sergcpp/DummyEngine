#include "ExTransparent.h"

#include "../PrimDraw.h"
#include "../Renderer_Structs.h"

#include <Ren/Context.h>
#include <Ren/DebugMarker.h>
#include <Ren/RastState.h>
#include <Ren/VKCtx.h>

void Eng::ExTransparent::DrawTransparent_Simple(FgBuilder &builder, FgAllocBuf &instances_buf,
                                                FgAllocBuf &instance_indices_buf, FgAllocBuf &unif_shared_data_buf,
                                                FgAllocBuf &materials_buf, FgAllocBuf &cells_buf, FgAllocBuf &items_buf,
                                                FgAllocBuf &lights_buf, FgAllocBuf &decals_buf, FgAllocTex &shad_tex,
                                                FgAllocTex &color_tex, FgAllocTex &ssao_tex) {
    auto &ctx = builder.ctx();
    auto *api_ctx = ctx.api_ctx();

    [[maybe_unused]] FgAllocTex &brdf_lut = builder.GetReadTexture(brdf_lut_);
    FgAllocTex &noise_tex = builder.GetReadTexture(noise_tex_);
    [[maybe_unused]] FgAllocTex &cone_rt_lut = builder.GetReadTexture(cone_rt_lut_);
    FgAllocTex &dummy_black = builder.GetReadTexture(dummy_black_);

    FgAllocTex *lm_tex[4];
    for (int i = 0; i < 4; ++i) {
        if (lm_tex_[i]) {
            lm_tex[i] = &builder.GetReadTexture(lm_tex_[i]);
        } else {
            lm_tex[i] = &dummy_black;
        }
    }

    if (/*!(*p_list_)->probe_storage ||*/ (*p_list_)->alpha_blend_start_index == -1) {
        return;
    }

    Ren::DescrSizes descr_sizes;
    descr_sizes.img_sampler_count = 10;
    descr_sizes.ubuf_count = 1;
    descr_sizes.utbuf_count = 4;
    descr_sizes.sbuf_count = 3;
    const VkDescriptorSet res_descr_set = ctx.default_descr_alloc()->Alloc(descr_sizes, descr_set_layout_);

    { // update descriptor set
        const VkDescriptorImageInfo shad_info = shad_tex.ref->vk_desc_image_info();
        VkDescriptorImageInfo lm_infos[4];

        if ((*p_list_)->render_settings.enable_lightmap && (*p_list_)->env.lm_direct) {
            for (int sh_l = 0; sh_l < 4; sh_l++) {
                lm_infos[sh_l] = lm_tex[sh_l]->ref->vk_desc_image_info();
            }
        } else {
            for (int sh_l = 0; sh_l < 4; sh_l++) {
                lm_infos[sh_l] = dummy_black.ref->vk_desc_image_info();
            }
        }

        const VkDescriptorImageInfo decal_info = dummy_black.ref->vk_desc_image_info();
        const VkDescriptorImageInfo ssao_info = ssao_tex.ref->vk_desc_image_info();

        const VkDescriptorImageInfo noise_info = noise_tex.ref->vk_desc_image_info();
        /*const VkDescriptorImageInfo env_info = {(*p_list_)->probe_storage->handle().sampler,
                                                (*p_list_)->probe_storage->handle().views[0],
                                                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};*/
        // const VkDescriptorImageInfo cone_rt_info = cone_rt_lut.ref->vk_desc_image_info();
        // const VkDescriptorImageInfo brdf_info = brdf_lut.ref->vk_desc_image_info();

        const VkBufferView lights_buf_view = lights_buf.ref->view(0).second;
        const VkBufferView decals_buf_view = decals_buf.ref->view(0).second;
        const VkBufferView cells_buf_view = cells_buf.ref->view(0).second;
        const VkBufferView items_buf_view = items_buf.ref->view(0).second;

        const VkDescriptorBufferInfo ubuf_info = {unif_shared_data_buf.ref->vk_handle(), 0, VK_WHOLE_SIZE};

        const VkBufferView instances_buf_view = instances_buf.ref->view(0).second;
        const VkDescriptorBufferInfo instance_indices_buf_info = {instance_indices_buf.ref->vk_handle(), 0,
                                                                  VK_WHOLE_SIZE};
        const VkDescriptorBufferInfo mat_buf_info = {materials_buf.ref->vk_handle(), 0, VK_WHOLE_SIZE};

        Ren::SmallVector<VkWriteDescriptorSet, 16> descr_writes;

        { // shadow map
            auto &descr_write = descr_writes.emplace_back();
            descr_write = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            descr_write.dstSet = res_descr_set;
            descr_write.dstBinding = BIND_SHAD_TEX;
            descr_write.dstArrayElement = 0;
            descr_write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            descr_write.descriptorCount = 1;
            descr_write.pImageInfo = &shad_info;
        }
        { // lightmap
            auto &descr_write = descr_writes.emplace_back();
            descr_write = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            descr_write.dstSet = res_descr_set;
            descr_write.dstBinding = BIND_LMAP_SH;
            descr_write.dstArrayElement = 0;
            descr_write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            descr_write.descriptorCount = 4;
            descr_write.pImageInfo = lm_infos;
        }
        { // decals tex
            auto &descr_write = descr_writes.emplace_back();
            descr_write = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            descr_write.dstSet = res_descr_set;
            descr_write.dstBinding = BIND_DECAL_TEX;
            descr_write.dstArrayElement = 0;
            descr_write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            descr_write.descriptorCount = 1;
            descr_write.pImageInfo = &decal_info;
        }
        { // ssao tex
            auto &descr_write = descr_writes.emplace_back();
            descr_write = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            descr_write.dstSet = res_descr_set;
            descr_write.dstBinding = BIND_SSAO_TEX_SLOT;
            descr_write.dstArrayElement = 0;
            descr_write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            descr_write.descriptorCount = 1;
            descr_write.pImageInfo = &ssao_info;
        }
        { // noise tex
            auto &descr_write = descr_writes.emplace_back();
            descr_write = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            descr_write.dstSet = res_descr_set;
            descr_write.dstBinding = BIND_NOISE_TEX;
            descr_write.dstArrayElement = 0;
            descr_write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            descr_write.descriptorCount = 1;
            descr_write.pImageInfo = &noise_info;
        }
        /*{ // env tex
            auto &descr_write = descr_writes.emplace_back();
            descr_write = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            descr_write.dstSet = res_descr_set;
            descr_write.dstBinding = BIND_ENV_TEX;
            descr_write.dstArrayElement = 0;
            descr_write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            descr_write.descriptorCount = 1;
            descr_write.pImageInfo = &env_info;
        }*/
        /*{ // cone rt lut
            auto &descr_write = descr_writes.emplace_back();
            descr_write = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            descr_write.dstSet = res_descr_set;
            descr_write.dstBinding = BIND_CONE_RT_LUT;
            descr_write.dstArrayElement = 0;
            descr_write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            descr_write.descriptorCount = 1;
            descr_write.pImageInfo = &cone_rt_info;
        }*/
        /*{ // brdf lut
            auto &descr_write = descr_writes.emplace_back();
            descr_write = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            descr_write.dstSet = res_descr_set;
            descr_write.dstBinding = BIND_BRDF_LUT;
            descr_write.dstArrayElement = 0;
            descr_write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            descr_write.descriptorCount = 1;
            descr_write.pImageInfo = &brdf_info;
        }*/
        { // lights tbuf
            auto &descr_write = descr_writes.emplace_back();
            descr_write = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            descr_write.dstSet = res_descr_set;
            descr_write.dstBinding = BIND_LIGHT_BUF;
            descr_write.dstArrayElement = 0;
            descr_write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
            descr_write.descriptorCount = 1;
            descr_write.pTexelBufferView = &lights_buf_view;
        }
        { // decals tbuf
            auto &descr_write = descr_writes.emplace_back();
            descr_write = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            descr_write.dstSet = res_descr_set;
            descr_write.dstBinding = BIND_DECAL_BUF;
            descr_write.dstArrayElement = 0;
            descr_write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
            descr_write.descriptorCount = 1;
            descr_write.pTexelBufferView = &decals_buf_view;
        }
        { // cells tbuf
            auto &descr_write = descr_writes.emplace_back();
            descr_write = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            descr_write.dstSet = res_descr_set;
            descr_write.dstBinding = BIND_CELLS_BUF;
            descr_write.dstArrayElement = 0;
            descr_write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
            descr_write.descriptorCount = 1;
            descr_write.pTexelBufferView = &cells_buf_view;
        }
        { // items tbuf
            auto &descr_write = descr_writes.emplace_back();
            descr_write = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            descr_write.dstSet = res_descr_set;
            descr_write.dstBinding = BIND_ITEMS_BUF;
            descr_write.dstArrayElement = 0;
            descr_write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
            descr_write.descriptorCount = 1;
            descr_write.pTexelBufferView = &items_buf_view;
        }
        { // instances tbuf
            auto &descr_write = descr_writes.emplace_back();
            descr_write = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            descr_write.dstSet = res_descr_set;
            descr_write.dstBinding = BIND_INST_BUF;
            descr_write.dstArrayElement = 0;
            descr_write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
            descr_write.descriptorCount = 1;
            descr_write.pTexelBufferView = &instances_buf_view;
        }
        { // instance indices sbuf
            auto &descr_write = descr_writes.emplace_back();
            descr_write = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            descr_write.dstSet = res_descr_set;
            descr_write.dstBinding = BIND_INST_NDX_BUF;
            descr_write.dstArrayElement = 0;
            descr_write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            descr_write.descriptorCount = 1;
            descr_write.pBufferInfo = &instance_indices_buf_info;
        }
        { // shared data ubuf
            auto &descr_write = descr_writes.emplace_back();
            descr_write = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            descr_write.dstSet = res_descr_set;
            descr_write.dstBinding = BIND_UB_SHARED_DATA_BUF;
            descr_write.dstArrayElement = 0;
            descr_write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            descr_write.descriptorCount = 1;
            descr_write.pBufferInfo = &ubuf_info;
        }
        { // materials sbuf
            auto &descr_write = descr_writes.emplace_back();
            descr_write = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            descr_write.dstSet = res_descr_set;
            descr_write.dstBinding = BIND_MATERIALS_BUF;
            descr_write.dstArrayElement = 0;
            descr_write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            descr_write.descriptorCount = 1;
            descr_write.pBufferInfo = &mat_buf_info;
        }

        api_ctx->vkUpdateDescriptorSets(api_ctx->device, descr_writes.size(), descr_writes.cdata(), 0, nullptr);
    }

    const auto &texture_descr_sets = bindless_tex_->textures_descr_sets;

    VkCommandBuffer cmd_buf = api_ctx->draw_cmd_buf[api_ctx->backend_frame];

    //
    // Setup viewport
    //
    const VkViewport viewport = {0.0f, 0.0f, float(view_state_->act_res[0]), float(view_state_->act_res[1]),
                                 0.0f, 1.0f};
    api_ctx->vkCmdSetViewport(cmd_buf, 0, 1, &viewport);
    const VkRect2D scissor = {{0, 0}, {uint32_t(view_state_->act_res[0]), uint32_t(view_state_->act_res[1])}};
    api_ctx->vkCmdSetScissor(cmd_buf, 0, 1, &scissor);

    const uint32_t materials_per_descriptor = api_ctx->max_combined_image_samplers / MAX_TEX_PER_MATERIAL;

    static backend_info_t backend_info;

    {
        uint64_t cur_pipe_id = 0xffffffffffffffff;
        uint64_t cur_mat_id = 0xffffffffffffffff;
        uint32_t bound_descr_id = 0xffffffff;

        VkRenderPassBeginInfo rp_begin_info = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
        rp_begin_info.renderPass = rp_transparent_->vk_handle();
        rp_begin_info.framebuffer = transparent_draw_fb_[ctx.backend_frame()][fb_to_use_].vk_handle();
        rp_begin_info.renderArea = {{0, 0}, {uint32_t(view_state_->scr_res[0]), uint32_t(view_state_->scr_res[1])}};
        api_ctx->vkCmdBeginRenderPass(cmd_buf, &rp_begin_info, VK_SUBPASS_CONTENTS_INLINE);

        draw_pass_vi_->BindBuffers(api_ctx, cmd_buf, 0, VK_INDEX_TYPE_UINT32);

        for (int j = int((*p_list_)->custom_batch_indices.size()) - 1; j >= (*p_list_)->alpha_blend_start_index; j--) {
            const auto &batch = (*p_list_)->custom_batches[(*p_list_)->custom_batch_indices[j]];
            if (!batch.alpha_blend_bit || !batch.two_sided_bit) {
                continue;
            }

            if (!batch.instance_count) {
                continue;
            }

            if (cur_mat_id != batch.mat_id) {
                const uint32_t pipe_id =
                    (*(*p_list_)->materials)[batch.mat_id].pipelines[int(eFwdPipeline::BackfaceDraw)].index();

                if (cur_pipe_id != pipe_id) {
                    api_ctx->vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines_[pipe_id].handle());
                    api_ctx->vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                                     pipelines_[pipe_id].layout(), 0, 1, &res_descr_set, 0, nullptr);
                    cur_pipe_id = pipe_id;
                    bound_descr_id = 0xffffffff;
                }

                cur_mat_id = batch.mat_id;
            }

            const uint32_t descr_id = batch.material_index / materials_per_descriptor;
            if (descr_id != bound_descr_id) {
                api_ctx->vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                                 pipelines_[batch.pipe_id].layout(), 1, 1,
                                                 &texture_descr_sets[descr_id], 0, nullptr);
                bound_descr_id = descr_id;
            }

            api_ctx->vkCmdDrawIndexed(cmd_buf, batch.indices_count, // index count
                                      batch.instance_count,         // instance count
                                      batch.indices_offset,         // first index
                                      batch.base_vertex,            // vertex offset
                                      batch.instance_start);        // first instance

            backend_info.opaque_draw_calls_count += 2;
            backend_info.tris_rendered += (batch.indices_count / 3) * batch.instance_count;
        }

        cur_mat_id = 0xffffffffffffffff;

        for (int j = int((*p_list_)->custom_batch_indices.size()) - 1; j >= (*p_list_)->alpha_blend_start_index; j--) {
            const auto &batch = (*p_list_)->custom_batches[(*p_list_)->custom_batch_indices[j]];
            if (!batch.instance_count) {
                continue;
            }

            if (cur_mat_id != batch.mat_id) {
                const uint32_t pipe_id =
                    (*(*p_list_)->materials)[batch.mat_id].pipelines[int(eFwdPipeline::FrontfaceDraw)].index();

                if (cur_pipe_id != pipe_id) {
                    api_ctx->vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines_[pipe_id].handle());
                    api_ctx->vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                                     pipelines_[pipe_id].layout(), 0, 1, &res_descr_set, 0, nullptr);
                    cur_pipe_id = pipe_id;
                    bound_descr_id = 0xffffffff;
                }

                cur_mat_id = batch.mat_id;
            }

            const uint32_t descr_id = batch.material_index / materials_per_descriptor;
            if (descr_id != bound_descr_id) {
                api_ctx->vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                                 pipelines_[batch.pipe_id].layout(), 1, 1,
                                                 &texture_descr_sets[descr_id], 0, nullptr);
                bound_descr_id = descr_id;
            }

            api_ctx->vkCmdDrawIndexed(cmd_buf, batch.indices_count, // index count
                                      batch.instance_count,         // instance count
                                      batch.indices_offset,         // first index
                                      batch.base_vertex,            // vertex offset
                                      batch.instance_start);        // first instance

            backend_info.opaque_draw_calls_count += 2;
            backend_info.tris_rendered += (batch.indices_count / 3) * batch.instance_count;
        }

        api_ctx->vkCmdEndRenderPass(cmd_buf);
    }
}

void Eng::ExTransparent::DrawTransparent_OIT_MomentBased(FgBuilder &builder) { assert(false && "Not implemented!"); }

void Eng::ExTransparent::DrawTransparent_OIT_WeightedBlended(FgBuilder &builder) {
    assert(false && "Not implemented!");
}

void Eng::ExTransparent::InitDescrSetLayout() {
    VkDescriptorSetLayoutBinding bindings[] = {
        // textures (10)
        {BIND_SHAD_TEX, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
        {BIND_LMAP_SH, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4, VK_SHADER_STAGE_FRAGMENT_BIT},
        {BIND_DECAL_TEX, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
        {BIND_SSAO_TEX_SLOT, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
        {BIND_NOISE_TEX, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_VERTEX_BIT},
        {BIND_ENV_TEX, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
        //{BIND_CONE_RT_LUT, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
        //{BIND_BRDF_LUT, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
        // texel buffers (4)
        {BIND_LIGHT_BUF, VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
        {BIND_DECAL_BUF, VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
        {BIND_CELLS_BUF, VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
        {BIND_ITEMS_BUF, VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
        {BIND_INST_BUF, VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT},
        // uniform buffers (1)
        {BIND_UB_SHARED_DATA_BUF, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1,
         VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT},
        // storage buffers (2)
        {BIND_MATERIALS_BUF, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT},
        {BIND_INST_NDX_BUF, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT}};

    VkDescriptorSetLayoutCreateInfo layout_info = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    layout_info.bindingCount = uint32_t(std::size(bindings));
    layout_info.pBindings = bindings;

    const VkResult res =
        api_ctx_->vkCreateDescriptorSetLayout(api_ctx_->device, &layout_info, nullptr, &descr_set_layout_);
    assert(res == VK_SUCCESS);
}

Eng::ExTransparent::~ExTransparent() {
    if (descr_set_layout_) {
        api_ctx_->vkDestroyDescriptorSetLayout(api_ctx_->device, descr_set_layout_, nullptr);
    }
}
