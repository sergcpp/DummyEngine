#include "RpRTReflections.h"

#include <Ren/Context.h>
#include <Ren/RastState.h>
#include <Ren/Texture.h>

#include "../../Utils/ShaderLoader.h"
#include "../PrimDraw.h"
#include "../Renderer_Structs.h"

#include "../assets/shaders/internal/rt_reflections_interface.glsl"

void RpRTReflections::Execute(RpBuilder &builder) {
    LazyInit(builder.ctx(), builder.sh());

    RpAllocBuf &sobol_buf = builder.GetReadBuffer(sobol_buf_);
    RpAllocBuf &scrambling_tile_buf = builder.GetReadBuffer(scrambling_tile_buf_);
    RpAllocBuf &ranking_tile_buf = builder.GetReadBuffer(ranking_tile_buf_);
    RpAllocBuf &geo_data_buf = builder.GetReadBuffer(geo_data_buf_);
    RpAllocBuf &materials_buf = builder.GetReadBuffer(materials_buf_);
    RpAllocBuf &vtx_buf1 = builder.GetReadBuffer(vtx_buf1_);
    RpAllocBuf &vtx_buf2 = builder.GetReadBuffer(vtx_buf2_);
    RpAllocBuf &ndx_buf = builder.GetReadBuffer(ndx_buf_);
    RpAllocBuf &unif_sh_data_buf = builder.GetReadBuffer(shared_data_buf_);
    RpAllocTex &depth_tex = builder.GetReadTexture(depth_tex_);
    RpAllocTex &normal_tex = builder.GetReadTexture(normal_tex_);
    RpAllocTex &spec_tex = builder.GetReadTexture(spec_tex_);
    RpAllocTex &rough_tex = builder.GetReadTexture(rough_tex_);
    RpAllocTex &env_tex = builder.GetReadTexture(env_tex_);
    RpAllocBuf &ray_list_buf = builder.GetReadBuffer(ray_list_buf_);
    RpAllocBuf &indir_args_buf = builder.GetReadBuffer(indir_args_buf_);
    RpAllocTex &dummy_black = builder.GetReadTexture(dummy_black_);
    RpAllocTex *lm_tex[5];
    for (int i = 0; i < 5; ++i) {
        if (lm_tex_[i]) {
            lm_tex[i] = &builder.GetReadTexture(lm_tex_[i]);
        } else {
            lm_tex[i] = &dummy_black;
        }
    }

    RpAllocTex &out_color_tex = builder.GetWriteTexture(out_color_tex_);
    RpAllocTex &out_raylen_tex = builder.GetWriteTexture(out_raylen_tex_);

    Ren::Context &ctx = builder.ctx();
    Ren::ApiContext *api_ctx = ctx.api_ctx();

    auto *acc_struct = static_cast<Ren::AccStructureVK *>(acc_struct_data_->rt_tlas);

    VkCommandBuffer cmd_buf = api_ctx->draw_cmd_buf[api_ctx->backend_frame];

    VkDescriptorSetLayout descr_set_layout = pi_rt_reflections_.prog()->descr_set_layouts()[0];
    Ren::DescrSizes descr_sizes;
    descr_sizes.img_sampler_count = 12;
    descr_sizes.store_img_count = 1;
    descr_sizes.ubuf_count = 1;
    descr_sizes.acc_count = 1;
    descr_sizes.sbuf_count = 5;
    descr_sizes.tbuf_count = 3;
    VkDescriptorSet descr_sets[2];
    descr_sets[0] = ctx.default_descr_alloc()->Alloc(descr_sizes, descr_set_layout);
    descr_sets[1] = bindless_tex_->rt_textures_descr_set;

    { // update descriptor set
        const VkDescriptorBufferInfo ubuf_info = {unif_sh_data_buf.ref->vk_handle(), 0, VK_WHOLE_SIZE};
        const VkDescriptorImageInfo depth_info =
            depth_tex.ref->vk_desc_image_info(1, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        const VkDescriptorImageInfo normal_info =
            normal_tex.ref->vk_desc_image_info(0, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        const VkDescriptorImageInfo spec_info =
            spec_tex.ref->vk_desc_image_info(0, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        const VkDescriptorImageInfo rough_info =
            rough_tex.ref->vk_desc_image_info(0, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        const VkDescriptorImageInfo env_info = {env_tex.ref->handle().sampler, env_tex.ref->handle().views[0],
                                                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        const VkDescriptorBufferInfo geo_data_info = {geo_data_buf.ref->vk_handle(), 0, VK_WHOLE_SIZE};
        const VkDescriptorBufferInfo mat_data_info = {materials_buf.ref->vk_handle(), 0, VK_WHOLE_SIZE};
        const VkDescriptorBufferInfo vtx_buf1_info = {vtx_buf1.ref->vk_handle(), 0, VK_WHOLE_SIZE};
        const VkDescriptorBufferInfo vtx_buf2_info = {vtx_buf2.ref->vk_handle(), 0, VK_WHOLE_SIZE};
        const VkDescriptorBufferInfo ndx_buf_info = {ndx_buf.ref->vk_handle(), 0, VK_WHOLE_SIZE};
        const VkDescriptorBufferInfo ray_list_info = {ray_list_buf.ref->vk_handle(), 0, VK_WHOLE_SIZE};
        const VkDescriptorImageInfo lm_infos[] = {
            lm_tex[0]->ref->vk_desc_image_info(), lm_tex[1]->ref->vk_desc_image_info(),
            lm_tex[2]->ref->vk_desc_image_info(), lm_tex[3]->ref->vk_desc_image_info(),
            lm_tex[4]->ref->vk_desc_image_info()};
        const VkAccelerationStructureKHR tlas = acc_struct->vk_handle();
        const VkDescriptorImageInfo out_color_img_info =
            out_color_tex.ref->vk_desc_image_info(0, VK_IMAGE_LAYOUT_GENERAL);
        const VkDescriptorImageInfo out_raylen_img_info =
            out_raylen_tex.ref->vk_desc_image_info(0, VK_IMAGE_LAYOUT_GENERAL);

        Ren::SmallVector<VkWriteDescriptorSet, 32> descr_writes;
        { // shared buf
            auto &descr_write = descr_writes.emplace_back();
            descr_write = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            descr_write.dstSet = descr_sets[0];
            descr_write.dstBinding = REN_UB_SHARED_DATA_LOC;
            descr_write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            descr_write.descriptorCount = 1;
            descr_write.pBufferInfo = &ubuf_info;
        }
        { // depth texture
            auto &descr_write = descr_writes.emplace_back();
            descr_write = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            descr_write.dstSet = descr_sets[0];
            descr_write.dstBinding = RTReflections::DEPTH_TEX_SLOT;
            descr_write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            descr_write.descriptorCount = 1;
            descr_write.pImageInfo = &depth_info;
        }
        { // normal texture
            auto &descr_write = descr_writes.emplace_back();
            descr_write = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            descr_write.dstSet = descr_sets[0];
            descr_write.dstBinding = RTReflections::NORM_TEX_SLOT;
            descr_write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            descr_write.descriptorCount = 1;
            descr_write.pImageInfo = &normal_info;
        }
        { // spec texture
            auto &descr_write = descr_writes.emplace_back();
            descr_write = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            descr_write.dstSet = descr_sets[0];
            descr_write.dstBinding = RTReflections::SPEC_TEX_SLOT;
            descr_write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            descr_write.descriptorCount = 1;
            descr_write.pImageInfo = &spec_info;
        }
        { // roughness texture
            auto &descr_write = descr_writes.emplace_back();
            descr_write = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            descr_write.dstSet = descr_sets[0];
            descr_write.dstBinding = RTReflections::ROUGH_TEX_SLOT;
            descr_write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            descr_write.descriptorCount = 1;
            descr_write.pImageInfo = &rough_info;
        }
        { // sobol buf
            auto &descr_write = descr_writes.emplace_back();
            descr_write = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            descr_write.dstSet = descr_sets[0];
            descr_write.dstBinding = RTReflections::SOBOL_BUF_SLOT;
            descr_write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
            descr_write.descriptorCount = 1;
            descr_write.pTexelBufferView = &sobol_buf.tbos[0]->view();
        }
        { // scrambling tile buf
            auto &descr_write = descr_writes.emplace_back();
            descr_write = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            descr_write.dstSet = descr_sets[0];
            descr_write.dstBinding = RTReflections::SCRAMLING_TILE_BUF_SLOT;
            descr_write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
            descr_write.descriptorCount = 1;
            descr_write.pTexelBufferView = &scrambling_tile_buf.tbos[0]->view();
        }
        { // ranking tile buf
            auto &descr_write = descr_writes.emplace_back();
            descr_write = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            descr_write.dstSet = descr_sets[0];
            descr_write.dstBinding = RTReflections::RANKING_TILE_BUF_SLOT;
            descr_write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
            descr_write.descriptorCount = 1;
            descr_write.pTexelBufferView = &ranking_tile_buf.tbos[0]->view();
        }
        { // ray list
            auto &descr_write = descr_writes.emplace_back();
            descr_write = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            descr_write.dstSet = descr_sets[0];
            descr_write.dstBinding = RTReflections::RAY_LIST_SLOT;
            descr_write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            descr_write.descriptorCount = 1;
            descr_write.pBufferInfo = &ray_list_info;
        }
        { // env texture
            auto &descr_write = descr_writes.emplace_back();
            descr_write = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            descr_write.dstSet = descr_sets[0];
            descr_write.dstBinding = RTReflections::ENV_TEX_SLOT;
            descr_write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            descr_write.descriptorCount = 1;
            descr_write.pImageInfo = &env_info;
        }
        VkWriteDescriptorSetAccelerationStructureKHR desc_tlas_info = {
            VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR};
        desc_tlas_info.pAccelerationStructures = &tlas;
        desc_tlas_info.accelerationStructureCount = 1;
        { // acceleration structure
            auto &descr_write = descr_writes.emplace_back();
            descr_write = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            descr_write.dstSet = descr_sets[0];
            descr_write.dstBinding = RTReflections::TLAS_SLOT;
            descr_write.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
            descr_write.descriptorCount = 1;
            descr_write.pNext = &desc_tlas_info;
        }
        { // geometry data
            auto &descr_write = descr_writes.emplace_back();
            descr_write = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            descr_write.dstSet = descr_sets[0];
            descr_write.dstBinding = RTReflections::GEO_DATA_BUF_SLOT;
            descr_write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            descr_write.descriptorCount = 1;
            descr_write.pBufferInfo = &geo_data_info;
        }
        { // materials
            auto &descr_write = descr_writes.emplace_back();
            descr_write = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            descr_write.dstSet = descr_sets[0];
            descr_write.dstBinding = RTReflections::MATERIAL_BUF_SLOT;
            descr_write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            descr_write.descriptorCount = 1;
            descr_write.pBufferInfo = &mat_data_info;
        }
        { // vtx_buf1
            auto &descr_write = descr_writes.emplace_back();
            descr_write = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            descr_write.dstSet = descr_sets[0];
            descr_write.dstBinding = RTReflections::VTX_BUF1_SLOT;
            descr_write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            descr_write.descriptorCount = 1;
            descr_write.pBufferInfo = &vtx_buf1_info;
        }
        { // vtx_buf1
            auto &descr_write = descr_writes.emplace_back();
            descr_write = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            descr_write.dstSet = descr_sets[0];
            descr_write.dstBinding = RTReflections::VTX_BUF2_SLOT;
            descr_write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            descr_write.descriptorCount = 1;
            descr_write.pBufferInfo = &vtx_buf2_info;
        }
        { // ndx_buf
            auto &descr_write = descr_writes.emplace_back();
            descr_write = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            descr_write.dstSet = descr_sets[0];
            descr_write.dstBinding = RTReflections::NDX_BUF_SLOT;
            descr_write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            descr_write.descriptorCount = 1;
            descr_write.pBufferInfo = &ndx_buf_info;
        }
        { // lightmap
            auto &descr_write = descr_writes.emplace_back();
            descr_write = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            descr_write.dstSet = descr_sets[0];
            descr_write.dstBinding = RTReflections::LMAP_TEX_SLOTS;
            descr_write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            descr_write.descriptorCount = 5;
            descr_write.pImageInfo = lm_infos;
        }
        { // output image
            auto &descr_write = descr_writes.emplace_back();
            descr_write = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            descr_write.dstSet = descr_sets[0];
            descr_write.dstBinding = RTReflections::OUT_COLOR_IMG_SLOT;
            descr_write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            descr_write.descriptorCount = 1;
            descr_write.pImageInfo = &out_color_img_info;
        }
        { // output raylen
            auto &descr_write = descr_writes.emplace_back();
            descr_write = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            descr_write.dstSet = descr_sets[0];
            descr_write.dstBinding = RTReflections::OUT_RAYLEN_IMG_SLOT;
            descr_write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            descr_write.descriptorCount = 1;
            descr_write.pImageInfo = &out_raylen_img_info;
        }

        vkUpdateDescriptorSets(api_ctx->device, uint32_t(descr_writes.size()), descr_writes.cdata(), 0, nullptr);
    }

    vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pi_rt_reflections_.handle());
    vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pi_rt_reflections_.layout(), 0, 2,
                            descr_sets, 0, nullptr);

    RTReflections::Params uniform_params;
    uniform_params.img_size = Ren::Vec2u{uint32_t(view_state_->act_res[0]), uint32_t(view_state_->act_res[1])};
    uniform_params.pixel_spread_angle = std::atan(
        2.0f * std::tan(0.5f * view_state_->vertical_fov * Ren::Pi<float>() / 180.0f) / float(view_state_->scr_res[1]));

    vkCmdPushConstants(cmd_buf, pi_rt_reflections_.layout(),
                       VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, 0, sizeof(uniform_params),
                       &uniform_params);

    vkCmdTraceRaysIndirectKHR(cmd_buf, pi_rt_reflections_.rgen_table(), pi_rt_reflections_.miss_table(),
                              pi_rt_reflections_.hit_table(), pi_rt_reflections_.call_table(),
                              indir_args_buf.ref->vk_device_address());
}

void RpRTReflections::LazyInit(Ren::Context &ctx, ShaderLoader &sh) {
    if (!initialized) {
        Ren::ProgramRef rt_reflections_prog = sh.LoadProgram(
            ctx, "rt_reflections", "internal/rt_reflections.rgen.glsl", "internal/rt_reflections.rchit.glsl",
            "internal/rt_reflections.rahit.glsl", "internal/rt_reflections.rmiss.glsl", nullptr);
        assert(rt_reflections_prog->ready());

        if (!pi_rt_reflections_.Init(ctx.api_ctx(), std::move(rt_reflections_prog), ctx.log())) {
            ctx.log()->Error("RpDebugRT: Failed to initialize pipeline!");
        }

        initialized = true;
    }
}