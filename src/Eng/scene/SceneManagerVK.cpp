#include "SceneManager.h"

#include <Ren/Context.h>
#include <Ren/DescriptorPool.h>
#include <Ren/Vk/VKCtx.h>
#include <Ren/utils/Utils.h>
#include <Sys/ScopeExit.h>

#include "../renderer/Renderer_Structs.h"

#include <vtune/ittnotify.h>
extern __itt_domain *__g_itt_domain;

namespace Eng::SceneManagerConstants {} // namespace Eng::SceneManagerConstants

namespace Eng::SceneManagerInternal {
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

VkDeviceSize align_up(const VkDeviceSize size, const VkDeviceSize alignment) {
    return (size + alignment - 1) & ~(alignment - 1);
}

void to_khr_xform(const Ren::Mat4f &xform, float matrix[3][4]) {
    // transpose
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 4; ++j) {
            matrix[i][j] = xform[j][i];
        }
    }
}

const VkDeviceSize AccStructAlignment = 256;
} // namespace Eng::SceneManagerInternal

bool Eng::SceneManager::UpdateMaterialsBuffer() {
    using namespace SceneManagerInternal;

    const Ren::ApiContext &api = ren_ctx_.api();
    const Ren::StoragesRef &storages = ren_ctx_.storages();

    auto &pers_data = *scene_data_.persistent_data;

    const uint32_t max_mat_count = storages.materials.capacity();
    const uint32_t req_mat_buf_size = std::max(1u, max_mat_count) * sizeof(material_data_t);

    const auto &[mat_buf_main, mat_buf_cold] = storages.buffers[pers_data.materials];
    if (mat_buf_cold.size < req_mat_buf_size) {
        if (!Buffer_Resize(api, mat_buf_main, mat_buf_cold, req_mat_buf_size, ren_ctx_.log())) {
            return false;
        }
    }

    const uint32_t max_tex_count = std::max(1u, MAX_TEX_PER_MATERIAL * max_mat_count);
    // const uint32_t req_tex_buf_size = max_tex_count * sizeof(GLuint64);

    if (!pers_data.textures_descr_pool) {
        pers_data.textures_descr_pool = std::make_unique<Ren::DescrPool>(api);
    }

    const int materials_per_descriptor = int(api.max_combined_image_samplers / MAX_TEX_PER_MATERIAL);

    if (pers_data.textures_descr_pool->descr_count(Ren::eDescrType::SampledImage) < max_tex_count) {
        assert(materials_per_descriptor > 0);
        const int needed_descriptors_count =
            std::max(1, int(max_mat_count + materials_per_descriptor - 1) / materials_per_descriptor);

        Ren::DescrSizes descr_sizes;
        descr_sizes.img_count = Ren::MaxFramesInFlight * needed_descriptors_count * api.max_combined_image_samplers;
        descr_sizes.sampler_count = Ren::MaxFramesInFlight * needed_descriptors_count;
        [[maybe_unused]] bool res = pers_data.textures_descr_pool->Init(
            descr_sizes, Ren::MaxFramesInFlight * needed_descriptors_count /* sets_count */);
        if (ren_ctx_.capabilities.hwrt) {
            assert(needed_descriptors_count == 1); // we have to be able to bind all textures at once
            if (!pers_data.rt_textures_descr_pool) {
                pers_data.rt_textures_descr_pool = std::make_unique<Ren::DescrPool>(api);
            }
            res &= pers_data.rt_textures_descr_pool->Init(descr_sizes, Ren::MaxFramesInFlight /* sets_count */);
        }

        if (ren_ctx_.capabilities.hwrt || ren_ctx_.capabilities.swrt) {
            if (!pers_data.rt_inline_textures_descr_pool) {
                pers_data.rt_inline_textures_descr_pool = std::make_unique<Ren::DescrPool>(api);
            }
            res &= pers_data.rt_inline_textures_descr_pool->Init(descr_sizes, Ren::MaxFramesInFlight /* sets_count */);
        }

        assert(res);

        if (!pers_data.textures_descr_layout) {
            VkDescriptorSetLayoutBinding bindings[2] = {};

            bindings[0].binding = BIND_BINDLESS_TEX;
            bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
            bindings[0].descriptorCount = api.max_combined_image_samplers;
            bindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

            bindings[1].binding = BIND_SCENE_SAMPLERS;
            bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
            bindings[1].descriptorCount = 1;
            bindings[1].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

            VkDescriptorSetLayoutCreateInfo layout_info = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
            layout_info.bindingCount = std::size(bindings);
            layout_info.pBindings = &bindings[0];

            const VkDescriptorBindingFlagsEXT bind_flags[2] = {VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT_EXT, 0};

            VkDescriptorSetLayoutBindingFlagsCreateInfoEXT extended_info = {
                VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO_EXT};
            extended_info.bindingCount = std::size(bindings);
            extended_info.pBindingFlags = &bind_flags[0];
            layout_info.pNext = &extended_info;

            const VkResult _res =
                api.vkCreateDescriptorSetLayout(api.device, &layout_info, nullptr, &pers_data.textures_descr_layout);
            assert(_res == VK_SUCCESS);
        }

        if (ren_ctx_.capabilities.hwrt && !pers_data.rt_textures_descr_layout) {
            VkDescriptorSetLayoutBinding bindings[2] = {};

            bindings[0].binding = BIND_BINDLESS_TEX;
            bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
            bindings[0].descriptorCount = api.max_combined_image_samplers;
            bindings[0].stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR;

            bindings[1].binding = BIND_SCENE_SAMPLERS;
            bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
            bindings[1].descriptorCount = 1;
            bindings[1].stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR;

            VkDescriptorSetLayoutCreateInfo layout_info = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
            layout_info.bindingCount = std::size(bindings);
            layout_info.pBindings = &bindings[0];

            const VkDescriptorBindingFlagsEXT bind_flags[2] = {VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT_EXT, 0};

            VkDescriptorSetLayoutBindingFlagsCreateInfoEXT extended_info = {
                VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO_EXT};
            extended_info.bindingCount = std::size(bindings);
            extended_info.pBindingFlags = &bind_flags[0];
            layout_info.pNext = &extended_info;

            const VkResult _res =
                api.vkCreateDescriptorSetLayout(api.device, &layout_info, nullptr, &pers_data.rt_textures_descr_layout);
            assert(_res == VK_SUCCESS);
        }

        if ((ren_ctx_.capabilities.hwrt || ren_ctx_.capabilities.swrt) && !pers_data.rt_inline_textures_descr_layout) {
            VkDescriptorSetLayoutBinding bindings[2] = {};

            bindings[0].binding = BIND_BINDLESS_TEX;
            bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
            bindings[0].descriptorCount = api.max_combined_image_samplers;
            bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

            bindings[1].binding = BIND_SCENE_SAMPLERS;
            bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
            bindings[1].descriptorCount = 1;
            bindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

            VkDescriptorSetLayoutCreateInfo layout_info = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
            layout_info.bindingCount = std::size(bindings);
            layout_info.pBindings = &bindings[0];

            const VkDescriptorBindingFlagsEXT bind_flags[2] = {VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT_EXT, 0};

            VkDescriptorSetLayoutBindingFlagsCreateInfoEXT extended_info = {
                VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO_EXT};
            extended_info.bindingCount = std::size(bindings);
            extended_info.pBindingFlags = &bind_flags[0];
            layout_info.pNext = &extended_info;

            const VkResult _res = api.vkCreateDescriptorSetLayout(api.device, &layout_info, nullptr,
                                                                  &pers_data.rt_inline_textures_descr_layout);
            assert(_res == VK_SUCCESS);
        }

        const Ren::Sampler &sampler = ren_ctx_.storages().samplers[pers_data.trilinear_sampler];

        VkDescriptorImageInfo sampler_info = {};
        sampler_info.sampler = sampler.handle;

        VkWriteDescriptorSet descr_write = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        descr_write.dstBinding = BIND_SCENE_SAMPLERS;
        descr_write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
        descr_write.descriptorCount = 1;
        descr_write.pImageInfo = &sampler_info;

        for (int j = 0; j < Ren::MaxFramesInFlight; ++j) {
            for (int k = 0; k < needed_descriptors_count; ++k) {
                pers_data.textures_descr_sets[j].push_back(
                    pers_data.textures_descr_pool->Alloc(pers_data.textures_descr_layout));
                assert(pers_data.textures_descr_sets[j].back());

                descr_write.dstSet = pers_data.textures_descr_sets[j].back();
                api.vkUpdateDescriptorSets(api.device, 1, &descr_write, 0, nullptr);
            }
            if (ren_ctx_.capabilities.hwrt) {
                pers_data.rt_textures_descr_sets[j] =
                    pers_data.rt_textures_descr_pool->Alloc(pers_data.rt_textures_descr_layout);

                descr_write.dstSet = pers_data.rt_textures_descr_sets[j];
                api.vkUpdateDescriptorSets(api.device, 1, &descr_write, 0, nullptr);
            }
            if (ren_ctx_.capabilities.hwrt || ren_ctx_.capabilities.swrt) {
                pers_data.rt_inline_textures_descr_sets[j] =
                    pers_data.rt_inline_textures_descr_pool->Alloc(pers_data.rt_inline_textures_descr_layout);

                descr_write.dstSet = pers_data.rt_inline_textures_descr_sets[j];
                api.vkUpdateDescriptorSets(api.device, 1, &descr_write, 0, nullptr);
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
        bool finished = true;
        for (int j = 0; j < Ren::MaxFramesInFlight; ++j) {
            finished &= (scene_data_.mat_update_ranges[j].second <= scene_data_.mat_update_ranges[j].first);
        }
        return finished;
    }

    Ren::BufferMain materials_upload_buf_main = {};
    Ren::BufferCold materials_upload_buf_cold = {};
    if (!Buffer_Init(api, materials_upload_buf_main, materials_upload_buf_cold, Ren::String{"Materials Upload Buffer"},
                     Ren::eBufType::Upload, (update_range.second - update_range.first) * sizeof(material_data_t),
                     ren_ctx_.log())) {
        return false;
    }
    SCOPE_EXIT({ Buffer_Destroy(api, materials_upload_buf_main, materials_upload_buf_cold); })

    auto *material_data =
        reinterpret_cast<material_data_t *>(Buffer_Map(api, materials_upload_buf_main, materials_upload_buf_cold));

    Ren::SmallVector<VkDescriptorImageInfo, 256> img_infos;
    Ren::SmallVector<Ren::TransitionInfo, 256> img_transitions;
    img_infos.reserve((update_range.second - update_range.first) * MAX_TEX_PER_MATERIAL);

    img_transitions.emplace_back(white_tex_, Ren::eResState::ShaderResource);
    img_transitions.emplace_back(error_tex_, Ren::eResState::ShaderResource);

    for (uint32_t i = update_range.first; i < update_range.second; ++i) {
        const uint32_t rel_i = i - update_range.first;

        // const uint32_t set_index = i / materials_per_descriptor;
        const uint32_t arr_offset = i % materials_per_descriptor;
        if (storages.materials.IsOccupied(i)) {
            const auto &[mat_main, mat_cold] = storages.materials.GetUnsafe(i);

            int j = 0;
            for (; j < int(mat_main.textures.size()); ++j) {
                material_data[rel_i].texture_indices[j] = arr_offset * MAX_TEX_PER_MATERIAL + j;

                const Ren::ImageMain &img_main = storages.images[mat_main.textures[j]].first;
                if (img_main.resource_state != Ren::eResState::ShaderResource) {
                    img_transitions.emplace_back(mat_main.textures[j], Ren::eResState::ShaderResource);
                }

                auto &img_info = img_infos.emplace_back();
                img_info.sampler = storages.samplers[mat_main.samplers[j]].handle;
                img_info.imageView = img_main.views[0];
                img_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            }
            for (; j < MAX_TEX_PER_MATERIAL; ++j) {
                material_data[rel_i].texture_indices[j] = i * MAX_TEX_PER_MATERIAL + j;

                const Ren::ImageMain &white_main = storages.images[white_tex_].first;
                img_infos.push_back(Image_GetDescriptorImageInfo(api, white_main));
            }

            int k = 0;
            for (; k < int(mat_cold.params.size()); ++k) {
                material_data[rel_i].params[k] = mat_cold.params[k];
            }
            for (; k < MAX_MATERIAL_PARAMS; ++k) {
                material_data[rel_i].params[k] = Ren::Vec4f{0.0f};
            }
        } else {
            for (int j = 0; j < MAX_TEX_PER_MATERIAL; ++j) {
                material_data[rel_i].texture_indices[j] = i * MAX_TEX_PER_MATERIAL + j;

                const Ren::ImageMain &error_main = storages.images[error_tex_].first;
                img_infos.push_back(Image_GetDescriptorImageInfo(api, error_main));
            }
        }
    }

    if (!img_transitions.empty()) {
        TransitionResourceStates(api, ren_ctx_.storages(), ren_ctx_.current_cmd_buf(), Ren::AllStages, Ren::AllStages,
                                 img_transitions);
    }

    if (!img_infos.empty()) {
        for (uint32_t i = update_range.first; i < update_range.second; ++i) {
            const uint32_t rel_i = i - update_range.first;

            const uint32_t set_index = i / materials_per_descriptor;
            const uint32_t arr_offset = i % materials_per_descriptor;
            // const uint32_t arr_count = (materials_per_descriptor - arr_offset);

            VkWriteDescriptorSet descr_write = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            descr_write.dstSet = scene_data_.persistent_data->textures_descr_sets[ren_ctx_.backend_frame()][set_index];
            descr_write.dstBinding = BIND_BINDLESS_TEX;
            descr_write.dstArrayElement = uint32_t(arr_offset * MAX_TEX_PER_MATERIAL);
            descr_write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
            descr_write.descriptorCount = uint32_t(MAX_TEX_PER_MATERIAL);
            descr_write.pImageInfo = img_infos.cdata() + rel_i * MAX_TEX_PER_MATERIAL;

            // TODO: group this calls!!!
            api.vkUpdateDescriptorSets(api.device, 1, &descr_write, 0, nullptr);

            if (ren_ctx_.capabilities.hwrt) {
                descr_write.dstSet = scene_data_.persistent_data->rt_textures_descr_sets[ren_ctx_.backend_frame()];
                api.vkUpdateDescriptorSets(api.device, 1, &descr_write, 0, nullptr);
            }
            if (ren_ctx_.capabilities.hwrt || ren_ctx_.capabilities.swrt) {
                descr_write.dstSet =
                    scene_data_.persistent_data->rt_inline_textures_descr_sets[ren_ctx_.backend_frame()];
                api.vkUpdateDescriptorSets(api.device, 1, &descr_write, 0, nullptr);
            }
        }
    }

    Buffer_Unmap(api, materials_upload_buf_main, materials_upload_buf_cold);

    Buffer_UpdateSubRegion(api, mat_buf_main, mat_buf_cold, update_range.first * sizeof(material_data_t),
                           (update_range.second - update_range.first) * sizeof(material_data_t),
                           materials_upload_buf_main, 0, ren_ctx_.current_cmd_buf());

    update_range = std::pair{std::numeric_limits<uint32_t>::max(), 0};

    return false;
}

Ren::AccStructHandle Eng::SceneManager::Build_HWRT_BLAS(const AccStructure &acc) {
    using namespace SceneManagerInternal;

    Ren::ApiContext &api = ren_ctx_.api();
    const Ren::StoragesRef &storages = ren_ctx_.storages();

    const auto &[mesh_main, mesh_cold] = storages.meshes[acc.mesh];
    const Ren::BufferRange &attribs = mesh_main.attribs_buf1, &indices = mesh_main.indices_buf;

    VkAccelerationStructureGeometryTrianglesDataKHR tri_data = {
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR};
    tri_data.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
    tri_data.vertexData.deviceAddress = Buffer_GetDeviceAddress(api, storages.buffers[attribs.buf].first);
    tri_data.vertexStride = 16;
    tri_data.indexType = VK_INDEX_TYPE_UINT32;
    tri_data.indexData.deviceAddress = Buffer_GetDeviceAddress(api, storages.buffers[indices.buf].first);
    tri_data.maxVertex = uint32_t(mesh_cold.attribs.size() / 13);

    //
    // Gather geometries
    //
    Ren::SmallVector<VkAccelerationStructureGeometryKHR, 16> geometries;
    Ren::SmallVector<VkAccelerationStructureBuildRangeInfoKHR, 16> build_ranges;
    Ren::SmallVector<uint32_t, 16> prim_counts;

    const uint32_t indices_start = indices.sub.offset;
    const Ren::Span<const Ren::tri_group_t> groups = mesh_cold.groups;
    for (int j = 0; j < int(groups.size()); ++j) {
        const Ren::tri_group_t &grp = groups[j];
        const Ren::MaterialHandle front_mat =
            (j >= acc.material_override.size()) ? grp.front_mat : acc.material_override[j][0];
        const Ren::MaterialMain &front_main = storages.materials[front_mat].first;

        const Ren::MaterialHandle back_mat =
            (j >= acc.material_override.size()) ? grp.back_mat : acc.material_override[j][1];
        const Ren::MaterialMain &back_main = storages.materials[back_mat].first;

        const Ren::MaterialHandle vol_mat =
            (j >= acc.material_override.size()) ? grp.vol_mat : acc.material_override[j][2];

        auto &new_geo = geometries.emplace_back();
        new_geo = {VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR};
        new_geo.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
        new_geo.flags = 0;
        if (front_mat && back_mat && !(front_main.flags & Ren::eMatFlags::AlphaTest) &&
            !(back_main.flags & Ren::eMatFlags::AlphaTest) && !(front_main.flags & Ren::eMatFlags::AlphaBlend) &&
            !(back_main.flags & Ren::eMatFlags::AlphaBlend)) {
            new_geo.flags |= VK_GEOMETRY_OPAQUE_BIT_KHR;
        }
        if (vol_mat) {
            new_geo.flags |= VK_GEOMETRY_OPAQUE_BIT_KHR;
        }
        new_geo.geometry.triangles = tri_data;

        auto &new_range = build_ranges.emplace_back();
        new_range.firstVertex = attribs.sub.offset / 16;
        new_range.primitiveCount = grp.num_indices / 3;
        new_range.primitiveOffset = indices_start + grp.byte_offset;
        new_range.transformOffset = 0;

        prim_counts.push_back(grp.num_indices / 3);
    }

    //
    // Query needed memory
    //
    VkAccelerationStructureBuildGeometryInfoKHR build_info = {
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR};
    build_info.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    build_info.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    build_info.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR |
                       VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR;
    build_info.geometryCount = uint32_t(geometries.size());
    build_info.pGeometries = geometries.cdata();

    VkAccelerationStructureBuildSizesInfoKHR size_info = {
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR};
    api.vkGetAccelerationStructureBuildSizesKHR(api.device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
                                                &build_info, prim_counts.cdata(), &size_info);

    // make sure we will not use this potentially stale pointer
    build_info.pGeometries = nullptr;

    const auto needed_total_acc_struct_size =
        uint32_t(align_up(size_info.accelerationStructureSize, AccStructAlignment));

    Ren::BufferMain acc_structs_buf_main = {};
    Ren::BufferCold acc_structs_buf_cold = {};
    if (!Buffer_Init(api, acc_structs_buf_main, acc_structs_buf_cold, Ren::String{"BLAS Before-Compaction Buf"},
                     Ren::eBufType::AccStructure, needed_total_acc_struct_size, ren_ctx_.log())) {
        ren_ctx_.log()->Error("Failed to initialize %s", acc_structs_buf_cold.name.c_str());
        return {};
    }
    SCOPE_EXIT({ Buffer_DestroyImmediately(api, acc_structs_buf_main, acc_structs_buf_cold); })

    VkQueryPoolCreateInfo query_pool_create_info = {VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};
    query_pool_create_info.queryCount = 1;
    query_pool_create_info.queryType = VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR;

    VkQueryPool query_pool = {};
    VkResult res = api.vkCreateQueryPool(api.device, &query_pool_create_info, nullptr, &query_pool);
    if (res != VK_SUCCESS) {
        ren_ctx_.log()->Error("Failed to create query pool!");
        return {};
    }
    SCOPE_EXIT({ api.vkDestroyQueryPool(api.device, query_pool, nullptr); })

    VkAccelerationStructureKHR blas_before_compaction = {};

    { // Submit build commands
        const auto needed_build_scratch_size = uint32_t(size_info.buildScratchSize);

        Ren::BufferMain scratch_buf_main = {};
        Ren::BufferCold scratch_buf_cold = {};
        if (!Buffer_Init(api, scratch_buf_main, scratch_buf_cold, Ren::String{"BLAS Scratch Buf"},
                         Ren::eBufType::Storage, needed_build_scratch_size, ren_ctx_.log())) {
            ren_ctx_.log()->Error("Failed to initialize %s", scratch_buf_cold.name.c_str());
            return {};
        }
        SCOPE_EXIT({ Buffer_DestroyImmediately(api, scratch_buf_main, scratch_buf_cold); })

        VkAccelerationStructureCreateInfoKHR acc_create_info = {
            VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR};
        acc_create_info.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
        acc_create_info.buffer = acc_structs_buf_main.buf;
        acc_create_info.offset = 0;
        acc_create_info.size = size_info.accelerationStructureSize;

        VkResult _res =
            api.vkCreateAccelerationStructureKHR(api.device, &acc_create_info, nullptr, &blas_before_compaction);
        if (_res != VK_SUCCESS) {
            ren_ctx_.log()->Error("Failed to create acceleration structure!");
            return {};
        }

        build_info.pGeometries = geometries.cdata();

        build_info.dstAccelerationStructure = blas_before_compaction;
        build_info.scratchData.deviceAddress = Buffer_GetDeviceAddress(api, scratch_buf_main);

        VkCommandBuffer cmd_buf = api.BegSingleTimeCommands();

        api.vkCmdResetQueryPool(cmd_buf, query_pool, 0, 1);

        const VkAccelerationStructureBuildRangeInfoKHR *_build_ranges = build_ranges.cdata();
        api.vkCmdBuildAccelerationStructuresKHR(cmd_buf, 1, &build_info, &_build_ranges);

        { // Place barrier
            VkMemoryBarrier scr_buf_barrier = {VK_STRUCTURE_TYPE_MEMORY_BARRIER};
            scr_buf_barrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
            scr_buf_barrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;

            api.vkCmdPipelineBarrier(cmd_buf, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                                     VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, 0, 1, &scr_buf_barrier, 0,
                                     nullptr, 0, nullptr);
        }

        api.vkCmdWriteAccelerationStructuresPropertiesKHR(cmd_buf, 1, &build_info.dstAccelerationStructure,
                                                          VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR,
                                                          query_pool, 0);

        api.EndSingleTimeCommands(cmd_buf);
    }

    VkDeviceSize compact_size = {};
    res = api.vkGetQueryPoolResults(api.device, query_pool, 0, 1, sizeof(VkDeviceSize), &compact_size,
                                    sizeof(VkDeviceSize), VK_QUERY_RESULT_WAIT_BIT);
    if (res != VK_SUCCESS) {
        ren_ctx_.log()->Error("Failed to query compacted structure size!");
        return {};
    }

    Ren::FreelistAlloc::Allocation mem_alloc =
        scene_data_.persistent_data->hwrt.rt_blas_mem_alloc.Alloc(AccStructAlignment, uint32_t(compact_size));
    if (mem_alloc.offset == 0xffffffff) {
        // allocate one more buffer
        const uint32_t buf_size = std::max(next_power_of_two(uint32_t(compact_size)), Eng::RtBLASChunkSize);
        const std::string buf_name =
            "RT BLAS Buffer #" + std::to_string(scene_data_.persistent_data->hwrt.rt_blas_buffers.size());
        scene_data_.persistent_data->hwrt.rt_blas_buffers.emplace_back(
            ren_ctx_.CreateBuffer(Ren::String{buf_name}, Ren::eBufType::AccStructure, buf_size));
        const uint16_t pool_index = scene_data_.persistent_data->hwrt.rt_blas_mem_alloc.AddPool(buf_size);
        if (pool_index != scene_data_.persistent_data->hwrt.rt_blas_buffers.size() - 1) {
            ren_ctx_.log()->Error("Invalid pool index!");
            return {};
        }
        // try to allocate again
        mem_alloc =
            scene_data_.persistent_data->hwrt.rt_blas_mem_alloc.Alloc(AccStructAlignment, uint32_t(compact_size));
        assert(mem_alloc.offset != 0xffffffff);
    }

    const Ren::AccStructHandle compacted_blas = ren_ctx_.CreateAccStruct();

    { // Submit compaction commands
        VkCommandBuffer cmd_buf = api.BegSingleTimeCommands();

        const auto &[buf_main, buf_cold] =
            storages.buffers[scene_data_.persistent_data->hwrt.rt_blas_buffers[mem_alloc.pool]];

        VkAccelerationStructureCreateInfoKHR acc_create_info = {
            VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR};
        acc_create_info.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
        acc_create_info.buffer = buf_main.buf;
        acc_create_info.offset = mem_alloc.offset;
        acc_create_info.size = compact_size;

        VkAccelerationStructureKHR compact_acc_struct;
        const VkResult _res =
            api.vkCreateAccelerationStructureKHR(api.device, &acc_create_info, nullptr, &compact_acc_struct);
        if (_res != VK_SUCCESS) {
            ren_ctx_.log()->Error("Failed to create acceleration structure!");
        }

        VkCopyAccelerationStructureInfoKHR copy_info = {VK_STRUCTURE_TYPE_COPY_ACCELERATION_STRUCTURE_INFO_KHR};
        copy_info.mode = VK_COPY_ACCELERATION_STRUCTURE_MODE_COMPACT_KHR;
        copy_info.src = blas_before_compaction;
        copy_info.dst = compact_acc_struct;

        api.vkCmdCopyAccelerationStructureKHR(cmd_buf, &copy_info);

        api.EndSingleTimeCommands(cmd_buf);

        const auto &[blas_main, blas_cold] = storages.acc_structs[compacted_blas];
        if (!AccStruct_Init(blas_main, blas_cold, mesh_cold.name, compact_acc_struct, mem_alloc)) {
            ren_ctx_.ReleaseAccStruct(compacted_blas);
            ren_ctx_.log()->Error("Blas compaction failed!");
            return {};
        }

        api.vkDestroyAccelerationStructureKHR(api.device, blas_before_compaction, nullptr);
    }

    scene_data_.persistent_data->rt_blases.push_back(compacted_blas);

    return compacted_blas;
}

void Eng::SceneManager::Alloc_HWRT_TLAS() {
    using namespace SceneManagerInternal;

    const Ren::ApiContext &api = ren_ctx_.api();
    const Ren::StoragesRef &storages = ren_ctx_.storages();

    const uint32_t max_instance_count = MAX_RT_OBJ_INSTANCES_TOTAL; // allocate for worst case

    VkAccelerationStructureGeometryInstancesDataKHR instances_data = {
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR};
    instances_data.data.deviceAddress = {};

    VkAccelerationStructureGeometryKHR tlas_geo = {VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR};
    tlas_geo.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
    tlas_geo.geometry.instances = instances_data;

    VkAccelerationStructureBuildGeometryInfoKHR tlas_build_info = {
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR};
    tlas_build_info.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR |
                            VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR;
    tlas_build_info.geometryCount = 1;
    tlas_build_info.pGeometries = &tlas_geo;
    tlas_build_info.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    tlas_build_info.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    tlas_build_info.srcAccelerationStructure = VK_NULL_HANDLE;

    VkAccelerationStructureBuildSizesInfoKHR size_info = {
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR};
    api.vkGetAccelerationStructureBuildSizesKHR(api.device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
                                                &tlas_build_info, &max_instance_count, &size_info);

    scene_data_.persistent_data->rt_tlas_buf[int(eTLASIndex::Main)] = ren_ctx_.CreateBuffer(
        Ren::String{"TLAS Buf"}, Ren::eBufType::AccStructure, uint32_t(size_info.accelerationStructureSize));
    scene_data_.persistent_data->rt_tlas_buf[int(eTLASIndex::Shadow)] = ren_ctx_.CreateBuffer(
        Ren::String{"TLAS Shadow Buf"}, Ren::eBufType::AccStructure, uint32_t(size_info.accelerationStructureSize));
    scene_data_.persistent_data->rt_tlas_buf[int(eTLASIndex::Volume)] = ren_ctx_.CreateBuffer(
        Ren::String{"TLAS Volume Buf"}, Ren::eBufType::AccStructure, uint32_t(size_info.accelerationStructureSize));

    { // Main TLAS
        const auto &[buf_main, buf_cold] =
            storages.buffers[scene_data_.persistent_data->rt_tlas_buf[int(eTLASIndex::Main)]];

        VkAccelerationStructureCreateInfoKHR create_info = {VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR};
        create_info.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
        create_info.buffer = buf_main.buf;
        create_info.offset = 0;
        create_info.size = size_info.accelerationStructureSize;

        VkAccelerationStructureKHR tlas_handle;
        VkResult res = api.vkCreateAccelerationStructureKHR(api.device, &create_info, nullptr, &tlas_handle);
        if (res != VK_SUCCESS) {
            ren_ctx_.log()->Error("Failed to create acceleration structure!");
        }

        const Ren::AccStructHandle tlas = ren_ctx_.CreateAccStruct();
        const auto &[tlas_main, tlas_cold] = storages.acc_structs[tlas];
        if (!AccStruct_Init(tlas_main, tlas_cold, Ren::String{"TLAS Main"}, tlas_handle, {})) {
            ren_ctx_.log()->Error("Failed to init TLAS!");
        }
        scene_data_.persistent_data->rt_tlases[int(eTLASIndex::Main)] = tlas;
    }
    { // Shadow TLAS
        const auto &[buf_main, buf_cold] =
            storages.buffers[scene_data_.persistent_data->rt_tlas_buf[int(eTLASIndex::Shadow)]];

        VkAccelerationStructureCreateInfoKHR create_info = {VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR};
        create_info.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
        create_info.buffer = buf_main.buf;
        create_info.offset = 0;
        create_info.size = size_info.accelerationStructureSize;

        VkAccelerationStructureKHR tlas_handle;
        VkResult res = api.vkCreateAccelerationStructureKHR(api.device, &create_info, nullptr, &tlas_handle);
        if (res != VK_SUCCESS) {
            ren_ctx_.log()->Error("Failed to create acceleration structure!");
        }

        const Ren::AccStructHandle tlas = ren_ctx_.CreateAccStruct();
        const auto &[tlas_main, tlas_cold] = storages.acc_structs[tlas];
        if (!AccStruct_Init(tlas_main, tlas_cold, Ren::String{"TLAS Shadow"}, tlas_handle, {})) {
            ren_ctx_.log()->Error("Failed to init TLAS!");
        }
        scene_data_.persistent_data->rt_tlases[int(eTLASIndex::Shadow)] = tlas;
    }
    { // Volume TLAS
        const auto &[buf_main, buf_cold] =
            storages.buffers[scene_data_.persistent_data->rt_tlas_buf[int(eTLASIndex::Volume)]];

        VkAccelerationStructureCreateInfoKHR create_info = {VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR};
        create_info.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
        create_info.buffer = buf_main.buf;
        create_info.offset = 0;
        create_info.size = size_info.accelerationStructureSize;

        VkAccelerationStructureKHR tlas_handle;
        VkResult res = api.vkCreateAccelerationStructureKHR(api.device, &create_info, nullptr, &tlas_handle);
        if (res != VK_SUCCESS) {
            ren_ctx_.log()->Error("Failed to create acceleration structure!");
        }

        const Ren::AccStructHandle tlas = ren_ctx_.CreateAccStruct();
        const auto &[tlas_main, tlas_cold] = storages.acc_structs[tlas];
        if (!AccStruct_Init(tlas_main, tlas_cold, Ren::String{"TLAS Volume"}, tlas_handle, {})) {
            ren_ctx_.log()->Error("Failed to init TLAS!");
        }
        scene_data_.persistent_data->rt_tlases[int(eTLASIndex::Volume)] = tlas;
    }

    scene_data_.persistent_data->hwrt.rt_tlas_build_scratch_size = uint32_t(size_info.buildScratchSize);
}
