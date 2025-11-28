#include "ExDepthHierarchy.h"

#include <Ren/Context.h>
#include <Ren/DescriptorPool.h>
#include <Ren/VKCtx.h>

#include "../../utils/ShaderLoader.h"
#include "../Renderer_Structs.h"
#include "../shaders/depth_hierarchy_interface.h"

void Eng::ExDepthHierarchy::Execute(FgContext &fg) {
    const Ren::Image &depth_tex = fg.AccessROImage(depth_tex_);
    Ren::Buffer &atomic_buf = fg.AccessRWBuffer(atomic_buf_);
    Ren::Image &output_tex = fg.AccessRWImage(output_tex_);

    Ren::ApiContext *api_ctx = fg.ren_ctx().api_ctx();

    VkCommandBuffer cmd_buf = api_ctx->draw_cmd_buf[api_ctx->backend_frame];

    VkDescriptorSetLayout descr_set_layout = pi_depth_hierarchy_->prog()->descr_set_layouts()[0];
    Ren::DescrSizes descr_sizes;
    descr_sizes.img_sampler_count = 1;
    descr_sizes.store_img_count = output_tex.params.mip_count;
    VkDescriptorSet descr_set = fg.descr_alloc().Alloc(descr_sizes, descr_set_layout);

    { // update descriptor set
        const VkDescriptorImageInfo depth_tex_info = depth_tex.vk_desc_image_info(1);
        const VkDescriptorBufferInfo atomic_buf_info = {atomic_buf.vk_handle(), 0, VK_WHOLE_SIZE};
        VkDescriptorImageInfo depth_img_infos[7];
        for (int i = 0; i < 7; ++i) {
            depth_img_infos[i] = output_tex.vk_desc_image_info(std::min(i, output_tex.params.mip_count - 1) + 1,
                                                               VK_IMAGE_LAYOUT_GENERAL);
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

        api_ctx->vkUpdateDescriptorSets(api_ctx->device, 3, descr_writes, 0, nullptr);
    }

    api_ctx->vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE, pi_depth_hierarchy_->handle());
    api_ctx->vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE, pi_depth_hierarchy_->layout(), 0, 1,
                                     &descr_set, 0, nullptr);

    const int grp_x = (output_tex.params.w + DepthHierarchy::GRP_SIZE_X - 1) / DepthHierarchy::GRP_SIZE_X;
    const int grp_y = (output_tex.params.h + DepthHierarchy::GRP_SIZE_Y - 1) / DepthHierarchy::GRP_SIZE_Y;

    DepthHierarchy::Params uniform_params;
    uniform_params.depth_size =
        Ren::Vec4i{view_state_->ren_res[0], view_state_->ren_res[1], output_tex.params.mip_count, grp_x * grp_y};
    uniform_params.clip_info = view_state_->clip_info;

    api_ctx->vkCmdPushConstants(cmd_buf, pi_depth_hierarchy_->layout(), VK_SHADER_STAGE_COMPUTE_BIT, 0,
                                sizeof(uniform_params), &uniform_params);

    api_ctx->vkCmdDispatch(cmd_buf, grp_x, grp_y, 1);
}
