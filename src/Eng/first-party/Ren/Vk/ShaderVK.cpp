#include "ShaderVK.h"

#include "../Config.h"
#include "../Log.h"
#include "VKCtx.h"

#include "SPIRV-Reflect/spirv_reflect.h"

namespace Ren {
extern const VkShaderStageFlagBits g_shader_stages_vk[] = {
    VK_SHADER_STAGE_VERTEX_BIT,                  // Vert
    VK_SHADER_STAGE_FRAGMENT_BIT,                // Frag
    VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,    // Tesc
    VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT, // Tese
    VK_SHADER_STAGE_GEOMETRY_BIT,                // Geometry
    VK_SHADER_STAGE_COMPUTE_BIT,                 // Comp
    VK_SHADER_STAGE_RAYGEN_BIT_KHR,              // RayGen
    VK_SHADER_STAGE_MISS_BIT_KHR,                // Miss
    VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,         // ClosestHit
    VK_SHADER_STAGE_ANY_HIT_BIT_KHR,             // AnyHit
    VK_SHADER_STAGE_INTERSECTION_BIT_KHR         // Intersection
};
static_assert(std::size(g_shader_stages_vk) == int(eShaderType::_Count));

// TODO: not rely on this order somehow
static_assert(int(eShaderType::RayGen) < int(eShaderType::Miss));
static_assert(int(eShaderType::Miss) < int(eShaderType::ClosestHit));
static_assert(int(eShaderType::ClosestHit) < int(eShaderType::AnyHit));
static_assert(int(eShaderType::AnyHit) < int(eShaderType::Intersection));
} // namespace Ren

bool Ren::Shader_Init(const ApiContext &api, ShaderMain &shader_main, ShaderCold &shader_cold,
                      Span<const uint8_t> shader_code, const String &name, const eShaderType type, ILog *log) {
    if (shader_code.empty()) {
        return false;
    }

    assert(shader_main.vk_module == VK_NULL_HANDLE);

    shader_cold.name = name;
    shader_cold.type = type;

    { // init module
        VkShaderModuleCreateInfo create_info = {VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
        create_info.codeSize = shader_code.size();
        create_info.pCode = reinterpret_cast<const uint32_t *>(shader_code.data());

        const VkResult res = api.vkCreateShaderModule(api.device, &create_info, nullptr, &shader_main.vk_module);
        if (res != VK_SUCCESS) {
            log->Error("Failed to create shader module!");
            return false;
        }
#ifdef REN_ENABLE_GPU_DEBUG
        VkDebugUtilsObjectNameInfoEXT name_info = {VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT};
        name_info.objectType = VK_OBJECT_TYPE_SHADER_MODULE;
        name_info.objectHandle = uint64_t(shader_main.vk_module);
        name_info.pObjectName = name.c_str();
        api.vkSetDebugUtilsObjectNameEXT(api.device, &name_info);
#endif
    }

    SpvReflectShaderModule spv_module = {};
    const SpvReflectResult res = spvReflectCreateShaderModule(shader_code.size(), shader_code.data(), &spv_module);
    if (res != SPV_REFLECT_RESULT_SUCCESS) {
        log->Error("Failed to reflect shader module!");
        return false;
    }

    assert(shader_cold.attr_bindings.empty());
    assert(shader_cold.unif_bindings.empty());
    assert(shader_cold.pc_ranges.empty());

    for (uint32_t i = 0; i < spv_module.input_variable_count; i++) {
        const auto *var = spv_module.input_variables[i];
        if (var->built_in == SpvBuiltIn(-1)) {
            Descr &new_item = shader_cold.attr_bindings.emplace_back();
            if (var->name) {
                new_item.name = String{var->name};
            }
            new_item.loc = var->location;
            new_item.format = VkFormat(var->format);
        }
    }

    for (uint32_t i = 0; i < spv_module.descriptor_binding_count; i++) {
        const auto &desc = spv_module.descriptor_bindings[i];
        Descr &new_item = shader_cold.unif_bindings.emplace_back();
        if (desc.name) {
            new_item.name = String{desc.name};
        }
        new_item.desc_type = VkDescriptorType(desc.descriptor_type);
        new_item.loc = desc.binding;
        new_item.set = desc.set;
        new_item.count = desc.count;
        if (desc.descriptor_type == SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLED_IMAGE && desc.count == 1 &&
            (desc.type_description->op == SpvOpTypeRuntimeArray || desc.type_description->op == SpvOpTypeArray)) {
            new_item.flags |= eDescrFlags::UnboundedArray;
        }
        if (desc.block.decoration_flags & SPV_REFLECT_DECORATION_NON_WRITABLE) {
            new_item.flags |= eDescrFlags::ReadOnly;
        }
    }

    std::sort(std::begin(shader_cold.unif_bindings), std::end(shader_cold.unif_bindings),
              [](const Descr &lhs, const Descr &rhs) {
                  if (lhs.set == rhs.set) {
                      return (lhs.loc < rhs.loc);
                  }
                  return (lhs.set < rhs.set);
              });

    for (uint32_t i = 0; i < spv_module.push_constant_block_count; ++i) {
        const auto &blck = spv_module.push_constant_blocks[i];
        shader_cold.pc_ranges.push_back({uint16_t(blck.offset), uint16_t(blck.size)});
    }

    spvReflectDestroyShaderModule(&spv_module);

    return true;
}

void Ren::Shader_Destroy(const ApiContext &api, ShaderMain &shader_main, ShaderCold &shader_cold) {
    if (shader_main.vk_module) {
        api.vkDestroyShaderModule(api.device, shader_main.vk_module, nullptr);
    }
    shader_main = {};
    shader_cold = {};
}
