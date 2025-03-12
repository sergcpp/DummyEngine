#include "ProgramSW.h"

#include "SW/SW.h"

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4996)
#endif

Ren::Program::Program(const char *name, void *vs_shader, void *fs_shader, int num_fvars,
                      const Attribute *attrs, const Uniform *unifs, eProgLoadStatus *status) {
    Init(name, vs_shader, fs_shader, num_fvars, attrs, unifs, status);
}

Ren::Program::~Program() {
    if (prog_id_) {
        SWint prog = (SWint)prog_id_;
        swDeleteProgram(prog);
    }
}

Ren::Program &Ren::Program::operator=(Program &&rhs) {
    RefCounter::operator=(std::move(rhs));

    if (prog_id_) {
        SWint prog = (SWint)prog_id_;
        swDeleteProgram(prog);
    }

    prog_id_ = rhs.prog_id_;
    rhs.prog_id_ = 0;
    attributes_ = std::move(rhs.attributes_);
    uniforms_ = std::move(rhs.uniforms_);
    ready_ = rhs.ready_;
    rhs.ready_ = false;
    strcpy(name_, rhs.name_);
    rhs.name_[0] = '\0';

    return *this;
}

void Ren::Program::Init(const char *name, void *vs_shader, void *fs_shader, int num_fvars,
                        const Attribute *attrs, const Uniform *unifs, eProgLoadStatus *status) {
    strcpy(name_, name);
    if (!vs_shader || !fs_shader || !attrs || !unifs) {
        if (status) *status = SetToDefault;
        return;
    }

    InitFromFuncs(vs_shader, fs_shader, num_fvars, status);

    for (int i = 0; i < MaxAttributesCount; i++) {
        if (attrs[i].loc == -1) break;
        attributes_[i] = attrs[i];

    }
    for (int i = 0; i < MaxUniformsCount; i++) {
        if (unifs[i].loc == -1) break;
        uniforms_[i] = unifs[i];
        swRegisterUniformv(unifs[i].loc, (SWenum)unifs[i].type, unifs[i].size);
    }
}

void Ren::Program::InitFromFuncs(void *vs_shader, void *fs_shader, int num_fvars, eProgLoadStatus *status) {
    if (!vs_shader || !fs_shader) {
        if (status) *status = SetToDefault;
        return;
    }

    assert(!ready_);

    SWint prog_id = swCreateProgram();
    swUseProgram(prog_id);
    swInitProgram((vtx_shader_proc)vs_shader, (frag_shader_proc)fs_shader, num_fvars);

    prog_id_ = uint32_t(prog_id);
    ready_ = true;

    if (status) *status = CreatedFromData;
}

#ifdef _MSC_VER
#pragma warning(pop)
#endif