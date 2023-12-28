#pragma once

#include <cstdint>

#include "Shader.h"
#include "SmallVector.h"
#include "Storage.h"
#include "VK.h"

namespace Ren {
class ILog;
struct ApiContext;

struct Range {
    uint32_t offset;
    uint32_t size;
};

class Shader : public RefCounter {
    VkDevice device_ = VK_NULL_HANDLE;
    VkShaderModule module_ = VK_NULL_HANDLE;
    eShaderType type_ = eShaderType::_Count;
    String name_;

    void InitFromSPIRV(const uint8_t *shader_code, int code_size, eShaderType type, eShaderLoadStatus *status,
                       ILog *log);

  public:
    SmallVector<Descr, 16> attr_bindings, unif_bindings;
    SmallVector<Range, 4> pc_ranges;

    Shader(const char *name, ApiContext *api_ctx, const char *shader_src, eShaderType type, eShaderLoadStatus *status,
           ILog *log);
    Shader(const char *name, ApiContext *api_ctx, const uint8_t *shader_code, int code_size, eShaderType type,
           eShaderLoadStatus *status, ILog *log);

    Shader(const Shader &rhs) = delete;
    Shader(Shader &&rhs) noexcept { (*this) = std::move(rhs); }
    ~Shader();

    Shader &operator=(const Shader &rhs) = delete;
    Shader &operator=(Shader &&rhs) noexcept;

    bool ready() const { return module_ != VK_NULL_HANDLE; }
    VkShaderModule module() const { return module_; }
    eShaderType type() const { return type_; }
    const String &name() const { return name_; }

    void Init(const char *shader_src, eShaderType type, eShaderLoadStatus *status, ILog *log);
    void Init(const uint8_t *shader_code, int code_size, eShaderType type, eShaderLoadStatus *status, ILog *log);
};

typedef StrongRef<Shader> ShaderRef;
typedef Storage<Shader> ShaderStorage;
} // namespace Ren