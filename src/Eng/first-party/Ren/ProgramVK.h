#pragma once

#include <cstdint>
#include <cstring>

#include <array>
#include <string>

#include "Shader.h"
#include "SmallVector.h"
#include "Span.h"
#include "Storage.h"
#include "String.h"

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4996)
#endif

namespace Ren {
class ILog;

enum class eProgLoadStatus { Found, SetToDefault, CreatedFromData };

class Program : public RefCounter {
    uint32_t flags_ = 0;
    std::array<ShaderRef, int(eShaderType::_Count)> shaders_;
    SmallVector<Attribute, 8> attributes_;
    SmallVector<Uniform, 16> uniforms_;
    String name_;

    ApiContext *api_ctx_ = nullptr;
    SmallVector<VkDescriptorSetLayout, 4> descr_set_layouts_;
    SmallVector<VkPushConstantRange, 8> pc_ranges_;

    bool InitDescrSetLayouts(ILog *log);
    void InitBindings(ILog *log);

    void Destroy();

  public:
    Program() = default;
    Program(const char *name, ApiContext *api_ctx, ShaderRef vs_ref, ShaderRef fs_ref, ShaderRef tcs_ref,
            ShaderRef tes_ref, eProgLoadStatus *status, ILog *log);
    Program(const char *name, ApiContext *api_ctx, ShaderRef cs_ref, eProgLoadStatus *status, ILog *log);
    Program(const char *name, ApiContext *api_ctx, ShaderRef raygen_ref, ShaderRef closesthit_ref, ShaderRef anyhit_ref,
            ShaderRef miss_ref, ShaderRef intersection_ref, eProgLoadStatus *status, ILog *log);

    Program(const Program &rhs) = delete;
    Program(Program &&rhs) noexcept { (*this) = std::move(rhs); }
    ~Program();

    Program &operator=(const Program &rhs) = delete;
    Program &operator=(Program &&rhs) noexcept;

    uint32_t flags() const { return flags_; }
    bool ready() const {
        return (shaders_[int(eShaderType::Vertex)] && shaders_[int(eShaderType::Fragment)]) ||
               shaders_[int(eShaderType::Compute)] ||
               (shaders_[int(eShaderType::RayGen)] &&
                (shaders_[int(eShaderType::ClosestHit)] || shaders_[int(eShaderType::AnyHit)]) &&
                shaders_[int(eShaderType::Miss)]);
    }
    bool has_tessellation() const {
        return shaders_[int(eShaderType::TesselationControl)] && shaders_[int(eShaderType::TesselationEvaluation)];
    }
    const String &name() const { return name_; }

    const Attribute &attribute(const int i) const { return attributes_[i]; }

    const Attribute &attribute(const char *name) const {
        for (int i = 0; i < int(attributes_.size()); i++) {
            if (attributes_[i].name == name) {
                return attributes_[i];
            }
        }
        return attributes_[0];
    }

    const Uniform &uniform(const int i) const { return uniforms_[i]; }

    const Uniform &uniform(const char *name) const {
        for (int i = 0; i < int(uniforms_.size()); i++) {
            if (uniforms_[i].name == name) {
                return uniforms_[i];
            }
        }
        return uniforms_[0];
    }

    const ShaderRef &shader(eShaderType type) const { return shaders_[int(type)]; }

    Span<const VkDescriptorSetLayout> descr_set_layouts() const { return descr_set_layouts_; }
    Span<const VkPushConstantRange> pc_ranges() const { return pc_ranges_; }

    void Init(ShaderRef vs_ref, ShaderRef fs_ref, ShaderRef tcs_ref, ShaderRef tes_ref, eProgLoadStatus *status,
              ILog *log);
    void Init(ShaderRef cs_ref, eProgLoadStatus *status, ILog *log);
    void Init(ShaderRef raygen_ref, ShaderRef closesthit_ref, ShaderRef anyhit_ref, ShaderRef miss_ref,
              ShaderRef intersection_ref, eProgLoadStatus *status, ILog *log);
};

typedef StrongRef<Program> ProgramRef;
typedef Storage<Program> ProgramStorage;
} // namespace Ren

#ifdef _MSC_VER
#pragma warning(pop)
#endif