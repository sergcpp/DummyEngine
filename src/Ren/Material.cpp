#include "Material.h"

#include <cstdlib>

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4996)
#endif

Ren::Material::Material(const char *name, const char *mat_src, eMatLoadStatus *status,
                        const program_load_callback &on_prog_load, const texture_load_callback &on_tex_load) {
    Init(name, mat_src, status, on_prog_load, on_tex_load);
}

Ren::Material &Ren::Material::operator=(Material &&rhs) {
    RefCounter::operator=(std::move(rhs));
    flags_ = rhs.flags_;
    ready_ = rhs.ready_;
    strcpy(name_, rhs.name_);
    for (int i = 0; i < 4; i++) {
        programs_[i] = std::move(rhs.programs_[i]);
    }
    for (int i = 0; i < 8; i++) {
        textures_[i] = rhs.textures_[i];
    }
    for (int i = 0; i < 8; i++) {
        params_[i] = rhs.params_[i];
    }
    return *this;
}

void Ren::Material::Init(const char *name, const char *mat_src, eMatLoadStatus *status,
                         const program_load_callback &on_prog_load, const texture_load_callback &on_tex_load) {
    strcpy(name_, name);
    InitFromTXT(mat_src, status, on_prog_load, on_tex_load);
}

void Ren::Material::InitFromTXT(const char *mat_src, eMatLoadStatus *status,
                                const program_load_callback &on_prog_load, const texture_load_callback &on_tex_load) {
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

    for (; p != NULL && q != NULL; q = strpbrk(p, delims)) {
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

            programs_[num_programs] = on_prog_load(program_name.c_str(), v_shader_name.c_str(), f_shader_name.c_str());
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
                fprintf(stderr, "Unknown flag %s", flag.c_str());
            }
        } else if (item == "texture:") {
            p = q + 1;
            q = strpbrk(p, delims);
            std::string texture_name = std::string(p, q);

            textures_[num_textures] = on_tex_load(texture_name.c_str());
            num_textures++;
        } else if (item == "param:") {
            Vec4f &par = params_[num_params++];
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