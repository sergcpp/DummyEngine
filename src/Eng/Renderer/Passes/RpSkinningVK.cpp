#include "RpSkinning.h"

#include <Ren/Buffer.h>
#include <Ren/Context.h>
#include <Ren/DescriptorPool.h>
#include <Ren/VKCtx.h>

#include "../Renderer_Structs.h"

#include "../assets/shaders/internal/skinning_interface.glsl"

void RpSkinning::Execute(RpBuilder &builder) {
    LazyInit(builder.ctx(), builder.sh());

    RpAllocBuf &skin_vtx_buf = builder.GetReadBuffer(skin_vtx_buf_);
    RpAllocBuf &skin_transforms_buf = builder.GetReadBuffer(skin_transforms_buf_);
    RpAllocBuf &shape_keys_buf = builder.GetReadBuffer(shape_keys_buf_);
    RpAllocBuf &delta_buf = builder.GetReadBuffer(delta_buf_);

    RpAllocBuf &vtx_buf1 = builder.GetWriteBuffer(vtx_buf1_);
    RpAllocBuf &vtx_buf2 = builder.GetWriteBuffer(vtx_buf2_);

    if (skin_regions_.count) {
        Ren::Context &ctx = builder.ctx();
        Ren::ApiContext *api_ctx = builder.ctx().api_ctx();
        VkCommandBuffer cmd_buf = api_ctx->draw_cmd_buf[api_ctx->backend_frame];

        VkDescriptorSetLayout descr_set_layout = skinning_prog_->descr_set_layouts()[0];
        VkDescriptorSet descr_set = ctx.default_descr_alloc()->Alloc(
            0 /* img_count */, 0 /* ubuf_count */, 6 /* sbuf_count */, 0 /* tbuf_count */, descr_set_layout);

        { // update descriptor set
            const VkDescriptorBufferInfo buf_infos[6] = {
                {skin_vtx_buf.ref->handle().buf, 0, VK_WHOLE_SIZE},        // input vertices binding
                {skin_transforms_buf.ref->handle().buf, 0, VK_WHOLE_SIZE}, // input matrices binding
                {shape_keys_buf.ref->handle().buf, 0, VK_WHOLE_SIZE},      // input shape keys binding
                {delta_buf.ref->handle().buf, 0, VK_WHOLE_SIZE},           // input vertex deltas binding
                {vtx_buf1.ref->handle().buf, 0, VK_WHOLE_SIZE},            // output vertices0 binding
                {vtx_buf2.ref->handle().buf, 0, VK_WHOLE_SIZE}             // output vertices1 binding
            };

            VkWriteDescriptorSet descr_write;
            descr_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descr_write.dstSet = descr_set;
            descr_write.dstBinding = 0;
            descr_write.dstArrayElement = 0;
            descr_write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            descr_write.descriptorCount = 6;
            descr_write.pBufferInfo = buf_infos;
            descr_write.pImageInfo = nullptr;
            descr_write.pTexelBufferView = nullptr;
            descr_write.pNext = nullptr;

            vkUpdateDescriptorSets(api_ctx->device, 1, &descr_write, 0, nullptr);
        }

        const int SkinLocalGroupSize = 128;

        vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_);
        vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_layout_, 0, 1, &descr_set, 0,
                                nullptr);

        for (uint32_t i = 0; i < skin_regions_.count; i++) {
            const SkinRegion &sr = skin_regions_.data[i];

            const uint32_t non_shapekeyed_vertex_count = sr.vertex_count - sr.shape_keyed_vertex_count;

            if (non_shapekeyed_vertex_count) {
                Skinning::Params uniform_params;
                uniform_params.uSkinParams =
                    Ren::Vec4u{sr.in_vtx_offset, non_shapekeyed_vertex_count, sr.xform_offset, sr.out_vtx_offset};
                uniform_params.uShapeParamsCurr = Ren::Vec4u{0, 0, 0, 0};
                uniform_params.uShapeParamsPrev = Ren::Vec4u{0, 0, 0, 0};

                vkCmdPushConstants(cmd_buf, pipeline_layout_, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(Skinning::Params),
                                   &uniform_params);

                vkCmdDispatch(cmd_buf, (sr.vertex_count + SkinLocalGroupSize - 1) / SkinLocalGroupSize, 1, 1);
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

                vkCmdPushConstants(cmd_buf, pipeline_layout_, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(Skinning::Params),
                                   &uniform_params);

                vkCmdDispatch(cmd_buf, (sr.shape_keyed_vertex_count + SkinLocalGroupSize - 1) / SkinLocalGroupSize, 1,
                              1);
            }
        }
    }
}

bool RpSkinning::InitPipeline(Ren::Context &ctx) {
    Ren::ApiContext *api_ctx = ctx.api_ctx();

    api_ctx_ = api_ctx;

    { // create pipeline layout
        VkPipelineLayoutCreateInfo layout_create_info = {};
        layout_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layout_create_info.setLayoutCount = 1;
        layout_create_info.pSetLayouts = skinning_prog_->descr_set_layouts();

        VkPushConstantRange pc_ranges[1] = {};
        pc_ranges[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        pc_ranges[0].offset = 0;
        pc_ranges[0].size = 3 * sizeof(Ren::Vec4u);

        layout_create_info.pushConstantRangeCount = 1;
        layout_create_info.pPushConstantRanges = pc_ranges;

        const VkResult res = vkCreatePipelineLayout(api_ctx->device, &layout_create_info, nullptr, &pipeline_layout_);
        if (res != VK_SUCCESS) {
            ctx.log()->Error("Failed to create pipeline layout!");
            return false;
        }
    }

    { // create pipeline
        VkPipelineShaderStageCreateInfo shader_stage_create_info = {};
        shader_stage_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shader_stage_create_info.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        shader_stage_create_info.module = skinning_prog_->shader(Ren::eShaderType::Comp)->module();
        shader_stage_create_info.pName = "main";
        shader_stage_create_info.pSpecializationInfo = nullptr;

        VkComputePipelineCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        info.stage = shader_stage_create_info;
        info.layout = pipeline_layout_;

        const VkResult res = vkCreateComputePipelines(api_ctx->device, VK_NULL_HANDLE, 1, &info, nullptr, &pipeline_);
        if (res != VK_SUCCESS) {
            ctx.log()->Error("Failed to create pipeline!");
            return false;
        }
    }

    return true;
}

RpSkinning::~RpSkinning() {
    /*if (desc_pool_) {
        vkDestroyDescriptorPool(api_ctx_->device, desc_pool_, nullptr);
    }*/
    if (pipeline_layout_) {
        vkDestroyPipelineLayout(api_ctx_->device, pipeline_layout_, nullptr);
    }
    if (pipeline_) {
        vkDestroyPipeline(api_ctx_->device, pipeline_, nullptr);
    }
}