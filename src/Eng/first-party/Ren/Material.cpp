#include "Material.h"

#include <cstdlib>

#include "Pipeline.h"
#include "SamplingParams.h"

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4996)
#endif

namespace Ren {
uint8_t from_hex_char(const char c) { return (c >= 'A') ? (c >= 'a') ? (c - 'a' + 10) : (c - 'A' + 10) : (c - '0'); }

const SamplingParams g_default_mat_sampler = {eTexFilter::Trilinear, eTexWrap::Repeat, eTexCompare::None, Fixed8{},
                                              Fixed8::lowest(),      Fixed8::max()};
} // namespace Ren

Ren::Material::Material(std::string_view name, std::string_view mat_src, eMatLoadStatus *status,
                        const pipelines_load_callback &on_pipes_load, const texture_load_callback &on_tex_load,
                        const sampler_load_callback &on_sampler_load, ILog *log) {
    name_ = String{name};
    Init(mat_src, status, on_pipes_load, on_tex_load, on_sampler_load, log);
}

Ren::Material::Material(std::string_view name, Bitmask<eMatFlags> flags, Span<const PipelineRef> _pipelines,
                        Span<const Tex2DRef> _textures, Span<const SamplerRef> _samplers, Span<const Vec4f> _params,
                        ILog *log) {
    name_ = String{name};
    Init(flags, _pipelines, _textures, _samplers, _params, log);
}

void Ren::Material::Init(const Bitmask<eMatFlags> flags, Span<const PipelineRef> _pipelines,
                         Span<const Tex2DRef> _textures, Span<const SamplerRef> _samplers, Span<const Vec4f> _params,
                         ILog *log) {
    flags_ = flags;
    ready_ = true;

    pipelines.clear();
    textures.clear();
    samplers.clear();
    params.clear();

    for (int i = 0; i < int(pipelines.size()); i++) {
        pipelines.emplace_back(_pipelines[i]);
    }
    assert(_textures.size() == _samplers.size());
    for (int i = 0; i < int(_textures.size()); i++) {
        textures.emplace_back(_textures[i]);
        samplers.emplace_back(_samplers[i]);
    }
    for (int i = 0; i < int(_params.size()); i++) {
        params.emplace_back(_params[i]);
    }
}

void Ren::Material::Init(std::string_view mat_src, eMatLoadStatus *status, const pipelines_load_callback &on_pipes_load,
                         const texture_load_callback &on_tex_load, const sampler_load_callback &on_sampler_load,
                         ILog *log) {
    // if (name_.EndsWith(".mat")) {
    InitFromMAT(mat_src, status, on_pipes_load, on_tex_load, on_sampler_load, log);
    /*} else {
        assert(false);
    }*/
}

