#include "SceneManager.h"

#include <filesystem>
#include <fstream>
#include <sstream>

#include <Sys/ScopeExit.h>
#include <Sys/ThreadPool.h>

#include <glslang/Include/glslang_c_interface.h>
#include <glslx/glslx.h>

namespace SceneManagerInternal {
bool SkipAssetForCurrentBuild(Ren::Bitmask<Eng::eAssetBuildFlags> flags);
}

extern "C" {
int (*compile_spirv_shader)(const glslang_input_t *glslang_input, const int use_spv14, const int optimize,
                            const char *output_file, void *ctx, void (*msg_callback)(void *ctx, const char *m));
}

bool Eng::SceneManager::ResolveIncludes(assets_context_t &ctx, const char *in_file, std::string &output,
                                        Ren::SmallVectorImpl<std::string> &out_dependencies) {
    std::ifstream src_stream(in_file, std::ios::binary);
    if (!src_stream) {
        ctx.log->Error("Failed to open %s", in_file);
        return false;
    }

    int line_counter = 0;

    std::string line;
    while (std::getline(src_stream, line)) {
        if (!line.empty() && line.back() == '\r') {
            line = line.substr(0, line.size() - 1);
        }

        if (line.rfind("#include ") == 0) {
            const size_t n1 = line.find_first_of('\"');
            const size_t n2 = line.find_last_of('\"');

            const std::string file_name = line.substr(n1 + 1, n2 - n1 - 1);

            const auto slash_pos = size_t(intptr_t(strrchr(in_file, '/') - in_file));
            const std::string full_path = std::string(in_file, slash_pos + 1) + file_name;

            output += "#line 0\r\n";

            auto it = std::find(std::begin(out_dependencies), std::end(out_dependencies), full_path);
            if (it == std::end(out_dependencies)) {
                out_dependencies.emplace_back(full_path);
            }

            if (!ResolveIncludes(ctx, full_path.c_str(), output, out_dependencies)) {
                return false;
            }

            output += "\r\n#line " + std::to_string(line_counter) + "\r\n";
        } else {
            output += line + "\r\n";
        }
    }

    return true;
}

