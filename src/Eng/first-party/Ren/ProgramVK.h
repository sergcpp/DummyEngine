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

class Program : public RefCounter {
    std::array<ShaderRef, int(eShaderType::_Count)> shaders_;
    SmallVector<Attribute, 8> attributes_;
    SmallVector<Uniform, 16> uniforms_;

    ApiContext *api_ctx_ = nullptr;
    SmallVector<VkDescriptorSetLayout, 4> descr_set_layouts_;
    SmallVector<VkPushConstantRange, 8> pc_ranges_;

    bool InitDescrSetLayouts(ILog *log);
    void InitBindings(ILog *log);

    void Destroy();

  public:
    Program() = default;
    Program(ApiContext *api_ctx, ShaderRef vs_ref, ShaderRef fs_ref, ShaderRef tcs_ref, ShaderRef tes_ref,
            ShaderRef gs_ref, ILog *log);
    Program(ApiContext *api_ctx, ShaderRef cs_ref, ILog *log);
    Program(ApiContext *api_ctx, ShaderRef raygen_ref, ShaderRef closesthit_ref, ShaderRef anyhit_ref,
            ShaderRef miss_ref, ShaderRef intersection_ref, ILog *log, int);

    Program(const Program &rhs) = delete;
    Program(Program &&rhs) noexcept { (*this) = std::move(rhs); }
    ~Program();

    Program &operator=(const Program &rhs) = delete;
    Program &operator=(Program &&rhs) noexcept;

    bool has_tessellation() const {
        return shaders_[int(eShaderType::TesselationControl)] && shaders_[int(eShaderType::TesselationEvaluation)];
    }

    bool operator==(const Program &rhs) const { return shaders_ == rhs.shaders_; }
    bool operator!=(const Program &rhs) const { return shaders_ != rhs.shaders_; }
    bool operator<(const Program &rhs) const { return shaders_ < rhs.shaders_; }

    const Attribute &attribute(const int i) const { return attributes_[i]; }
    const Attribute &attribute(std::string_view name) const {
        for (int i = 0; i < int(attributes_.size()); i++) {
            if (attributes_[i].name == name) {
                return attributes_[i];
            }
        }
        return attributes_[0];
    }

    const Uniform &uniform(const int i) const { return uniforms_[i]; }
    const Uniform &uniform(std::string_view name) const {
        for (int i = 0; i < int(uniforms_.size()); i++) {
            if (uniforms_[i].name == name) {
                return uniforms_[i];
            }
        }
        return uniforms_[0];
    }
    const Uniform &uniform_at(const int loc) const {
        int left = 0, right = uniforms_.size() - 1;
        while (left <= right) {
            const int mid = left + (right - left) / 2;
            if (uniforms_[mid].set == 0 && uniforms_[mid].loc == loc) {
                return uniforms_[mid];
            } else if (uniforms_[mid].set == 0 && uniforms_[mid].loc < loc) {
                left = mid + 1;
            } else {
                right = mid - 1;
            }
        }
        return uniforms_[0];
    }

    const std::array<ShaderRef, int(eShaderType::_Count)> &shaders() const { return shaders_; }
    const ShaderRef &shader(eShaderType type) const { return shaders_[int(type)]; }

    Span<const VkDescriptorSetLayout> descr_set_layouts() const { return descr_set_layouts_; }
    Span<const VkPushConstantRange> pc_ranges() const { return pc_ranges_; }
    const VkPushConstantRange &pc_range(const int i) const { return pc_ranges_[i]; }

    void Init(ShaderRef vs_ref, ShaderRef fs_ref, ShaderRef tcs_ref, ShaderRef tes_ref, ShaderRef gs_ref, ILog *log);
    void Init(ShaderRef cs_ref, ILog *log);
    void Init2(ShaderRef raygen_ref, ShaderRef closesthit_ref, ShaderRef anyhit_ref, ShaderRef miss_ref,
               ShaderRef intersection_ref, ILog *log);
};
} // namespace Ren

#ifdef _MSC_VER
#pragma warning(pop)
#endif