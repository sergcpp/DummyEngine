#include "SceneManager.h"

#include <filesystem>
#include <fstream>
#include <sstream>

#include <Sys/ScopeExit.h>

#include <glslang/Include/glslang_c_interface.h>
#include <glslang/Public/resource_limits_c.h>
#include <glslang/include/spirv-tools/optimizer.hpp>
#include <glslx/glslx.h>

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
                                       Ren::SmallVectorImpl<std::string> &out_dependencies) {
    std::remove(out_file);

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
                if (strcmp(ctx.platform, "pc") == 0 && line.rfind("es") != std::string::npos) {
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

    enum class eShaderOutput { GLSL, GL_SPIRV, VK_SPIRV };

    for (const eShaderOutput sh_output : {eShaderOutput::GLSL, eShaderOutput::GL_SPIRV, eShaderOutput::VK_SPIRV}) {
        for (const std::string &perm : permutations) {
            const std::string prep_glsl_file = out_file + perm;

            std::string output_file = out_file + perm;
            if (sh_output != eShaderOutput::GLSL) { // replace extension
                const size_t n = output_file.rfind(".glsl");
                assert(n != std::string::npos);
                if (sh_output == eShaderOutput::VK_SPIRV) {
                    output_file.replace(n + 1, 4, "spv");
                } else if (sh_output == eShaderOutput::GL_SPIRV) {
                    output_file.replace(n + 1, 4, "spv_ogl");
                }
            }

            ctx.log->Info("Prep %s", output_file.c_str());
            std::remove(output_file.c_str());

            const bool TestShaderRewrite = true;

            std::string preamble;
            if (TestShaderRewrite) {
                if (sh_output == eShaderOutput::VK_SPIRV) {
                    preamble += "#define VULKAN 1\n";
                } else if (sh_output == eShaderOutput::GL_SPIRV) {
                    preamble += "#define GL_SPIRV 1\n";
                }
            }
            if (!perm.empty()) {
                const char *params = perm.c_str();
                if (!params || params[0] != '@') {
                    continue;
                }

                int count = 0;

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
                        ++count;
                    } else if (*p2 == ';') {
                        preamble += "#define ";
                        preamble += std::string(p1, p2);
                        preamble += "\n";

                        p1 = ++p2;
                        ++count;
                    }

                    if (*p2) {
                        ++p2;
                    }
                }

                if (p1 != p2) {
                    preamble += "#define ";
                    preamble += std::string(p1, p2);
                    preamble += "\n";

                    ++count;
                }
            }

            std::string glsl_file_data = orig_glsl_file_data;
            if (TestShaderRewrite) {
                glsl_file_data = preamble + glsl_file_data;
            }

            glslx::eTrUnitType unit_type;
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

            bool use_spv14 = unit_type == glslx::eTrUnitType::RayGen || unit_type == glslx::eTrUnitType::Intersect ||
                             unit_type == glslx::eTrUnitType::AnyHit || unit_type == glslx::eTrUnitType::ClosestHit ||
                             unit_type == glslx::eTrUnitType::Miss || unit_type == glslx::eTrUnitType::Callable;

            if (TestShaderRewrite) {
                glslx::Preprocessor preprocessor(glsl_file_data);
                std::string preprocessed = preprocessor.Process();
                if (!preprocessor.error().empty()) {
                    ctx.log->Error("GLSL preprocessing failed %s", out_file);
                    ctx.log->Error("%s", preprocessor.error().data());
                }

                glslx::Parser parser(preprocessed, out_file);
                std::unique_ptr<glslx::TrUnit> ast = parser.Parse(unit_type);
                if (!ast) {
                    ctx.log->Error("GLSL parsing failed %s", out_file);
                    ctx.log->Error("%s", parser.error());
#if !defined(NDEBUG) && defined(_WIN32)
                    __debugbreak();
#endif
                    continue;
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
                }
                glslx::Fixup(config).Apply(ast.get());

                glslx::Prune_Unreachable(ast.get());

                std::stringstream ss;
                glslx::WriterGLSL().Write(ast.get(), ss);

                glsl_file_data = ss.str();
            }

            if (sh_output != eShaderOutput::VK_SPIRV && use_spv14) {
                continue;
            }

            if (sh_output == eShaderOutput::GLSL) {
                // write preprocessed file
                std::ofstream out_glsl_file(prep_glsl_file, std::ios::binary);
                out_glsl_file.write(glsl_file_data.c_str(), glsl_file_data.length());
            } else {
                glslang_input_t glslang_input = {};
                glslang_input.language = GLSLANG_SOURCE_GLSL;
                glslang_input.target_language = GLSLANG_TARGET_SPV;
                glslang_input.default_version = 100;
                glslang_input.default_profile = GLSLANG_NO_PROFILE;
                glslang_input.force_default_version_and_profile = false;
                glslang_input.forward_compatible = false;
                glslang_input.messages = GLSLANG_MSG_DEFAULT_BIT;
                glslang_input.resource = glslang_default_resource();

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
                } else if (sh_output == eShaderOutput::GL_SPIRV) {
                    glslang_input.client = GLSLANG_CLIENT_OPENGL;
                    glslang_input.client_version = GLSLANG_TARGET_OPENGL_450;
                    glslang_input.target_language_version = GLSLANG_TARGET_SPV_1_3;
                }

                glslang_shader_t *shader = glslang_shader_create(&glslang_input);
                SCOPE_EXIT(glslang_shader_delete(shader);)

                if (!TestShaderRewrite && !preamble.empty()) {
                    glslang_shader_set_preamble(shader, preamble.c_str());
                }

                if (!glslang_shader_preprocess(shader, &glslang_input)) {
                    ctx.log->Error("GLSL preprocessing failed %s", out_file);

                    ctx.log->Error("%s", glslang_shader_get_info_log(shader));
                    ctx.log->Error("%s", glslang_shader_get_info_debug_log(shader));
                    ctx.log->Error("%s", glslang_input.code);

#if !defined(NDEBUG) && defined(_WIN32)
                    __debugbreak();
#endif
                    return false;
                }

                if (!glslang_shader_parse(shader, &glslang_input)) {
                    ctx.log->Error("GLSL parsing failed %s", out_file);
                    ctx.log->Error("%s", glslang_shader_get_info_log(shader));
                    ctx.log->Error("%s", glslang_shader_get_info_debug_log(shader));
                    // ctx.log->Error("%s", glslang_shader_get_preprocessed_code(shader));

#if !defined(NDEBUG) && defined(_WIN32)
                    __debugbreak();
#endif
                    continue;
                }

                glslang_program_t *program = glslang_program_create();
                SCOPE_EXIT(glslang_program_delete(program);)
                glslang_program_add_shader(program, shader);

                int msg_rules = GLSLANG_MSG_SPV_RULES_BIT;
                if (sh_output == eShaderOutput::VK_SPIRV) {
                    msg_rules |= GLSLANG_MSG_VULKAN_RULES_BIT;
                }
                if (!glslang_program_link(program, msg_rules)) {
                    ctx.log->Error("GLSL linking failed %s\n", out_file);
                    ctx.log->Error("%s\n", glslang_program_get_info_log(program));
                    ctx.log->Error("%s\n", glslang_program_get_info_debug_log(program));

#if !defined(NDEBUG) && defined(_WIN32)
                    __debugbreak();
#endif
                    return false;
                }

                glslang_program_SPIRV_generate(program, glslang_input.stage);

                std::vector<uint32_t> out_shader_module(glslang_program_SPIRV_get_size(program));
                glslang_program_SPIRV_get(program, out_shader_module.data());

                { // write output file
                    std::ofstream out_spv_file(output_file, std::ios::binary);
                    out_spv_file.write((char *)out_shader_module.data(), out_shader_module.size() * sizeof(uint32_t));
                }
            }
        }
    }

    return true;
}

#undef AS_STR
#undef _AS_STR
