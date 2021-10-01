#include "RpSSRTraceHQ.h"

#include <Ren/Context.h>
#include <Ren/Program.h>

#include "../../Utils/ShaderLoader.h"
#include "../Renderer_Structs.h"

#include "../assets/shaders/internal/ssr_trace_hq_interface.glsl"

void RpSSRTraceHQ::Execute(RpBuilder &builder) {
    RpAllocBuf &unif_sh_data_buf = builder.GetReadBuffer(shared_data_buf_);
    RpAllocTex &normal_tex = builder.GetReadTexture(normal_tex_);
    RpAllocTex &depth_down_2x_tex = builder.GetReadTexture(depth_down_2x_tex_);
    RpAllocTex &output_tex = builder.GetWriteTexture(output_tex_);

    LazyInit(builder.ctx(), builder.sh());

    Ren::Context &ctx = builder.ctx();
    Ren::ApiContext *api_ctx = ctx.api_ctx();

    VkCommandBuffer cmd_buf = api_ctx->draw_cmd_buf[api_ctx->backend_frame];

    VkDescriptorSetLayout descr_set_layout = pi_ssr_trace_hq_.prog()->descr_set_layouts()[0];
    Ren::DescrSizes descr_sizes;
    descr_sizes.img_sampler_count = 7;
    descr_sizes.store_img_count = 1;
    descr_sizes.ubuf_count = 1;
    VkDescriptorSet descr_set = ctx.default_descr_alloc()->Alloc(descr_sizes, descr_set_layout);

    { // update descriptor set
        const VkDescriptorBufferInfo ubuf_info = {unif_sh_data_buf.ref->handle().buf, 0, VK_WHOLE_SIZE};

        const VkDescriptorImageInfo depth_tex_info = depth_down_2x_tex.ref->vk_desc_image_info();
        const VkDescriptorImageInfo norm_tex_info = normal_tex.ref->vk_desc_image_info();
        const VkDescriptorImageInfo output_img_info = output_tex.ref->vk_desc_image_info(0, VK_IMAGE_LAYOUT_GENERAL);

        VkWriteDescriptorSet descr_writes[4];
        descr_writes[0] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        descr_writes[0].dstSet = descr_set;
        descr_writes[0].dstBinding = REN_UB_SHARED_DATA_LOC;
        descr_writes[0].dstArrayElement = 0;
        descr_writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descr_writes[0].descriptorCount = 1;
        descr_writes[0].pBufferInfo = &ubuf_info;

        descr_writes[1] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        descr_writes[1].dstSet = descr_set;
        descr_writes[1].dstBinding = SSRTraceHQ::DEPTH_TEX_SLOT;
        descr_writes[1].dstArrayElement = 0;
        descr_writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descr_writes[1].descriptorCount = 1;
        descr_writes[1].pImageInfo = &depth_tex_info;

        descr_writes[2] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        descr_writes[2].dstSet = descr_set;
        descr_writes[2].dstBinding = SSRTraceHQ::NORM_TEX_SLOT;
        descr_writes[2].dstArrayElement = 0;
        descr_writes[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descr_writes[2].descriptorCount = 1;
        descr_writes[2].pImageInfo = &norm_tex_info;

        descr_writes[3] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        descr_writes[3].dstSet = descr_set;
        descr_writes[3].dstBinding = SSRTraceHQ::OUTPUT_TEX_SLOT;
        descr_writes[3].dstArrayElement = 0;
        descr_writes[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        descr_writes[3].descriptorCount = 1;
        descr_writes[3].pImageInfo = &output_img_info;

        vkUpdateDescriptorSets(api_ctx->device, COUNT_OF(descr_writes), descr_writes, 0, nullptr);
    }

    vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE, pi_ssr_trace_hq_.handle());
    vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE, pi_ssr_trace_hq_.layout(), 0, 1, &descr_set, 0,
                            nullptr);

    SSRTraceHQ::Params uniform_params;
    uniform_params.resolution = Ren::Vec4u{uint32_t(view_state_->act_res[0]), uint32_t(view_state_->act_res[1]), 0, 0};

    vkCmdPushConstants(cmd_buf, pi_ssr_trace_hq_.layout(), VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(uniform_params),
                       &uniform_params);

    vkCmdDispatch(cmd_buf,
                  (view_state_->act_res[0] + SSRTraceHQ::LOCAL_GROUP_SIZE_X - 1) / SSRTraceHQ::LOCAL_GROUP_SIZE_X,
                  (view_state_->act_res[1] + SSRTraceHQ::LOCAL_GROUP_SIZE_Y - 1) / SSRTraceHQ::LOCAL_GROUP_SIZE_Y, 1);
}
