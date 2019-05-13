#pragma once

#include <cstdint>

#include <functional>

#include "Fwd.h"
#include "Program.h"
#include "Storage.h"
#include "Texture.h"

#include "MVec.h"

namespace Ren {
enum eMaterialFlags {
    AlphaTest = (1 << 0),
    AlphaBlend = (1 << 1),
};

enum eMatLoadStatus { MatFound, MatSetToDefault, MatCreatedFromData };

typedef std::function<Texture2DRef(const char *name)> texture_load_callback;
typedef std::function<ProgramRef(const char *name, const char *arg1, const char *arg2)> program_load_callback;

class Material : public RefCounter {
    uint32_t        flags_ = 0;
    bool            ready_ = false;
    char            name_[32];
    ProgramRef      programs_[4];
    Texture2DRef    textures_[8];
    Vec4f           params_[8];

    void InitFromTXT(const char *mat_src, eMatLoadStatus *status, const program_load_callback &on_prog_load,
                     const texture_load_callback &on_tex_load);
public:
    Material() {
        name_[0] = '\0';
    }
    Material(const char *name, const char *mat_src, eMatLoadStatus *status,
             const program_load_callback &on_prog_load,
             const texture_load_callback &on_tex_load);

    Material(const Mesh &rhs) = delete;
    Material(Material &&rhs) {
        *this = std::move(rhs);
    }

    Material &operator=(const Material &rhs) = delete;
    Material &operator=(Material &&rhs);

    uint32_t flags() const {
        return flags_;
    }
    bool ready() const {
        return ready_;
    }
    const char *name() const {
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

    void Init(const char *name, const char *mat_src, eMatLoadStatus *status,
              const program_load_callback &on_prog_load,
              const texture_load_callback &on_tex_load);
};

//typedef StorageRef<Material> MaterialRef;
typedef Storage<Material> MaterialStorage;
}