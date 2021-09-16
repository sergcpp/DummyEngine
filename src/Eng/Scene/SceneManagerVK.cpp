#include "SceneManager.h"

#include <Ren/Context.h>
#include <Ren/DescriptorPool.h>
#include <Ren/Utils.h>
#include <Ren/VKCtx.h>

#include "../Renderer/Renderer_Structs.h"

#include <vtune/ittnotify.h>
extern __itt_domain *__g_itt_domain;

namespace SceneManagerConstants {} // namespace SceneManagerConstants

namespace SceneManagerInternal {
uint32_t next_power_of_two(uint32_t v) {
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v++;
    return v;
}
} // namespace SceneManagerInternal

void SceneManager::UpdateMaterialsBuffer() {
    using namespace SceneManagerInternal;

    Ren::ApiContext *api_ctx = ren_ctx_.api_ctx();
    auto &persistant_data = scene_data_.persistant_data;

    const uint32_t max_mat_count = scene_data_.materials.capacity();
    const uint32_t req_mat_buf_size = std::max(1u, max_mat_count) * sizeof(MaterialData);

    if (!persistant_data.materials_buf) {
        persistant_data.materials_buf =
            ren_ctx_.LoadBuffer("Materials Buffer", Ren::eBufType::Storage, req_mat_buf_size);
    }

    if (persistant_data.materials_buf->size() < req_mat_buf_size) {
        persistant_data.materials_buf->Resize(req_mat_buf_size);
    }

    const uint32_t max_tex_count = std::max(1u, REN_MAX_TEX_PER_MATERIAL * max_mat_count);
    // const uint32_t req_tex_buf_size = max_tex_count * sizeof(GLuint64);

    if (!persistant_data.textures_descr_pool) {
        persistant_data.textures_descr_pool.reset(new Ren::DescrPool(api_ctx));
    }

    const int materials_per_descriptor = api_ctx->max_combined_image_samplers / REN_MAX_TEX_PER_MATERIAL;

    if (persistant_data.textures_descr_pool->descr_count(Ren::eDescrType::CombinedImageSampler) < max_tex_count) {
        assert(materials_per_descriptor > 0);
        const int needed_descriptors_count = (max_mat_count + materials_per_descriptor - 1) / materials_per_descriptor;

        persistant_data.textures_descr_pool->Init(
            Ren::MaxFramesInFlight * needed_descriptors_count * api_ctx->max_combined_image_samplers, 0, 0, 0, 0,
            Ren::MaxFramesInFlight * needed_descriptors_count /* sets_count */);

        if (!persistant_data.textures_descr_layout) {
            VkDescriptorSetLayoutBinding textures_binding = {};
            textures_binding.binding = REN_BINDLESS_TEX_SLOT;
            textures_binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            textures_binding.descriptorCount = api_ctx->max_combined_image_samplers;
            textures_binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

            VkDescriptorSetLayoutCreateInfo layout_info = {};
            layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            layout_info.bindingCount = 1;
            layout_info.pBindings = &textures_binding;

            VkDescriptorBindingFlagsEXT bind_flag = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT_EXT;

            VkDescriptorSetLayoutBindingFlagsCreateInfoEXT extended_info = {};
            extended_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO_EXT;
            extended_info.pNext = nullptr;
            extended_info.bindingCount = 1u;
            extended_info.pBindingFlags = &bind_flag;
            layout_info.pNext = &extended_info;

            const VkResult res = vkCreateDescriptorSetLayout(api_ctx->device, &layout_info, nullptr,
                                                             &persistant_data.textures_descr_layout);
            assert(res == VK_SUCCESS);
        }

        for (int j = 0; j < Ren::MaxFramesInFlight; ++j) {
            for (int k = 0; k < needed_descriptors_count; ++k) {
                persistant_data.textures_descr_sets[j].push_back(
                    persistant_data.textures_descr_pool->Alloc(persistant_data.textures_descr_layout));
                assert(persistant_data.textures_descr_sets[j].back());
            }
        }
    }

    for (const uint32_t i : scene_data_.material_changes) {
        for (int j = 0; j < Ren::MaxFramesInFlight; ++j) {
            scene_data_.mat_update_ranges[j].first = std::min(scene_data_.mat_update_ranges[j].first, i);
            scene_data_.mat_update_ranges[j].second = std::max(scene_data_.mat_update_ranges[j].second, i + 1);
        }
    }
    scene_data_.material_changes.clear();

    auto &update_range = scene_data_.mat_update_ranges[ren_ctx_.backend_frame()];
    if (update_range.second <= update_range.first) {
        return;
    }

    Ren::Buffer materials_stage_buf("Materials Stage Buffer", ren_ctx_.api_ctx(), Ren::eBufType::Stage,
                                    (update_range.second - update_range.first) * sizeof(MaterialData));
    MaterialData *material_data = reinterpret_cast<MaterialData *>(materials_stage_buf.Map(Ren::BufMapWrite));

    Ren::SmallVector<VkDescriptorImageInfo, 256> img_infos;
    Ren::SmallVector<Ren::TransitionInfo, 256> img_transitions;
    img_infos.reserve((update_range.second - update_range.first) * REN_MAX_TEX_PER_MATERIAL);

    for (uint32_t i = update_range.first; i < update_range.second; ++i) {
        const uint32_t rel_i = i - update_range.first;

        const uint32_t set_index = i / materials_per_descriptor;
        const uint32_t arr_offset = i % materials_per_descriptor;

        const Ren::Material *mat = scene_data_.materials.GetOrNull(i);
        if (mat) {
            int j = 0;
            for (; j < int(mat->textures.size()); ++j) {
                material_data[rel_i].texture_indices[j] = arr_offset * REN_MAX_TEX_PER_MATERIAL + j;

                if (mat->textures[j]->resource_state != Ren::eResState::ShaderResource) {
                    img_transitions.emplace_back(mat->textures[j].get(), Ren::eResState::ShaderResource);
                }

                auto &img_info = img_infos.emplace_back();
                img_info.sampler = mat->samplers[j]->vk_handle();
                img_info.imageView = mat->textures[j]->handle().views[0];
                img_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            }
            for (; j < REN_MAX_TEX_PER_MATERIAL; ++j) {
                img_infos.push_back(error_tex_->vk_desc_image_info());
            }
            if (!mat->params.empty()) {
                material_data[rel_i].params = mat->params[0];
            }
        } else {
            for (int j = 0; j < REN_MAX_TEX_PER_MATERIAL; ++j) {
                img_infos.push_back(error_tex_->vk_desc_image_info());
            }
        }
    }

    if (!img_transitions.empty()) {
        Ren::TransitionResourceStates(ren_ctx_.current_cmd_buf(), Ren::AllStages, Ren::AllStages,
                                      img_transitions.cdata(), int(img_transitions.size()));
    }

    if (!img_infos.empty()) {
        for (uint32_t i = update_range.first; i < update_range.second; ++i) {
            const uint32_t rel_i = i - update_range.first;

            const uint32_t set_index = i / materials_per_descriptor;
            const uint32_t arr_offset = i % materials_per_descriptor;
            const uint32_t arr_count = (materials_per_descriptor - arr_offset);

            VkWriteDescriptorSet descr_write;
            descr_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descr_write.dstSet = scene_data_.persistant_data.textures_descr_sets[ren_ctx_.backend_frame()][set_index];
            descr_write.dstBinding = REN_BINDLESS_TEX_SLOT;
            descr_write.dstArrayElement = uint32_t(arr_offset * REN_MAX_TEX_PER_MATERIAL);
            descr_write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            descr_write.descriptorCount = uint32_t(REN_MAX_TEX_PER_MATERIAL);
            descr_write.pBufferInfo = nullptr;
            descr_write.pImageInfo = img_infos.cdata() + rel_i * REN_MAX_TEX_PER_MATERIAL;
            descr_write.pTexelBufferView = nullptr;
            descr_write.pNext = nullptr;

            // TODO: group this calls!!!
            vkUpdateDescriptorSets(api_ctx->device, 1, &descr_write, 0, nullptr);
        }

        // for (uint32_t start = 0, end = 1; end <= list.zfill_batches.count; end++) {
        //}

        /*VkWriteDescriptorSet descr_write;
        descr_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descr_write.dstSet = scene_data_.persistant_data.textures_descr_set[ren_ctx_.backend_frame()];
        descr_write.dstBinding = REN_BINDLESS_TEX_SLOT;
        descr_write.dstArrayElement = uint32_t(update_range.first * REN_MAX_TEX_PER_MATERIAL);
        descr_write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descr_write.descriptorCount = uint32_t(img_infos.size());
        descr_write.pBufferInfo = nullptr;
        descr_write.pImageInfo = img_infos.cdata();
        descr_write.pTexelBufferView = nullptr;
        descr_write.pNext = nullptr;

        vkUpdateDescriptorSets(api_ctx->device, 1, &descr_write, 0, nullptr);*/
    }

    materials_stage_buf.FlushMappedRange(
        0, materials_stage_buf.AlignMapOffset((update_range.second - update_range.first) * sizeof(MaterialData)));
    materials_stage_buf.Unmap();
    scene_data_.persistant_data.materials_buf->UpdateSubRegion(
        update_range.first * sizeof(MaterialData), (update_range.second - update_range.first) * sizeof(MaterialData),
        materials_stage_buf, 0, ren_ctx_.current_cmd_buf());

    update_range = std::make_pair(std::numeric_limits<uint32_t>::max(), 0);
}

