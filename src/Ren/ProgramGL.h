#pragma once

#include <cstdint>
#include <cstring>

#include <array>
#include <string>

#include "Storage.h"
#include "String.h"

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4996)
#endif

namespace Ren {
class ILog;

const int MaxAttributesCount = 32;
const int MaxUniformsCount = 32;
const int MaxUniformBlocksCount = 16;
struct Descr {
    String name;
    int loc = -1;
};
typedef Descr Attribute;
typedef Descr Uniform;
typedef Descr UniformBlock;

enum class eProgFlags {
    VertShaderPresent = (1u << 0u),
    FragShaderPresent = (1u << 1u),
    TescShaderPresent = (1u << 2u),
    TeseShaderPresent = (1u << 3u),
    CompShaderPresent = (1u << 4u)
};

enum class eProgLoadStatus { Found, SetToDefault, CreatedFromData };

class Program : public RefCounter {
    uint32_t prog_id_ = 0;
    uint32_t flags_ = 0;
    std::array<Attribute, MaxAttributesCount> attributes_;
    std::array<Uniform, MaxUniformsCount> uniforms_;
    std::array<UniformBlock, MaxUniformBlocksCount> uniform_blocks_;
    bool ready_ = false;
    String name_;

    struct ShadersSrc {
        const char *vs_source, *fs_source;
        const char *tcs_source, *tes_source;
        const char *cs_source;
    };

    struct ShadersBin {
        const uint8_t *vs_data;
        const int vs_data_size;
        const uint8_t *fs_data;
        const int fs_data_size;
        const uint8_t *cs_data;
        const int cs_data_size;
    };

    void InitFromGLSL(const ShadersSrc &shaders, eProgLoadStatus *status, ILog *log);
#ifndef __ANDROID__
    void InitFromSPIRV(const ShadersBin &shaders, eProgLoadStatus *status, ILog *log);
#endif
  public:
    Program() = default;
    Program(const char *name, uint32_t prog_id, const Attribute *attrs,
            const Uniform *unifs, const UniformBlock *unif_blocks)
        : prog_id_(prog_id) {
        for (int i = 0; i < MaxAttributesCount; i++) {
            if (attrs[i].loc == -1)
                break;
            attributes_[i] = attrs[i];
        }
        for (int i = 0; i < MaxUniformsCount; i++) {
            if (unifs[i].loc == -1)
                break;
            uniforms_[i] = unifs[i];
        }
        for (int i = 0; i < MaxUniformBlocksCount; i++) {
            if (unif_blocks[i].loc == -1)
                break;
            uniform_blocks_[i] = unif_blocks[i];
        }
        ready_ = true;
        name_ = String{name};
    }
    Program(const char *name, const char *vs_source, const char *fs_source,
            const char *tcs_source, const char *tes_source, eProgLoadStatus *status,
            ILog *log);
    Program(const char *name, const char *cs_source, eProgLoadStatus *status, ILog *log);
#ifndef __ANDROID__
    Program(const char *name, const uint8_t *vs_data, int vs_data_size,
            const uint8_t *fs_data, int fs_data_size, eProgLoadStatus *status, ILog *log);
    Program(const char *name, const uint8_t *cs_data, int cs_data_size,
            eProgLoadStatus *status, ILog *log);
#endif
    Program(const Program &rhs) = delete;
    Program(Program &&rhs) noexcept { *this = std::move(rhs); }
    ~Program();

    Program &operator=(const Program &rhs) = delete;
    Program &operator=(Program &&rhs) noexcept;

    uint32_t prog_id() const { return prog_id_; }
    uint32_t flags() const { return flags_; }
    bool ready() const { return ready_; }
    bool has_tessellation() const {
        return (flags_ & (uint32_t(eProgFlags::TescShaderPresent) |
                          uint32_t(eProgFlags::TeseShaderPresent))) ==
               (uint32_t(eProgFlags::TescShaderPresent) |
                uint32_t(eProgFlags::TeseShaderPresent));
    }
    const String &name() const { return name_; }

    const Attribute &attribute(int i) const { return attributes_[i]; }

    const Attribute &attribute(const char *name) const {
        for (int i = 0; i < MaxAttributesCount; i++) {
            if (attributes_[i].name == name) {
                return attributes_[i];
            }
        }
        return attributes_[0];
    }

    const Uniform &uniform(int i) const { return uniforms_[i]; }

    const Uniform &uniform(const char *name) const {
        for (int i = 0; i < MaxUniformsCount; i++) {
            if (uniforms_[i].name == name) {
                return uniforms_[i];
            }
        }
        return uniforms_[0];
    }

    const UniformBlock &uniform_block(int i) const { return uniform_blocks_[i]; }

    const UniformBlock &uniform_block(const char *name) const {
        for (int i = 0; i < MaxUniformBlocksCount; i++) {
            if (uniform_blocks_[i].name == name) {
                return uniform_blocks_[i];
            }
        }
        return uniform_blocks_[0];
    }

    void Init(const char *vs_source, const char *fs_source, const char *tcs_source,
              const char *tes_source, eProgLoadStatus *status, ILog *log);
    void Init(const char *cs_source, eProgLoadStatus *status, ILog *log);
#ifndef __ANDROID__
    void Init(const uint8_t *vs_data, int vs_data_size, const uint8_t *fs_data,
              int fs_data_size, eProgLoadStatus *status, ILog *log);
    void Init(const uint8_t *cs_data, int cs_data_size, eProgLoadStatus *status,
              ILog *log);
#endif
};

typedef StorageRef<Program> ProgramRef;
typedef Storage<Program> ProgramStorage;
} // namespace Ren

#ifdef _MSC_VER
#pragma warning(pop)
#endif