bool Eng::SceneManager::HCompileShader(assets_context_t &ctx, const char *in_file, const char *out_file,
                                       Ren::SmallVectorImpl<std::string> &out_dependencies,
                                       Ren::SmallVectorImpl<asset_output_t> &out_outputs) {
    using namespace SceneManagerInternal;

    std::vector<std::string> permutations;
    permutations.emplace_back();

    std::string orig_glsl_file_data;

    { // resolve includes, inline constants
        std::ifstream src_stream(in_file, std::ios::binary);
        if (!src_stream) {
            return false;
        }
        std::string line;

        int line_counter = 0;

        while (std::getline(src_stream, line)) {
            /*if (!line.empty() && line.back() == '\r') {
                line = line.substr(0, line.size() - 1);
            }*/

            if (line.rfind("#version ") == 0) {
                if (ctx.platform == "pc" && line.rfind("es") != std::string::npos) {
                    line = "#version 430";
                }
                orig_glsl_file_data += line + "\r\n";
            } else if (line.rfind("#include ") == 0) {
                const size_t n1 = line.find_first_of('\"');
                const size_t n2 = line.find_last_of('\"');

                const std::string file_name = line.substr(n1 + 1, n2 - n1 - 1);
                const std::string full_path =
                    (std::filesystem::path(in_file).parent_path() / file_name).generic_string();

                orig_glsl_file_data += "#line 0\r\n";

                auto it = std::find(std::begin(out_dependencies), std::end(out_dependencies), full_path);
                if (it == std::end(out_dependencies)) {
                    out_dependencies.emplace_back(full_path);
                }

                if (!ResolveIncludes(ctx, full_path.c_str(), orig_glsl_file_data, out_dependencies)) {
                    ctx.log->Error("Failed to preprocess %s", full_path.c_str());
                    return false;
                }

                orig_glsl_file_data += "\r\n#line " + std::to_string(line_counter + 2) + "\r\n";
            } else if (line.find("#pragma multi_compile ") == 0) {
                std::vector<std::string> new_permutations;
                line = line.substr(22);

                size_t pos = 0;
                std::string token;
                while ((pos = line.find(' ')) != std::string::npos) {
                    token = line.substr(0, pos);
                    new_permutations.push_back(token);
                    line.erase(0, pos + 1);
                }
                new_permutations.push_back(line);

                std::vector<std::string> all_permutations;

                for (const std::string &new_perm : new_permutations) {
                    bool all_underscores = true;
                    for (const char c : new_perm) {
                        all_underscores &= (c == '_');
                    }

                    for (int i = 0; i < int(permutations.size()); ++i) {
                        std::string perm = permutations[i];
                        if (!all_underscores) {
                            if (!perm.empty()) {
                                perm += ";";
                            } else {
                                perm += "@";
                            }
                            perm += new_perm;
                            if (perm.back() == '\r') {
                                perm.pop_back();
                            }
                        }
                        all_permutations.emplace_back(std::move(perm));
                    }
                }

                permutations = std::move(all_permutations);
            } else {
                if (!line.empty() && line.back() == '\r') {
                    line.pop_back();
                }
                orig_glsl_file_data += line + "\r\n";
            }

            ++line_counter;
        }
    }

    enum class eShaderOutput { GLSL, VK_SPIRV };

    const size_t ext_pos = std::string_view(out_file).find('.');
    assert(ext_pos != std::string::npos);

    std::deque<std::future<bool>> futures;
    bool result = true;

    for (const eShaderOutput sh_output : {eShaderOutput::GLSL, eShaderOutput::VK_SPIRV}) {
        for (const bool EnableOptimization : {false, true}) {
            if (EnableOptimization && sh_output != eShaderOutput::VK_SPIRV) {
                continue;
            }
            for (const std::string &perm : permutations) {
                auto compile_job = [ext_pos, &orig_glsl_file_data, &ctx, &out_file,
                                    &out_outputs](const eShaderOutput sh_output, const bool EnableOptimization,
                                                  const std::string &perm) -> bool {
                    std::string prep_glsl_file = out_file;
                    prep_glsl_file.insert(ext_pos, perm);

                    Ren::Bitmask<eAssetBuildFlags> flags;
                    if (sh_output == eShaderOutput::GLSL) {
                        flags |= eAssetBuildFlags::GLOnly;
                    } else {
                        flags |= eAssetBuildFlags::VKOnly;
                    }

                    std::string output_file = prep_glsl_file;
                    if (sh_output != eShaderOutput::GLSL) { // replace extension
                        const size_t n = output_file.rfind(".glsl");
                        assert(n != std::string::npos);
                        if (sh_output == eShaderOutput::VK_SPIRV) {
                            if (EnableOptimization) {
                                output_file.replace(n + 1, 4, "spv");
                                flags |= eAssetBuildFlags::ReleaseOnly;
                            } else {
                                output_file.replace(n + 1, 4, "spv_dbg");
                                flags |= eAssetBuildFlags::DebugOnly;
                            }
                        }
                    }

                    std::string preamble;
                    if (sh_output == eShaderOutput::VK_SPIRV) {
                        preamble += "#define VULKAN 1\n";
                    }
                    if (!perm.empty()) {
                        const char *params = perm.c_str();
                        if (!params || params[0] != '@') {
                            return true;
                        }

                        const char *p1 = params + 1;
                        const char *p2 = p1 + 1;
                        while (*p2) {
                            if (*p2 == '=') {
                                preamble += "#define ";
                                preamble += std::string(p1, p2);

                                p1 = p2 + 1;
                                while (p2 && *p2 && *p2 != ';') {
                                    ++p2;
                                }

                                preamble += std::string(p1, p2);
                                preamble += "\n";

                                if (*p2) {
                                    p1 = ++p2;
                                }
                            } else if (*p2 == ';') {
                                preamble += "#define ";
                                preamble += std::string(p1, p2);
                                preamble += "\n";

                                p1 = ++p2;
                            }

                            if (*p2) {
                                ++p2;
                            }
                        }

                        if (p1 != p2) {
                            preamble += "#define ";
                            preamble += std::string(p1, p2);
                            preamble += "\n";
                        }
                    }

                    std::string glsl_file_data = preamble + orig_glsl_file_data;

                    glslx::eTrUnitType unit_type = glslx::eTrUnitType(-1);
                    if (strstr(out_file, ".vert.glsl")) {
                        unit_type = glslx::eTrUnitType::Vertex;
                    } else if (strstr(out_file, ".frag.glsl")) {
                        unit_type = glslx::eTrUnitType::Fragment;
                    } else if (strstr(out_file, ".comp.glsl")) {
                        unit_type = glslx::eTrUnitType::Compute;
                    } else if (strstr(out_file, ".geom.glsl")) {
                        unit_type = glslx::eTrUnitType::Geometry;
                    } else if (strstr(out_file, ".tesc.glsl")) {
                        unit_type = glslx::eTrUnitType::TessControl;
                    } else if (strstr(out_file, ".tese.glsl")) {
                        unit_type = glslx::eTrUnitType::TessEvaluation;
                    } else if (strstr(out_file, ".rgen.glsl")) {
                        unit_type = glslx::eTrUnitType::RayGen;
                    } else if (strstr(out_file, ".rchit.glsl")) {
                        unit_type = glslx::eTrUnitType::ClosestHit;
                    } else if (strstr(out_file, ".rahit.glsl")) {
                        unit_type = glslx::eTrUnitType::AnyHit;
                    } else if (strstr(out_file, ".rmiss.glsl")) {
                        unit_type = glslx::eTrUnitType::Miss;
                    } else if (strstr(out_file, ".rint.glsl")) {
                        unit_type = glslx::eTrUnitType::Intersect;
                    } else if (strstr(out_file, ".rcall.glsl")) {
                        unit_type = glslx::eTrUnitType::Callable;
                    }

                    bool use_spv14 =
                        unit_type == glslx::eTrUnitType::RayGen || unit_type == glslx::eTrUnitType::Intersect ||
                        unit_type == glslx::eTrUnitType::AnyHit || unit_type == glslx::eTrUnitType::ClosestHit ||
                        unit_type == glslx::eTrUnitType::Miss || unit_type == glslx::eTrUnitType::Callable;

                    { //
                        glslx::Preprocessor preprocessor(glsl_file_data);
                        std::string preprocessed = preprocessor.Process();
                        if (!preprocessor.error().empty()) {
                            ctx.log->Error("GLSL preprocessing failed %s", out_file);
                            ctx.log->Error("%s", preprocessor.error().data());
                        }

                        if (preprocessed.find("#pragma dont_compile") != std::string::npos) {
                            return true;
                        }

                        glslx::Parser parser(preprocessed, out_file);
                        std::unique_ptr<glslx::TrUnit> ast = parser.Parse(unit_type);
                        if (!ast) {
                            ctx.log->Error("GLSL parsing failed %s", out_file);
                            ctx.log->Error("%s", parser.error());
#if !defined(NDEBUG) && defined(_WIN32)
                            __debugbreak();
#endif
                            return false;
                        }

                        for (const glslx::ast_extension_directive *ext : ast->extensions) {
                            use_spv14 |= strcmp(ext->name, "GL_EXT_ray_query") == 0;
                            use_spv14 |= strcmp(ext->name, "GL_EXT_ray_tracing") == 0;
                        }

                        glslx::fixup_config_t config;
                        config.randomize_loop_counters = false;
                        if (sh_output == eShaderOutput::GLSL) {
                            config.remove_const = true;
                            config.remove_ctrl_flow_attributes = true;
                        } else if (sh_output == eShaderOutput::VK_SPIRV) {
                            config.flip_vertex_y = true;
                        }
                        if (use_spv14) {
                            config.force_version = 460;
                        }
                        glslx::Fixup(config).Apply(ast.get());

                        glslx::Prune_Unreachable(ast.get());

                        std::stringstream ss;
                        glslx::WriterGLSL().Write(ast.get(), ss);

                        glsl_file_data = ss.str();
                    }

                    if (sh_output != eShaderOutput::VK_SPIRV && use_spv14) {
                        return true;
                    }

                    out_outputs.push_back(asset_output_t{output_file, flags});

                    if (SkipAssetForCurrentBuild(flags)) {
                        return true;
                    }

                    ctx.log->Info("Prep %s", output_file.c_str());
                    std::remove(output_file.c_str());

                    if (sh_output == eShaderOutput::GLSL) {
                        // write preprocessed file
                        std::ofstream out_glsl_file(prep_glsl_file, std::ios::binary);
                        out_glsl_file.write(glsl_file_data.c_str(), glsl_file_data.length());
                        return out_glsl_file.good();
                    } else {
                        glslang_input_t glslang_input = {};
                        glslang_input.language = GLSLANG_SOURCE_GLSL;
                        glslang_input.target_language = GLSLANG_TARGET_SPV;
                        glslang_input.default_version = 100;
                        glslang_input.default_profile = GLSLANG_NO_PROFILE;
                        glslang_input.force_default_version_and_profile = false;
                        glslang_input.forward_compatible = false;
                        glslang_input.messages = GLSLANG_MSG_DEFAULT_BIT;

                        auto default_resource = reinterpret_cast<const glslang_resource_t *(*)()>(
                            ctx.spirv_compiler.GetProcAddress("default_resource"));
                        glslang_input.resource = default_resource();

                        switch (unit_type) {
                        case glslx::eTrUnitType::Vertex:
                            glslang_input.stage = GLSLANG_STAGE_VERTEX;
                            break;
                        case glslx::eTrUnitType::Fragment:
                            glslang_input.stage = GLSLANG_STAGE_FRAGMENT;
                            break;
                        case glslx::eTrUnitType::Compute:
                            glslang_input.stage = GLSLANG_STAGE_COMPUTE;
                            break;
                        case glslx::eTrUnitType::Geometry:
                            glslang_input.stage = GLSLANG_STAGE_GEOMETRY;
                            break;
                        case glslx::eTrUnitType::TessControl:
                            glslang_input.stage = GLSLANG_STAGE_TESSCONTROL;
                            break;
                        case glslx::eTrUnitType::TessEvaluation:
                            glslang_input.stage = GLSLANG_STAGE_TESSEVALUATION;
                            break;
                        case glslx::eTrUnitType::RayGen:
                            glslang_input.stage = GLSLANG_STAGE_RAYGEN;
                            break;
                        case glslx::eTrUnitType::ClosestHit:
                            glslang_input.stage = GLSLANG_STAGE_CLOSESTHIT;
                            break;
                        case glslx::eTrUnitType::AnyHit:
                            glslang_input.stage = GLSLANG_STAGE_ANYHIT;
                            break;
                        case glslx::eTrUnitType::Miss:
                            glslang_input.stage = GLSLANG_STAGE_MISS;
                            break;
                        case glslx::eTrUnitType::Intersect:
                            glslang_input.stage = GLSLANG_STAGE_INTERSECT;
                            break;
                        case glslx::eTrUnitType::Callable:
                            glslang_input.stage = GLSLANG_STAGE_CALLABLE;
                            break;
                        }

                        glslang_input.code = glsl_file_data.data();

                        if (sh_output == eShaderOutput::VK_SPIRV) {
                            glslang_input.client = GLSLANG_CLIENT_VULKAN;
                            glslang_input.client_version = GLSLANG_TARGET_VULKAN_1_1;
                            if (use_spv14) {
                                glslang_input.target_language_version = GLSLANG_TARGET_SPV_1_4;
                            } else {
                                glslang_input.target_language_version = GLSLANG_TARGET_SPV_1_3;
                            }
                        }

                        auto msg_callback = [](void *ctx, const char *m) { ((Ren::ILog *)ctx)->Error("%s", m); };
                        auto compler_shader = reinterpret_cast<decltype(compile_spirv_shader)>(
                            ctx.spirv_compiler.GetProcAddress("compile_spirv_shader"));
                        const int result = compler_shader(&glslang_input, use_spv14, EnableOptimization,
                                                          output_file.c_str(), ctx.log, msg_callback);
                        return result == 1;
                    }

                    return true;
                };
                if (ctx.p_threads) {
                    while (!futures.empty() &&
                           futures.front().wait_for(std::chrono::milliseconds(100)) == std::future_status::ready) {
                        futures.pop_front();
                    }
                    if (futures.size() < 2) {
                        futures.push_back(ctx.p_threads->Enqueue(compile_job, sh_output, EnableOptimization, perm));
                    } else {
                        result &= compile_job(sh_output, EnableOptimization, perm);
                    }
                } else {
                    result &= compile_job(sh_output, EnableOptimization, perm);
                }
            }
        }
    }

    for (auto &f : futures) {
        result &= f.get();
    }

    return result;
}
