#include "ExDepthHierarchy.h"

#include <Ren/Context.h>
#include <Ren/DescriptorPool.h>
#include <Ren/Vk/VKCtx.h>

#include "../Renderer_Structs.h"
#include "../framegraph/FgBuilder.h"
#include "../shaders/depth_hierarchy_interface.h"

void Eng::ExDepthHierarchy::Execute(const FgContext &fg) {
    const Ren::ImageROHandle depth = fg.AccessROImage(depth_);

    const Ren::BufferRWHandle atomic = fg.AccessRWBuffer(atomic_);
    const Ren::ImageRWHandle output = fg.AccessRWImage(output_);

    const Ren::ApiContext &api = fg.ren_ctx().api();
    const Ren::StoragesRef &storages = fg.storages();

    const auto &[input_main, input_cold] = storages.images[depth];
    const auto &[output_main, output_cold] = storages.images[output];

    VkCommandBuffer cmd_buf = fg.cmd_buf();

    const Ren::PipelineMain &pi = storages.pipelines[pi_depth_hierarchy_].first;
    const Ren::ProgramMain &pr = storages.programs[pi.prog].first;

    VkDescriptorSetLayout descr_set_layout = pr.descr_set_layouts[0];
    Ren::DescrSizes descr_sizes;
    descr_sizes.img_sampler_count = 1;
    descr_sizes.store_img_count = output_cold.params.mip_count;
    VkDescriptorSet descr_set = fg.descr_alloc().Alloc(descr_sizes, descr_set_layout);

    { // update descriptor set
        const Ren::BufferMain &atomic_main = storages.buffers[atomic].first;

        const VkDescriptorImageInfo depth_tex_info =
            Image_GetDescriptorImageInfo(api, input_main, 1, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        const VkDescriptorBufferInfo atomic_buf_info = {atomic_main.buf, 0, VK_WHOLE_SIZE};
        VkDescriptorImageInfo depth_img_infos[7];
        for (int i = 0; i < 7; ++i) {
            depth_img_infos[i] = Image_GetDescriptorImageInfo(
                api, output_main, std::min(i, output_cold.params.mip_count - 1) + 1, VK_IMAGE_LAYOUT_GENERAL);
        }

        VkWriteDescriptorSet descr_writes[3];
        descr_writes[0] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        descr_writes[0].dstSet = descr_set;
        descr_writes[0].dstBinding = DepthHierarchy::DEPTH_TEX_SLOT;
        descr_writes[0].dstArrayElement = 0;
        descr_writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descr_writes[0].descriptorCount = 1;
        descr_writes[0].pImageInfo = &depth_tex_info;

        descr_writes[1] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        descr_writes[1].dstSet = descr_set;
        descr_writes[1].dstBinding = DepthHierarchy::ATOMIC_CNT_SLOT;
        descr_writes[1].dstArrayElement = 0;
        descr_writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descr_writes[1].descriptorCount = 1;
        descr_writes[1].pBufferInfo = &atomic_buf_info;

        descr_writes[2] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        descr_writes[2].dstSet = descr_set;
        descr_writes[2].dstBinding = DepthHierarchy::DEPTH_IMG_SLOT;
        descr_writes[2].dstArrayElement = 0;
        descr_writes[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        descr_writes[2].descriptorCount = 7;
        descr_writes[2].pImageInfo = depth_img_infos;

        api.vkUpdateDescriptorSets(api.device, 3, descr_writes, 0, nullptr);
    }

    api.vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE, pi.pipeline);
    api.vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE, pi.layout, 0, 1, &descr_set, 0, nullptr);

    const uint32_t grp_x = Ren::DivCeil(uint32_t(output_cold.params.w), DepthHierarchy::GRP_SIZE_X);
    const uint32_t grp_y = Ren::DivCeil(uint32_t(output_cold.params.h), DepthHierarchy::GRP_SIZE_Y);

    DepthHierarchy::Params uniform_params;
    uniform_params.depth_size =
        Ren::Vec4u{view_state_->ren_res[0], view_state_->ren_res[1], output_cold.params.mip_count, grp_x * grp_y};
    uniform_params.clip_info = view_state_->clip_info;

    api.vkCmdPushConstants(cmd_buf, pi.layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(uniform_params), &uniform_params);

    api.vkCmdDispatch(cmd_buf, grp_x, grp_y, 1);
}
