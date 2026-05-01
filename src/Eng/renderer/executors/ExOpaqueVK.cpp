#include "ExOpaque.h"

#include <Ren/Context.h>
#include <Ren/DebugMarker.h>
#include <Ren/DescriptorPool.h>
#include <Ren/Vk/VKCtx.h>
#include <Ren/utils/Span.h>

#include "../Renderer_DrawList.h"
#include "../framegraph/FgBuilder.h"

namespace ExSharedInternal {
uint32_t _draw_list_range_full(const Ren::ApiContext &api, VkCommandBuffer cmd_buf, VkDescriptorSet res_descr_set,
                               Ren::Span<const VkDescriptorSet> texture_descr_sets, const Ren::PipelineMain pipelines[],
                               Ren::Span<const Eng::custom_draw_batch_t> main_batches,
                               Ren::Span<const uint32_t> main_batch_indices, uint32_t i, uint64_t mask,
                               const uint32_t materials_per_descriptor, uint64_t &cur_pipe_id, uint32_t &bound_descr_id,
                               Eng::backend_info_t &backend_info) {
    for (; i < main_batch_indices.size(); i++) {
        const Eng::custom_draw_batch_t &batch = main_batches[main_batch_indices[i]];
        if ((batch.sort_key & Eng::custom_draw_batch_t::FlagBits) != mask) {
            break;
        }

        if (!batch.instance_count) {
            continue;
        }

        if (cur_pipe_id != batch.pipe_id) {
            api.vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines[batch.pipe_id].pipeline);
            api.vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines[batch.pipe_id].layout, 0, 1,
                                        &res_descr_set, 0, nullptr);
            cur_pipe_id = batch.pipe_id;
            bound_descr_id = 0xffffffff;
        }

        const uint32_t descr_id = batch.material_index / materials_per_descriptor;
        if (descr_id != bound_descr_id) {
            api.vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines[batch.pipe_id].layout, 1, 1,
                                        &texture_descr_sets[descr_id], 0, nullptr);
            bound_descr_id = descr_id;
        }

        api.vkCmdDrawIndexed(cmd_buf, batch.indices_count, // index count
                             batch.instance_count,         // instance count
                             batch.indices_offset,         // first index
                             batch.base_vertex,            // vertex offset
                             batch.instance_start);        // first instance

        backend_info.opaque_draw_calls_count++;
        backend_info.tris_rendered += (batch.indices_count / 3) * batch.instance_count;
    }

    return i;
}

uint32_t _draw_list_range_full_rev(const Ren::ApiContext &api, VkCommandBuffer cmd_buf, VkDescriptorSet res_descr_set,
                                   Ren::Span<const VkDescriptorSet> texture_descr_sets,
                                   const Ren::PipelineMain pipelines[],
                                   Ren::Span<const Eng::custom_draw_batch_t> main_batches,
                                   Ren::Span<const uint32_t> main_batch_indices, uint32_t ndx, uint64_t mask,
                                   const uint32_t materials_per_descriptor, uint64_t &cur_pipe_id,
                                   uint32_t &bound_descr_id, Eng::backend_info_t &backend_info) {
    int i = int(ndx);
    for (; i >= 0; i--) {
        const Eng::custom_draw_batch_t &batch = main_batches[main_batch_indices[i]];
        if ((batch.sort_key & Eng::custom_draw_batch_t::FlagBits) != mask) {
            break;
        }

        if (!batch.instance_count) {
            continue;
        }

        if (cur_pipe_id != batch.pipe_id) {
            api.vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines[batch.pipe_id].pipeline);
            api.vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines[batch.pipe_id].layout, 0, 1,
                                        &res_descr_set, 0, nullptr);
            cur_pipe_id = batch.pipe_id;
            bound_descr_id = 0xffffffff;
        }

        const uint32_t descr_id = batch.material_index / materials_per_descriptor;
        if (descr_id != bound_descr_id) {
            api.vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines[batch.pipe_id].layout, 1, 1,
                                        &texture_descr_sets[descr_id], 0, nullptr);
            bound_descr_id = descr_id;
        }

        api.vkCmdDrawIndexed(cmd_buf, batch.indices_count, // index count
                             batch.instance_count,         // instance count
                             batch.indices_offset,         // first index
                             batch.base_vertex,            // vertex offset
                             batch.instance_start);        // first instance

        backend_info.opaque_draw_calls_count++;
        backend_info.tris_rendered += (batch.indices_count / 3) * batch.instance_count;
    }

    return uint32_t(i);
}
} // namespace ExSharedInternal