void SceneManager::InitPipelinesForProgram(const Ren::ProgramRef &prog, const uint32_t mat_flags,
                                           Ren::SmallVectorImpl<Ren::PipelineRef> &out_pipelines) {
    for (int i = 0; i < 2; ++i) {
        Ren::RastState rast_state;
        if (i == 0) {
            rast_state.poly.cull = uint8_t(Ren::eCullFace::Back);
        } else if (i == 1) {
            rast_state.poly.cull = uint8_t(Ren::eCullFace::Front);
        }
        rast_state.poly.mode = uint8_t(Ren::ePolygonMode::Fill);

        rast_state.depth.test_enabled = true;

        if (mat_flags & uint32_t(Ren::eMatFlags::AlphaBlend)) {
            rast_state.depth.compare_op = unsigned(Ren::eCompareOp::Less);

            rast_state.blend.enabled = true;
            rast_state.blend.src = unsigned(Ren::eBlendFactor::SrcAlpha);
            rast_state.blend.dst = unsigned(Ren::eBlendFactor::OneMinusSrcAlpha);
        } else {
            rast_state.depth.compare_op = unsigned(Ren::eCompareOp::Equal);
        }

        // find of create pipeline
        uint32_t new_index = 0xffffffff;
        for (auto it = std::begin(scene_data_.persistant_data.pipelines);
             it != std::end(scene_data_.persistant_data.pipelines); ++it) {
            if (it->prog() == prog && it->rast_state() == rast_state) {
                new_index = it.index();
                break;
            }
        }

        if (new_index == 0xffffffff) {
            new_index = scene_data_.persistant_data.pipelines.emplace();
            Ren::Pipeline &new_pipeline = scene_data_.persistant_data.pipelines.at(new_index);

            const bool res =
                new_pipeline.Init(ren_ctx_.api_ctx(), rast_state, prog, &draw_pass_vi_, &rp_main_draw_, ren_ctx_.log());
            if (!res) {
                ren_ctx_.log()->Error("Failed to initialize pipeline!");
            }
        }

        out_pipelines.emplace_back(&scene_data_.persistant_data.pipelines, new_index);
    }
}