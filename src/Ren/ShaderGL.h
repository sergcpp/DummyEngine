#pragma once

#include <cstdint>

#include "SmallVector.h"
#include "Storage.h"
#include "String.h"

namespace Ren {
struct ApiContext;
class ILog;

class Shader : public RefCounter {
    uint32_t id_ = 0;
    eShaderType type_ = eShaderType::_Count;
    String name_;

    void InitFromGLSL(const char *shader_src, eShaderType type, eShaderLoadStatus *status, ILog *log);
#ifndef __ANDROID__
    void InitFromSPIRV(const uint8_t *shader_data, int data_size, eShaderType type, eShaderLoadStatus *status,
                       ILog *log);
#endif
  public:
    SmallVector<Descr, 16> attr_bindings, unif_bindings, blck_bindings;

    Shader(const char *name, ApiContext *api_ctx, const char *shader_src, eShaderType type, eShaderLoadStatus *status,
           ILog *log);
#ifndef __ANDROID__
    Shader(const char *name, ApiContext *api_ctx, const uint8_t *shader_code, int code_size, eShaderType type,
           eShaderLoadStatus *status, ILog *log);
#endif
    Shader(const Shader &rhs) = delete;
    Shader(Shader &&rhs) noexcept { (*this) = std::move(rhs); }
    ~Shader();

    Shader &operator=(const Shader &rhs) = delete;
    Shader &operator=(Shader &&rhs) noexcept;

    bool ready() const { return id_ != 0; }
    uint32_t id() const { return id_; }
    eShaderType type() const { return type_; }
    const String &name() const { return name_; }

    void Init(const char *shader_src, eShaderType type, eShaderLoadStatus *status, ILog *log);
#ifndef __ANDROID__
    void Init(const uint8_t *shader_code, int code_size, eShaderType type, eShaderLoadStatus *status, ILog *log);
#endif
};

typedef StrongRef<Shader> ShaderRef;
typedef Storage<Shader> ShaderStorage;
} // namespace Ren