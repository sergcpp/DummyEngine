
#include <fstream>

#include <glslang/Include/glslang_c_interface.h>
#include <glslang/Public/resource_limits_c.h>
#include <spirv-tools/optimizer.hpp>

#include "../first-party/Sys/DynLib.h"
#include "../first-party/Sys/ScopeExit.h"

extern "C" {
DLL_EXPORT int initialize() { return glslang_initialize_process(); }
DLL_EXPORT void finalize() { glslang_finalize_process(); }
DLL_EXPORT const glslang_resource_t *default_resource() { return glslang_default_resource(); }
DLL_EXPORT int compile_spirv_shader(const glslang_input_t *glslang_input, const int use_spv14, const int optimize,
                                    const char *output_file, void *ctx,
                                    void (*msg_callback)(void *ctx, const char *m)) {
    glslang_shader_t *shader = glslang_shader_create(glslang_input);
    SCOPE_EXIT(glslang_shader_delete(shader);)

    if (!glslang_shader_preprocess(shader, glslang_input)) {
        const std::string msg = std::string("GLSL preprocessing failed ") + output_file;
        msg_callback(ctx, msg.c_str());
        msg_callback(ctx, glslang_shader_get_info_log(shader));
        msg_callback(ctx, glslang_shader_get_info_debug_log(shader));

#if !defined(NDEBUG) && defined(_WIN32)
        __debugbreak();
#endif
        return 0;
    }

    if (!glslang_shader_parse(shader, glslang_input)) {
        const std::string msg = std::string("GLSL parsing failed ") + output_file;
        msg_callback(ctx, msg.c_str());
        msg_callback(ctx, glslang_shader_get_info_log(shader));
        msg_callback(ctx, glslang_shader_get_info_debug_log(shader));

#if !defined(NDEBUG) && defined(_WIN32)
        __debugbreak();
#endif
        return 0;
    }

    glslang_program_t *program = glslang_program_create();
    SCOPE_EXIT(glslang_program_delete(program);)
    glslang_program_add_shader(program, shader);

    const int msg_rules = GLSLANG_MSG_SPV_RULES_BIT | GLSLANG_MSG_VULKAN_RULES_BIT;
    if (!glslang_program_link(program, msg_rules)) {
        const std::string msg = std::string("GLSL linking failed ") + output_file;
        msg_callback(ctx, msg.c_str());
        msg_callback(ctx, glslang_program_get_info_log(program));
        msg_callback(ctx, glslang_program_get_info_debug_log(program));

#if !defined(NDEBUG) && defined(_WIN32)
        __debugbreak();
#endif
        return 0;
    }

    glslang_program_SPIRV_generate(program, glslang_input->stage);

    std::vector<uint32_t> out_shader_module(glslang_program_SPIRV_get_size(program));
    glslang_program_SPIRV_get(program, out_shader_module.data());

    if (optimize) {
        spvtools::Optimizer opt(use_spv14 ? SPV_ENV_UNIVERSAL_1_4 : SPV_ENV_UNIVERSAL_1_3);

        auto print_msg_to_stderr = [&ctx, msg_callback](spv_message_level_t, const char *, const spv_position_t &,
                                                        const char *m) { msg_callback(ctx, m); };
        opt.SetMessageConsumer(print_msg_to_stderr);

        opt.RegisterPass(spvtools::CreateWrapOpKillPass())
            .RegisterPass(spvtools::CreateDeadBranchElimPass())
            .RegisterPass(spvtools::CreateMergeReturnPass())
            .RegisterPass(spvtools::CreateEliminateDeadFunctionsPass())
            .RegisterPass(spvtools::CreateAggressiveDCEPass(true, false))
            .RegisterPass(spvtools::CreatePrivateToLocalPass())
            .RegisterPass(spvtools::CreateLocalSingleBlockLoadStoreElimPass())
            .RegisterPass(spvtools::CreateLocalSingleStoreElimPass())
            .RegisterPass(spvtools::CreateAggressiveDCEPass(true, false))
            .RegisterPass(spvtools::CreateScalarReplacementPass())
            .RegisterPass(spvtools::CreateLocalAccessChainConvertPass())
            .RegisterPass(spvtools::CreateLocalSingleBlockLoadStoreElimPass())
            .RegisterPass(spvtools::CreateLocalSingleStoreElimPass())
            .RegisterPass(spvtools::CreateAggressiveDCEPass(true, false))
            .RegisterPass(spvtools::CreateAggressiveDCEPass(true, false))
            .RegisterPass(spvtools::CreateEliminateDeadConstantPass())
            .RegisterPass(spvtools::CreateUnifyConstantPass())
            .RegisterPass(spvtools::CreateCCPPass())
            .RegisterPass(spvtools::CreateAggressiveDCEPass(true, false))
            .RegisterPass(spvtools::CreateLoopUnrollPass(true))
            .RegisterPass(spvtools::CreateDeadBranchElimPass())
            .RegisterPass(spvtools::CreateLocalRedundancyEliminationPass())
            .RegisterPass(spvtools::CreateCombineAccessChainsPass())
            .RegisterPass(spvtools::CreateSimplificationPass())
            .RegisterPass(spvtools::CreateScalarReplacementPass())
            .RegisterPass(spvtools::CreateLocalAccessChainConvertPass())
            .RegisterPass(spvtools::CreateLocalSingleStoreElimPass())
            .RegisterPass(spvtools::CreateAggressiveDCEPass(true, false))
            .RegisterPass(spvtools::CreateAggressiveDCEPass(true, false))
            .RegisterPass(spvtools::CreateVectorDCEPass())
            .RegisterPass(spvtools::CreateDeadInsertElimPass())
            .RegisterPass(spvtools::CreateDeadBranchElimPass())
            .RegisterPass(spvtools::CreateSimplificationPass())
            .RegisterPass(spvtools::CreateIfConversionPass())
            .RegisterPass(spvtools::CreateCopyPropagateArraysPass())
            .RegisterPass(spvtools::CreateReduceLoadSizePass())
            .RegisterPass(spvtools::CreateAggressiveDCEPass(true, false))
            .RegisterPass(spvtools::CreateBlockMergePass())
            .RegisterPass(spvtools::CreateRedundancyEliminationPass())
            .RegisterPass(spvtools::CreateDeadBranchElimPass())
            .RegisterPass(spvtools::CreateBlockMergePass())
            .RegisterPass(spvtools::CreateSimplificationPass())
            .RegisterPass(spvtools::CreateStripDebugInfoPass())
            .RegisterPass(spvtools::CreateStripNonSemanticInfoPass());
        opt.SetValidateAfterAll(true);

        spvtools::OptimizerOptions opt_options;
        opt_options.set_preserve_bindings(true);

        if (!opt.Run(out_shader_module.data(), out_shader_module.size(), &out_shader_module, opt_options)) {
#if !defined(NDEBUG) && defined(_WIN32)
            __debugbreak();
#endif
            return false;
        }
    }

    { // write output file
        std::ofstream out_spv_file(output_file, std::ios::binary);
        out_spv_file.write((char *)out_shader_module.data(), out_shader_module.size() * sizeof(uint32_t));
        return out_spv_file.good() ? 1 : 0;
    }

    return 1;
}
}