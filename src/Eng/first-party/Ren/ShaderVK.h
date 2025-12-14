#pragma once

#include <cstdint>

#include "Shader.h"
#include "SmallVector.h"
#include "Span.h"
#include "Storage.h"

namespace Ren {
class ILog;
struct ApiContext;

struct Range {
    uint16_t offset;
    uint16_t size;
};

class Shader : public RefCounter {
    ApiContext *api_ctx_ = nullptr;
    VkShaderModule module_ = {};
    eShaderType type_ = eShaderType::_Count;
    String name_;

    void InitFromSPIRV(Span<const uint8_t> shader_code, eShaderType type, ILog *log);

  public:
    SmallVector<Descr, 16> attr_bindings, unif_bindings;
    SmallVector<Range, 1> pc_ranges;

    Shader(std::string_view name, ApiContext *api_ctx, Span<const uint8_t> shader_code, eShaderType type, ILog *log);

    Shader(const Shader &rhs) = delete;
    Shader(Shader &&rhs) noexcept { (*this) = std::move(rhs); }
    ~Shader();

    Shader &operator=(const Shader &rhs) = delete;
    Shader &operator=(Shader &&rhs) noexcept;

    bool ready() const { return module_ != VkShaderModule{}; }
    VkShaderModule module() const { return module_; }
    eShaderType type() const { return type_; }
    const String &name() const { return name_; }

    void Init(Span<const uint8_t> shader_code, eShaderType type, ILog *log);
};

using ShaderRef = StrongRef<Shader, NamedStorage<Shader>>;
using ShaderStorage = NamedStorage<Shader>;
} // namespace Ren