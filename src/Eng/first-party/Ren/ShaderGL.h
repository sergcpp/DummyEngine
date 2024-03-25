#pragma once

#include <cstdint>

#include "Shader.h"
#include "SmallVector.h"
#include "Span.h"
#include "Storage.h"
#include "String.h"

namespace Ren {
struct ApiContext;
class ILog;

class Shader : public RefCounter {
    uint32_t id_ = 0;
    eShaderType type_ = eShaderType::_Count;
    eShaderSource source_ = eShaderSource::_Count;
    String name_;

    void InitFromGLSL(std::string_view shader_src, eShaderType type, eShaderLoadStatus *status, ILog *log);
#ifndef __ANDROID__
    void InitFromSPIRV(Span<const uint8_t> shader_data, eShaderType type, eShaderLoadStatus *status, ILog *log);
#endif
  public:
    SmallVector<Descr, 16> attr_bindings, unif_bindings, blck_bindings;

    Shader(std::string_view name, ApiContext *api_ctx, std::string_view shader_src, eShaderType type,
           eShaderLoadStatus *status, ILog *log);
#ifndef __ANDROID__
    Shader(std::string_view name, ApiContext *api_ctx, Span<const uint8_t> shader_code, eShaderType type,
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
    eShaderSource source() const { return source_; }
    const String &name() const { return name_; }

    void Init(std::string_view shader_src, eShaderType type, eShaderLoadStatus *status, ILog *log);
#ifndef __ANDROID__
    void Init(Span<const uint8_t> shader_code, eShaderType type, eShaderLoadStatus *status, ILog *log);
#endif
};

typedef StrongRef<Shader> ShaderRef;
typedef Storage<Shader> ShaderStorage;
} // namespace Ren