void Eng::ExOpaque::DrawOpaque(const FgContext &fg, const Ren::ImageRWHandle color, const Ren::ImageRWHandle normal,
                               const Ren::ImageRWHandle spec, const Ren::ImageRWHandle depth) {
    using namespace ExSharedInternal;

    const Ren::ApiContext &api = fg.ren_ctx().api();
    const Ren::StoragesRef &storages = fg.storages();

    //
    // Prepare descriptor sets
    //
    const Ren::BufferROHandle attrib_bufs[] = {fg.AccessROBuffer(args_->vtx_buf1), fg.AccessROBuffer(args_->vtx_buf2)};
    const Ren::BufferROHandle ndx_buf = fg.AccessROBuffer(args_->ndx_buf);

    const Ren::BufferROHandle instances = fg.AccessROBuffer(args_->instances);
    const Ren::BufferROHandle instance_indices = fg.AccessROBuffer(args_->instance_indices);
    const Ren::BufferROHandle unif_shared_data = fg.AccessROBuffer(args_->shared_data);
    const Ren::BufferROHandle materials = fg.AccessROBuffer(args_->materials);
    const Ren::BufferROHandle cells = fg.AccessROBuffer(args_->cells);
    const Ren::BufferROHandle items = fg.AccessROBuffer(args_->items);
    const Ren::BufferROHandle lights = fg.AccessROBuffer(args_->lights);
    const Ren::BufferROHandle decals = fg.AccessROBuffer(args_->decals);

    const Ren::ImageROHandle shadow_depth = fg.AccessROImage(args_->shadow_depth);
    [[maybe_unused]] const Ren::ImageROHandle brdf_lut = fg.AccessROImage(args_->brdf_lut);
    const Ren::ImageROHandle noise = fg.AccessROImage(args_->noise);
    [[maybe_unused]] const Ren::ImageROHandle cone_rt_lut = fg.AccessROImage(args_->cone_rt_lut);

    const Ren::ImageROHandle dummy_black = fg.AccessROImage(args_->dummy_black);
    const Ren::ImageROHandle ssao = fg.AccessROImage(args_->ssao);

    /*const Ren::Image *lm_tex[4];
    for (int i = 0; i < 4; ++i) {
        if (args_->lm_tex[i]) {
            lm_tex[i] = &fg.AccessROImage(args_->lm_tex[i]);
        } else {
            // lm_tex[i] = &dummy_black;
        }
    }*/

    // if (!(*p_list_)->probe_storage) {
    //     return;
    // }

    Ren::DescrSizes descr_sizes;
    descr_sizes.img_sampler_count = 10;
    descr_sizes.ubuf_count = 1;
    descr_sizes.utbuf_count = 4;
    descr_sizes.sbuf_count = 3;
    const VkDescriptorSet res_descr_set = fg.descr_alloc().Alloc(descr_sizes, descr_set_layout_);

    { // update descriptor set
        const Ren::ImageMain &dummy_black_main = storages.images[dummy_black].first;
        const Ren::ImageMain &ssao_main = storages.images[ssao].first;
        const Ren::ImageMain &shadow_depth_main = storages.images[shadow_depth].first;

        const VkDescriptorImageInfo shad_info = Image_GetDescriptorImageInfo(api, shadow_depth_main);
        VkDescriptorImageInfo lm_infos[4];
        for (int sh_l = 0; sh_l < 4; sh_l++) {
            // lm_infos[sh_l] = lm_tex[sh_l]->vk_desc_image_info();
            lm_infos[sh_l] = Image_GetDescriptorImageInfo(api, dummy_black_main);
        }

        const VkDescriptorImageInfo decal_info = (*p_list_)->decals_atlas
                                                     ? (*p_list_)->decals_atlas->vk_desc_image_info()
                                                     : Image_GetDescriptorImageInfo(api, dummy_black_main);
        const VkDescriptorImageInfo ssao_info = Image_GetDescriptorImageInfo(api, ssao_main);

        const Ren::ImageMain &noise_main = storages.images[noise].first;
        const VkDescriptorImageInfo noise_info = Image_GetDescriptorImageInfo(api, noise_main);
        /*const VkDescriptorImageInfo env_info = {(*p_list_)->probe_storage->handle().sampler,
                                                (*p_list_)->probe_storage->handle().views[0],
                                                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};*/
        // const VkDescriptorImageInfo cone_rt_info = cone_rt_lut.vk_desc_image_info();
        // const VkDescriptorImageInfo brdf_info = brdf_lut.vk_desc_image_info();

        const VkBufferView lights_buf_view = storages.buffers[lights].first.views[0].second;
        const VkBufferView decals_buf_view = storages.buffers[decals].first.views[0].second;
        const VkBufferView cells_buf_view = storages.buffers[cells].first.views[0].second;
        const VkBufferView items_buf_view = storages.buffers[items].first.views[0].second;

        const Ren::BufferMain &unif_shared_data_buf_main = storages.buffers[unif_shared_data].first;
        const VkDescriptorBufferInfo ubuf_info = {unif_shared_data_buf_main.buf, 0, VK_WHOLE_SIZE};

        const Ren::BufferMain &instances_buf_main = storages.buffers[instances].first;
        const VkBufferView instances_buf_view = instances_buf_main.views[0].second;
        const Ren::BufferMain &instance_indices_buf_main = storages.buffers[instance_indices].first;
        const VkDescriptorBufferInfo instance_indices_buf_info = {instance_indices_buf_main.buf, 0, VK_WHOLE_SIZE};
        const Ren::BufferMain &materials_buf_main = storages.buffers[materials].first;
        const VkDescriptorBufferInfo mat_buf_info = {materials_buf_main.buf, 0, VK_WHOLE_SIZE};

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

        api.vkUpdateDescriptorSets(api.device, descr_writes.size(), descr_writes.cdata(), 0, nullptr);
    }

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

    Ren::Span<const custom_draw_batch_t> batches = {(*p_list_)->custom_batches};
    Ren::Span<const uint32_t> batch_indices = {(*p_list_)->custom_batch_indices};

    static backend_info_t _dummy = {};

    { // actual drawing
        using CDB = custom_draw_batch_t;

        uint64_t cur_pipe_id = 0xffffffffffffffff;
        uint32_t bound_descr_id = 0xffffffff;

        uint32_t i = 0;

        const Ren::ImageRWHandle color_targets[] = {color, normal, spec};
        const Ren::FramebufferHandle fb = fg.FindOrCreateFramebuffer(rp_opaque_, depth, depth, color_targets);

        VkRenderPassBeginInfo rp_begin_info = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
        rp_begin_info.renderPass = storages.render_passes[rp_opaque_].handle;
        rp_begin_info.framebuffer = storages.framebuffers[fb].first.handle;
        rp_begin_info.renderArea = {{0, 0}, {uint32_t(view_state_->ren_res[0]), uint32_t(view_state_->ren_res[1])}};
        api.vkCmdBeginRenderPass(cmd_buf, &rp_begin_info, VK_SUBPASS_CONTENTS_INLINE);

        const Ren::VertexInput &vi_main = storages.vtx_inputs[draw_pass_vi_];
        VertexInput_BindBuffers(api, vi_main, storages.buffers, attrib_bufs, ndx_buf, cmd_buf, 0, VK_INDEX_TYPE_UINT32);

        { // one-sided1
            Ren::DebugMarker _m(api, cmd_buf, "ONE-SIDED-1");
            i = _draw_list_range_full(api, cmd_buf, res_descr_set, bindless_tex_->textures_descr_sets,
                                      storages.pipelines.data_main(), batches, batch_indices, i, 0ull,
                                      materials_per_descriptor, cur_pipe_id, bound_descr_id, _dummy);
        }

        { // two-sided1
            Ren::DebugMarker _m(api, cmd_buf, "TWO-SIDED-1");
            i = _draw_list_range_full(api, cmd_buf, res_descr_set, bindless_tex_->textures_descr_sets,
                                      storages.pipelines.data_main(), batches, batch_indices, i, CDB::BitTwoSided,
                                      materials_per_descriptor, cur_pipe_id, bound_descr_id, _dummy);
        }

        { // one-sided2
            Ren::DebugMarker _m(api, cmd_buf, "ONE-SIDED-2");
            i = _draw_list_range_full(api, cmd_buf, res_descr_set, bindless_tex_->textures_descr_sets,
                                      storages.pipelines.data_main(), batches, batch_indices, i, CDB::BitAlphaTest,
                                      materials_per_descriptor, cur_pipe_id, bound_descr_id, _dummy);
        }

        { // two-sided2
            Ren::DebugMarker _m(api, cmd_buf, "TWO-SIDED-2");
            i = _draw_list_range_full(api, cmd_buf, res_descr_set, bindless_tex_->textures_descr_sets,
                                      storages.pipelines.data_main(), batches, batch_indices, i,
                                      CDB::BitAlphaTest | CDB::BitTwoSided, materials_per_descriptor, cur_pipe_id,
                                      bound_descr_id, _dummy);
        }

        { // two-sided-tested-blended
            Ren::DebugMarker _m(api, cmd_buf, "TWO-SIDED-TESTED-BLENDED");
            i = _draw_list_range_full_rev(api, cmd_buf, res_descr_set, bindless_tex_->textures_descr_sets,
                                          storages.pipelines.data_main(), batches, batch_indices,
                                          uint32_t(batch_indices.size() - 1),
                                          CDB::BitAlphaBlend | CDB::BitAlphaTest | CDB::BitTwoSided,
                                          materials_per_descriptor, cur_pipe_id, bound_descr_id, _dummy);
        }

        { // one-sided-tested-blended
            Ren::DebugMarker _m(api, cmd_buf, "ONE-SIDED-TESTED-BLENDED");
            _draw_list_range_full_rev(api, cmd_buf, res_descr_set, bindless_tex_->textures_descr_sets,
                                      storages.pipelines.data_main(), batches, batch_indices, i,
                                      CDB::BitAlphaBlend | CDB::BitAlphaTest, materials_per_descriptor, cur_pipe_id,
                                      bound_descr_id, _dummy);
        }

        api.vkCmdEndRenderPass(cmd_buf);
    }
}

void Eng::ExOpaque::InitDescrSetLayout() {
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

    const VkResult res = api_.vkCreateDescriptorSetLayout(api_.device, &layout_info, nullptr, &descr_set_layout_);
    assert(res == VK_SUCCESS);
}

Eng::ExOpaque::~ExOpaque() {
    if (descr_set_layout_ != VK_NULL_HANDLE) {
        api_.vkDestroyDescriptorSetLayout(api_.device, descr_set_layout_, nullptr);
    }
}
