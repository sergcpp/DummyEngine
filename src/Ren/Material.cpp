#include "Material.h"

#include <cstdlib>

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4996)
#endif

namespace Ren {
bool IsMainThread();
}

Ren::Material::Material(
        const char *name, const char *mat_src, eMatLoadStatus *status,
        const program_load_callback &on_prog_load, const texture_load_callback &on_tex_load, ILog *log) {
    name_ = String{ name };
    Init(mat_src, status, on_prog_load, on_tex_load, log);
}

Ren::Material::Material(const char *name, uint32_t flags, ProgramRef programs[], Texture2DRef textures[], const Vec4f params[], ILog *log) {
    name_ = String{ name };
    Init(flags, programs, textures, params, log);
}

Ren::Material &Ren::Material::operator=(Material &&rhs) noexcept {
    assert(IsMainThread());
    flags_ = rhs.flags_;
    ready_ = rhs.ready_;
    name_ = std::move(rhs.name_);
    for (int i = 0; i < MaxMaterialProgramCount; i++) {
        programs[i] = std::move(rhs.programs[i]);
    }
    for (int i = 0; i < MaxMaterialTextureCount; i++) {
        textures[i] = std::move(rhs.textures[i]);
    }
    for (int i = 0; i < MaxMaterialParamCount; i++) {
        params[i] = rhs.params[i];
    }
    RefCounter::operator=(std::move(rhs));
    return *this;
}

void Ren::Material::Init(uint32_t flags, ProgramRef programs[], Texture2DRef textures[], const Vec4f params[], ILog *log) {
    assert(IsMainThread());
    flags_ = flags;
    ready_ = true;
    for (int i = 0; i < MaxMaterialProgramCount; i++) {
        this->programs[i] = programs[i];
    }
    for (int i = 0; i < MaxMaterialTextureCount; i++) {
        this->textures[i] = textures[i];
    }
    for (int i = 0; i < MaxMaterialParamCount; i++) {
        this->params[i] = params[i];
    }
}

void Ren::Material::Init(
        const char *mat_src, eMatLoadStatus *status,
        const program_load_callback &on_prog_load, const texture_load_callback &on_tex_load, ILog *log) {
    InitFromTXT(mat_src, status, on_prog_load, on_tex_load, log);
}

void Ren::Material::InitFromTXT(
        const char *mat_src, eMatLoadStatus *status,
        const program_load_callback &on_prog_load, const texture_load_callback &on_tex_load, ILog *log) {
    if (!mat_src) {
        if (status) *status = MatSetToDefault;
        return;
    }

    // Parse material
    const char *delims = " \r\n";
    const char *p = mat_src;
    const char *q = strpbrk(p + 1, delims);

    int num_programs = 0;
    int num_textures = 0;
    int num_params = 0;

    for (; p != nullptr && q != nullptr; q = strpbrk(p, delims)) {
        if (p == q) {
            p = q + 1;
            continue;
        }
        std::string item(p, q);

        if (item == "gl_program:") {
#ifdef USE_GL_RENDER
            p = q + 1;
            q = strpbrk(p, delims);
            std::string program_name = std::string(p, q);
            p = q + 1;
            q = strpbrk(p, delims);
            std::string v_shader_name = std::string(p, q);
            p = q + 1;
            q = strpbrk(p, delims);
            std::string f_shader_name = std::string(p, q);

            programs[num_programs] = on_prog_load(program_name.c_str(), v_shader_name.c_str(), f_shader_name.c_str());
            num_programs++;
#endif
        } else if (item == "sw_program:") {
#ifdef USE_SW_RENDER
            p = q + 1;
            q = strpbrk(p, delims);
            std::string program_name = std::string(p, q);

            program_ = on_prog_load(program_name.c_str(), nullptr, nullptr);
#endif
        } else if (item == "flag:") {
            p = q + 1;
            q = strpbrk(p, delims);
            std::string flag = std::string(p, q);

            if (flag == "alpha_test") {
                flags_ |= AlphaTest;
            } else if (flag == "alpha_blend") {
                flags_ |= AlphaBlend;
            } else {
                log->Error("Unknown flag %s", flag.c_str());
            }
        } else if (item == "texture:") {
            p = q + 1;
            q = strpbrk(p, delims);
            std::string texture_name = std::string(p, q);

            uint32_t texture_flags = 0;

            const char *_p = q + 1;
            const char *_q = strpbrk(_p, delims);

            for (; _p != nullptr && _q != nullptr; _q = strpbrk(_p, delims)) {
                if (_p == _q) break;

                std::string flag = std::string(_p, _q);
                if (flag == "signed") {
                    texture_flags |= TexSigned;
                    p = _p;
                    q = _q;
                } else if (flag == "srgb") {
                    texture_flags |= TexSRGB;
                    p = _p;
                    q = _q;
                } else if (flag == "norepeat") {
                    texture_flags |= TexNoRepeat;
                    p = _p;
                    q = _q;
                } else {
                    break;
                }

                _p = _q + 1;
            }

            textures[num_textures] = on_tex_load(texture_name.c_str(), texture_flags);
            num_textures++;
        } else if (item == "param:") {
            Vec4f &par = params[num_params++];
            p = q + 1;
            q = strpbrk(p, delims);
            par[0] = (float)atof(p);
            p = q + 1;
            q = strpbrk(p, delims);
            par[1] = (float)atof(p);
            p = q + 1;
            q = strpbrk(p, delims);
            par[2] = (float)atof(p);
            p = q + 1;
            q = strpbrk(p, delims);
            par[3] = (float)atof(p);
        }

        if (!q) break;
        p = q + 1;
    }

    ready_ = true;
    if (status) *status = MatCreatedFromData;
}

#ifdef _MSC_VER
#pragma warning(pop)
#endif
