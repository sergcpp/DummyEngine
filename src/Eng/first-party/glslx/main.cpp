#include <filesystem>
#include <fstream>

#include "glslx.h"

int main(int argc, char *argv[]) {
    printf("glslx version: %s\n", glslx::Version());
    // puts(" ---------------");

    const char *input_name = nullptr;
    const char *output_name = nullptr;
    const char *preprocessed_name = nullptr;
    const char *shader = nullptr;
    const char *target = nullptr;
    bool prune = true;

    glslx::preprocessor_config_t preprocessor_config;

    for (int i = 1; i < argc; ++i) {
        if ((strcmp(argv[i], "--input") == 0 || strcmp(argv[i], "-i") == 0) && (++i != argc)) {
            input_name = argv[i];
        } else if ((strcmp(argv[i], "--output") == 0 || strcmp(argv[i], "-o") == 0) && (++i != argc)) {
            output_name = argv[i];
        } else if ((strcmp(argv[i], "--shader") == 0 || strcmp(argv[i], "-s") == 0) && (++i != argc)) {
            shader = argv[i];
        } else if ((strcmp(argv[i], "--target") == 0 || strcmp(argv[i], "-t") == 0) && (++i != argc)) {
            target = argv[i];
        } else if ((strcmp(argv[i], "--preprocess") == 0 || strcmp(argv[i], "-p") == 0) && (++i != argc)) {
            preprocessed_name = argv[i];
        } else if (argv[i][0] == '-' && argv[i][1] == 'D') {
            int name_len = 0;
            while (argv[i][name_len] != '\0' && argv[i][name_len] != '=') {
                ++name_len;
            }
            const std::string def_name(&argv[i][2], name_len - 2);
            int def_value = preprocessor_config.empty_macro_value;
            if (argv[i][name_len] == '=') {
                def_value = atoi(&argv[i][name_len + 1]);
            }
            preprocessor_config.default_macros.push_back({def_name, def_value});
        } else if (strcmp(argv[i], "--noprune") == 0) {
            prune = false;
        }
    }

    if (!input_name || (!output_name && !preprocessed_name)) {
        printf("Usage: %s -i <input> -o <output> -t <target>\n", argv[0]);
        printf(" --input,-i  : Input file name\n");
        printf(" --output,-o : Output file name\n");
        printf(" --shader,-s : (optional) Shader type ('vertex', 'geometry', 'tesscontrol', 'tesseval', 'fragment', "
               "'compute', 'raygen', 'closesthit', 'anyhit', 'miss', 'callable')\n");
        printf(" --target,-t : (optional) Target name (GLSL, HLSL)\n");
        printf(" --preprocess,-p : (optional) Intermediate preprocessed file name\n");
        printf(" --noprune : (optional) Do not prune unreachable objects\n");
        return 0;
    }

    std::filesystem::path base_path = input_name;
    base_path = std::filesystem::absolute(base_path);
    base_path = base_path.parent_path();

    preprocessor_config.include_callback = [base_path](const char *path, bool is_system_path) {
        if (!is_system_path) {
            return std::make_unique<std::ifstream>(base_path / path, std::ios::binary);
        }
        return std::make_unique<std::ifstream>(path, std::ios::binary);
    };

    if (!shader) {
        // set to compute by default;
        shader = "compute";
        // deduce shader from input file name
        if (strstr(input_name, ".vert")) {
            shader = "vertex";
        } else if (strstr(input_name, ".geom")) {
            shader = "geometry";
        } else if (strstr(input_name, ".tesc")) {
            shader = "tesscontrol";
        } else if (strstr(input_name, ".tese")) {
            shader = "tesseval";
        } else if (strstr(input_name, ".frag")) {
            shader = "fragment";
        } else if (strstr(input_name, ".comp")) {
            shader = "compute";
        } else if (strstr(input_name, ".rgen")) {
            shader = "raygen";
        } else if (strstr(input_name, ".rchit")) {
            shader = "closesthit";
        } else if (strstr(input_name, ".rahit")) {
            shader = "anyhit";
        } else if (strstr(input_name, ".rmiss")) {
            shader = "miss";
        } else if (strstr(input_name, ".rcall")) {
            shader = "callable";
        }
    }

    if (!target && output_name) {
        // set to GLSL by default;
        target = "GLSL";
        // deduce target from output file name
        if (strstr(output_name, ".glsl")) {
            target = "GLSL";
        } else if (strstr(output_name, ".hlsl")) {
            target = "HLSL";
        }
    }

    std::string preprocessed_source;
    { // preprocess input file
        glslx::Preprocessor preprocessor(std::make_unique<std::ifstream>(input_name, std::ios::binary),
                                         preprocessor_config);
        preprocessed_source = preprocessor.Process();
        if (!preprocessor.error().empty()) {
            printf("Failed to preprocess shader: %s\n", preprocessor.error().data());
            return -1;
        }
    }

    if (preprocessed_name) {
        std::ofstream out_file(preprocessed_name, std::ios::binary);
        out_file.write(preprocessed_source.c_str(), preprocessed_source.length());
    }

    if (output_name) {
        std::string final_source;
        if (strcmp(target, "GLSL") != 0) {
            final_source += glslx::g_glsl_prelude;
        }
        final_source += preprocessed_source;

        glslx::eTrUnitType tu_type = glslx::eTrUnitType::Compute;
        if (strcmp(shader, "vertex") == 0) {
            tu_type = glslx::eTrUnitType::Vertex;
        } else if (strcmp(shader, "geometry") == 0) {
            tu_type = glslx::eTrUnitType::Geometry;
        } else if (strcmp(shader, "tesscontrol") == 0) {
            tu_type = glslx::eTrUnitType::TessControl;
        } else if (strcmp(shader, "tesseval") == 0) {
            tu_type = glslx::eTrUnitType::TessEvaluation;
        } else if (strcmp(shader, "fragment") == 0) {
            tu_type = glslx::eTrUnitType::Compute;
        } else if (strcmp(shader, "compute") == 0) {
            tu_type = glslx::eTrUnitType::Compute;
        } else if (strcmp(shader, "raygen") == 0) {
            tu_type = glslx::eTrUnitType::RayGen;
        } else if (strcmp(shader, "closesthit") == 0) {
            tu_type = glslx::eTrUnitType::ClosestHit;
        } else if (strcmp(shader, "anyhit") == 0) {
            tu_type = glslx::eTrUnitType::AnyHit;
        } else if (strcmp(shader, "miss") == 0) {
            tu_type = glslx::eTrUnitType::Miss;
        } else if (strcmp(shader, "callable") == 0) {
            tu_type = glslx::eTrUnitType::Callable;
        }

        glslx::Parser parser(final_source, input_name);
        std::unique_ptr<glslx::TrUnit> tu = parser.Parse(tu_type);
        if (!tu) {
            printf("Failed to parse shader: %s\n", parser.error());
            return -1;
        }

        if (prune) {
            Prune_Unreachable(tu.get());
        }

        if (strcmp(target, "GLSL") == 0) {
            std::ofstream out_file(output_name, std::ios::binary);
            glslx::WriterGLSL().Write(tu.get(), out_file);
        } else if (strcmp(target, "HLSL") == 0) {
            glslx::Fixup().Apply(tu.get());
            std::ofstream out_file(output_name, std::ios::binary);
            glslx::WriterHLSL().Write(tu.get(), out_file);
        } else {
            printf("Unsupported target %s\n", target);
        }
    }

    return 0;
}