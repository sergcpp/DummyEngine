#include "ShaderVK.h"

#include "Config.h"
#include "Log.h"
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

Ren::Shader::Shader(std::string_view name, ApiContext *api_ctx, Span<const uint8_t> shader_code, const eShaderType type,
                    ILog *log) {
    name_ = String{name};
    api_ctx_ = api_ctx;
    Init(shader_code, type, log);
}

Ren::Shader::~Shader() {
    if (module_) {
        api_ctx_->vkDestroyShaderModule(api_ctx_->device, module_, nullptr);
    }
}

Ren::Shader &Ren::Shader::operator=(Shader &&rhs) noexcept {
    RefCounter::operator=(static_cast<RefCounter &&>(rhs));

    if (module_) {
        api_ctx_->vkDestroyShaderModule(api_ctx_->device, module_, nullptr);
    }

    api_ctx_ = std::exchange(rhs.api_ctx_, nullptr);
    module_ = std::exchange(rhs.module_, VkShaderModule(VK_NULL_HANDLE));
    type_ = rhs.type_;
    name_ = std::move(rhs.name_);

    attr_bindings = std::move(rhs.attr_bindings);
    unif_bindings = std::move(rhs.unif_bindings);
    pc_ranges = std::move(rhs.pc_ranges);

    return (*this);
}

void Ren::Shader::Init(Span<const uint8_t> shader_code, const eShaderType type, ILog *log) {
    InitFromSPIRV(shader_code, type, log);
#ifdef ENABLE_GPU_DEBUG
    if (module_ != VkShaderModule{}) {
        VkDebugUtilsObjectNameInfoEXT name_info = {VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT};
        name_info.objectType = VK_OBJECT_TYPE_SHADER_MODULE;
        name_info.objectHandle = uint64_t(module_);
        name_info.pObjectName = name_.c_str();
        api_ctx_->vkSetDebugUtilsObjectNameEXT(api_ctx_->device, &name_info);
    }
#endif
}

void Ren::Shader::InitFromSPIRV(Span<const uint8_t> shader_code, const eShaderType type, ILog *log) {
    if (shader_code.empty()) {
        return;
    }

    type_ = type;

    { // init module
        VkShaderModuleCreateInfo create_info = {VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
        create_info.codeSize = shader_code.size();
        create_info.pCode = reinterpret_cast<const uint32_t *>(shader_code.data());

        const VkResult res = api_ctx_->vkCreateShaderModule(api_ctx_->device, &create_info, nullptr, &module_);
        if (res != VK_SUCCESS) {
            log->Error("Failed to create shader module!");
            return;
        }
    }

    SpvReflectShaderModule module = {};
    const SpvReflectResult res = spvReflectCreateShaderModule(shader_code.size(), shader_code.data(), &module);
    if (res != SPV_REFLECT_RESULT_SUCCESS) {
        log->Error("Failed to reflect shader module!");
        return;
    }

    attr_bindings.clear();
    unif_bindings.clear();
    pc_ranges.clear();

    for (uint32_t i = 0; i < module.input_variable_count; i++) {
        const auto *var = module.input_variables[i];
        if (var->built_in == SpvBuiltIn(-1)) {
            Descr &new_item = attr_bindings.emplace_back();
            if (var->name) {
                new_item.name = String{var->name};
            }
            new_item.loc = var->location;
            new_item.format = VkFormat(var->format);
        }
    }

    for (uint32_t i = 0; i < module.descriptor_binding_count; i++) {
        const auto &desc = module.descriptor_bindings[i];
        Descr &new_item = unif_bindings.emplace_back();
        if (desc.name) {
            new_item.name = String{desc.name};
        }
        new_item.desc_type = VkDescriptorType(desc.descriptor_type);
        new_item.loc = desc.binding;
        new_item.set = desc.set;
        new_item.count = desc.count;
        if (desc.descriptor_type == SPV_REFLECT_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER && desc.count == 1 &&
            (desc.type_description->op == SpvOpTypeRuntimeArray || desc.type_description->op == SpvOpTypeArray)) {
            new_item.unbounded_array = true;
        } else {
            new_item.unbounded_array = false;
        }
    }

    for (uint32_t i = 0; i < module.push_constant_block_count; ++i) {
        const auto &blck = module.push_constant_blocks[i];
        pc_ranges.push_back({blck.offset, blck.size});
    }

    spvReflectDestroyShaderModule(&module);
}
