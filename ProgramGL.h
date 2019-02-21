#pragma once

#include <cstdint>
#include <cstring>

#include <array>
#include <string>

#include "Storage.h"

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4996)
#endif

namespace Ren {
const int MAX_NUM_ATTRIBUTES = 32;
const int MAX_NUM_UNIFORMS = 32;
const int MAX_NUM_UNIFORM_BLOCKS = 16;
struct Descr {
    std::string name;
    int loc = -1;
};
typedef Descr Attribute;
typedef Descr Uniform;
typedef Descr UniformBlock;

enum eProgLoadStatus { ProgFound, ProgSetToDefault, ProgCreatedFromData };

class Program : public RefCounter {
    uint32_t    prog_id_ = 0;
    std::array<Attribute, MAX_NUM_ATTRIBUTES>   attributes_;
    std::array<Uniform, MAX_NUM_UNIFORMS>       uniforms_;
    std::array<UniformBlock, MAX_NUM_UNIFORM_BLOCKS> uniform_blocks_;
    bool        ready_ = false;
    char        name_[32];

    struct Shaders {
        const char *vs_source, *fs_source;
        const char *cs_source;
    };

    void InitFromGLSL(const char *name, const Shaders &shaders, eProgLoadStatus *status);
public:
    Program() {
        name_[0] = '\0';
    }
    Program(const char *name, uint32_t prog_id, const Attribute *attrs, const Uniform *unifs, const UniformBlock *unif_blocks) : prog_id_(prog_id) {
        for (int i = 0; i < MAX_NUM_ATTRIBUTES; i++) {
            if (attrs[i].loc == -1) break;
            attributes_[i] = attrs[i];
        }
        for (int i = 0; i < MAX_NUM_UNIFORMS; i++) {
            if (unifs[i].loc == -1) break;
            uniforms_[i] = unifs[i];
        }
        for (int i = 0; i < MAX_NUM_UNIFORM_BLOCKS; i++) {
            if (unif_blocks[i].loc == -1) break;
            uniform_blocks_[i] = unif_blocks[i];
        }
        ready_ = true;
        strcpy(name_, name);
    }
    Program(const char *name, const char *vs_source, const char *fs_source, eProgLoadStatus *status = nullptr);
    Program(const char *name, const char *cs_source, eProgLoadStatus *status = nullptr);
    Program(const Program &rhs) = delete;
    Program(Program &&rhs) {
        *this = std::move(rhs);
    }
    ~Program();

    Program &operator=(const Program &rhs) = delete;
    Program &operator=(Program &&rhs);

    uint32_t prog_id() const {
        return prog_id_;
    }
    bool ready() const {
        return ready_;
    }
    const char *name() const {
        return name_;
    }

    const Attribute &attribute(int i) const {
        return attributes_[i];
    }

    const Attribute &attribute(const char *name) const {
        for (int i = 0; i < MAX_NUM_ATTRIBUTES; i++) {
            if (attributes_[i].name == name) {
                return attributes_[i];
            }
        }
        return attributes_[0];
    }

    const Uniform &uniform(int i) const {
        return uniforms_[i];
    }

    const Uniform &uniform(const char *name) const {
        for (int i = 0; i < MAX_NUM_UNIFORMS; i++) {
            if (uniforms_[i].name == name) {
                return uniforms_[i];
            }
        }
        return uniforms_[0];
    }

    const UniformBlock &uniform_block(int i) const {
        return uniform_blocks_[i];
    }

    const UniformBlock &uniform_block(const char *name) const {
        for (int i = 0; i < MAX_NUM_UNIFORM_BLOCKS; i++) {
            if (uniform_blocks_[i].name == name) {
                return uniform_blocks_[i];
            }
        }
        return uniform_blocks_[0];
    }

    void Init(const char *name, const char *vs_source, const char *fs_source, eProgLoadStatus *status);
    void Init(const char *name, const char *cs_source, eProgLoadStatus *status);
};

typedef StorageRef<Program> ProgramRef;
typedef Storage<Program> ProgramStorage;
}

#ifdef _MSC_VER
#pragma warning(pop)
#endif