void Ren::Material::InitFromMAT(std::string_view mat_src, eMatLoadStatus *status,
                                const pipelines_load_callback &on_pipes_load, const texture_load_callback &on_tex_load,
                                const sampler_load_callback &on_sampler_load, ILog *log) {
    if (mat_src.empty()) {
        (*status) = eMatLoadStatus::SetToDefault;
        return;
    }

    // Parse material
    const char *delims = " \r\n";
    const char *p = mat_src.data();
    const char *q = strpbrk(p + 1, delims);

    pipelines.clear();
    textures.clear();
    samplers.clear();
    params.clear();
    flags_ = {};

    bool multi_doc = false;
    SmallVector<std::string, 4> v_shader_names, f_shader_names, tc_shader_names, te_shader_names;

    for (; p && q; q = strpbrk(p, delims)) {
        if (p == q) {
            p = q + 1;
            continue;
        }
        std::string item(p, q);
        if (item == "pipelines:") {
            p = q + 1;
            q = strpbrk(p, delims);
            for (; p && q; q = strpbrk(p, delims)) {
                if (p == q) {
                    p = q + 1;
                    continue;
                }
                if (*p != '-') {
                    break;
                }
#if defined(USE_GL_RENDER) || defined(USE_VK_RENDER)
                p = q + 1;
                q = strpbrk(p, delims);
                v_shader_names.emplace_back(p, q);
                p = q + 1;
                q = strpbrk(p, delims);
                f_shader_names.emplace_back(p, q);

                if (q && q[0] == '\r' && q[0] == '\n') {
                    p = q + 1;
                    q = strpbrk(p, delims);
                    tc_shader_names.emplace_back(p, q);
                    p = q + 1;
                    q = strpbrk(p, delims);
                    te_shader_names.emplace_back(p, q);
                } else {
                    tc_shader_names.emplace_back();
                    te_shader_names.emplace_back();
                }
#endif
                if (!q) {
                    break;
                }
                while (*q != '\n') {
                    ++q;
                }
                p = q + 1;
            }
            item.clear();
            if (p && q) {
                item = std::string(p, q);
            }
        }
        if (item == "flags:") {
            p = q + 1;
            q = strpbrk(p, delims);
            for (; p && q; q = strpbrk(p, delims)) {
                if (p == q) {
                    p = q + 1;
                    continue;
                }
                if (*p != '-') {
                    break;
                }
                p = q + 1;
                q = strpbrk(p, delims);
                const std::string flag = std::string(p, q);

                if (flag == "alpha_test") {
                    flags_ |= eMatFlags::AlphaTest;
                } else if (flag == "alpha_blend") {
                    flags_ |= eMatFlags::AlphaBlend;
                } else if (flag == "depth_write") {
                    flags_ |= eMatFlags::DepthWrite;
                } else if (flag == "two_sided") {
                    flags_ |= eMatFlags::TwoSided;
                } else if (flag == "emissive") {
                    flags_ |= eMatFlags::Emissive;
                } else if (flag == "custom_shaded") {
                    flags_ |= eMatFlags::CustomShaded;
                } else {
                    log->Error("Unknown flag %s", flag.c_str());
                }
                if (!q) {
                    break;
                }
                while (*q != '\n') {
                    ++q;
                }
                p = q + 1;
            }
            item.clear();
            if (p && q) {
                item = std::string(p, q);
            }
        }
        if (item == "textures:") {
            p = q + 1;
            q = strpbrk(p, delims);
            for (; p && q; q = strpbrk(p, delims)) {
                if (p == q) {
                    p = q + 1;
                    continue;
                }
                if (*p != '-') {
                    break;
                }
                p = q + 1;
                q = strpbrk(p, delims);
                const std::string texture_name = std::string(p, q);
                if (texture_name != "none") {
                    uint8_t texture_color[] = {0, 255, 255, 255};
                    eTexFlags texture_flags = {};

                    const char *_p = q + 1;
                    const char *_q = strpbrk(_p, delims);

                    SamplingParams sampler_params = g_default_mat_sampler;

                    for (; _p && _q; _q = strpbrk(_p, delims)) {
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
                            texture_flags |= eTexFlagBits::Signed;
                        } else if (strncmp(flag, "srgb", flag_len) == 0) {
                            texture_flags |= eTexFlagBits::SRGB;
                        } else if (strncmp(flag, "norepeat", flag_len) == 0) {
                            texture_flags |= eTexFlagBits::NoRepeat;
                            sampler_params.wrap = eTexWrap::ClampToEdge;
                        } else if (strncmp(flag, "nofilter", flag_len) == 0) {
                            texture_flags |= eTexFlagBits::NoFilter;
                            sampler_params.filter = eTexFilter::NoFilter;
                        } else if (strncmp(flag, "mip_min", flag_len) == 0) {
                            texture_flags |= eTexFlagBits::MIPMin;
                        } else if (strncmp(flag, "mip_max", flag_len) == 0) {
                            texture_flags |= eTexFlagBits::MIPMax;
                        } else if (strncmp(flag, "nobias", flag_len) == 0) {
                            texture_flags |= eTexFlagBits::NoBias;
                        } else {
                            break;
                        }

                        p = _p;
                        q = _q;

                        _p = _q + 1;
                    }

                    textures.emplace_back(on_tex_load(texture_name.c_str(), texture_color, texture_flags));
                    samplers.emplace_back(on_sampler_load(sampler_params));
                } else {
                    textures.emplace_back();
                    samplers.emplace_back();
                }
                if (!q) {
                    break;
                }
                while (*q != '\n') {
                    ++q;
                }
                p = q + 1;
            }
            item.clear();
            if (p && q) {
                item = std::string(p, q);
            }
        }
        if (item == "params:") {
            p = q + 1;
            q = strpbrk(p, delims);
            for (; p && q; q = strpbrk(p, delims)) {
                if (p == q) {
                    p = q + 1;
                    continue;
                }
                if (p[0] != '-' || p[1] != ' ') {
                    break;
                }
                Vec4f &par = params.emplace_back();
                p = q + 1;
                q = strpbrk(p, delims);
                par[0] = strtof(p, nullptr);
                p = q + 1;
                q = strpbrk(p, delims);
                par[1] = strtof(p, nullptr);
                p = q + 1;
                q = strpbrk(p, delims);
                par[2] = strtof(p, nullptr);
                p = q + 1;
                q = strpbrk(p, delims);
                par[3] = strtof(p, nullptr);
                if (!q) {
                    break;
                }
                while (*q != '\n') {
                    ++q;
                }
                p = q + 1;
            }
            item.clear();
            if (p && q) {
                item = std::string(p, q);
            }
        }
        if (item == "---") {
            multi_doc = true;
            break;
        }

        if (!q) {
            break;
        }
        p = q + 1;
    }

    assert(textures.size() == samplers.size());

    for (size_t i = 0; i < v_shader_names.size(); ++i) {
        on_pipes_load(flags_, v_shader_names[i], f_shader_names[i], tc_shader_names[i], te_shader_names[i], pipelines);
    }

    ready_ = true;
    (*status) = multi_doc ? eMatLoadStatus::CreatedFromData_NeedsMore : eMatLoadStatus::CreatedFromData;
}

#ifdef _MSC_VER
#pragma warning(pop)
#endif
