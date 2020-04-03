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
const int MAX_NUM_ATTRIBUTES = 16;
const int MAX_NUM_UNIFORMS = 16;
struct Descr {
    std::string name;
    int loc = -1;
    int type = -1, size = 1;

    Descr() {}
    Descr(const std::string &_name, int _loc, int _type, int _size) : name(_name), loc(_loc), type(_type), size(_size) {}
};
typedef Descr Uniform;
typedef Descr Attribute;

enum eProgLoadStatus { Found, SetToDefault, CreatedFromData };

class Program : public RefCounter {
    uint32_t    prog_id_ = 0;
    std::array<Attribute, MAX_NUM_ATTRIBUTES>   attributes_;
    std::array<Uniform, MAX_NUM_UNIFORMS>       uniforms_;
    bool        ready_ = false;
    char        name_[32];

    void InitFromFuncs(void *vs_shader, void *fs_shader, int num_fvars, eProgLoadStatus *status);
public:
    Program() {
        name_[0] = '\0';
    }
    Program(const char *name, uint32_t prog_id, const Attribute *attrs, const Uniform *unifs) : prog_id_(prog_id) {
        for (int i = 0; i < MAX_NUM_ATTRIBUTES; i++) {
            if (attrs[i].loc == -1) break;
            attributes_[i] = attrs[i];
        }
        for (int i = 0; i < MAX_NUM_UNIFORMS; i++) {
            if (unifs[i].loc == -1) break;
            uniforms_[i] = unifs[i];
        }
        ready_ = true;
        strcpy(name_, name);
    }
    Program(const char *name, void *vs_shader, void *fs_shader, int num_fvars,
            const Attribute *attrs, const Uniform *unifs, eProgLoadStatus *status = nullptr);
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

    void Init(const char *name, void *vs_shader, void *fs_shader, int num_fvars,
              const Attribute *attrs, const Uniform *unifs, eProgLoadStatus *status);
};

typedef StorageRef<Program> ProgramRef;
typedef Storage<Program> ProgramStorage;
}

#ifdef _MSC_VER
#pragma warning(pop)
#endif