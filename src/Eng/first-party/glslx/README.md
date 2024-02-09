# GLSL cross-compiler

Compiles GLSL code into following backends:
- Readable HLSL
- Preprocessed and stripped GLSL (pruned AST writeback)
- Vectorized SPMD-like C/C++ (compute shader only) (WIP)

Commandline tool usage:

```console
glslx -i <input> -o <output> -t <target>
 --input,-i  : Input file name
 --output,-o : Output file name
 --shader,-s : (optional) Shader type ('vertex', 'geometry', 'tesscontrol', 'tesseval', 'fragment', ...)
 --target,-t : (optional) Target name (GLSL, HLSL)
 --preprocess,-p : (optional) Intermediate preprocessed file name
 --noprune : (optional) Do not prune unreachable objects
```

Library usage:
```c++
glslx::Preprocessor preprocessor(std::make_unique<std::ifstream>(input_name, std::ios::binary));
std::string preprocessed_source = preprocessor.Process();

std::string final_source = glslx::g_builtin_prototypes;
final_source += glslx::g_glsl_prelude;
final_source += preprocessed_source;

glslx::Parser parser(final_source, input_name);
std::unique_ptr<glslx::TrUnit> tu = parser.Parse(glslx::eTrUnitType::Compute);

std::ofstream out_file(output_name, std::ios::binary);
glslx::WriterHLSL().Write(tu.get(), out_file);
```