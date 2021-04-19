#include "Material.h"

#include <cstdlib>

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4996)
#endif

namespace Ren {
bool IsMainThread();

uint8_t from_hex_char(const char c) {
    return (c >= 'A') ? (c >= 'a') ? (c - 'a' + 10) : (c - 'A' + 10) : (c - '0');
}
}

Ren::Material::Material(const char *name, const char *mat_src, eMatLoadStatus *status,
                        const program_load_callback &on_prog_load,
                        const texture_load_callback &on_tex_load, ILog *log) {
    name_ = String{name};
    Init(mat_src, status, on_prog_load, on_tex_load, log);
}

Ren::Material::Material(const char *name, const uint32_t flags, ProgramRef programs[],
                        Tex2DRef textures[], const Vec4f params[], ILog *log) {
    name_ = String{name};
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
    params_count = rhs.params_count;
    RefCounter::operator=(std::move(rhs));
    return *this;
}

void Ren::Material::Init(uint32_t flags, ProgramRef _programs[], Tex2DRef _textures[],
                         const Vec4f _params[], ILog *log) {
    assert(IsMainThread());
    flags_ = flags;
    ready_ = true;
    for (int i = 0; i < MaxMaterialProgramCount; i++) {
        this->programs[i] = _programs[i];
    }
    for (int i = 0; i < MaxMaterialTextureCount; i++) {
        this->textures[i] = _textures[i];
    }
    for (int i = 0; i < MaxMaterialParamCount; i++) {
        this->params[i] = _params[i];
    }
}

void Ren::Material::Init(const char *mat_src, eMatLoadStatus *status,
                         const program_load_callback &on_prog_load,
                         const texture_load_callback &on_tex_load, ILog *log) {
    InitFromTXT(mat_src, status, on_prog_load, on_tex_load, log);
}

void Ren::Material::InitFromTXT(const char *mat_src, eMatLoadStatus *status,
                                const program_load_callback &on_prog_load,
                                const texture_load_callback &on_tex_load, ILog *log) {
    if (!mat_src) {
        (*status) = eMatLoadStatus::SetToDefault;
        return;
    }

    // Parse material
    const char *delims = " \r\n";
    const char *p = mat_src;
    const char *q = strpbrk(p + 1, delims);

    int programs_count = 0;
    int textures_count = 0;
    params_count = 0;

    for (; p != nullptr && q != nullptr; q = strpbrk(p, delims)) {
        if (p == q) {
            p = q + 1;
            continue;
        }
        const std::string item(p, q);

        if (item == "gl_program:") {
#ifdef USE_GL_RENDER
            p = q + 1;
            q = strpbrk(p, delims);
            const std::string program_name = std::string(p, q);
            p = q + 1;
            q = strpbrk(p, delims);
            const std::string v_shader_name = std::string(p, q);
            p = q + 1;
            q = strpbrk(p, delims);
            const std::string f_shader_name = std::string(p, q);

            std::string tc_shader_name, te_shader_name;
            if (q && q[0] == '\r' && q[0] == '\n') {
                p = q + 1;
                q = strpbrk(p, delims);
                tc_shader_name = std::string(p, q);
                p = q + 1;
                q = strpbrk(p, delims);
                te_shader_name = std::string(p, q);
            }

            programs[programs_count++] = on_prog_load(
                program_name.c_str(), v_shader_name.c_str(), f_shader_name.c_str(),
                tc_shader_name.empty() ? nullptr : tc_shader_name.c_str(),
                te_shader_name.empty() ? nullptr : te_shader_name.c_str());
#endif
        } else if (item == "sw_program:") {
#ifdef USE_SW_RENDER
            p = q + 1;
            q = strpbrk(p, delims);
            const std::string program_name = std::string(p, q);

            program_ = on_prog_load(program_name.c_str(), nullptr, nullptr);
#endif
        } else if (item == "flag:") {
            p = q + 1;
            q = strpbrk(p, delims);
            const std::string flag = std::string(p, q);

            if (flag == "alpha_test") {
                flags_ |= uint32_t(eMatFlags::AlphaTest);
            } else if (flag == "alpha_blend") {
                flags_ |= uint32_t(eMatFlags::AlphaBlend);
            } else if (flag == "depth_write") {
                flags_ |= uint32_t(eMatFlags::DepthWrite);
            } else if (flag == "two_sided") {
                flags_ |= uint32_t(eMatFlags::TwoSided);
            } else if (flag == "taa_responsive") {
                flags_ |= uint32_t(eMatFlags::TaaResponsive);
            } else {
                log->Error("Unknown flag %s", flag.c_str());
            }
        } else if (item == "texture:") {
            p = q + 1;
            q = strpbrk(p, delims);
            const std::string texture_name = std::string(p, q);

            uint8_t texture_color[] = {0, 255, 255, 255};
            uint32_t texture_flags = 0;

            const char *_p = q + 1;
            const char *_q = strpbrk(_p, delims);

            for (; _p != nullptr && _q != nullptr; _q = strpbrk(_p, delims)) {
                if (_p == _q) {
                    break;
                }

                const char *flag = _p;
                const int flag_len = int(_q - _p);

                if (flag[0] == '#') {
                    texture_color[0] = from_hex_char(flag[1]) * 16 + from_hex_char(flag[2]);
                    texture_color[1] = from_hex_char(flag[3]) * 16 + from_hex_char(flag[4]);
                    texture_color[2] = from_hex_char(flag[5]) * 16 + from_hex_char(flag[6]);
                    texture_color[3] = from_hex_char(flag[7]) * 16 + from_hex_char(flag[8]);
                } else if (strncmp(flag, "signed", flag_len) == 0) {
                    texture_flags |= TexSigned;
                } else if (strncmp(flag, "srgb", flag_len) == 0) {
                    texture_flags |= TexSRGB;
                } else if (strncmp(flag, "norepeat", flag_len) == 0) {
                    texture_flags |= TexNoRepeat;
                } else if (strncmp(flag, "mip_min", flag_len) == 0) {
                    texture_flags |= TexMIPMin;
                } else if (strncmp(flag, "mip_max", flag_len) == 0) {
                    texture_flags |= TexMIPMax;
                } else if (strncmp(flag, "nobias", flag_len) == 0) {
                    texture_flags |= TexNoBias;
                } else {
                    break;
                }

                p = _p;
                q = _q;

                _p = _q + 1;
            }

            textures[textures_count++] = on_tex_load(texture_name.c_str(), texture_color, texture_flags);
        } else if (item == "param:") {
            Vec4f &par = params[params_count++];
            p = q + 1;
            q = strpbrk(p, delims);
            par[0] = (float)strtod(p, nullptr);
            p = q + 1;
            q = strpbrk(p, delims);
            par[1] = (float)strtod(p, nullptr);
            p = q + 1;
            q = strpbrk(p, delims);
            par[2] = (float)strtod(p, nullptr);
            p = q + 1;
            q = strpbrk(p, delims);
            par[3] = (float)strtod(p, nullptr);
        }

        if (!q) {
            break;
        }
        p = q + 1;
    }

    ready_ = true;
    (*status) = eMatLoadStatus::CreatedFromData;
}

#ifdef _MSC_VER
#pragma warning(pop)
#endif
