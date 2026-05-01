#include "ExSkinning.h"

#include <Ren/Buffer.h>
#include <Ren/Context.h>
#include <Ren/DescriptorPool.h>
#include <Ren/Vk/VKCtx.h>

#include "../Renderer_Structs.h"
#include "../framegraph/FgBuilder.h"
#include "../shaders/skinning_interface.h"

void Eng::ExSkinning::Execute(const FgContext &fg) {
    LazyInit(fg);

    const Ren::BufferROHandle skin_vtx = fg.AccessROBuffer(skin_vtx_);
    const Ren::BufferROHandle skin_transforms = fg.AccessROBuffer(skin_transforms_);
    const Ren::BufferROHandle shape_keys = fg.AccessROBuffer(shape_keys_);
    const Ren::BufferROHandle delta = fg.AccessROBuffer(delta_);

    const Ren::BufferHandle vtx_buf1 = fg.AccessRWBuffer(vtx_buf1_);
    const Ren::BufferHandle vtx_buf2 = fg.AccessRWBuffer(vtx_buf2_);

    const Ren::ApiContext &api = fg.ren_ctx().api();
    const Ren::StoragesRef &storages = fg.storages();

    const Ren::PipelineMain &pi = storages.pipelines[pi_skinning_].first;
    const Ren::ProgramMain &pr = storages.programs[pi.prog].first;

    if (!p_list_->skin_regions.empty()) {
        VkCommandBuffer cmd_buf = api.draw_cmd_buf[api.backend_frame];

        VkDescriptorSetLayout descr_set_layout = pr.descr_set_layouts[0];
        Ren::DescrSizes descr_sizes;
        descr_sizes.sbuf_count = 6;
        VkDescriptorSet descr_set = fg.descr_alloc().Alloc(descr_sizes, descr_set_layout);

        const Ren::BufferMain &skin_vtx_main = storages.buffers[skin_vtx].first;
        const Ren::BufferMain &skin_transforms_main = storages.buffers[skin_transforms].first;
        const Ren::BufferMain &shape_keys_main = storages.buffers[shape_keys].first;
        const Ren::BufferMain &delta_main = storages.buffers[delta].first;

        const Ren::BufferMain &vtx_buf1_main = storages.buffers[vtx_buf1].first;
        const Ren::BufferMain &vtx_buf2_main = storages.buffers[vtx_buf2].first;

        { // update descriptor set
            const VkDescriptorBufferInfo buf_infos[6] = {
                {skin_vtx_main.buf, 0, VK_WHOLE_SIZE},        // input vertices binding
                {skin_transforms_main.buf, 0, VK_WHOLE_SIZE}, // input matrices binding
                {shape_keys_main.buf, 0, VK_WHOLE_SIZE},      // input shape keys binding
                {delta_main.buf, 0, VK_WHOLE_SIZE},           // input vertex deltas binding
                {vtx_buf1_main.buf, 0, VK_WHOLE_SIZE},        // output vertices0 binding
                {vtx_buf2_main.buf, 0, VK_WHOLE_SIZE}         // output vertices1 binding
            };

            VkWriteDescriptorSet descr_write = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            descr_write.dstSet = descr_set;
            descr_write.dstBinding = 0;
            descr_write.dstArrayElement = 0;
            descr_write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            descr_write.descriptorCount = 6;
            descr_write.pBufferInfo = buf_infos;

            api.vkUpdateDescriptorSets(api.device, 1, &descr_write, 0, nullptr);
        }

        api.vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE, pi.pipeline);
        api.vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE, pi.layout, 0, 1, &descr_set, 0, nullptr);

        for (uint32_t i = 0; i < uint32_t(p_list_->skin_regions.size()); i++) {
            const skin_region_t &sr = p_list_->skin_regions[i];

            const uint32_t non_shapekeyed_vertex_count = sr.vertex_count - sr.shape_keyed_vertex_count;

            if (non_shapekeyed_vertex_count) {
                Skinning::Params uniform_params;
                uniform_params.uSkinParams =
                    Ren::Vec4u{sr.in_vtx_offset, non_shapekeyed_vertex_count, sr.xform_offset, sr.out_vtx_offset};
                uniform_params.uShapeParamsCurr = Ren::Vec4u{0, 0, 0, 0};
                uniform_params.uShapeParamsPrev = Ren::Vec4u{0, 0, 0, 0};

                api.vkCmdPushConstants(cmd_buf, pi.layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(Skinning::Params),
                                       &uniform_params);

                api.vkCmdDispatch(cmd_buf, (sr.vertex_count + Skinning::GRP_SIZE - 1) / Skinning::GRP_SIZE, 1, 1);
            }

            if (sr.shape_keyed_vertex_count) {
                Skinning::Params uniform_params;
                uniform_params.uSkinParams =
                    Ren::Vec4u{sr.in_vtx_offset + non_shapekeyed_vertex_count, sr.shape_keyed_vertex_count,
                               sr.xform_offset, sr.out_vtx_offset + non_shapekeyed_vertex_count};
                uniform_params.uShapeParamsCurr =
                    Ren::Vec4u{sr.shape_key_offset_curr, sr.shape_key_count_curr, sr.delta_offset, 0};
                uniform_params.uShapeParamsPrev =
                    Ren::Vec4u{sr.shape_key_offset_prev, sr.shape_key_count_prev, sr.delta_offset, 0};

                api.vkCmdPushConstants(cmd_buf, pi.layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(Skinning::Params),
                                       &uniform_params);

                api.vkCmdDispatch(cmd_buf, (sr.shape_keyed_vertex_count + Skinning::GRP_SIZE - 1) / Skinning::GRP_SIZE,
                                  1, 1);
            }
        }
    }
}
