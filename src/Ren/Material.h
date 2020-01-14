#pragma once

#include <cstdint>

#include <functional>

#include "Fwd.h"
#include "Program.h"
#include "Storage.h"
#include "String.h"
#include "Texture.h"

#include "MVec.h"

namespace Ren {
class ILog;

enum eMaterialFlags {
    AlphaTest   = (1u << 0u),
    AlphaBlend  = (1u << 1u),
};

enum eTextureFlags {
    Signed  = (1u << 0u),
    SRGB    = (1u << 1u)
};

const int MaxMaterialProgramCount = 4;
const int MaxMaterialTextureCount = 8;
const int MaxMaterialParamCount = 8;

enum eMatLoadStatus { MatFound, MatSetToDefault, MatCreatedFromData };

typedef std::function<Texture2DRef(const char *name, uint32_t flags)> texture_load_callback;
typedef std::function<ProgramRef(const char *name, const char *arg1, const char *arg2)> program_load_callback;

class Material : public RefCounter {
    uint32_t        flags_ = 0;
    bool            ready_ = false;
    ProgramRef      programs_[MaxMaterialProgramCount];
    Texture2DRef    textures_[MaxMaterialTextureCount];
    Vec4f           params_[MaxMaterialParamCount];

    String          name_;

    void InitFromTXT(const char *mat_src, eMatLoadStatus *status, const program_load_callback &on_prog_load,
                     const texture_load_callback &on_tex_load, ILog *log);
public:
    Material() = default;
    Material(
        const char *name, const char *mat_src, eMatLoadStatus *status,
        const program_load_callback &on_prog_load, const texture_load_callback &on_tex_load, ILog *log);
    Material(const char *name, uint32_t flags, ProgramRef programs[], Texture2DRef textures[], const Vec4f params[], ILog *log);

    Material(const Mesh &rhs) = delete;
    Material(Material &&rhs) noexcept {
        *this = std::move(rhs);
    }

    Material &operator=(const Material &rhs) = delete;
    Material &operator=(Material &&rhs) noexcept;

    uint32_t flags() const {
        return flags_;
    }
    bool ready() const {
        return ready_;
    }
    const String &name() const {
        return name_;
    }
    const ProgramRef &program(int i) const {
        return programs_[i];
    }
    const Texture2DRef &texture(int i) const {
        return textures_[i];
    }
    const Vec4f &param(int i) const {
        return params_[i];
    }

    void Init(uint32_t flags, ProgramRef programs[], Texture2DRef textures[], const Vec4f params[], ILog *log);
    void Init(
        const char *mat_src, eMatLoadStatus *status,
        const program_load_callback &on_prog_load, const texture_load_callback &on_tex_load, ILog *log);
};

//typedef StorageRef<Material> MaterialRef;
typedef Storage<Material> MaterialStorage;
}