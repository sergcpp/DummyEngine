#pragma once

#include <cstdint>

#include "Storage.h"
#include "String.h"

namespace Ren {
class ILog;

const int MaxBindingsCount = 32;
struct Descr {
    String name;
    int loc = -1;
};
typedef Descr Attribute;
typedef Descr Uniform;
typedef Descr UniformBlock;

enum class eShaderType { None, Vert, Frag, Tesc, Tese, Comp, _Count };

enum class eShaderLoadStatus { Found, SetToDefault, CreatedFromData };

class Shader : public RefCounter {
    uint32_t id_ = 0;
    eShaderType type_ = eShaderType::None;
    String name_;

    void InitFromGLSL(const char *shader_src, eShaderType type, eShaderLoadStatus *status,
                      ILog *log);
#ifndef __ANDROID__
    void InitFromSPIRV(const uint8_t *shader_data, int data_size, eShaderType type,
                       eShaderLoadStatus *status, ILog *log);
#endif
  public:
    Descr bindings[3][MaxBindingsCount];
    int bindings_count[3] = {};

    Shader() = default;
    Shader(const char *name, const char *shader_src, eShaderType type,
           eShaderLoadStatus *status, ILog *log);
#ifndef __ANDROID__
    Shader(const char *name, const uint8_t *shader_data, int data_size, eShaderType type,
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

    void Init(const char *shader_src, eShaderType type, eShaderLoadStatus *status,
              ILog *log);
#ifndef __ANDROID__
    void Init(const uint8_t *shader_data, int data_size, eShaderType type,
              eShaderLoadStatus *status, ILog *log);
#endif
};

typedef StrongRef<Shader> ShaderRef;
typedef Storage<Shader> ShaderStorage;
} // namespace Ren