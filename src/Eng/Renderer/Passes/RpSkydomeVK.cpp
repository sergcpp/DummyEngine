#include "RpSkydome.h"

#include <Ren/Context.h>
#include <Ren/DescriptorPool.h>
#include <Ren/VKCtx.h>

#include "../../Renderer/PrimDraw.h"

#include "../Renderer_Structs.h"
#include "../Shaders/skydome_interface.h"

namespace RpSkydomeInternal {
extern const float __skydome_positions[];
extern const int __skydome_vertices_count;
} // namespace RpSkydomeInternal

void Eng::RpSkydome::DrawSkydome(RpBuilder &builder, RpAllocBuf &vtx_buf1, RpAllocBuf &vtx_buf2, RpAllocBuf &ndx_buf,
                                 RpAllocTex &color_tex, RpAllocTex &spec_tex, RpAllocTex &depth_tex) {
    using namespace RpSkydomeInternal;

    RpAllocBuf &unif_shared_data_buf = builder.GetReadBuffer(shared_data_buf_);
    RpAllocTex &env_tex = builder.GetReadTexture(env_tex_);

    Ren::Context &ctx = builder.ctx();
    Ren::ApiContext *api_ctx = ctx.api_ctx();

    const int rp_index = clear_ ? 1 : 0;

    VkDescriptorSetLayout descr_set_layout = pipeline_[rp_index].prog()->descr_set_layouts()[0];
    Ren::DescrSizes descr_sizes;
    descr_sizes.img_sampler_count = 1;
    descr_sizes.ubuf_count = 1;
    VkDescriptorSet descr_set = ctx.default_descr_alloc()->Alloc(descr_sizes, descr_set_layout);

    { // update descriptor set
        const VkDescriptorBufferInfo buf_infos[] = {
            {unif_shared_data_buf.ref->vk_handle(), 0, VK_WHOLE_SIZE} // shared data
        };

        const VkDescriptorImageInfo img_infos[] = {env_tex.ref->handle().sampler, env_tex.ref->handle().views[0],
                                                   VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL}; // environment texture

        VkWriteDescriptorSet descr_writes[2];
        descr_writes[0] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        descr_writes[0].dstSet = descr_set;
        descr_writes[0].dstBinding = REN_UB_SHARED_DATA_LOC;
        descr_writes[0].dstArrayElement = 0;
        descr_writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descr_writes[0].descriptorCount = 1;
        descr_writes[0].pBufferInfo = buf_infos;

        descr_writes[1] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        descr_writes[1].dstSet = descr_set;
        descr_writes[1].dstBinding = Skydome::ENV_TEX_SLOT;
        descr_writes[1].dstArrayElement = 0;
        descr_writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descr_writes[1].descriptorCount = 1;
        descr_writes[1].pImageInfo = img_infos;

        vkUpdateDescriptorSets(api_ctx->device, 2, descr_writes, 0, nullptr);
    }

    VkCommandBuffer cmd_buf = api_ctx->draw_cmd_buf[api_ctx->backend_frame];

    VkRenderPassBeginInfo render_pass_begin_info = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    render_pass_begin_info.renderPass = render_pass_[rp_index].handle();
    render_pass_begin_info.framebuffer = framebuf_[ctx.backend_frame()][fb_to_use_].handle();
    render_pass_begin_info.renderArea = {0, 0, uint32_t(view_state_->act_res[0]), uint32_t(view_state_->act_res[1])};
    VkClearValue clear_values[3] = {{}, {}, {}};
    render_pass_begin_info.pClearValues = clear_values;
    render_pass_begin_info.clearValueCount = 3;

    vkCmdBeginRenderPass(cmd_buf, &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_[rp_index].handle());

    const VkViewport viewport = {0.0f, 0.0f, float(view_state_->act_res[0]), float(view_state_->act_res[1]),
                                 0.0f, 1.0f};
    vkCmdSetViewport(cmd_buf, 0, 1, &viewport);

    const VkRect2D scissor = {0, 0, uint32_t(view_state_->act_res[0]), uint32_t(view_state_->act_res[1])};
    vkCmdSetScissor(cmd_buf, 0, 1, &scissor);

    vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_[rp_index].layout(), 0, 1, &descr_set, 0,
                            nullptr);

    Ren::Mat4f translate_matrix;
    translate_matrix = Translate(translate_matrix, draw_cam_pos_);

    Ren::Mat4f scale_matrix;
    scale_matrix = Scale(scale_matrix, Ren::Vec3f{5000.0f, 5000.0f, 5000.0f});

    const Ren::Mat4f push_constant_data = translate_matrix * scale_matrix;
    vkCmdPushConstants(cmd_buf, pipeline_[rp_index].layout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(Ren::Mat4f),
                       &push_constant_data);

    vtx_input_.BindBuffers(cmd_buf, 0, VK_INDEX_TYPE_UINT32);

    const Ren::Mesh *skydome_mesh = prim_draw_.skydome_mesh();
    vkCmdDrawIndexed(cmd_buf, uint32_t(skydome_mesh->indices_buf().size / sizeof(uint32_t)), // index count
                     1,                                                                      // instance count
                     uint32_t(skydome_mesh->indices_buf().offset / sizeof(uint32_t)),        // first index
                     int32_t(skydome_mesh->attribs_buf1().offset / 16),                      // vertex offset
                     0);                                                                     // first instance

    vkCmdEndRenderPass(cmd_buf);
}

Eng::RpSkydome::~RpSkydome() {}