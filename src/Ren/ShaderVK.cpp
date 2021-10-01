#include "ShaderVK.h"

//#include "GL.h"
#include "Log.h"

#include "SPIRV-Reflect/spirv_reflect.h"

namespace Ren {
void ParseGLSLBindings(const char *shader_str, SmallVectorImpl<Descr> &attr_bindings,
                       SmallVectorImpl<Descr> &unif_bindings, SmallVectorImpl<Descr> &blck_bindings, ILog *log);
bool IsMainThread();

const VkShaderStageFlagBits g_shader_stages_vk[] = {
    VK_SHADER_STAGE_VERTEX_BIT,                  // Vert
    VK_SHADER_STAGE_FRAGMENT_BIT,                // Frag
    VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,    // Tesc
    VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT, // Tese
    VK_SHADER_STAGE_COMPUTE_BIT,                 // Comp
    VK_SHADER_STAGE_RAYGEN_BIT_KHR,              // RayGen
    VK_SHADER_STAGE_MISS_BIT_KHR,                // Miss
    VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,         // ClosestHit
    VK_SHADER_STAGE_ANY_HIT_BIT_KHR,             // AnyHit
    VK_SHADER_STAGE_INTERSECTION_BIT_KHR         // Intersection
};
static_assert(COUNT_OF(g_shader_stages_vk) == int(eShaderType::_Count), "!");

// TODO: not rely on this order somehow
static_assert(int(eShaderType::RayGen) < int(eShaderType::Miss), "!");
static_assert(int(eShaderType::Miss) < int(eShaderType::ClosestHit), "!");
static_assert(int(eShaderType::ClosestHit) < int(eShaderType::AnyHit), "!");
static_assert(int(eShaderType::AnyHit) < int(eShaderType::Intersection), "!");
} // namespace Ren

Ren::Shader::Shader(const char *name, ApiContext *api_ctx, const char *shader_src, eShaderType type,
                    eShaderLoadStatus *status, ILog *log) {
    name_ = String{name};
    device_ = api_ctx->device;
    Init(shader_src, type, status, log);
}

Ren::Shader::Shader(const char *name, ApiContext *api_ctx, const uint8_t *shader_code, const int code_size,
                    const eShaderType type, eShaderLoadStatus *status, ILog *log) {
    name_ = String{name};
    device_ = api_ctx->device;
    Init(shader_code, code_size, type, status, log);
}

Ren::Shader::~Shader() {
    if (module_) {
        assert(IsMainThread());
        vkDestroyShaderModule(device_, module_, nullptr);
    }
}

Ren::Shader &Ren::Shader::operator=(Shader &&rhs) noexcept {
    RefCounter::operator=(static_cast<RefCounter &&>(rhs));

    if (module_) {
        assert(IsMainThread());
        vkDestroyShaderModule(device_, module_, nullptr);
    }

    device_ = exchange(rhs.device_, VkDevice(VK_NULL_HANDLE));
    module_ = exchange(rhs.module_, VkShaderModule(VK_NULL_HANDLE));
    type_ = rhs.type_;
    name_ = std::move(rhs.name_);

    attr_bindings = std::move(rhs.attr_bindings);
    unif_bindings = std::move(rhs.unif_bindings);
    pc_ranges = std::move(rhs.pc_ranges);

    return (*this);
}

void Ren::Shader::Init(const char *shader_src, eShaderType type, eShaderLoadStatus *status, ILog *log) {}

void Ren::Shader::Init(const uint8_t *shader_code, const int code_size, const eShaderType type,
                       eShaderLoadStatus *status, ILog *log) {
    assert(IsMainThread());
    InitFromSPIRV(shader_code, code_size, type, status, log);

#ifdef ENABLE_OBJ_LABELS
    VkDebugUtilsObjectNameInfoEXT name_info = {VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT};
    name_info.objectType = VK_OBJECT_TYPE_SHADER_MODULE;
    name_info.objectHandle = uint64_t(module_);
    name_info.pObjectName = name_.c_str();
    vkSetDebugUtilsObjectNameEXT(device_, &name_info);
#endif
}

void Ren::Shader::InitFromSPIRV(const uint8_t *shader_code, const int code_size, const eShaderType type,
                                eShaderLoadStatus *status, ILog *log) {
    if (!shader_code) {
        (*status) = eShaderLoadStatus::SetToDefault;
        return;
    }

    type_ = type;

    { // init module
        VkShaderModuleCreateInfo create_info = {VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
        create_info.codeSize = static_cast<size_t>(code_size);
        create_info.pCode = reinterpret_cast<const uint32_t *>(shader_code);

        const VkResult res = vkCreateShaderModule(device_, &create_info, nullptr, &module_);
        if (res != VK_SUCCESS) {
            log->Error("Failed to create shader module!");
            (*status) = eShaderLoadStatus::Error;
            return;
        }
    }

    SpvReflectShaderModule module = {};
    const SpvReflectResult res = spvReflectCreateShaderModule(code_size, shader_code, &module);
    if (res != SPV_REFLECT_RESULT_SUCCESS) {
        log->Error("Failed to reflect shader module!");
        (*status) = eShaderLoadStatus::Error;
        return;
    }

    attr_bindings.clear();
    unif_bindings.clear();
    pc_ranges.clear();

    for (uint32_t i = 0; i < module.input_variable_count; i++) {
        const auto *var = module.input_variables[i];
        if (var->built_in == -1) {
            Descr &new_item = attr_bindings.emplace_back();
            new_item.name = String{var->name};
            new_item.loc = var->location;
            new_item.format = VkFormat(var->format);
        }
    }

    for (uint32_t i = 0; i < module.descriptor_binding_count; i++) {
        const auto &desc = module.descriptor_bindings[i];
        Descr &new_item = unif_bindings.emplace_back();
        new_item.name = String{desc.name};
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

    (*status) = eShaderLoadStatus::CreatedFromData;
    spvReflectDestroyShaderModule(&module);
}

void Ren::ParseGLSLBindings(const char *shader_str, SmallVectorImpl<Descr> &attr_bindings,
                            SmallVectorImpl<Descr> &unif_bindings, SmallVectorImpl<Descr> &blck_bindings, ILog *log) {
    const char *delims = " \r\n\t";
    const char *p = strstr(shader_str, "/*");
    const char *q = p ? strpbrk(p + 2, delims) : nullptr;

    SmallVectorImpl<Descr> *cur_bind_target = nullptr;
    for (; p != nullptr && q != nullptr; q = strpbrk(p, delims)) {
        if (p == q) {
            p = q + 1;
            continue;
        }

        std::string item(p, q);
        if (item == "/*") {
            cur_bind_target = nullptr;
        } else if (item == "*/" && cur_bind_target) {
            break;
        } else if (item == "ATTRIBUTES") {
            cur_bind_target = &attr_bindings;
        } else if (item == "UNIFORMS") {
            cur_bind_target = &unif_bindings;
        } else if (item == "UNIFORM_BLOCKS") {
            cur_bind_target = &blck_bindings;
        } else if (cur_bind_target) {
            p = q + 1;
            q = strpbrk(p, delims);
            if (*p != ':') {
                log->Error("Error parsing shader!");
            }
            p = q + 1;
            q = strpbrk(p, delims);
            int loc = std::atoi(p);

            Descr &new_item = cur_bind_target->emplace_back();
            new_item.name = String{item.c_str()};
            new_item.loc = loc;
        }

        if (!q) {
            break;
        }
        p = q + 1;
    }
}