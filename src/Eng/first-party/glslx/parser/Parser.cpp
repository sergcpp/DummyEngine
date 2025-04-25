#include "Parser.h"

#include "Utils.h"

namespace glslx {
#define X(_0, _1) {#_0, _1},
struct {
    const char *qualifier;
    bool is_assign;
} g_layout_qualifiers[] = {
#include "LayoutQualifiers.inl"
};
#undef X

static const eKeyword g_GenTypesTable[][4] = {
    {eKeyword::K_float, eKeyword::K_vec2, eKeyword::K_vec3, eKeyword::K_vec4},
    {eKeyword::K_double, eKeyword::K_dvec2, eKeyword::K_dvec3, eKeyword::K_dvec4},
    {eKeyword::K_int, eKeyword::K_ivec2, eKeyword::K_ivec3, eKeyword::K_ivec4},
    {eKeyword::K_uint, eKeyword::K_uvec2, eKeyword::K_uvec3, eKeyword::K_uvec4},
    {eKeyword::K_bool, eKeyword::K_bvec2, eKeyword::K_bvec3, eKeyword::K_bvec4},
    {eKeyword::K_vec2, eKeyword::K_vec3, eKeyword::K_vec4},
    {eKeyword::K_ivec2, eKeyword::K_ivec3, eKeyword::K_ivec4},
    {eKeyword::K_uvec2, eKeyword::K_uvec3, eKeyword::K_uvec4},
    {eKeyword::K_bvec2, eKeyword::K_bvec3, eKeyword::K_bvec4},
    {eKeyword::K_vec4, eKeyword::K_ivec4, eKeyword::K_uvec4},
    {eKeyword::K_sampler1D, eKeyword::K_isampler1D, eKeyword::K_usampler1D},
    {eKeyword::K_sampler1DArray, eKeyword::K_isampler1DArray, eKeyword::K_usampler1DArray},
    {eKeyword::K_sampler2D, eKeyword::K_isampler2D, eKeyword::K_usampler2D},
    {eKeyword::K_sampler2DRect, eKeyword::K_isampler2DRect, eKeyword::K_usampler2DRect},
    {eKeyword::K_sampler2DArray, eKeyword::K_isampler2DArray, eKeyword::K_usampler2DArray},
    {eKeyword::K_sampler2DMS, eKeyword::K_isampler2DMS, eKeyword::K_usampler2DMS},
    {eKeyword::K_sampler2DMSArray, eKeyword::K_isampler2DMSArray, eKeyword::K_usampler2DMSArray},
    {eKeyword::K_sampler3D, eKeyword::K_isampler3D, eKeyword::K_usampler3D},
    {eKeyword::K_samplerCube, eKeyword::K_isamplerCube, eKeyword::K_usamplerCube},
    {eKeyword::K_samplerCubeArray, eKeyword::K_isamplerCubeArray, eKeyword::K_usamplerCubeArray},
    {eKeyword::K_samplerBuffer, eKeyword::K_isamplerBuffer, eKeyword::K_usamplerBuffer},
    {eKeyword::K_image1D, eKeyword::K_iimage1D, eKeyword::K_uimage1D},
    {eKeyword::K_image2D, eKeyword::K_iimage2D, eKeyword::K_uimage2D},
    {eKeyword::K_image3D, eKeyword::K_iimage3D, eKeyword::K_uimage3D},
    {eKeyword::K_imageCube, eKeyword::K_iimageCube, eKeyword::K_uimageCube},
    {eKeyword::K_imageCubeArray, eKeyword::K_iimageCubeArray, eKeyword::K_uimageCubeArray},
    {eKeyword::K_image2DArray, eKeyword::K_iimage2DArray, eKeyword::K_uimage2DArray},
    {eKeyword::K_image2DRect, eKeyword::K_iimage2DRect, eKeyword::K_uimage2DRect},
    {eKeyword::K_image1DArray, eKeyword::K_iimage1DArray, eKeyword::K_uimage1DArray},
    {eKeyword::K_image2DMS, eKeyword::K_iimage2DMS, eKeyword::K_uimage2DMS},
    {eKeyword::K_image2DMSArray, eKeyword::K_iimage2DMSArray, eKeyword::K_uimage2DMSArray},
    {eKeyword::K_imageBuffer, eKeyword::K_iimageBuffer, eKeyword::K_uimageBuffer},
    {eKeyword::K_subpassInput, eKeyword::K_isubpassInput, eKeyword::K_usubpassInput},
    {eKeyword::K_subpassInputMS, eKeyword::K_isubpassInputMS, eKeyword::K_usubpassInputMS}};
static_assert(int(eKeyword::K_genFType) == 332);
static_assert(int(eKeyword::K_gsubpassInputMS) == 365);

extern const char g_builtin_prototypes[] =
#include "BuiltinPrototypes.inl"
    ;
} // namespace glslx

#define IVAL(x) static_cast<ast_int_constant *>(x)->value
#define UVAL(x) static_cast<ast_uint_constant *>(x)->value
#define HVAL(x) static_cast<ast_half_constant *>(x)->value
#define FVAL(x) static_cast<ast_float_constant *>(x)->value
#define DVAL(x) static_cast<ast_double_constant *>(x)->value
#define BVAL(x) static_cast<ast_bool_constant *>(x)->value

glslx::Parser::Parser(std::string_view source, const char *file_name)
    : lexer_(allocator_), source_(source), tok_(allocator_), file_name_(file_name) {}

std::unique_ptr<glslx::TrUnit> glslx::Parser::Parse(const eTrUnitType type) {
    ast_ = std::make_unique<TrUnit>(type);
    scopes_.emplace_back();
    if (!InitSpecialGlobals(type)) {
        fatal("failed to initialize special globals");
        return nullptr;
    }
    if (!ParseSource(g_builtin_prototypes)) {
        return nullptr;
    }
    if (!ParseSource(source_)) {
        return nullptr;
    }

    // Resolve function prototypes
    for (auto it = ast_->functions_by_name.begin(); it != ast_->functions_by_name.end(); ++it) {
        for (ast_function *func : it->val) {
            if (func->is_prototype) {
                continue;
            }
            for (ast_function *proto : it->val) {
                if (!proto->is_prototype) {
                    continue;
                }
                assert(strcmp(func->name, proto->name) == 0);
                if (func->parameters.size() == proto->parameters.size()) {
                    bool args_match = true;
                    for (int i = 0; i < int(func->parameters.size()) && args_match; ++i) {
                        args_match &= is_same_type(proto->parameters[i]->base_type, func->parameters[i]->base_type);
                    }
                    if (args_match) {
                        func->prototype = proto;
                        break;
                    }
                }
            }
        }
    }

    // Resolve function calls
    for (ast_function_call *call : function_calls_) {
        auto *p_find = ast_->functions_by_name.Find(call->name);
        if (!p_find) {
            continue;
        }
        if ((*p_find).size() == 1) {
            call->func = (*p_find)[0];
        } else {
            // Try to find exact match
            for (ast_function *func : *p_find) {
                if (func->is_prototype && call->func) {
                    continue;
                }
                if (func->parameters.size() == call->parameters.size()) {
                    bool args_match = true;
                    for (int i = 0; i < int(func->parameters.size()); ++i) {
                        int array_dims = 0;
                        const ast_type *result_type =
                            Evaluate_ExpressionResultType(ast_.get(), call->parameters[i], array_dims);
                        if (!result_type) {
                            args_match = false;
                            break;
                        }
                        args_match &= is_same_type(result_type, func->parameters[i]->base_type);
                    }
                    if (args_match) {
                        call->func = func;
                        if (!func->is_prototype) {
                            break;
                        }
                    }
                }
            }
        }

        // Try to find compatible
        if (!call->func) {
            for (ast_function *func : *p_find) {
                if (func->is_prototype && call->func) {
                    continue;
                }
                if (func->parameters.size() == call->parameters.size()) {
                    bool args_match = true;
                    for (int i = 0; i < int(func->parameters.size()); ++i) {
                        int array_dims = 0;
                        const ast_type *result_type =
                            Evaluate_ExpressionResultType(ast_.get(), call->parameters[i], array_dims);
                        if (!result_type) {
                            args_match = false;
                            break;
                        }
                        args_match &= is_compatible_type(result_type, func->parameters[i]->base_type);
                    }
                    if (args_match) {
                        call->func = func;
                        if (!func->is_prototype) {
                            break;
                        }
                    }
                }
            }
        }
    }

    return std::move(ast_);
}

bool glslx::Parser::ParseSource(std::string_view source) {
    lexer_ = Lexer{allocator_, source};
    while (true) {
        lexer_.Read(tok_, true);

        if (lexer_.error()) {
            fatal("%s", lexer_.error());
            return false;
        }

        if (is_type(eTokType::Eof)) {
            break;
        }

        if (is_type(eTokType::Directive)) {
            if (tok_.as_directive.type == eDirType::Version) {
                if (ast_->version) {
                    fatal("multiple version directives are not allowed");
                    return false;
                }
                auto *directive = astnew<ast_version_directive>();
                if (!directive) {
                    fatal("internal error");
                    return false;
                }
                directive->number = tok_.as_directive.as_version.number;
                directive->type = tok_.as_directive.as_version.type;
                ast_->version = directive;
            } else if (tok_.as_directive.type == eDirType::Extension) {
                auto *extension = astnew<ast_extension_directive>();
                if (!extension) {
                    fatal("internal error");
                    return false;
                }
                extension->name = strnew(tok_.as_directive.as_extension.name);
                extension->behavior = tok_.as_directive.as_extension.behavior;
                ast_->extensions.push_back(extension);
            }
            continue;
        }

        global_vector<top_level_t> items;
        if (!ParseTopLevel(items)) {
            return false;
        }

        if (is_type(eTokType::Semicolon)) {
            for (int i = 0; i < int(items.size()); ++i) {
                top_level_t &parse = items[i];
                auto *global = astnew<ast_global_variable>(ast_->alloc.allocator);
                if (!global) {
                    return false;
                }
                global->storage = parse.storage;
                global->aux_storage = parse.aux_storage;
                global->memory_flags = parse.memory_flags;
                global->precision = parse.precision;
                global->interpolation = parse.interpolation;
                global->base_type = parse.type;
                global->name = strnew(parse.name);
                global->is_invariant = parse.is_invariant;
                global->is_precise = parse.is_precise;
                global->layout_qualifiers = parse.layout_qualifiers;
                if (parse.initial_value) {
                    if (!(global->initial_value = Evaluate(parse.initial_value))) {
                        return false;
                    }
                }
                global->is_array = parse.is_array;
                global->array_sizes = parse.array_sizes;
                ast_->globals.push_back(global);
                scopes_.back().push_back(global);
            }
        } else if (is_operator(eOperator::parenthesis_begin)) {
            ast_function *function = ParseFunction(items.front());
            if (!function) {
                return false;
            }

            local_vector<ast_builtin *> gen_types(ast_->alloc.allocator);
            if (function->return_type->builtin) {
                auto *return_type = static_cast<ast_builtin *>(function->return_type);
                if (is_generic_type(return_type->type)) {
                    gen_types.push_back(return_type);
                }
            }
            for (int i = 0; i < int(function->parameters.size()); ++i) {
                if (function->parameters[i]->base_type->builtin) {
                    auto *param_type = static_cast<ast_builtin *>(function->parameters[i]->base_type);
                    if (is_generic_type(param_type->type) &&
                        std::find(begin(gen_types), end(gen_types), param_type) == end(gen_types)) {
                        gen_types.push_back(param_type);
                    }
                }
            }

            if (!gen_types.empty()) {
                if (!function->is_prototype || !(function->attributes & eFunctionAttribute::Builtin)) {
                    fatal("generics are only allowed for builtin prototypes");
                    return false;
                }

                for (int s = 0; s < 4; ++s) {
                    ast_function *new_func = nullptr;
                    if (function->return_type->builtin) {
                        auto *return_type = static_cast<ast_builtin *>(function->return_type);
                        if (is_generic_type(return_type->type) &&
                            g_GenTypesTable[int(return_type->type) - int(eKeyword::K_genFType)][s] != eKeyword(0)) {
                            if (!new_func) {
                                new_func = astnew<ast_function>(*function);
                                if (!new_func) {
                                    return false;
                                }
                            }
                            new_func->return_type = FindOrAddBuiltin(
                                g_GenTypesTable[int(return_type->type) - int(eKeyword::K_genFType)][s]);
                        }
                    }
                    for (int i = 0; i < int(function->parameters.size()); ++i) {
                        if (function->parameters[i]->base_type->builtin) {
                            auto *param_type = static_cast<ast_builtin *>(function->parameters[i]->base_type);
                            if (is_generic_type(param_type->type) &&
                                g_GenTypesTable[int(param_type->type) - int(eKeyword::K_genFType)][s] != eKeyword(0)) {
                                if (!new_func) {
                                    new_func = astnew<ast_function>(*function);
                                    if (!new_func) {
                                        return false;
                                    }
                                }
                                new_func->parameters[i] = astnew<ast_function_parameter>(*new_func->parameters[i]);
                                new_func->parameters[i]->base_type = FindOrAddBuiltin(
                                    g_GenTypesTable[int(param_type->type) - int(eKeyword::K_genFType)][s]);
                            }
                        }
                    }

                    if (new_func) {
                        ast_->functions.push_back(new_func);
                        auto *p_find = ast_->functions_by_name.Find(new_func->name);
                        if (p_find) {
                            p_find->push_back(new_func);
                        } else {
                            ast_->functions_by_name.Insert(new_func->name, {new_func});
                        }
                    }
                }
            } else {
                ast_->functions.push_back(function);
                auto *p_find = ast_->functions_by_name.Find(function->name);
                if (p_find) {
                    p_find->push_back(function);
                } else {
                    ast_->functions_by_name.Insert(function->name, {function});
                }
            }
        }
    }
    return true;
}

bool glslx::Parser::next() {
    lexer_.Read(tok_, true);
    if (is_type(eTokType::Eof)) {
        fatal("premature end of file");
        return false;
    }
    if (lexer_.error()) {
        fatal("%s", lexer_.error());
        return false;
    }
    return true;
}

bool glslx::Parser::expect(const eTokType type) {
    if (!is_type(type)) {
        return false;
    }
    return next();
}

bool glslx::Parser::expect(const eOperator op) {
    if (!is_operator(op)) {
        return false;
    }
    return next();
}

bool glslx::Parser::ParseTopLevel(global_vector<top_level_t> &items) {
    top_level_t item(ast_->alloc.allocator);
    if (!ParseTopLevelItem(item)) {
        return false;
    }
    int item_index = int(items.size());
    if (item.type) {
        items.push_back(item);
    }
    while (!items.empty() && is_operator(eOperator::comma)) {
        if (!next()) { // skip ','
            return false;
        }
        top_level_t next_item(ast_->alloc.allocator);
        if (!ParseTopLevelItem(next_item, &items[item_index])) {
            return false;
        }
        if (next_item.type) {
            items.push_back(next_item);
        }
    }
    return true;
}

bool glslx::Parser::ParseTopLevelItem(top_level_t &level, top_level_t *continuation) {
    global_vector<top_level_t> items;
    while (!is_builtin() && !is_type(eTokType::Identifier)) {
        { // check EOF
            const token_t &tok = lexer_.Peek();
            if (tok.type == eTokType::Eof) {
                return false;
            }
        }
        top_level_t item(ast_->alloc.allocator);
        if (continuation) {
            item = *continuation;
        }

        // clang-format off
        // TODO: fix this!!!
        for (int i = 0; i < 4; ++i) {
            if (!ParseStorage(item)) return false;
            if (!ParseAuxStorage(item)) return false;
            if (!ParseInterpolation(item)) return false;
            if (!ParsePrecision(item.precision)) return false;
            if (!ParseInvariant(item)) return false;
            if (!ParsePrecise(item)) return false;
            if (!ParseMemoryFlags(item)) return false;
            if (!ParseLayout(item)) return false;
        }
        // clang-format on

        if (is_type(eTokType::Keyword) && is_reserved_keyword(tok_.as_keyword)) {
            fatal("cannot use a reserved keyword");
            return false;
        }

        bool is_typename = false;
        if (is_type(eTokType::Identifier)) {
            is_typename = FindType(tok_.as_identifier) != nullptr;
        }

        if ((is_type(eTokType::Identifier) || is_type(eTokType::Semicolon)) && !is_typename &&
            is_interface_block_storage(item.storage)) {
            ast_interface_block *unique = ParseInterfaceBlock(item.storage);
            if (!unique) {
                return false;
            }
            unique->layout_qualifiers = std::move(item.layout_qualifiers);
            unique->memory_flags = item.memory_flags;
            ast_->interface_blocks.push_back(unique);
            if (is_type(eTokType::Semicolon)) {
                return true;
            } else {
                level.type = unique;
            }
        } else if (is_keyword(eKeyword::K_struct)) {
            if (!next()) { // skip struct
                return false;
            }
            ast_struct *unique = ParseStruct();
            if (!unique) {
                return false;
            }
            ast_->structures.push_back(unique);
            ast_->structures_by_name.Insert(unique->name, unique);
            if (is_type(eTokType::Semicolon)) {
                return true;
            } else {
                level.type = unique;
            }
        } else if (is_keyword(eKeyword::K_precision)) {
            if (!next()) { // skip precision
                return false;
            }
            auto *pre = astnew<ast_default_precision>();
            if (!ParsePrecision(pre->precision)) {
                return false;
            }
            pre->type = ParseBuiltin();
            if (!pre->type) {
                return false;
            }
            ast_->default_precision.push_back(pre);
            if (!next() || is_type(eTokType::Semicolon)) {
                return true;
            }
        } else {
            items.push_back(item);
        }
    }

    if (continuation) {
        level = *continuation;
        level.array_sizes.erase(begin(level.array_sizes) + level.array_on_type_offset, end(level.array_sizes));
    }

    for (int i = 0; i < int(items.size()); ++i) {
        top_level_t &next = items[i];
        const eStorage storage = level.storage != eStorage::None ? level.storage : next.storage;
        if (ast_->type == eTrUnitType::Vertex && storage == eStorage::In) {
            if (level.aux_storage != eAuxStorage::None || next.aux_storage != eAuxStorage::None) {
                fatal("cannot use auxiliary storage qualifier on vertex shader input");
                return false;
            } else if (level.interpolation != eInterpolation::None || next.interpolation != eInterpolation::None) {
                fatal("cannot use interpolation qualifier on vertex shader input");
                return false;
            }
        }
        if (ast_->type == eTrUnitType::Fragment && storage == eStorage::Out) {
            if (level.aux_storage != eAuxStorage::None || next.aux_storage != eAuxStorage::None) {
                fatal("cannot use auxiliary storage qualifier on fragment shader input");
                return false;
            } else if (level.interpolation != eInterpolation::None || next.interpolation != eInterpolation::None) {
                fatal("cannot use interpolation qualifier on fragment shader input");
                return false;
            }
        }
        if (ast_->type != eTrUnitType::TessEvaluation && storage == eStorage::In) {
            if (level.aux_storage == eAuxStorage::Patch || next.aux_storage == eAuxStorage::Patch) {
                fatal("applying 'patch' qualifier to output can only be done in tesselation control shader");
                return false;
            }
        }
        if (next.storage != eStorage::None && level.storage != eStorage::None) {
            fatal("multiple storage qualifiers in declaration");
            return false;
        } else if (next.aux_storage != eAuxStorage::None && level.aux_storage != eAuxStorage::None) {
            fatal("multiple auxiliary storage qualifiers in declaration");
            return false;
        } else if (next.interpolation != eInterpolation::None && level.interpolation != eInterpolation::None) {
            fatal("multiple interpolation qualifiers in declaration");
            return false;
        } else if (next.precision != ePrecision::None && level.precision != ePrecision::None) {
            fatal("multiple precision qualifiers in declaration");
            return false;
        }

        level.storage = next.storage;
        level.aux_storage = next.aux_storage;
        level.interpolation = next.interpolation;
        level.precision = next.precision;
        level.memory_flags |= next.memory_flags;
        level.is_invariant = next.is_invariant;

        for (int j = 0; j < int(next.layout_qualifiers.size()); ++j) {
            for (int k = 0; k < int(level.layout_qualifiers.size()); ++k) {
                if (next.layout_qualifiers[j]->name == level.layout_qualifiers[k]->name) {
                    level.layout_qualifiers.erase(begin(level.layout_qualifiers) + k);
                }
            }
            level.layout_qualifiers.push_back(next.layout_qualifiers[j]);
        }
    }

    if (level.aux_storage == eAuxStorage::Patch && level.interpolation != eInterpolation::None) {
        fatal("cannot use interpolation qualifier with auxiliary storage qualifier 'patch'");
        return false;
    }

    if (!continuation && !level.type) {
        if (is_type(eTokType::Identifier)) {
            const token_t &peek = lexer_.Peek();
            if (level.is_invariant && peek.type == eTokType::Semicolon) {
                ast_variable *var = FindVariable(tok_.as_identifier);
                if (!var) {
                    fatal("variable not found");
                    return false;
                }
                if (var->type != eVariableType::Global) {
                    fatal("expected global variable");
                    return false;
                }
                auto *gvar = static_cast<ast_global_variable *>(var);
                gvar->is_invariant = true;

                if (!next()) { // skip identifier
                    return false;
                }
                return true;
            } else {
                level.type = FindType(tok_.as_identifier);
                if (!next()) { // skip identifier
                    return false;
                }
            }
        } else {
            level.type = ParseBuiltin();
            if (!next()) { // skip typename
                return false;
            }
        }

        if (level.type) {
            // array
            while (is_operator(eOperator::bracket_begin)) {
                level.is_array = true;
                ast_constant_expression *array_size = ParseArraySize();
                if (!array_size) {
                    return false;
                }
                level.array_sizes.insert(begin(level.array_sizes), array_size);
                ++level.array_on_type_offset;
                if (!next()) { // skip ']'
                    return false;
                }
            }
        }
    }

    if (!level.type) {
        fatal("expected typename");
        return false;
    }

    if (is_type(eTokType::Identifier)) {
        level.name = strnew(tok_.as_identifier);
        if (!next()) { // skip identifier
            return false;
        }
    }

    while (is_operator(eOperator::bracket_begin)) {
        level.is_array = true;
        level.array_sizes.push_back(ParseArraySize());
        if (!next()) { // skip ']'
            return false;
        }
    }

    if (level.storage == eStorage::Const || level.storage == eStorage::Uniform) {
        // constant expression assignment
        if (is_operator(eOperator::assign)) {
            if (!next()) { // skip '='
                return false;
            }
            if (level.is_array || is_vector_type(level.type)) {
                if (!(level.initial_value = ParseArraySpecifier(eEndCondition::Semicolon))) {
                    return false;
                }
            } else {
                if (!(level.initial_value = ParseExpression(eEndCondition::Semicolon))) {
                    return false;
                }
            }
            if (!IsConstant(level.initial_value)) {
                fatal("not a valid constant expression");
                return false;
            }
        } else if (level.storage != eStorage::Uniform) {
            fatal("const-qualified variable declared but not initialized");
            return false;
        }
    }

    if (strnil(level.name)) {
        fatal("expected name for declaration");
        return false;
    }

    return true;
}

bool glslx::Parser::ParseStorage(top_level_t &current) {
    if (is_keyword(eKeyword::K_const)) {
        current.storage = eStorage::Const;
        if (!next()) {
            return false;
        }
    } else if (is_keyword(eKeyword::K_in)) {
        current.storage = eStorage::In;
        if (!next()) {
            return false;
        }
    } else if (is_keyword(eKeyword::K_out)) {
        current.storage = eStorage::Out;
        if (!next()) {
            return false;
        }
    } else if (is_keyword(eKeyword::K_attribute)) {
        current.storage = eStorage::Attribute;
        if (!next()) {
            return false;
        }
    } else if (is_keyword(eKeyword::K_uniform)) {
        current.storage = eStorage::Uniform;
        if (!next()) {
            return false;
        }
    } else if (is_keyword(eKeyword::K_varying)) {
        current.storage = eStorage::Varying;
        if (!next()) {
            return false;
        }
    } else if (is_keyword(eKeyword::K_buffer)) {
        current.storage = eStorage::Buffer;
        if (!next()) {
            return false;
        }
    } else if (is_keyword(eKeyword::K_shared)) {
        current.storage = eStorage::Shared;
        if (!next()) {
            return false;
        }
    } else if (is_keyword(eKeyword::K_rayPayloadEXT)) {
        current.storage = eStorage::RayPayload;
        if (!next()) {
            return false;
        }
    } else if (is_keyword(eKeyword::K_rayPayloadInEXT)) {
        current.storage = eStorage::RayPayloadIn;
        if (!next()) {
            return false;
        }
    } else if (is_keyword(eKeyword::K_hitAttributeEXT)) {
        current.storage = eStorage::HitAttribute;
        if (!next()) {
            return false;
        }
    } else if (is_keyword(eKeyword::K_callableDataEXT)) {
        current.storage = eStorage::CallableData;
        if (!next()) {
            return false;
        }
    } else if (is_keyword(eKeyword::K_callableDataInEXT)) {
        current.storage = eStorage::CallableDataIn;
        if (!next()) {
            return false;
        }
    }
    return true;
}

bool glslx::Parser::ParseAuxStorage(top_level_t &current) {
    if (is_keyword(eKeyword::K_centroid)) {
        if (current.storage != eStorage::In && current.storage != eStorage::Out) {
            fatal("aux storage qualifiers can only be used with in/out storage");
            return false;
        }
        current.aux_storage = eAuxStorage::Centroid;
        if (!next()) {
            return false;
        }
    } else if (is_keyword(eKeyword::K_sample)) {
        if (current.storage != eStorage::In && current.storage != eStorage::Out) {
            fatal("aux storage qualifiers can only be used with in/out storage");
            return false;
        }
        current.aux_storage = eAuxStorage::Sample;
        if (!next()) {
            return false;
        }
    } else if (is_keyword(eKeyword::K_patch)) {
        if (current.storage != eStorage::In && current.storage != eStorage::Out) {
            fatal("aux storage qualifiers can only be used with in/out storage");
            return false;
        }
        current.aux_storage = eAuxStorage::Patch;
        if (!next()) {
            return false;
        }
    }
    return true;
}

bool glslx::Parser::ParseInterpolation(top_level_t &current) {
    if (is_keyword(eKeyword::K_smooth)) {
        current.interpolation = eInterpolation::Smooth;
        if (!next()) {
            return false;
        }
    } else if (is_keyword(eKeyword::K_flat)) {
        current.interpolation = eInterpolation::Flat;
        if (!next()) {
            return false;
        }
    } else if (is_keyword(eKeyword::K_noperspective)) {
        current.interpolation = eInterpolation::Noperspective;
        if (!next()) {
            return false;
        }
    }
    return true;
}

bool glslx::Parser::ParsePrecision(ePrecision &precision) {
    if (is_keyword(eKeyword::K_highp)) {
        precision = ePrecision::Highp;
        if (!next()) {
            return false;
        }
    } else if (is_keyword(eKeyword::K_mediump)) {
        precision = ePrecision::Mediump;
        if (!next()) {
            return false;
        }
    } else if (is_keyword(eKeyword::K_lowp)) {
        precision = ePrecision::Lowp;
        if (!next()) {
            return false;
        }
    }
    return true;
}

bool glslx::Parser::ParseInvariant(top_level_t &current) {
    if (is_keyword(eKeyword::K_invariant)) {
        current.is_invariant = true;
        if (!next()) {
            return false;
        }
    }
    return true;
}

bool glslx::Parser::ParsePrecise(top_level_t &current) {
    if (is_keyword(eKeyword::K_precise)) {
        current.is_precise = true;
        if (!next()) {
            return false;
        }
    }
    return true;
}

bool glslx::Parser::ParseMemoryFlags(top_level_t &current) {
    if (is_keyword(eKeyword::K_coherent)) {
        current.memory_flags |= eMemory::Coherent;
        if (!next()) {
            return false;
        }
    } else if (is_keyword(eKeyword::K_volatile)) {
        current.memory_flags |= eMemory::Volatile;
        if (!next()) {
            return false;
        }
    } else if (is_keyword(eKeyword::K_restrict)) {
        current.memory_flags |= eMemory::Restrict;
        if (!next()) {
            return false;
        }
    } else if (is_keyword(eKeyword::K_readonly)) {
        current.memory_flags |= eMemory::Readonly;
        if (!next()) {
            return false;
        }
    } else if (is_keyword(eKeyword::K_writeonly)) {
        current.memory_flags |= eMemory::Writeonly;
        if (!next()) {
            return false;
        }
    }
    return true;
}

bool glslx::Parser::ParseLayout(top_level_t &current) {
    local_vector<ast_layout_qualifier *> &qualifiers = current.layout_qualifiers;
    if (is_keyword(eKeyword::K_layout)) {
        if (!next()) { // skip 'layout'
            return false;
        }
        if (!is_operator(eOperator::parenthesis_begin)) {
            fatal("expected '(' after 'layout'");
            return false;
        }
        if (!next()) { // skip '('
            return false;
        }
        while (!is_operator(eOperator::parenthesis_end)) {
            auto *qualifier = astnew<ast_layout_qualifier>();
            if (!qualifier) {
                return false;
            }
            if (!is_type(eTokType::Identifier) && !is_keyword(eKeyword::K_shared)) {
                return false;
            }

            int found = -1;
            qualifier->name = strnew(is_type(eTokType::Identifier) ? tok_.as_identifier : "shared");
            for (int i = 0; i < std::size(g_layout_qualifiers); ++i) {
                if (strcmp(qualifier->name, g_layout_qualifiers[i].qualifier) == 0) {
                    found = i;
                    break;
                }
            }

            if (found == -1) {
                fatal("unknown layout qualifier");
                return false;
            }

            if (!next()) {
                return false;
            }

            if (is_operator(eOperator::assign)) {
                if (!g_layout_qualifiers[found].is_assign) {
                    fatal("unexpected layout qualifier value");
                    return false;
                }
                if (!next()) { // skip '='
                    return false;
                }
                if (!(qualifier->initial_value =
                          ParseExpression(Bitmask{eEndCondition::Comma} | eEndCondition::Parenthesis))) {
                    return false;
                }
                if (!IsConstant(qualifier->initial_value)) {
                    fatal("value for layout qualifier `%s' is not a valid constant expression", qualifier->name);
                    return false;
                }
                if (!(qualifier->initial_value = Evaluate(qualifier->initial_value))) {
                    return false;
                }
            } else if (g_layout_qualifiers[found].is_assign) {
                fatal("expected layout qualifier value for `%s' layout qualifier", qualifier->name);
                return false;
            }

            if (is_operator(eOperator::comma)) {
                if (!next()) { // skip ','
                    return false;
                }
            }

            qualifiers.push_back(qualifier);
        }
        if (!next()) { // skip ')'
            return false;
        }
    }
    return true;
}

bool glslx::Parser::is_reserved_keyword(const eKeyword keyword) { return g_keywords[int(keyword)].is_reserved; }

bool glslx::Parser::is_interface_block_storage(const eStorage storage) {
    return storage == eStorage::In || storage == eStorage::Out || storage == eStorage::Uniform ||
           storage == eStorage::Buffer;
}

bool glslx::Parser::is_generic_type(const eKeyword keyword) {
    return keyword == eKeyword::K_genFType || keyword == eKeyword::K_genDType || keyword == eKeyword::K_genIType ||
           keyword == eKeyword::K_genUType || keyword == eKeyword::K_genBType || keyword == eKeyword::K_vec ||
           keyword == eKeyword::K_ivec || keyword == eKeyword::K_uvec || keyword == eKeyword::K_bvec ||
           keyword == eKeyword::K_gvec4 || keyword == eKeyword::K_gsampler1D ||
           keyword == eKeyword::K_gsampler1DArray || keyword == eKeyword::K_gsampler2D ||
           keyword == eKeyword::K_gsampler2DRect || keyword == eKeyword::K_gsampler2DArray ||
           keyword == eKeyword::K_gsampler2DMS || keyword == eKeyword::K_gsampler2DMSArray ||
           keyword == eKeyword::K_gsampler3D || keyword == eKeyword::K_gsamplerCube ||
           keyword == eKeyword::K_gsamplerCubeArray || keyword == eKeyword::K_gsamplerBuffer ||
           keyword == eKeyword::K_gimage1D || keyword == eKeyword::K_gimage2D || keyword == eKeyword::K_gimage3D ||
           keyword == eKeyword::K_gimageCube || keyword == eKeyword::K_gimageCubeArray ||
           keyword == eKeyword::K_gimage2DArray || keyword == eKeyword::K_gimage2DRect ||
           keyword == eKeyword::K_gimage1DArray || keyword == eKeyword::K_gimage2DMS ||
           keyword == eKeyword::K_gimage2DMSArray || keyword == eKeyword::K_gimageBuffer;
}

bool glslx::Parser::is_end_condition(Bitmask<eEndCondition> condition) const {
    return ((condition & eEndCondition::Semicolon) && is_type(eTokType::Semicolon)) ||
           ((condition & eEndCondition::Parenthesis) && is_operator(eOperator::parenthesis_end)) ||
           ((condition & eEndCondition::Bracket) && is_operator(eOperator::bracket_end)) ||
           ((condition & eEndCondition::Colon) && is_operator(eOperator::colon)) ||
           ((condition & eEndCondition::Comma) && is_operator(eOperator::comma));
}

bool glslx::Parser::is_builtin() const {
    if (!is_type(eTokType::Keyword)) {
        return false;
    }
    return g_keywords[int(tok_.as_keyword)].is_typename;
}

bool glslx::Parser::is_vector_type(const ast_type *type) {
    if (!type->builtin) {
        return false;
    }
    const auto *btype = static_cast<const ast_builtin *>(type);
    return btype->type == eKeyword::K_vec2 || btype->type == eKeyword::K_vec3 || btype->type == eKeyword::K_vec4 ||
           btype->type == eKeyword::K_ivec2 || btype->type == eKeyword::K_ivec3 || btype->type == eKeyword::K_ivec4 ||
           btype->type == eKeyword::K_uvec2 || btype->type == eKeyword::K_uvec3 || btype->type == eKeyword::K_uvec4;
}

void CHECK_FORMAT_STRING(2, 3) glslx::Parser::fatal(_Printf_format_string_ const char *fmt, ...) {
    char *banner = nullptr;
    const int banner_len = allocfmt(&banner, "%s:%zu:%zu: error: ", file_name_, lexer_.line(), lexer_.column());
    if (banner_len == -1) {
        error_ = "Out of memory";
        return;
    }

    char *message = nullptr;
    va_list va;
    va_start(va, fmt);
    const int message_len = allocvfmt(&message, fmt, va);
    if (message_len == -1) {
        va_end(va);
        error_ = "Out of memory";
        return;
    }
    va_end(va);

    // concatenate
    char *concat = ast_->alloc.allocator.allocate(banner_len + message_len + 1);
    if (!concat) {
        free(banner);
        free(message);
        error_ = "Out of memory";
        return;
    }

    memcpy(concat, banner, banner_len);
    memcpy(concat + banner_len, message, message_len + 1);
    free(banner);
    free(message);

    error_ = concat;
    const bool inserted = ast_->str.Insert(concat);
    if (!inserted) {
        error_ = *ast_->str.Find(concat);
        ast_->alloc.allocator.deallocate(concat, banner_len + message_len + 1);
    }
}

glslx::ast_constant_expression *glslx::Parser::Evaluate(ast_expression *expression) {
    if (!expression) {
        return nullptr;
    } else if (IsConstantValue(expression)) {
        return expression;
    } else if (expression->type == eExprType::VariableIdentifier) {
        auto *var = static_cast<ast_variable_identifier *>(expression)->variable;
        if (var->type == eVariableType::Global) {
            return Evaluate(static_cast<ast_global_variable *>(var)->initial_value);
        } else if (var->type == eVariableType::Function) {
            return Evaluate(static_cast<ast_function_variable *>(var)->initial_value);
        }
        return expression;
    } else if (expression->type == eExprType::UnaryMinus) {
        ast_expression *operand = Evaluate(static_cast<ast_unary_expression *>(expression)->operand);
        if (!operand) {
            return nullptr;
        }
        switch (operand->type) {
        case eExprType::IntConstant:
            return astnew<ast_int_constant>(-IVAL(operand));
        case eExprType::HalfConstant:
            return astnew<ast_half_constant>(-HVAL(operand));
        case eExprType::FloatConstant:
            return astnew<ast_float_constant>(-FVAL(operand));
        case eExprType::DoubleConstant:
            return astnew<ast_double_constant>(-DVAL(operand));
        default:
            fatal("invalid operation in constant expression");
            return nullptr;
        }
    } else if (expression->type == eExprType::UnaryPlus) {
        ast_expression *operand = Evaluate(static_cast<ast_unary_expression *>(expression)->operand);
        if (!operand) {
            return nullptr;
        }
        switch (operand->type) {
        case eExprType::IntConstant:
        case eExprType::UIntConstant:
        case eExprType::HalfConstant:
        case eExprType::FloatConstant:
        case eExprType::DoubleConstant:
            return operand;
        default:
            fatal("invalid operation in constant expression");
            return nullptr;
        }
    } else if (expression->type == eExprType::BitNot) {
        ast_expression *operand = Evaluate(static_cast<ast_unary_expression *>(expression)->operand);
        switch (operand->type) {
        case eExprType::IntConstant:
            return astnew<ast_int_constant>(~IVAL(operand));
        case eExprType::UIntConstant:
            return astnew<ast_uint_constant>(~UVAL(operand));
        default:
            fatal("invalid operation in constant expression");
            return nullptr;
        }
    } else if (expression->type == eExprType::Operation) {
        const eOperator oper = static_cast<ast_operation_expression *>(expression)->oper;
        ast_expression *lhs = Evaluate(static_cast<ast_binary_expression *>(expression)->operand1);
        ast_expression *rhs = Evaluate(static_cast<ast_binary_expression *>(expression)->operand2);
        if (!lhs || !rhs) {
            return nullptr;
        }
        switch (lhs->type) {
        case eExprType::IntConstant:
            switch (oper) {
            case eOperator::multiply:
                return astnew<ast_int_constant>(IVAL(lhs) * IVAL(rhs));
            case eOperator::divide:
                return astnew<ast_int_constant>(IVAL(lhs) / IVAL(rhs));
            case eOperator::modulus:
                return astnew<ast_int_constant>(IVAL(lhs) % IVAL(rhs));
            case eOperator::plus:
                return astnew<ast_int_constant>(IVAL(lhs) + IVAL(rhs));
            case eOperator::minus:
                return astnew<ast_int_constant>(IVAL(lhs) - IVAL(rhs));
            case eOperator::shift_left:
                return astnew<ast_int_constant>(IVAL(lhs) << IVAL(rhs));
            case eOperator::shift_right:
                return astnew<ast_int_constant>(IVAL(lhs) >> IVAL(rhs));
            case eOperator::less:
                return astnew<ast_bool_constant>(IVAL(lhs) < IVAL(rhs));
            case eOperator::greater:
                return astnew<ast_bool_constant>(IVAL(lhs) > IVAL(rhs));
            case eOperator::less_equal:
                return astnew<ast_bool_constant>(IVAL(lhs) <= IVAL(rhs));
            case eOperator::greater_equal:
                return astnew<ast_bool_constant>(IVAL(lhs) >= IVAL(rhs));
            case eOperator::equal:
                return astnew<ast_bool_constant>(IVAL(lhs) == IVAL(rhs));
            case eOperator::not_equal:
                return astnew<ast_bool_constant>(IVAL(lhs) != IVAL(rhs));
            case eOperator::bit_and:
                return astnew<ast_int_constant>(IVAL(lhs) & IVAL(rhs));
            case eOperator::bit_xor:
                return astnew<ast_int_constant>(IVAL(lhs) ^ IVAL(rhs));
            case eOperator::logical_and:
                return astnew<ast_bool_constant>(IVAL(lhs) && IVAL(rhs));
            case eOperator::logical_xor:
                return astnew<ast_bool_constant>(!IVAL(lhs) != !IVAL(rhs));
            case eOperator::logical_or:
                return astnew<ast_bool_constant>(IVAL(lhs) || IVAL(rhs));
            case eOperator::bit_or:
                return astnew<ast_int_constant>(IVAL(lhs) | IVAL(rhs));
            default:
                fatal("invalid operation in constant expression");
                return nullptr;
            }
            break;
        case eExprType::UIntConstant:
            switch (oper) {
            case eOperator::multiply:
                return astnew<ast_uint_constant>(UVAL(lhs) * UVAL(rhs));
            case eOperator::divide:
                return astnew<ast_uint_constant>(UVAL(lhs) / UVAL(rhs));
            case eOperator::modulus:
                return astnew<ast_uint_constant>(UVAL(lhs) % UVAL(rhs));
            case eOperator::plus:
                return astnew<ast_uint_constant>(UVAL(lhs) + UVAL(rhs));
            case eOperator::minus:
                return astnew<ast_uint_constant>(UVAL(lhs) - UVAL(rhs));
            case eOperator::shift_left:
                return astnew<ast_uint_constant>(UVAL(lhs) << UVAL(rhs));
            case eOperator::shift_right:
                return astnew<ast_uint_constant>(UVAL(lhs) >> UVAL(rhs));
            case eOperator::less:
                return astnew<ast_bool_constant>(UVAL(lhs) < UVAL(rhs));
            case eOperator::greater:
                return astnew<ast_bool_constant>(UVAL(lhs) > UVAL(rhs));
            case eOperator::less_equal:
                return astnew<ast_bool_constant>(UVAL(lhs) <= UVAL(rhs));
            case eOperator::greater_equal:
                return astnew<ast_bool_constant>(UVAL(lhs) >= UVAL(rhs));
            case eOperator::equal:
                return astnew<ast_bool_constant>(UVAL(lhs) == UVAL(rhs));
            case eOperator::not_equal:
                return astnew<ast_bool_constant>(UVAL(lhs) != UVAL(rhs));
            case eOperator::bit_and:
                return astnew<ast_uint_constant>(UVAL(lhs) & UVAL(rhs));
            case eOperator::bit_xor:
                return astnew<ast_uint_constant>(UVAL(lhs) ^ UVAL(rhs));
            case eOperator::logical_and:
                return astnew<ast_bool_constant>(UVAL(lhs) && UVAL(rhs));
            case eOperator::logical_xor:
                return astnew<ast_bool_constant>(!UVAL(lhs) != !UVAL(rhs));
            case eOperator::logical_or:
                return astnew<ast_bool_constant>(UVAL(lhs) || UVAL(rhs));
            case eOperator::bit_or:
                return astnew<ast_uint_constant>(UVAL(lhs) | UVAL(rhs));
            default:
                fatal("invalid operation in constant expression");
                return nullptr;
            }
            break;
        case eExprType::HalfConstant:
            switch (oper) {
            case eOperator::multiply:
                return astnew<ast_half_constant>(HVAL(lhs) * HVAL(rhs));
            case eOperator::divide:
                return astnew<ast_half_constant>(HVAL(lhs) / HVAL(rhs));
            case eOperator::plus:
                return astnew<ast_half_constant>(HVAL(lhs) + HVAL(rhs));
            case eOperator::minus:
                return astnew<ast_half_constant>(HVAL(lhs) - HVAL(rhs));
            case eOperator::less:
                return astnew<ast_half_constant>(HVAL(lhs) < HVAL(rhs));
            case eOperator::greater:
                return astnew<ast_half_constant>(HVAL(lhs) > HVAL(rhs));
            case eOperator::less_equal:
                return astnew<ast_half_constant>(HVAL(lhs) <= HVAL(rhs));
            case eOperator::greater_equal:
                return astnew<ast_half_constant>(HVAL(lhs) >= HVAL(rhs));
            case eOperator::equal:
                return astnew<ast_half_constant>(HVAL(lhs) == HVAL(rhs));
            case eOperator::not_equal:
                return astnew<ast_half_constant>(HVAL(lhs) != HVAL(rhs));
            case eOperator::logical_and:
                return astnew<ast_half_constant>(HVAL(lhs) && HVAL(rhs));
            case eOperator::logical_xor:
                return astnew<ast_half_constant>(!HVAL(lhs) != !HVAL(rhs));
            case eOperator::logical_or:
                return astnew<ast_half_constant>(HVAL(lhs) || HVAL(rhs));
            default:
                fatal("invalid operation in constant expression");
                return nullptr;
            }
            break;
        case eExprType::FloatConstant:
            switch (oper) {
            case eOperator::multiply:
                return astnew<ast_float_constant>(FVAL(lhs) * FVAL(rhs));
            case eOperator::divide:
                return astnew<ast_float_constant>(FVAL(lhs) / FVAL(rhs));
            case eOperator::plus:
                return astnew<ast_float_constant>(FVAL(lhs) + FVAL(rhs));
            case eOperator::minus:
                return astnew<ast_float_constant>(FVAL(lhs) - FVAL(rhs));
            case eOperator::less:
                return astnew<ast_float_constant>(FVAL(lhs) < FVAL(rhs));
            case eOperator::greater:
                return astnew<ast_float_constant>(FVAL(lhs) > FVAL(rhs));
            case eOperator::less_equal:
                return astnew<ast_float_constant>(FVAL(lhs) <= FVAL(rhs));
            case eOperator::greater_equal:
                return astnew<ast_float_constant>(FVAL(lhs) >= FVAL(rhs));
            case eOperator::equal:
                return astnew<ast_float_constant>(FVAL(lhs) == FVAL(rhs));
            case eOperator::not_equal:
                return astnew<ast_float_constant>(FVAL(lhs) != FVAL(rhs));
            case eOperator::logical_and:
                return astnew<ast_float_constant>(FVAL(lhs) && FVAL(rhs));
            case eOperator::logical_xor:
                return astnew<ast_float_constant>(!FVAL(lhs) != !FVAL(rhs));
            case eOperator::logical_or:
                return astnew<ast_float_constant>(FVAL(lhs) || FVAL(rhs));
            default:
                fatal("invalid operation in constant expression");
                return nullptr;
            }
            break;
        case eExprType::DoubleConstant:
            switch (oper) {
            case eOperator::multiply:
                return astnew<ast_double_constant>(DVAL(lhs) * DVAL(rhs));
            case eOperator::divide:
                return astnew<ast_double_constant>(DVAL(lhs) / DVAL(rhs));
            case eOperator::plus:
                return astnew<ast_double_constant>(DVAL(lhs) + DVAL(rhs));
            case eOperator::minus:
                return astnew<ast_double_constant>(DVAL(lhs) - DVAL(rhs));
            case eOperator::less:
                return astnew<ast_double_constant>(DVAL(lhs) < DVAL(rhs));
            case eOperator::greater:
                return astnew<ast_double_constant>(DVAL(lhs) > DVAL(rhs));
            case eOperator::less_equal:
                return astnew<ast_double_constant>(DVAL(lhs) <= DVAL(rhs));
            case eOperator::greater_equal:
                return astnew<ast_double_constant>(DVAL(lhs) >= DVAL(rhs));
            case eOperator::equal:
                return astnew<ast_double_constant>(DVAL(lhs) == DVAL(rhs));
            case eOperator::not_equal:
                return astnew<ast_double_constant>(DVAL(lhs) != DVAL(rhs));
            case eOperator::logical_and:
                return astnew<ast_double_constant>(DVAL(lhs) && DVAL(rhs));
            case eOperator::logical_xor:
                return astnew<ast_double_constant>(!DVAL(lhs) != !DVAL(rhs));
            case eOperator::logical_or:
                return astnew<ast_double_constant>(DVAL(lhs) || DVAL(rhs));
            default:
                fatal("invalid operation in constant expression");
                return nullptr;
            }
            break;
        case eExprType::BoolConstant:
            switch (oper) {
            case eOperator::equal:
                return astnew<ast_bool_constant>(BVAL(lhs) == BVAL(rhs));
            case eOperator::not_equal:
                return astnew<ast_bool_constant>(BVAL(lhs) != BVAL(rhs));
            case eOperator::logical_and:
                return astnew<ast_bool_constant>(BVAL(lhs) && BVAL(rhs));
            case eOperator::logical_or:
                return astnew<ast_bool_constant>(BVAL(lhs) || BVAL(rhs));
            case eOperator::logical_xor:
                return astnew<ast_bool_constant>(!BVAL(lhs) != !BVAL(rhs));
            default:
                fatal("invalid operation in constant expression");
                return nullptr;
            }
            break;
        default:
            fatal("invalid operation in constant expression");
            return nullptr;
        }
    } else if (expression->type == eExprType::Sequence) {
        return expression;
    } else if (expression->type == eExprType::ConstructorCall) {
        return expression;
    } else if (expression->type == eExprType::ArraySpecifier) {
        auto *arr_specifier = static_cast<ast_array_specifier *>(expression);
        for (int i = 0; i < int(arr_specifier->expressions.size()); ++i) {
            arr_specifier->expressions[i] = Evaluate(arr_specifier->expressions[i]);
        }
        return expression;
    } else {
        return Evaluate(expression);
    }
    return nullptr;
}

glslx::ast_builtin *glslx::Parser::ParseBuiltin() {
    if (!is_type(eTokType::Keyword)) {
        fatal("expected keyword");
        return nullptr;
    }

    if (!g_keywords[int(tok_.as_keyword)].is_typename) {
        fatal("exprected typename");
        return nullptr;
    }

    return FindOrAddBuiltin(tok_.as_keyword);
}

glslx::ast_builtin *glslx::Parser::FindOrAddBuiltin(eKeyword type) {
    for (int i = 0; i < int(builtins_.size()); ++i) {
        if (builtins_[i]->type == type) {
            return builtins_[i];
        }
    }

    builtins_.push_back(astnew<ast_builtin>(type));
    return builtins_.back();
}

template <typename T> T *glslx::Parser::ParseBlock(const char *type) {
    T *unique = astnew<T>(ast_->alloc.allocator);
    if (!unique) {
        return nullptr;
    }

    if (is_type(eTokType::Semicolon)) {
        return unique;
    }

    if (is_type(eTokType::Identifier)) {
        unique->name = strnew(tok_.as_identifier);
        if (!next()) { // skip identifier
            return nullptr;
        }
    }

    if (!is_type(eTokType::Scope_Begin)) {
        fatal("expected '{' for %s definition", type);
        return nullptr;
    }

    if (!next()) { // skip '{'
        return nullptr;
    }

    global_vector<top_level_t> items;
    while (!is_type(eTokType::Scope_End)) {
        if (!ParseTopLevel(items)) {
            return nullptr;
        }
        if (!next()) {
            return nullptr;
        }
    }

    for (int i = 0; i < int(items.size()); ++i) {
        top_level_t &parse = items[i];
        auto *field = astnew<ast_variable>(eVariableType::Field, ast_->alloc.allocator);
        if (!field) {
            return nullptr;
        }
        field->base_type = parse.type;
        field->name = strnew(parse.name);
        field->is_precise = parse.is_precise;
        field->is_array = parse.is_array;
        field->array_sizes = std::move(parse.array_sizes);
        unique->fields.push_back(field);
    }

    if (!next()) { // skip '}'
        return nullptr;
    }

    return unique;
}

glslx::ast_struct *glslx::Parser::ParseStruct() { return ParseBlock<ast_struct>("structure"); }

glslx::ast_interface_block *glslx::Parser::ParseInterfaceBlock(const eStorage storage) {
    ast_interface_block *unique = nullptr;
    switch (storage) {
    case eStorage::In:
        unique = ParseBlock<ast_interface_block>("input block");
        break;
    case eStorage::Out:
        unique = ParseBlock<ast_interface_block>("output block");
        break;
    case eStorage::Uniform:
        unique = ParseBlock<ast_interface_block>("uniform block");
        break;
    case eStorage::Buffer:
        unique = ParseBlock<ast_interface_block>("buffer block");
        break;
    default:
        fatal("unknown storage");
        return nullptr;
    }

    if (!unique) {
        return nullptr;
    }

    if (!is_type(eTokType::Identifier)) {
        const int fields_count = int(unique->fields.size());
        for (int i = 0; i < fields_count; ++i) {
            // check if variable already exists
            const ast_variable *variable = unique->fields[i];
            if (FindVariable(variable->name)) {
                fatal("'%s' is already declared in this scope", variable->name);
                return nullptr;
            }
            scopes_.back().push_back(unique->fields[i]);
        }
    }

    unique->storage = storage;
    return unique;
}

glslx::ast_function *glslx::Parser::ParseFunction(const top_level_t &parse) {
    auto *function = astnew<ast_function>(ast_->alloc.allocator);
    if (!function) {
        return nullptr;
    }
    function->return_type = parse.type;
    function->name = strnew(parse.name);

    if (!expect(eOperator::parenthesis_begin)) {
        return nullptr;
    }

    while (!is_operator(eOperator::parenthesis_end)) {
        auto *parameter = astnew<ast_function_parameter>(ast_->alloc.allocator);
        if (!parameter) {
            return nullptr;
        }
        while (!is_operator(eOperator::comma) && !is_operator(eOperator::parenthesis_end)) {
            if (is_keyword(eKeyword::K_const)) {
                parameter->qualifiers |= eParamQualifier::Const;
            } else if (is_keyword(eKeyword::K_in)) {
                parameter->qualifiers |= eParamQualifier::In;
            } else if (is_keyword(eKeyword::K_out)) {
                parameter->qualifiers |= eParamQualifier::Out;
            } else if (is_keyword(eKeyword::K_inout)) {
                parameter->qualifiers |= eParamQualifier::Inout;
            } else if (is_keyword(eKeyword::K_highp)) {
                parameter->precision = ePrecision::Highp;
            } else if (is_keyword(eKeyword::K_mediump)) {
                parameter->precision = ePrecision::Mediump;
            } else if (is_keyword(eKeyword::K_lowp)) {
                parameter->precision = ePrecision::Lowp;
            } else if (is_operator(eOperator::bracket_begin)) {
                parameter->is_array = true;
                while (is_operator(eOperator::bracket_begin)) {
                    ast_constant_expression *array_size = ParseArraySize();
                    if (!array_size) {
                        return nullptr;
                    }
                    parameter->array_sizes.push_back(array_size);
                }
            } else {
                if (is_builtin()) {
                    parameter->base_type = ParseBuiltin();
                    if (parameter->base_type && parameter->base_type->builtin) {
                        auto *builtin = static_cast<ast_builtin *>(parameter->base_type);
                        if (builtin->type == eKeyword::K_void && !strnil(parameter->name)) {
                            fatal("'void' parameter cannot be named");
                            return nullptr;
                        }
                    }
                } else if (is_type(eTokType::Identifier)) {
                    parameter->base_type = FindType(tok_.as_identifier);
                }

                const token_t &peek = lexer_.Peek();
                if (peek.type == eTokType::Identifier) {
                    parameter->name = strnew(peek.as_identifier);
                    if (!next()) {
                        return nullptr;
                    }
                }
            }
            if (!next()) {
                return nullptr;
            }
        }

        if (!parameter->base_type) {
            fatal("expected type");
            return nullptr;
        }
        function->parameters.push_back(parameter);
        if (is_operator(eOperator::comma)) {
            if (!next()) { // skip ','
                return nullptr;
            }
        }
    }

    if (!expect(eOperator::parenthesis_end)) {
        return nullptr;
    }

    if (expect(eOperator::bracket_begin)) {
        if (!expect(eOperator::bracket_begin)) {
            return nullptr;
        }

        if (tok_.type == eTokType::Identifier) {
            if (strcmp(tok_.as_identifier, "builtin") == 0) {
                function->attributes |= eFunctionAttribute::Builtin;
            }
        }
        if (!next()) {
            return nullptr;
        }

        if (!expect(eOperator::bracket_end) || !expect(eOperator::bracket_end)) {
            return nullptr;
        }
    }

    if (function->parameters.size() == 1 && function->parameters[0]->base_type->builtin) {
        auto *type = static_cast<ast_builtin *>(function->parameters[0]->base_type);
        if (type->type == eKeyword::K_void) {
            // drop single void parameter
            function->parameters.clear();
        }
    }

    if (strcmp(function->name, "main") == 0) {
        if (!function->parameters.empty()) {
            fatal("'main' cannot have parameters");
            return nullptr;
        }
        if (!function->return_type->builtin ||
            static_cast<ast_builtin *>(function->return_type)->type != eKeyword::K_void) {
            fatal("'main' must be declared to return void");
            return nullptr;
        }
    }

    if (is_type(eTokType::Scope_Begin)) {
        function->is_prototype = false;
        if (!next()) { // skip '{'
            return nullptr;
        }

        scopes_.emplace_back();
        for (int i = 0; i < int(function->parameters.size()); ++i) {
            scopes_.back().push_back(function->parameters[i]);
        }
        while (!is_type(eTokType::Scope_End)) {
            ast_statement *statement = ParseStatement();
            if (!statement) {
                return nullptr;
            }
            function->statements.push_back(statement);
            if (!next()) { // skip ';'
                return nullptr;
            }
        }

        scopes_.pop_back();
    } else if (is_type(eTokType::Semicolon)) {
        function->is_prototype = true;
    } else {
        fatal("expected '{' or ';'");
        return nullptr;
    }
    return function;
}

glslx::ast_constructor_call *glslx::Parser::ParseConstructorCall() {
    auto *expression = astnew<ast_constructor_call>(ast_->alloc.allocator);
    if (!expression) {
        return nullptr;
    }
    if (is_builtin()) {
        if (!(expression->type = ParseBuiltin())) {
            return nullptr;
        }
    } else {
        expression->type = FindType(tok_.as_identifier);
        if (!expression->type) {
            return nullptr;
        }
    }
    if (!next()) {
        return nullptr;
    }
    if (!is_operator(eOperator::parenthesis_begin)) {
        fatal("expected '(' for constructor call");
        return nullptr;
    }
    if (!next()) { // skip '('
        return nullptr;
    }
    while (!is_operator(eOperator::parenthesis_end)) {
        ast_expression *parameter = ParseExpression(Bitmask{eEndCondition::Comma} | eEndCondition::Parenthesis);
        if (!parameter) {
            return nullptr;
        }
        expression->parameters.push_back(parameter);
        if (is_operator(eOperator::comma)) {
            if (!next()) { // skip ','
                return nullptr;
            }
        }
    }
    return expression;
}

glslx::ast_function_call *glslx::Parser::ParseFunctionCall() {
    auto *expression = astnew<ast_function_call>(ast_->alloc.allocator);
    function_calls_.push_back(expression);
    if (!expression) {
        return nullptr;
    }
    expression->name = strnew(tok_.as_identifier);
    if (!next()) { // skip identifier
        return nullptr;
    }
    if (!is_operator(eOperator::parenthesis_begin)) {
        fatal("expected '(' for function call");
        return nullptr;
    }
    if (!next()) {
        return nullptr;
    }
    while (!is_operator(eOperator::parenthesis_end)) {
        ast_expression *parameter = ParseExpression(Bitmask{eEndCondition::Comma} | eEndCondition::Parenthesis);
        if (!parameter) {
            return nullptr;
        }
        expression->parameters.push_back(parameter);
        if (is_operator(eOperator::comma)) {
            if (!next()) { // skip ','
                return nullptr;
            }
        }
    }
    return expression;
}

glslx::ast_expression *glslx::Parser::ParseExpression(const Bitmask<eEndCondition> condition) {
    ast_expression *lhs = ParseUnary(condition);
    if (!lhs) {
        return nullptr;
    }
    if (!next()) {
        return nullptr;
    }
    return ParseBinary(0, lhs, condition);
}

glslx::ast_expression *glslx::Parser::ParseUnary(const Bitmask<eEndCondition> condition) {
    ast_expression *operand = ParseUnaryPrefix(condition);
    if (!operand) {
        return nullptr;
    }
    while (true) {
        const token_t &peek = lexer_.Peek();
        if (peek.type == eTokType::Operator && peek.as_operator == eOperator::dot) {
            if (!next()) { // skip last
                return nullptr;
            }
            if (!next()) { // skip '.'
                return nullptr;
            }
            if (!is_type(eTokType::Identifier)) {
                fatal("expected field identifier or swizzle after '.'");
                return nullptr;
            }
            auto *expression = astnew<ast_field_or_swizzle>();
            if (!expression) {
                return nullptr;
            }
            int array_dims = 0;
            const ast_type *type = Evaluate_ExpressionResultType(ast_.get(), operand, array_dims);
            if (type && !type->builtin) {
                ast_variable *field = nullptr;
                const auto *kind = static_cast<const ast_struct *>(type);
                for (int i = 0; i < int(kind->fields.size()); ++i) {
                    if (strcmp(kind->fields[i]->name, tok_.as_identifier) == 0) {
                        field = kind->fields[i];
                        break;
                    }
                }
                if (!field) {
                    fatal("field '%s' does not exist in structure %s", tok_.as_identifier, kind->name);
                    return nullptr;
                }

                expression->field = astnew<ast_variable_identifier>(field);
            }

            expression->operand = operand;
            expression->name = strnew(tok_.as_identifier);
            operand = expression;
        } else if (peek.type == eTokType::Operator && peek.as_operator == eOperator::increment) {
            if (!next()) { // skip last
                return nullptr;
            }
            return astnew<ast_post_increment_expression>(operand);
        } else if (peek.type == eTokType::Operator && peek.as_operator == eOperator::decrement) {
            if (!next()) { // skip last
                return nullptr;
            }
            return astnew<ast_post_decrement_expression>(operand);
        } else if (peek.type == eTokType::Operator && peek.as_operator == eOperator::bracket_begin) {
            if (!next()) { // skip last
                return nullptr;
            }
            if (!next()) { // skip '['
                return nullptr;
            }
            auto *expression = astnew<ast_array_subscript>();
            if (!expression) {
                return nullptr;
            }
            ast_expression *find = operand;
            while (true) {
                if (find->type == eExprType::ArraySubscript) {
                    find = static_cast<ast_array_subscript *>(find)->operand;
                } else if (find->type == eExprType::FieldOrSwizzle) {
                    find = static_cast<ast_field_or_swizzle *>(find)->field;
                } else {
                    break;
                }
            }
            if (find->type != eExprType::VariableIdentifier && find->type != eExprType::FunctionCall) {
                fatal("cannot be subscripted");
                return nullptr;
            }
            expression->operand = operand;
            if (!(expression->index = ParseExpression(eEndCondition::Bracket))) {
                return nullptr;
            }
            if (IsConstant(expression->index)) {
                if (!(expression->index = Evaluate(expression->index))) {
                    return nullptr;
                }
            }
            operand = expression;
        } else /*if (peek.type == eTokType::Operator && peek.as_operator == eOperator::questionmark) {
            if (!next()) { // skip last
                return nullptr;
            }
            if (!next()) { // skip '?'
                return nullptr;
            }
            ast_ternary_expression *expression = new (&ast_mem_) ast_ternary_expression();
            if (!expression) {
                return nullptr;
            }
            expression->condition = operand;
            expression->on_true = ParseExpression(eEndCondition::Colon);
            if (!is_operator(eOperator::colon)) {
                fatal("expected ':' for else case in ternary statement");
                return nullptr;
            }
            if (!next()) { // skip ':'
                return nullptr;
            }
            if (!(expression->on_false = ParseUnary(condition))) {
                fatal("expected expression after ':' in ternary statement");
                return nullptr;
            }
            operand = expression;
        } else*/
        {
            break;
        }
    }
    return operand;
}

glslx::ast_expression *glslx::Parser::ParseBinary(const int lhs_precedence, ast_expression *lhs,
                                                  Bitmask<eEndCondition> condition) {
    // https://eli.thegreenplace.net/2012/08/02/parsing-expressions-by-precedence-climbing
    while (!is_end_condition(condition)) {
        const int binary_precedence = tok_.precedence();
        if (binary_precedence < lhs_precedence) {
            break;
        }

        if (is_operator(eOperator::questionmark)) {
            if (!next()) { // skip '?'
                return nullptr;
            }
            auto *expression = astnew<ast_ternary_expression>();
            if (!expression) {
                return nullptr;
            }
            expression->condition = lhs;
            expression->on_true = ParseExpression(eEndCondition::Colon);
            if (!is_operator(eOperator::colon)) {
                fatal("expected ':' for else case in ternary statement");
                return nullptr;
            }
            if (!next()) { // skip ':'
                return nullptr;
            }
            if (!(expression->on_false = ParseExpression(condition))) {
                fatal("expected expression after ':' in ternary statement");
                return nullptr;
            }
            lhs = expression;
            continue;
        }

        ast_binary_expression *expression = CreateExpression();
        if (!next()) {
            return nullptr;
        }

        ast_expression *rhs = ParseUnary(condition);
        if (!rhs || !next()) {
            return nullptr;
        }

        if (static_cast<ast_expression *>(expression)->type == eExprType::Assign) {
            ast_expression *find = lhs;
            while (find->type == eExprType::ArraySubscript || find->type == eExprType::FieldOrSwizzle ||
                   find->type == eExprType::Assign) {
                if (find->type == eExprType::ArraySubscript) {
                    find = static_cast<ast_array_subscript *>(find)->operand;
                } else if (find->type == eExprType::FieldOrSwizzle) {
                    find = static_cast<ast_field_or_swizzle *>(find)->operand;
                } else if (find->type == eExprType::Assign) {
                    find = static_cast<ast_assignment_expression *>(find)->operand2;
                }
            }
            if (find->type != eExprType::VariableIdentifier) {
                fatal("not a valid lvalue");
                return nullptr;
            }
            ast_variable *variable = static_cast<ast_variable_identifier *>(find)->variable;
            if (variable->type == eVariableType::Global) {
                auto *global = static_cast<ast_global_variable *>(variable);
                if (global->storage == eStorage::In) {
                    fatal("cannot write to a variable declared as input");
                    return nullptr;
                }
                if (global->storage == eStorage::Const) {
                    fatal("cannot write to a const variable outside of its declaration");
                    return nullptr;
                }
            }
        }

        const int rhs_precedence = tok_.precedence();

        if (binary_precedence < rhs_precedence) {
            if (!(rhs = ParseBinary(binary_precedence + 1, rhs, condition))) {
                return nullptr;
            }
        }

        expression->operand1 = lhs;
        expression->operand2 = rhs;
        lhs = expression;
    }
    return lhs;
}

glslx::ast_expression *glslx::Parser::ParseUnaryPrefix(const Bitmask<eEndCondition> condition) {
    if (is_operator(eOperator::parenthesis_begin)) {
        if (!next()) { // skip '('
            return nullptr;
        }
        return ParseExpression(eEndCondition::Parenthesis);
    } else if (is_operator(eOperator::logital_not)) {
        if (!next()) { // skip '!'
            return nullptr;
        }
        return astnew<ast_unary_logical_not_expression>(ParseUnary(condition));
    } else if (is_operator(eOperator::bit_not)) {
        if (!next()) { // skip '~'
            return nullptr;
        }
        return astnew<ast_unary_bit_not_expression>(ParseUnary(condition));
    } else if (is_operator(eOperator::plus)) {
        if (!next()) { // skip '+'
            return nullptr;
        }
        return astnew<ast_unary_plus_expression>(ParseUnary(condition));
    } else if (is_operator(eOperator::minus)) {
        if (!next()) { // skip '+'
            return nullptr;
        }
        return astnew<ast_unary_minus_expression>(ParseUnary(condition));
    } else if (is_operator(eOperator::increment)) {
        if (!next()) { // skip '++'
            return nullptr;
        }
        return astnew<ast_prefix_increment_expression>(ParseUnary(condition));
    } else if (is_operator(eOperator::decrement)) {
        if (!next()) { // skip '--'
            return nullptr;
        }
        return astnew<ast_prefix_decrement_expression>(ParseUnary(condition));
    } else if (is_builtin()) {
        return ParseConstructorCall();
    } else if (is_type(eTokType::Identifier)) {
        const token_t &peek = lexer_.Peek();
        if (peek.type == eTokType::Operator && peek.as_operator == eOperator::parenthesis_begin) {
            ast_type *type = FindType(tok_.as_identifier);
            if (type) {
                return ParseConstructorCall();
            } else {
                return ParseFunctionCall();
            }
        } else {
            ast_variable *find = FindVariable(tok_.as_identifier);
            if (!find) {
                fatal("'%s' was not declared in this scope", tok_.as_identifier);
                return nullptr;
            }
            return astnew<ast_variable_identifier>(find);
        }
    } else if (is_keyword(eKeyword::K_true)) {
        return astnew<ast_bool_constant>(true);
    } else if (is_keyword(eKeyword::K_false)) {
        return astnew<ast_bool_constant>(false);
    } else if (is_type(eTokType::Const_short)) {
        return astnew<ast_short_constant>(tok_.as_short);
    } else if (is_type(eTokType::Const_ushort)) {
        return astnew<ast_ushort_constant>(tok_.as_ushort);
    } else if (is_type(eTokType::Const_int)) {
        return astnew<ast_int_constant>(tok_.as_int);
    } else if (is_type(eTokType::Const_uint)) {
        return astnew<ast_uint_constant>(tok_.as_uint);
    } else if (is_type(eTokType::Const_long)) {
        return astnew<ast_long_constant>(tok_.as_long);
    } else if (is_type(eTokType::Const_ulong)) {
        return astnew<ast_ulong_constant>(tok_.as_ulong);
    } else if (is_type(eTokType::Const_half)) {
        return astnew<ast_half_constant>(tok_.as_half);
    } else if (is_type(eTokType::Const_float)) {
        return astnew<ast_float_constant>(tok_.as_float);
    } else if (is_type(eTokType::Const_double)) {
        return astnew<ast_double_constant>(tok_.as_double);
    } else if (condition == eEndCondition::Bracket) {
        return nullptr;
    }
    fatal("syntax error");
    return nullptr;
}

glslx::ast_constant_expression *glslx::Parser::ParseArraySize() {
    if (!next()) { // skip '['
        return nullptr;
    }
    return Evaluate(ParseExpression(eEndCondition::Bracket));
}

glslx::ast_expression *glslx::Parser::ParseArraySpecifier(Bitmask<eEndCondition> condition) {
    bool accept_paren = false;
    if (is_builtin()) {
        const token_t &peek = lexer_.Peek();
        if (peek.type == eTokType::Operator && peek.as_operator == eOperator::bracket_begin) {
            if (!next()) {
                return nullptr;
            }
            if (!expect(eOperator::bracket_begin)) {
                return nullptr;
            }
            if (is_type(eTokType::Const_int)) {
                // skip array size
                if (!next()) {
                    return nullptr;
                }
            }
            if (!expect(eOperator::bracket_end)) {
                return nullptr;
            }
            accept_paren = true;
        }
    }
    if (is_type(eTokType::Scope_Begin) || (accept_paren && is_operator(eOperator::parenthesis_begin))) {
        if (!next()) {
            return nullptr;
        }
        auto *arr_specifier = astnew<ast_array_specifier>(ast_->alloc.allocator);
        if (!arr_specifier) {
            return nullptr;
        }
        while (!is_type(eTokType::Scope_End) && !is_operator(eOperator::parenthesis_end)) {
            ast_expression *next_expression =
                ParseArraySpecifier(condition | eEndCondition::Parenthesis | eEndCondition::Comma);
            if (!next_expression) {
                return nullptr;
            }
            arr_specifier->expressions.push_back(next_expression);
            if (is_operator(eOperator::comma)) {
                if (!next()) {
                    return nullptr;
                }
            }
        }
        if (!is_type(eTokType::Scope_End) && !is_operator(eOperator::parenthesis_end)) {
            return nullptr;
        }
        if (!next()) {
            return nullptr;
        }
        return arr_specifier;
    }
    return ParseExpression(condition);
}

glslx::ast_statement *glslx::Parser::ParseStatement() {
    ctrl_flow_params_t ctrl_flow;
    if (is_operator(eOperator::bracket_begin)) {
        // Parse control flow attributes
        if (!next()) { // skip first '['
            return nullptr;
        }
        if (!expect(eOperator::bracket_begin)) { // skip second '['
            return nullptr;
        }
        while (!is_operator(eOperator::bracket_end)) {
            if (is_type(eTokType::Identifier)) {
                if (strcmp(tok_.as_identifier, "unroll") == 0) {
                    ctrl_flow.attributes |= eCtrlFlowAttribute::Unroll;
                } else if (strcmp(tok_.as_identifier, "dont_unroll") == 0) {
                    ctrl_flow.attributes |= eCtrlFlowAttribute::DontUnroll;
                } else if (strcmp(tok_.as_identifier, "loop") == 0) {
                    ctrl_flow.attributes |= eCtrlFlowAttribute::Loop;
                } else if (strcmp(tok_.as_identifier, "dependency_infinite") == 0) {
                    ctrl_flow.attributes |= eCtrlFlowAttribute::DependencyInfinite;
                } else if (strcmp(tok_.as_identifier, "dependency_length") == 0) {
                    ctrl_flow.attributes |= eCtrlFlowAttribute::DependencyLength;
                    if (!next()) {
                        fatal("premature end of file");
                        return nullptr;
                    }
                    if (!expect(eOperator::parenthesis_begin)) {
                        fatal("expected '('");
                        return nullptr;
                    }
                    if (!is_type(eTokType::Const_int)) {
                        fatal("expected const int");
                        return nullptr;
                    }
                    ctrl_flow.dependency_length = tok_.as_int;
                    if (!next()) {
                        fatal("premature end of file");
                        return nullptr;
                    }
                } else if (strcmp(tok_.as_identifier, "flatten") == 0) {
                    ctrl_flow.attributes |= eCtrlFlowAttribute::Flatten;
                } else if (strcmp(tok_.as_identifier, "dont_flatten") == 0) {
                    ctrl_flow.attributes |= eCtrlFlowAttribute::DontFlatten;
                } else if (strcmp(tok_.as_identifier, "branch") == 0) {
                    ctrl_flow.attributes |= eCtrlFlowAttribute::Branch;
                } else if (strcmp(tok_.as_identifier, "min_iterations") == 0) {
                    ctrl_flow.attributes |= eCtrlFlowAttribute::MinIterations;
                    if (!next()) {
                        fatal("premature end of file");
                        return nullptr;
                    }
                    if (!expect(eOperator::parenthesis_begin)) {
                        fatal("expected '('");
                        return nullptr;
                    }
                    if (!is_type(eTokType::Const_int)) {
                        fatal("expected const int");
                        return nullptr;
                    }
                    ctrl_flow.min_iterations = tok_.as_int;
                    if (!next()) {
                        fatal("premature end of file");
                        return nullptr;
                    }
                } else if (strcmp(tok_.as_identifier, "max_iterations") == 0) {
                    ctrl_flow.attributes |= eCtrlFlowAttribute::MaxIterations;
                    if (!next()) {
                        fatal("premature end of file");
                        return nullptr;
                    }
                    if (!expect(eOperator::parenthesis_begin)) {
                        fatal("expected '('");
                        return nullptr;
                    }
                    if (!is_type(eTokType::Const_int)) {
                        fatal("expected const int");
                        return nullptr;
                    }
                    ctrl_flow.max_iterations = tok_.as_int;
                    if (!next()) {
                        fatal("premature end of file");
                        return nullptr;
                    }
                } else if (strcmp(tok_.as_identifier, "iteration_multiple") == 0) {
                    ctrl_flow.attributes |= eCtrlFlowAttribute::IterationMultiple;
                    if (!next()) {
                        fatal("premature end of file");
                        return nullptr;
                    }
                    if (!expect(eOperator::parenthesis_begin)) {
                        fatal("expected '('");
                        return nullptr;
                    }
                    if (!is_type(eTokType::Const_int)) {
                        fatal("expected const int");
                        return nullptr;
                    }
                    ctrl_flow.iteration_multiple = tok_.as_int;
                    if (!next()) {
                        fatal("premature end of file");
                        return nullptr;
                    }
                } else if (strcmp(tok_.as_identifier, "peel_count") == 0) {
                    ctrl_flow.attributes |= eCtrlFlowAttribute::PeelCount;
                    if (!next()) {
                        fatal("premature end of file");
                        return nullptr;
                    }
                    if (!expect(eOperator::parenthesis_begin)) {
                        fatal("expected '('");
                        return nullptr;
                    }
                    if (!is_type(eTokType::Const_int)) {
                        fatal("expected const int");
                        return nullptr;
                    }
                    ctrl_flow.peel_count = tok_.as_int;
                    if (!next()) {
                        fatal("premature end of file");
                        return nullptr;
                    }
                } else if (strcmp(tok_.as_identifier, "partial_count") == 0) {
                    ctrl_flow.attributes |= eCtrlFlowAttribute::PartialCount;
                    if (!next()) {
                        fatal("premature end of file");
                        return nullptr;
                    }
                    if (!expect(eOperator::parenthesis_begin)) {
                        fatal("expected '('");
                        return nullptr;
                    }
                    if (!is_type(eTokType::Const_int)) {
                        fatal("expected const int");
                        return nullptr;
                    }
                    ctrl_flow.partial_count = tok_.as_int;
                    if (!next()) {
                        fatal("premature end of file");
                        return nullptr;
                    }
                }
                if (!next()) {
                    fatal("premature end of file");
                    return nullptr;
                }
                if (is_operator(eOperator::comma)) {
                    if (!next()) {
                        fatal("premature end of file");
                        return nullptr;
                    }
                }
            } else {
                fatal("invalid attribute");
                return nullptr;
            }
        }
        if (!expect(eOperator::bracket_end)) { // skip first ']'
            return nullptr;
        }
        if (!expect(eOperator::bracket_end)) { // skip second ']'
            return nullptr;
        }
    }

    if (is_type(eTokType::Scope_Begin)) {
        scopes_.emplace_back();
        ast_compound_statement *ret = ParseCompoundStatement();
        scopes_.pop_back();
        return ret;
    } else if (is_keyword(eKeyword::K_if)) {
        return ParseIfStatement(ctrl_flow.attributes);
    } else if (is_keyword(eKeyword::K_switch)) {
        return ParseSwitchStatement(ctrl_flow.attributes);
    } else if (is_keyword(eKeyword::K_case) || is_keyword(eKeyword::K_default)) {
        return ParseCaseLabelStatement();
    } else if (is_keyword(eKeyword::K_for)) {
        // TODO: This creates one more scope than needed
        scopes_.emplace_back();
        ast_for_statement *ret = ParseForStatement(ctrl_flow);
        scopes_.pop_back();
        return ret;
    } else if (is_keyword(eKeyword::K_do)) {
        return ParseDoStatement(ctrl_flow);
    } else if (is_keyword(eKeyword::K_while)) {
        return ParseWhileStatement(ctrl_flow);
    } else if (is_keyword(eKeyword::K_continue)) {
        return ParseContinueStatement();
    } else if (is_keyword(eKeyword::K_break)) {
        return ParseBreakStatement();
    } else if (is_keyword(eKeyword::K_discard)) {
        return ParseDiscardStatement();
    } else if (is_keyword(eKeyword::K_return)) {
        return ParseReturnStatement();
    } else if (is_keyword(eKeyword::K_ignoreIntersectionEXT) || is_keyword(eKeyword::K_terminateRayEXT)) {
        return ParseExtJumpStatement();
    } else if (is_type(eTokType::Semicolon)) {
        return astnew<ast_empty_statement>();
    } else {
        return ParseDeclarationOrExpressionStatement(eEndCondition::Semicolon);
    }
}

glslx::ast_compound_statement *glslx::Parser::ParseCompoundStatement() {
    auto *statement = astnew<ast_compound_statement>(ast_->alloc.allocator);
    if (!statement) {
        return nullptr;
    }
    if (!next()) { // skip '{'
        return nullptr;
    }
    while (!is_type(eTokType::Scope_End)) {
        ast_statement *next_statement = ParseStatement();
        if (!next_statement) {
            return nullptr;
        }
        statement->statements.push_back(next_statement);
        if (!next()) { // skip ';'
            return nullptr;
        }
    }
    return statement;
}

glslx::ast_if_statement *glslx::Parser::ParseIfStatement(const Bitmask<eCtrlFlowAttribute> attributes) {
    auto *statement = astnew<ast_if_statement>(attributes);
    if (!statement) {
        return nullptr;
    }
    if (!next()) { // skip 'if'
        return nullptr;
    }
    if (!is_operator(eOperator::parenthesis_begin)) {
        fatal("expected '(' after 'if'");
        return nullptr;
    }
    if (!next()) { // skip '('
        return nullptr;
    }
    if (!(statement->condition = ParseExpression(eEndCondition::Parenthesis))) {
        return nullptr;
    }
    if (!next()) { // skip ')'
        return nullptr;
    }
    statement->then_statement = ParseStatement();
    const token_t &peek = lexer_.Peek();
    if (peek.type == eTokType::Keyword && peek.as_keyword == eKeyword::K_else) {
        if (!next()) { // skip ';' or '}'
            return nullptr;
        }
        if (!next()) { // skip 'else'
            return nullptr;
        }
        if (!(statement->else_statement = ParseStatement())) {
            return nullptr;
        }
    }
    return statement;
}

glslx::ast_switch_statement *glslx::Parser::ParseSwitchStatement(const Bitmask<eCtrlFlowAttribute> attributes) {
    auto *statement = astnew<ast_switch_statement>(ast_->alloc.allocator, attributes);
    if (!statement) {
        return nullptr;
    }
    if (!next()) { // skip 'switch'
        return nullptr;
    }
    if (!is_operator(eOperator::parenthesis_begin)) {
        fatal("expected '(' after 'switch'");
        return nullptr;
    }
    if (!next()) { // skip '('
        return nullptr;
    }
    if (!(statement->expression = ParseExpression(eEndCondition::Parenthesis))) {
        return nullptr;
    }
    if (!next()) { // skip next
        return nullptr;
    }
    if (!is_type(eTokType::Scope_Begin)) {
        fatal("expected '{' after ')'");
        return nullptr;
    }
    if (!next()) { // skip '{'
        return nullptr;
    }

    SmallVector<int64_t, 16> seen_labels;
    bool had_default = false;
    while (!is_type(eTokType::Scope_End)) {
        ast_statement *next_statement = ParseStatement();
        if (!next_statement) {
            return nullptr;
        }
        if (next_statement->type == eStatement::CaseLabel) {
            auto *case_label = static_cast<ast_case_label_statement *>(next_statement);
            if (!case_label->is_default) {
                if (!IsConstant(case_label->condition)) {
                    fatal("case label is not a valid constant expression");
                    return nullptr;
                }
                ast_constant_expression *value = Evaluate(case_label->condition);

                long long label_value;
                // TODO: Add other explicit types (uint64_t etc.)
                if (value->type == eExprType::IntConstant) {
                    label_value = IVAL(value);
                } else if (value->type == eExprType::UIntConstant) {
                    label_value = UVAL(value);
                } else {
                    fatal("case label must be scalar 'int' or 'uint'");
                    return nullptr;
                }

                const auto it = std::lower_bound(std::begin(seen_labels), std::end(seen_labels), label_value);
                if (it != std::end(seen_labels) && label_value == (*it)) {
                    fatal("duplicate case label '%lld'", label_value);
                    return nullptr;
                }
                seen_labels.insert(it, label_value);
            } else {
                if (had_default) {
                    fatal("duplicate 'default' case label");
                    return nullptr;
                }
                had_default = true;
            }
        }
        statement->statements.push_back(next_statement);
        if (!next()) {
            return nullptr;
        }
    }

    return statement;
}

glslx::ast_case_label_statement *glslx::Parser::ParseCaseLabelStatement() {
    auto *statement = astnew<ast_case_label_statement>();
    if (!statement) {
        return nullptr;
    }
    if (is_keyword(eKeyword::K_default)) {
        statement->is_default = true;
        if (!next()) { // skip 'default'
            return nullptr;
        }
        if (!is_operator(eOperator::colon)) {
            fatal("expected ':' after 'default' in case label");
            return nullptr;
        }
    } else {
        if (!next()) { // skip 'case'
            return nullptr;
        }
        statement->condition = ParseExpression(eEndCondition::Colon);
    }
    return statement;
}

glslx::ast_simple_statement *
glslx::Parser::ParseDeclarationOrExpressionStatement(const Bitmask<eEndCondition> condition) {
    ast_simple_statement *declaration = ParseDeclarationStatement(condition);
    if (declaration) {
        return declaration;
    } else {
        return ParseExpressionStatement(condition);
    }
}

glslx::ast_declaration_statement *glslx::Parser::ParseDeclarationStatement(const Bitmask<eEndCondition> condition) {
    const location_t before = lexer_.location();

    bool is_const = false;
    if (is_keyword(eKeyword::K_const)) {
        is_const = true;
        if (!next()) { // skip 'const'
            return nullptr;
        }
    }

    ePrecision precision = ePrecision::None;
    if (!ParsePrecision(precision)) {
        return nullptr;
    }

    ast_type *type = nullptr;
    if (is_builtin()) {
        type = ParseBuiltin();
    } else if (is_type(eTokType::Identifier)) {
        type = FindType(tok_.as_identifier);
    }

    if (!type) {
        lexer_.set_location(before);
        return nullptr;
    }

    if (!next()) {
        return nullptr;
    }

    bool is_array = false;
    local_vector<ast_constant_expression *> array_sizes(ast_->alloc.allocator);
    while (is_operator(eOperator::bracket_begin)) {
        is_array = true;
        array_sizes.insert(begin(array_sizes), ParseArraySize());
        if (!next()) { // skip ']'
            return nullptr;
        }
    }

    auto *statement = astnew<ast_declaration_statement>(ast_->alloc.allocator);
    if (!statement) {
        return nullptr;
    }
    while (true) {
        int parenthesis_count = 0;
        while (is_operator(eOperator::parenthesis_begin)) {
            ++parenthesis_count;
            if (!next()) { // skip ','
                return nullptr;
            }
        }
        if (!is_type(eTokType::Identifier)) {
            lexer_.set_location(before);
            return nullptr;
        }

        const char *name = strnew(tok_.as_identifier);
        if (!next()) { // skip identifier
            return nullptr;
        }

        for (int i = 0; i < parenthesis_count; ++i) {
            if (!is_operator(eOperator::parenthesis_end)) {
                lexer_.set_location(before);
                return nullptr;
            }
            if (!next()) {
                return nullptr;
            }
        }

        auto *variable = astnew<ast_function_variable>(ast_->alloc.allocator);
        if (!variable) {
            return nullptr;
        }
        variable->is_array = is_array;
        variable->array_sizes = array_sizes;

        if (is_operator(eOperator::bracket_begin)) {
            while (is_operator(eOperator::bracket_begin)) {
                variable->is_array = true;
                ast_constant_expression *array_size = ParseArraySize();
                // if (!array_size) {
                //     return nullptr;
                // }
                variable->array_sizes.push_back(array_size);
                if (!next()) { // skip ']'
                    return nullptr;
                }
            }
        }

        if (statement->variables.empty() && !is_operator(eOperator::assign) && !is_operator(eOperator::comma) &&
            !is_end_condition(condition)) {
            lexer_.set_location(before);
            return nullptr;
        }

        if (is_operator(eOperator::assign)) {
            if (!next()) { // skip '='
                return nullptr;
            }
            if (variable->is_array || is_vector_type(type)) {
                if (!(variable->initial_value = ParseArraySpecifier(condition | eEndCondition::Comma))) {
                    return nullptr;
                }
            } else {
                if (!(variable->initial_value = ParseExpression(condition | eEndCondition::Comma))) {
                    return nullptr;
                }
            }
        }

        variable->is_const = is_const;
        variable->precision = precision;
        variable->base_type = type;
        variable->name = strnew(name);
        statement->variables.push_back(variable);
        scopes_.back().push_back(variable);

        if (is_end_condition(condition)) {
            break;
        } else if (is_operator(eOperator::comma)) {
            if (!next()) { // skip ','
                return nullptr;
            }
        } else {
            fatal("syntax error during declaration statement");
            return nullptr;
        }
    }

    return statement;
}

glslx::ast_expression_statement *glslx::Parser::ParseExpressionStatement(const Bitmask<eEndCondition> condition) {
    ast_expression *expression = ParseExpression(condition);
    if (!expression) {
        return nullptr;
    }
    return astnew<ast_expression_statement>(expression);
}

glslx::ast_continue_statement *glslx::Parser::ParseContinueStatement() {
    auto *statement = astnew<ast_continue_statement>();
    if (!next()) { // skip 'continue'
        return nullptr;
    }
    return statement;
}

glslx::ast_break_statement *glslx::Parser::ParseBreakStatement() {
    auto *statement = astnew<ast_break_statement>();
    if (!statement) {
        return nullptr;
    }
    if (!next()) { // skip 'break'
        return nullptr;
    }
    if (!is_type(eTokType::Semicolon)) {
        fatal("expected semicolon after break statement");
        return nullptr;
    }
    return statement;
}

glslx::ast_discard_statement *glslx::Parser::ParseDiscardStatement() {
    auto *statement = astnew<ast_discard_statement>();
    if (!statement) {
        return nullptr;
    }
    if (!next()) { // skip 'discard'
        return nullptr;
    }
    if (!is_type(eTokType::Semicolon)) {
        fatal("expected semicolon after discard statement");
        return nullptr;
    }
    return statement;
}

glslx::ast_return_statement *glslx::Parser::ParseReturnStatement() {
    auto *statement = astnew<ast_return_statement>();
    if (!statement) {
        return nullptr;
    }
    if (!next()) { // skip 'return'
        return nullptr;
    }
    if (!is_type(eTokType::Semicolon)) {
        if (!(statement->expression = ParseExpression(eEndCondition::Semicolon))) {
            return nullptr;
        }
        if (!is_type(eTokType::Semicolon)) {
            fatal("expected semicolon after return statement");
            return nullptr;
        }
    }
    return statement;
}

glslx::ast_ext_jump_statement *glslx::Parser::ParseExtJumpStatement() {
    auto *statement = astnew<ast_ext_jump_statement>(tok_.as_keyword);
    if (!statement) {
        return nullptr;
    }
    if (!next()) {
        return nullptr;
    }
    if (!is_type(eTokType::Semicolon)) {
        fatal("expected semicolon after discard statement");
        return nullptr;
    }
    return statement;
}

glslx::ast_for_statement *glslx::Parser::ParseForStatement(const ctrl_flow_params_t &ctrl_flow) {
    auto *statement = astnew<ast_for_statement>(ctrl_flow);
    if (!statement) {
        return nullptr;
    }
    if (!next()) { // skip 'for'
        return nullptr;
    }
    if (!is_operator(eOperator::parenthesis_begin)) {
        fatal("expected '(' after 'for'");
        return nullptr;
    }
    if (!next()) { // skip '('
        return nullptr;
    }
    if (!is_type(eTokType::Semicolon)) {
        if (!(statement->init = ParseDeclarationOrExpressionStatement(eEndCondition::Semicolon))) {
            return nullptr;
        }
    }
    if (!next()) { // skip ';'
        return nullptr;
    }
    if (!is_type(eTokType::Semicolon)) {
        if (!(statement->condition = ParseExpression(eEndCondition::Semicolon))) {
            return nullptr;
        }
    }
    if (!next()) { // skip ';'
        return nullptr;
    }
    if (!is_operator(eOperator::parenthesis_end)) {
        if (!(statement->loop = ParseExpression(eEndCondition::Parenthesis))) {
            return nullptr;
        }
    }
    if (!next()) { // skip ')'
        return nullptr;
    }
    statement->body = ParseStatement();
    return statement;
}

glslx::ast_do_statement *glslx::Parser::ParseDoStatement(const ctrl_flow_params_t &ctrl_flow) {
    auto *statement = astnew<ast_do_statement>(ctrl_flow);
    if (!statement) {
        return nullptr;
    }
    if (!next()) { // skip 'do'
        return nullptr;
    }
    if (!(statement->body = ParseStatement())) {
        return nullptr;
    }
    if (!next()) {
        return nullptr;
    }
    if (!is_keyword(eKeyword::K_while)) {
        fatal("expected 'while' after 'do'");
        return nullptr;
    }
    if (!next()) { // skip 'while'
        return nullptr;
    }
    if (!is_operator(eOperator::parenthesis_begin)) {
        fatal("expected '(' after 'while'");
        return nullptr;
    }
    if (!next()) { // skip '('
        return nullptr;
    }
    if (!(statement->condition = ParseExpression(eEndCondition::Parenthesis))) {
        return nullptr;
    }
    if (!next()) {
        return nullptr;
    }
    return statement;
}

glslx::ast_while_statement *glslx::Parser::ParseWhileStatement(const ctrl_flow_params_t &ctrl_flow) {
    auto *statement = astnew<ast_while_statement>(ctrl_flow);
    if (!statement) {
        return nullptr;
    }
    if (!next()) { // skip 'while'
        return nullptr;
    }
    if (!is_operator(eOperator::parenthesis_begin)) {
        fatal("expected '(' after 'while'");
        return nullptr;
    }
    if (!next()) { // skip '('
        return nullptr;
    }
    if (!(statement->condition = ParseDeclarationOrExpressionStatement(eEndCondition::Parenthesis))) {
        return nullptr;
    }
    if (!next()) {
        return nullptr;
    }
    if (!(statement->body = ParseStatement())) {
        return nullptr;
    }
    return statement;
}

glslx::ast_binary_expression *glslx::Parser::CreateExpression() {
    if (!is_type(eTokType::Operator)) {
        return nullptr;
    }

    switch (tok_.as_operator) {
    case eOperator::multiply:
    case eOperator::divide:
    case eOperator::modulus:
    case eOperator::plus:
    case eOperator::minus:
    case eOperator::shift_left:
    case eOperator::shift_right:
    case eOperator::less:
    case eOperator::greater:
    case eOperator::less_equal:
    case eOperator::greater_equal:
    case eOperator::equal:
    case eOperator::not_equal:
    case eOperator::bit_and:
    case eOperator::bit_xor:
    case eOperator::bit_or:
    case eOperator::logical_and:
    case eOperator::logical_xor:
    case eOperator::logical_or:
        return astnew<ast_operation_expression>(tok_.as_operator);
    case eOperator::assign:
    case eOperator::add_assign:
    case eOperator::sub_assign:
    case eOperator::multiply_assign:
    case eOperator::divide_assign:
    case eOperator::modulus_assign:
    case eOperator::shift_left_assign:
    case eOperator::shift_right_assign:
    case eOperator::bit_and_assign:
    case eOperator::bit_xor_assign:
    case eOperator::bit_or_assign:
        return astnew<ast_assignment_expression>(tok_.as_operator);
    case eOperator::comma:
        return astnew<ast_sequence_expression>();
    default:
        return nullptr;
    }
}

glslx::ast_type *glslx::Parser::FindType(const char *name) {
    ast_struct **ret = ast_->structures_by_name.Find(name);
    if (ret) {
        return *ret;
    }
    return nullptr;
}

glslx::ast_variable *glslx::Parser::FindVariable(const char *name) {
    for (int scope_index = int(scopes_.size()) - 1; scope_index >= 0; --scope_index) {
        const scope &s = scopes_[scope_index];
        for (int var_index = 0; var_index < int(s.size()); ++var_index) {
            if (strcmp(s[var_index]->name, name) == 0) {
                return s[var_index];
            }
        }
    }
    return nullptr;
}

glslx::ast_type *glslx::Parser::GetType(ast_expression *expression) {
    switch (expression->type) {
    case eExprType::VariableIdentifier:
        return ((ast_variable_identifier *)expression)->variable->base_type;
    case eExprType::FieldOrSwizzle:
        return GetType(((ast_field_or_swizzle *)expression)->field);
    case eExprType::ArraySubscript:
        return GetType(((ast_array_subscript *)expression)->operand);
    case eExprType::FunctionCall: {
        auto *p_find = ast_->functions_by_name.Find(static_cast<ast_function_call *>(expression)->name);
        if (p_find) {
            return (*p_find)[0]->return_type;
        }
        break;
    }
    case eExprType::ConstructorCall:
        return static_cast<ast_constructor_call *>(expression)->type;
    default:
        return nullptr;
    }
    return nullptr;
}

glslx::ast_global_variable *glslx::Parser::AddHiddenGlobal(ast_builtin *type, const char *name, const bool is_array,
                                                           const eStorage storage, const ePrecision precision) {
    auto *var = astnew<ast_global_variable>(ast_->alloc.allocator);
    if (!var) {
        return nullptr;
    }
    var->storage = storage;
    var->is_array = is_array;
    var->is_hidden = true;
    var->base_type = type;
    var->precision = precision;
    var->name = strnew(name);
    ast_->globals.push_back(var);
    scopes_.back().push_back(var);
    return var;
}

bool glslx::Parser::InitSpecialGlobals(const eTrUnitType type) {
    auto *bool_type = astnew<ast_builtin>(eKeyword::K_bool);
    auto *float_type = astnew<ast_builtin>(eKeyword::K_float);
    auto *vec2 = astnew<ast_builtin>(eKeyword::K_vec2);
    auto *vec3 = astnew<ast_builtin>(eKeyword::K_vec3);
    auto *vec4 = astnew<ast_builtin>(eKeyword::K_vec4);
    auto *int_type = astnew<ast_builtin>(eKeyword::K_int);
    auto *uint = astnew<ast_builtin>(eKeyword::K_uint);
    auto *uvec3 = astnew<ast_builtin>(eKeyword::K_uvec3);
    auto *uvec4 = astnew<ast_builtin>(eKeyword::K_uvec4);
    auto *mat3x4 = astnew<ast_builtin>(eKeyword::K_mat3x4);
    auto *mat4x3 = astnew<ast_builtin>(eKeyword::K_mat4x3);
    auto *mat4x4 = astnew<ast_builtin>(eKeyword::K_mat4x4);
    if (!bool_type || !float_type || !vec2 || !vec3 || !vec4 || !int_type || !uint || !uvec3 || !uvec4 || !mat3x4 ||
        !mat4x3 || !mat4x4) {
        return false;
    }
    builtins_.push_back(bool_type);
    builtins_.push_back(float_type);
    builtins_.push_back(vec3);
    builtins_.push_back(vec4);
    builtins_.push_back(int_type);
    builtins_.push_back(uint);
    builtins_.push_back(uvec3);
    builtins_.push_back(uvec4);
    builtins_.push_back(mat3x4);
    builtins_.push_back(mat4x3);
    builtins_.push_back(mat4x4);

    bool res = true;

    if (type == eTrUnitType::Compute) {
        res &= AddHiddenGlobal(uvec3, "gl_NumWorkGroups") != nullptr;
        res &= AddHiddenGlobal(uvec3, "gl_WorkGroupSize") != nullptr;
        res &= AddHiddenGlobal(uvec3, "gl_WorkGroupID") != nullptr;
        res &= AddHiddenGlobal(uvec3, "gl_LocalInvocationID") != nullptr;
        res &= AddHiddenGlobal(uvec3, "gl_GlobalInvocationID") != nullptr;
        res &= AddHiddenGlobal(uint, "gl_LocalInvocationIndex") != nullptr;
        res &= AddHiddenGlobal(uint, "gl_NumSubgroups") != nullptr;
        res &= AddHiddenGlobal(uint, "gl_SubgroupID") != nullptr;
    } else if (type == eTrUnitType::Vertex) {
        res &= AddHiddenGlobal(int_type, "gl_VertexID") != nullptr;
        res &= AddHiddenGlobal(int_type, "gl_InstanceID") != nullptr;
        res &= AddHiddenGlobal(int_type, "gl_VertexIndex") != nullptr;
        res &= AddHiddenGlobal(int_type, "gl_InstanceIndex") != nullptr;
        res &= AddHiddenGlobal(int_type, "gl_DrawID") != nullptr;
        res &= AddHiddenGlobal(int_type, "gl_BaseVertex") != nullptr;
        res &= AddHiddenGlobal(int_type, "gl_BaseInstance") != nullptr;
        res &= AddHiddenGlobal(vec4, "gl_Position", false, eStorage::Out) != nullptr;
        res &= AddHiddenGlobal(float_type, "gl_PointSize", false, eStorage::Out) != nullptr;
        res &= AddHiddenGlobal(float_type, "gl_ClipDistance", true, eStorage::Out) != nullptr;
        res &= AddHiddenGlobal(float_type, "gl_CullDistance", true, eStorage::Out) != nullptr;
    } else if (type == eTrUnitType::TessControl) {
        res &= AddHiddenGlobal(int_type, "gl_PatchVerticesIn") != nullptr;
        res &= AddHiddenGlobal(int_type, "gl_PrimitiveID") != nullptr;
        res &= AddHiddenGlobal(int_type, "gl_InvocationID") != nullptr;
        res &= AddHiddenGlobal(float_type, "gl_TessLevelOuter", true, eStorage::Out) != nullptr;
        res &= AddHiddenGlobal(float_type, "gl_TessLevelInner", true, eStorage::Out) != nullptr;
        // TODO: add the rest
    } else if (type == eTrUnitType::TessEvaluation) {
        res &= AddHiddenGlobal(int_type, "gl_PatchVerticesIn") != nullptr;
        res &= AddHiddenGlobal(int_type, "gl_PrimitiveID") != nullptr;
        res &= AddHiddenGlobal(vec3, "gl_TessCoord") != nullptr;
        res &= AddHiddenGlobal(float_type, "gl_TessLevelOuter", true) != nullptr;
        res &= AddHiddenGlobal(float_type, "gl_TessLevelInner", true) != nullptr;
        res &= AddHiddenGlobal(vec4, "gl_Position", false, eStorage::Out) != nullptr;
        res &= AddHiddenGlobal(float_type, "gl_PointSize", false, eStorage::Out) != nullptr;
        res &= AddHiddenGlobal(float_type, "gl_ClipDistance", true, eStorage::Out) != nullptr;
        res &= AddHiddenGlobal(float_type, "gl_CullDistance", true, eStorage::Out) != nullptr;
        // TODO: add the rest
    } else if (type == eTrUnitType::Geometry) {
        res &= AddHiddenGlobal(int_type, "gl_PrimitiveIDIn") != nullptr;
        res &= AddHiddenGlobal(int_type, "gl_InvocationID") != nullptr;
        res &= AddHiddenGlobal(int_type, "gl_PrimitiveID") != nullptr;
        res &= AddHiddenGlobal(int_type, "gl_Layer") != nullptr;
        res &= AddHiddenGlobal(int_type, "gl_ViewportIndex") != nullptr;
        res &= AddHiddenGlobal(vec4, "gl_Position", false, eStorage::Out) != nullptr;
        res &= AddHiddenGlobal(float_type, "gl_PointSize", false, eStorage::Out) != nullptr;
        res &= AddHiddenGlobal(float_type, "gl_ClipDistance", true, eStorage::Out) != nullptr;
        res &= AddHiddenGlobal(float_type, "gl_CullDistance", true, eStorage::Out) != nullptr;
        // TODO: add the rest
    } else if (type == eTrUnitType::Fragment) {
        res &= AddHiddenGlobal(vec4, "gl_FragCoord") != nullptr;
        res &= AddHiddenGlobal(bool_type, "gl_FrontFacing") != nullptr;
        res &= AddHiddenGlobal(float_type, "gl_ClipDistance", true) != nullptr;
        res &= AddHiddenGlobal(float_type, "gl_CullDistance", true) != nullptr;
        res &= AddHiddenGlobal(vec2, "gl_PointCoord") != nullptr;
        res &= AddHiddenGlobal(int_type, "gl_PrimitiveID") != nullptr;
        res &= AddHiddenGlobal(int_type, "gl_SampleID") != nullptr;
        res &= AddHiddenGlobal(vec2, "gl_SamplePosition") != nullptr;
        res &= AddHiddenGlobal(int_type, "gl_SampleMaskIn", true) != nullptr;
        res &= AddHiddenGlobal(int_type, "gl_Layer") != nullptr;
        res &= AddHiddenGlobal(int_type, "gl_ViewportIndex") != nullptr;
        res &= AddHiddenGlobal(bool_type, "gl_HelperInvocation") != nullptr;
        res &= AddHiddenGlobal(float_type, "gl_FragDepth", false, eStorage::Out) != nullptr;
    }

    // https://github.com/KhronosGroup/GLSL/blob/master/extensions/khr/GL_KHR_shader_subgroup.txt
    if (type == eTrUnitType::Compute || type == eTrUnitType::Vertex || type == eTrUnitType::Geometry ||
        type == eTrUnitType::TessControl || type == eTrUnitType::TessEvaluation || type == eTrUnitType::Fragment) {
        res &= AddHiddenGlobal(uint, "gl_SubgroupSize", false, eStorage::In, ePrecision::Mediump) != nullptr;
        res &= AddHiddenGlobal(uint, "gl_SubgroupInvocationID", false, eStorage::In, ePrecision::Mediump) != nullptr;
        res &= AddHiddenGlobal(uvec4, "gl_SubgroupEqMask", false, eStorage::In, ePrecision::Highp) != nullptr;
        res &= AddHiddenGlobal(uvec4, "gl_SubgroupGeMask", false, eStorage::In, ePrecision::Highp) != nullptr;
        res &= AddHiddenGlobal(uvec4, "gl_SubgroupGtMask", false, eStorage::In, ePrecision::Highp) != nullptr;
        res &= AddHiddenGlobal(uvec4, "gl_SubgroupLeMask", false, eStorage::In, ePrecision::Highp) != nullptr;
        res &= AddHiddenGlobal(uvec4, "gl_SubgroupLtMask", false, eStorage::In, ePrecision::Highp) != nullptr;
    }

    // https://github.com/KhronosGroup/GLSL/blob/master/extensions/ext/GLSL_EXT_ray_query.txt
    ast_global_variable *gl_RayFlagsNoneEXT = AddHiddenGlobal(uint, "gl_RayFlagsNoneEXT");
    gl_RayFlagsNoneEXT->initial_value = astnew<ast_uint_constant>(0);
    ast_global_variable *gl_RayFlagsOpaqueEXT = AddHiddenGlobal(uint, "gl_RayFlagsOpaqueEXT");
    gl_RayFlagsOpaqueEXT->initial_value = astnew<ast_uint_constant>(1);
    ast_global_variable *gl_RayFlagsNoOpaqueEXT = AddHiddenGlobal(uint, "gl_RayFlagsNoOpaqueEXT");
    gl_RayFlagsNoOpaqueEXT->initial_value = astnew<ast_uint_constant>(2);
    ast_global_variable *gl_RayFlagsTerminateOnFirstHitEXT = AddHiddenGlobal(uint, "gl_RayFlagsTerminateOnFirstHitEXT");
    gl_RayFlagsTerminateOnFirstHitEXT->initial_value = astnew<ast_uint_constant>(4);
    ast_global_variable *gl_RayFlagsSkipClosestHitShaderEXT =
        AddHiddenGlobal(uint, "gl_RayFlagsSkipClosestHitShaderEXT");
    gl_RayFlagsSkipClosestHitShaderEXT->initial_value = astnew<ast_uint_constant>(8);
    ast_global_variable *gl_RayFlagsCullBackFacingTrianglesEXT =
        AddHiddenGlobal(uint, "gl_RayFlagsCullBackFacingTrianglesEXT");
    gl_RayFlagsCullBackFacingTrianglesEXT->initial_value = astnew<ast_uint_constant>(16);
    ast_global_variable *gl_RayFlagsCullFrontFacingTrianglesEXT =
        AddHiddenGlobal(uint, "gl_RayFlagsCullFrontFacingTrianglesEXT");
    gl_RayFlagsCullFrontFacingTrianglesEXT->initial_value = astnew<ast_uint_constant>(32);
    ast_global_variable *gl_RayFlagsCullOpaqueEXT = AddHiddenGlobal(uint, "gl_RayFlagsCullOpaqueEXT");
    gl_RayFlagsCullOpaqueEXT->initial_value = astnew<ast_uint_constant>(64);
    ast_global_variable *gl_RayFlagsCullNoOpaqueEXT = AddHiddenGlobal(uint, "gl_RayFlagsCullNoOpaqueEXT");
    gl_RayFlagsCullNoOpaqueEXT->initial_value = astnew<ast_uint_constant>(128);

    ast_global_variable *gl_HitKindFrontFacingTriangleEXT = AddHiddenGlobal(uint, "gl_HitKindFrontFacingTriangleEXT");
    gl_HitKindFrontFacingTriangleEXT->initial_value = astnew<ast_uint_constant>(0xFEU);
    ast_global_variable *gl_HitKindBackFacingTriangleEXT = AddHiddenGlobal(uint, "gl_HitKindBackFacingTriangleEXT");
    gl_HitKindBackFacingTriangleEXT->initial_value = astnew<ast_uint_constant>(0xFFU);

    ast_global_variable *gl_RayQueryCommittedIntersectionNoneEXT =
        AddHiddenGlobal(uint, "gl_RayQueryCommittedIntersectionNoneEXT");
    gl_RayQueryCommittedIntersectionNoneEXT->initial_value = astnew<ast_uint_constant>(0);
    ast_global_variable *gl_RayQueryCommittedIntersectionTriangleEXT =
        AddHiddenGlobal(uint, "gl_RayQueryCommittedIntersectionTriangleEXT");
    gl_RayQueryCommittedIntersectionTriangleEXT->initial_value = astnew<ast_uint_constant>(1);
    ast_global_variable *gl_RayQueryCommittedIntersectionGeneratedEXT =
        AddHiddenGlobal(uint, "gl_RayQueryCommittedIntersectionGeneratedEXT");
    gl_RayQueryCommittedIntersectionGeneratedEXT->initial_value = astnew<ast_uint_constant>(2);

    ast_global_variable *gl_RayQueryCandidateIntersectionTriangleEXT =
        AddHiddenGlobal(uint, "gl_RayQueryCandidateIntersectionTriangleEXT");
    gl_RayQueryCandidateIntersectionTriangleEXT->initial_value = astnew<ast_uint_constant>(0);
    ast_global_variable *gl_RayQueryCandidateIntersectionAABBEXT =
        AddHiddenGlobal(uint, "gl_RayQueryCandidateIntersectionAABBEXT");
    gl_RayQueryCandidateIntersectionAABBEXT->initial_value = astnew<ast_uint_constant>(1);

    // https://github.com/KhronosGroup/GLSL/blob/master/extensions/ext/GLSL_EXT_ray_tracing.txt
    if (type == eTrUnitType::RayGen) {
        res &= AddHiddenGlobal(uvec3, "gl_LaunchIDEXT") != nullptr;
        res &= AddHiddenGlobal(uvec3, "gl_LaunchSizeEXT") != nullptr;
    } else if (type == eTrUnitType::AnyHit || type == eTrUnitType::ClosestHit) {
        res &= AddHiddenGlobal(uvec3, "gl_LaunchIDEXT") != nullptr;
        res &= AddHiddenGlobal(uvec3, "gl_LaunchSizeEXT") != nullptr;

        res &= AddHiddenGlobal(int_type, "gl_PrimitiveID") != nullptr;
        res &= AddHiddenGlobal(int_type, "gl_InstanceID") != nullptr;
        res &= AddHiddenGlobal(int_type, "gl_InstanceCustomIndexEXT") != nullptr;
        res &= AddHiddenGlobal(int_type, "gl_GeometryIndexEXT") != nullptr;

        res &= AddHiddenGlobal(vec3, "gl_WorldRayOriginEXT") != nullptr;
        res &= AddHiddenGlobal(vec3, "gl_WorldRayDirectionEXT") != nullptr;
        res &= AddHiddenGlobal(vec3, "gl_ObjectRayOriginEXT") != nullptr;
        res &= AddHiddenGlobal(vec3, "gl_ObjectRayDirectionEXT") != nullptr;

        res &= AddHiddenGlobal(float_type, "gl_RayTminEXT") != nullptr;
        res &= AddHiddenGlobal(float_type, "gl_RayTmaxEXT") != nullptr;
        res &= AddHiddenGlobal(uint, "gl_IncomingRayFlagsEXT") != nullptr;

        res &= AddHiddenGlobal(float_type, "gl_HitTEXT") != nullptr;
        res &= AddHiddenGlobal(uint, "gl_HitKindEXT") != nullptr;

        res &= AddHiddenGlobal(mat4x3, "gl_ObjectToWorldEXT") != nullptr;
        res &= AddHiddenGlobal(mat3x4, "gl_ObjectToWorld3x4EXT") != nullptr;
        res &= AddHiddenGlobal(mat4x3, "gl_WorldToObjectEXT") != nullptr;
        res &= AddHiddenGlobal(mat3x4, "gl_WorldToObject3x4EXT") != nullptr;
    } else if (type == eTrUnitType::Intersect) {
        res &= AddHiddenGlobal(uvec3, "gl_LaunchIDEXT") != nullptr;
        res &= AddHiddenGlobal(uvec3, "gl_LaunchSizeEXT") != nullptr;

        res &= AddHiddenGlobal(int_type, "gl_PrimitiveID") != nullptr;
        res &= AddHiddenGlobal(int_type, "gl_InstanceID") != nullptr;
        res &= AddHiddenGlobal(int_type, "gl_InstanceCustomIndexEXT") != nullptr;
        res &= AddHiddenGlobal(int_type, "gl_GeometryIndexEXT") != nullptr;

        res &= AddHiddenGlobal(vec3, "gl_WorldRayOriginEXT") != nullptr;
        res &= AddHiddenGlobal(vec3, "gl_WorldRayDirectionEXT") != nullptr;
        res &= AddHiddenGlobal(vec3, "gl_ObjectRayOriginEXT") != nullptr;
        res &= AddHiddenGlobal(vec3, "gl_ObjectRayDirectionEXT") != nullptr;

        res &= AddHiddenGlobal(float_type, "gl_RayTminEXT") != nullptr;
        res &= AddHiddenGlobal(float_type, "gl_RayTmaxEXT") != nullptr;
        res &= AddHiddenGlobal(uint, "gl_IncomingRayFlagsEXT") != nullptr;

        res &= AddHiddenGlobal(mat4x3, "gl_ObjectToWorldEXT") != nullptr;
        res &= AddHiddenGlobal(mat3x4, "gl_ObjectToWorld3x4EXT") != nullptr;
        res &= AddHiddenGlobal(mat4x3, "gl_WorldToObjectEXT") != nullptr;
        res &= AddHiddenGlobal(mat3x4, "gl_WorldToObject3x4EXT") != nullptr;
    } else if (type == eTrUnitType::Miss) {
        res &= AddHiddenGlobal(uvec3, "gl_LaunchIDEXT") != nullptr;
        res &= AddHiddenGlobal(uvec3, "gl_LaunchSizeEXT") != nullptr;

        res &= AddHiddenGlobal(vec3, "gl_WorldRayOriginEXT") != nullptr;
        res &= AddHiddenGlobal(vec3, "gl_WorldRayDirectionEXT") != nullptr;

        res &= AddHiddenGlobal(float_type, "gl_RayTminEXT") != nullptr;
        res &= AddHiddenGlobal(float_type, "gl_RayTmaxEXT") != nullptr;
        res &= AddHiddenGlobal(uint, "gl_IncomingRayFlagsEXT") != nullptr;
    } else if (type == eTrUnitType::Callable) {
        res &= AddHiddenGlobal(uvec3, "gl_LaunchIDEXT") != nullptr;
        res &= AddHiddenGlobal(uvec3, "gl_LaunchSizeEXT") != nullptr;
    }

    return res;
}

namespace glslx {
const ast_builtin g_short_type(eKeyword::K_int16_t);
const ast_builtin g_ushort_type(eKeyword::K_uint16_t);
const ast_builtin g_int_type(eKeyword::K_int);
const ast_builtin g_uint_type(eKeyword::K_uint);
const ast_builtin g_long_type(eKeyword::K_int64_t);
const ast_builtin g_ulong_type(eKeyword::K_uint64_t);
const ast_builtin g_half_type(eKeyword::K_float16_t);
const ast_builtin g_float_type(eKeyword::K_float);
const ast_builtin g_double_type(eKeyword::K_double);
const ast_builtin g_bool_type(eKeyword::K_bool);

const ast_builtin g_i16vec2_type(eKeyword::K_i16vec2);
const ast_builtin g_u16vec2_type(eKeyword::K_u16vec2);
const ast_builtin g_ivec2_type(eKeyword::K_ivec2);
const ast_builtin g_uvec2_type(eKeyword::K_uvec2);
const ast_builtin g_i64vec2_type(eKeyword::K_i64vec2);
const ast_builtin g_u64vec2_type(eKeyword::K_u64vec2);
const ast_builtin g_f16vec2_type(eKeyword::K_f16vec2);
const ast_builtin g_vec2_type(eKeyword::K_vec2);
const ast_builtin g_dvec2_type(eKeyword::K_dvec2);
const ast_builtin g_bvec2_type(eKeyword::K_bvec2);

const ast_builtin g_i16vec3_type(eKeyword::K_i16vec3);
const ast_builtin g_u16vec3_type(eKeyword::K_u16vec3);
const ast_builtin g_ivec3_type(eKeyword::K_ivec3);
const ast_builtin g_uvec3_type(eKeyword::K_uvec3);
const ast_builtin g_i64vec3_type(eKeyword::K_i64vec3);
const ast_builtin g_u64vec3_type(eKeyword::K_u64vec3);
const ast_builtin g_f16vec3_type(eKeyword::K_f16vec3);
const ast_builtin g_vec3_type(eKeyword::K_vec3);
const ast_builtin g_dvec3_type(eKeyword::K_dvec3);
const ast_builtin g_bvec3_type(eKeyword::K_bvec3);

const ast_builtin g_i16vec4_type(eKeyword::K_i16vec4);
const ast_builtin g_u16vec4_type(eKeyword::K_u16vec4);
const ast_builtin g_ivec4_type(eKeyword::K_ivec4);
const ast_builtin g_uvec4_type(eKeyword::K_uvec4);
const ast_builtin g_i64vec4_type(eKeyword::K_i64vec4);
const ast_builtin g_u64vec4_type(eKeyword::K_u64vec4);
const ast_builtin g_f16vec4_type(eKeyword::K_f16vec4);
const ast_builtin g_vec4_type(eKeyword::K_vec4);
const ast_builtin g_dvec4_type(eKeyword::K_dvec4);
const ast_builtin g_bvec4_type(eKeyword::K_bvec4);

const eKeyword g_convertible_types[][3] = {{eKeyword::K_int16_t, eKeyword::K_int, eKeyword::K_int64_t},
                                           {eKeyword::K_i16vec2, eKeyword::K_ivec2, eKeyword::K_i64vec2},
                                           {eKeyword::K_i16vec3, eKeyword::K_ivec3, eKeyword::K_i64vec3},
                                           {eKeyword::K_i16vec4, eKeyword::K_ivec4, eKeyword::K_i64vec4},

                                           {eKeyword::K_int, eKeyword::K_int64_t},
                                           {eKeyword::K_ivec2, eKeyword::K_i64vec2},
                                           {eKeyword::K_ivec3, eKeyword::K_i64vec3},
                                           {eKeyword::K_ivec4, eKeyword::K_i64vec4},

                                           {eKeyword::K_uint16_t, eKeyword::K_uint, eKeyword::K_uint64_t},
                                           {eKeyword::K_u16vec2, eKeyword::K_uvec2, eKeyword::K_u64vec2},
                                           {eKeyword::K_u16vec3, eKeyword::K_uvec3, eKeyword::K_u64vec3},
                                           {eKeyword::K_u16vec4, eKeyword::K_uvec4, eKeyword::K_u64vec4},

                                           {eKeyword::K_uint, eKeyword::K_uint64_t},
                                           {eKeyword::K_uvec2, eKeyword::K_u64vec2},
                                           {eKeyword::K_uvec3, eKeyword::K_u64vec3},
                                           {eKeyword::K_uvec4, eKeyword::K_u64vec4},

                                           {eKeyword::K_float16_t, eKeyword::K_float, eKeyword::K_double},
                                           {eKeyword::K_f16vec2, eKeyword::K_vec2, eKeyword::K_dvec2},
                                           {eKeyword::K_f16vec3, eKeyword::K_vec3, eKeyword::K_dvec3},
                                           {eKeyword::K_f16vec4, eKeyword::K_vec4, eKeyword::K_dvec4},

                                           {eKeyword::K_float, eKeyword::K_double},
                                           {eKeyword::K_vec2, eKeyword::K_dvec2},
                                           {eKeyword::K_vec3, eKeyword::K_dvec3},
                                           {eKeyword::K_vec4, eKeyword::K_dvec4}};

extern const char *g_atomic_functions[];
extern const int g_atomic_functions_count;

int get_vector_size(const eKeyword type) {
    switch (type) {
    case eKeyword::K_i16vec4:
    case eKeyword::K_u16vec4:
    case eKeyword::K_ivec4:
    case eKeyword::K_uvec4:
    case eKeyword::K_i64vec4:
    case eKeyword::K_u64vec4:
    case eKeyword::K_f16vec4:
    case eKeyword::K_vec4:
    case eKeyword::K_dvec4:
    case eKeyword::K_bvec4:
        return 4;
    case eKeyword::K_i16vec3:
    case eKeyword::K_u16vec3:
    case eKeyword::K_ivec3:
    case eKeyword::K_uvec3:
    case eKeyword::K_i64vec3:
    case eKeyword::K_u64vec3:
    case eKeyword::K_f16vec3:
    case eKeyword::K_vec3:
    case eKeyword::K_dvec3:
    case eKeyword::K_bvec3:
        return 3;
    case eKeyword::K_i16vec2:
    case eKeyword::K_u16vec2:
    case eKeyword::K_ivec2:
    case eKeyword::K_uvec2:
    case eKeyword::K_i64vec2:
    case eKeyword::K_u64vec2:
    case eKeyword::K_f16vec2:
    case eKeyword::K_vec2:
    case eKeyword::K_dvec2:
    case eKeyword::K_bvec2:
        return 2;
    default:
        return 1;
    }
}

int get_vector_size(const ast_type *_type) {
    if (!_type->builtin) {
        return -1;
    }
    return get_vector_size(static_cast<const ast_builtin *>(_type)->type);
}

const ast_type *to_scalar_type(const ast_type *_type) {
    if (!_type->builtin) {
        return _type;
    }
    const auto *type = static_cast<const ast_builtin *>(_type);
    switch (type->type) {
    case eKeyword::K_i16vec4:
    case eKeyword::K_i16vec3:
    case eKeyword::K_i16vec2:
        return &g_short_type;
    case eKeyword::K_u16vec4:
    case eKeyword::K_u16vec3:
    case eKeyword::K_u16vec2:
        return &g_ushort_type;
    case eKeyword::K_ivec4:
    case eKeyword::K_ivec3:
    case eKeyword::K_ivec2:
        return &g_int_type;
    case eKeyword::K_uvec4:
    case eKeyword::K_uvec3:
    case eKeyword::K_uvec2:
        return &g_uint_type;
    case eKeyword::K_i64vec4:
    case eKeyword::K_i64vec3:
    case eKeyword::K_i64vec2:
        return &g_long_type;
    case eKeyword::K_u64vec4:
    case eKeyword::K_u64vec3:
    case eKeyword::K_u64vec2:
        return &g_ulong_type;
    case eKeyword::K_f16vec4:
    case eKeyword::K_f16vec3:
    case eKeyword::K_f16vec2:
        return &g_half_type;
    case eKeyword::K_vec4:
    case eKeyword::K_vec3:
    case eKeyword::K_vec2:
        return &g_float_type;
    case eKeyword::K_dvec4:
    case eKeyword::K_dvec3:
    case eKeyword::K_dvec2:
        return &g_double_type;
    case eKeyword::K_bvec4:
    case eKeyword::K_bvec3:
    case eKeyword::K_bvec2:
        return &g_bool_type;
    default:
        return type;
    }
}

const ast_type *to_vector_type(const ast_type *_scalar_type, int channels) {
    if (!_scalar_type->builtin) {
        return _scalar_type;
    }
    const auto *scalar_type = static_cast<const ast_builtin *>(_scalar_type);
    if (scalar_type->type == eKeyword::K_int16_t) {
        if (channels == 1) {
            return &g_short_type;
        } else if (channels == 2) {
            return &g_i16vec2_type;
        } else if (channels == 3) {
            return &g_i16vec3_type;
        } else if (channels == 4) {
            return &g_i16vec4_type;
        }
    } else if (scalar_type->type == eKeyword::K_uint16_t) {
        if (channels == 1) {
            return &g_ushort_type;
        } else if (channels == 2) {
            return &g_u16vec2_type;
        } else if (channels == 3) {
            return &g_u16vec3_type;
        } else if (channels == 4) {
            return &g_u16vec4_type;
        }
    } else if (scalar_type->type == eKeyword::K_int) {
        if (channels == 1) {
            return &g_int_type;
        } else if (channels == 2) {
            return &g_ivec2_type;
        } else if (channels == 3) {
            return &g_ivec3_type;
        } else if (channels == 4) {
            return &g_ivec4_type;
        }
    } else if (scalar_type->type == eKeyword::K_uint) {
        if (channels == 1) {
            return &g_uint_type;
        } else if (channels == 2) {
            return &g_uvec2_type;
        } else if (channels == 3) {
            return &g_uvec3_type;
        } else if (channels == 4) {
            return &g_uvec4_type;
        }
    } else if (scalar_type->type == eKeyword::K_long) {
        if (channels == 1) {
            return &g_long_type;
        } else if (channels == 2) {
            return &g_i64vec2_type;
        } else if (channels == 3) {
            return &g_i64vec3_type;
        } else if (channels == 4) {
            return &g_i64vec4_type;
        }
    } else if (scalar_type->type == eKeyword::K_uint64_t) {
        if (channels == 1) {
            return &g_ulong_type;
        } else if (channels == 2) {
            return &g_u64vec2_type;
        } else if (channels == 3) {
            return &g_u64vec3_type;
        } else if (channels == 4) {
            return &g_u64vec4_type;
        }
    } else if (scalar_type->type == eKeyword::K_float16_t) {
        if (channels == 1) {
            return &g_half_type;
        } else if (channels == 2) {
            return &g_f16vec2_type;
        } else if (channels == 3) {
            return &g_f16vec3_type;
        } else if (channels == 4) {
            return &g_f16vec4_type;
        }
    } else if (scalar_type->type == eKeyword::K_float) {
        if (channels == 1) {
            return &g_float_type;
        } else if (channels == 2) {
            return &g_vec2_type;
        } else if (channels == 3) {
            return &g_vec3_type;
        } else if (channels == 4) {
            return &g_vec4_type;
        }
    } else if (scalar_type->type == eKeyword::K_double) {
        if (channels == 1) {
            return &g_double_type;
        } else if (channels == 2) {
            return &g_dvec2_type;
        } else if (channels == 3) {
            return &g_dvec3_type;
        } else if (channels == 4) {
            return &g_dvec4_type;
        }
    } else if (scalar_type->type == eKeyword::K_bool) {
        if (channels == 1) {
            return &g_bool_type;
        } else if (channels == 2) {
            return &g_bvec2_type;
        } else if (channels == 3) {
            return &g_bvec3_type;
        } else if (channels == 4) {
            return &g_bvec4_type;
        }
    }
    return scalar_type;
}

bool is_integer_type(const eKeyword type) {
    switch (type) {
    case eKeyword::K_int16_t:
    case eKeyword::K_uint16_t:
    case eKeyword::K_int:
    case eKeyword::K_uint:
    case eKeyword::K_int64_t:
    case eKeyword::K_uint64_t:
        return true;
    default:
        return false;
    }
}

bool is_integer_type(const ast_type *_type) {
    if (!_type->builtin) {
        return false;
    }
    return is_integer_type(static_cast<const ast_builtin *>(_type)->type);
}

bool is_unsigned_type(const ast_type *_type) {
    if (!_type->builtin) {
        return false;
    }
    return static_cast<const ast_builtin *>(_type)->type == eKeyword::K_uint;
}

bool is_float_type(const eKeyword type) {
    switch (type) {
    case eKeyword::K_float:
    case eKeyword::K_double:
        return true;
    default:
        return false;
    }
}

bool is_float_type(const ast_type *_type) {
    if (!_type->builtin) {
        return false;
    }
    return is_float_type(static_cast<const ast_builtin *>(_type)->type);
}

bool is_matrix_type(const eKeyword type) {
    switch (type) {
    case eKeyword::K_mat2:
    case eKeyword::K_mat3:
    case eKeyword::K_mat4:
    case eKeyword::K_mat2x3:
    case eKeyword::K_mat2x4:
    case eKeyword::K_mat3x2:
    case eKeyword::K_mat3x4:
    case eKeyword::K_mat4x2:
    case eKeyword::K_mat4x3:
    case eKeyword::K_dmat2:
    case eKeyword::K_dmat3:
    case eKeyword::K_dmat4:
    case eKeyword::K_dmat2x3:
    case eKeyword::K_dmat2x4:
    case eKeyword::K_dmat3x2:
    case eKeyword::K_dmat3x4:
    case eKeyword::K_dmat4x2:
    case eKeyword::K_dmat4x3:
        return true;
    default:
        return false;
    }
}

int is_matrix_type(const ast_type *_type) {
    if (!_type->builtin) {
        return -1;
    }
    return is_matrix_type(static_cast<const ast_builtin *>(_type)->type);
}

const ast_type *to_matrix_subscript_type(const ast_type *_type) {
    if (!_type->builtin) {
        return nullptr;
    }
    const eKeyword type = static_cast<const ast_builtin *>(_type)->type;
    switch (type) {
    case eKeyword::K_mat2:
    case eKeyword::K_mat3x2:
    case eKeyword::K_mat4x2:
        return &g_vec2_type;
    case eKeyword::K_mat2x3:
    case eKeyword::K_mat3:
    case eKeyword::K_mat4x3:
        return &g_vec3_type;
    case eKeyword::K_mat2x4:
    case eKeyword::K_mat4:
    case eKeyword::K_mat3x4:
        return &g_vec4_type;
    case eKeyword::K_dmat2:
    case eKeyword::K_dmat3x2:
    case eKeyword::K_dmat4x2:
        return &g_dvec2_type;
    case eKeyword::K_dmat2x3:
    case eKeyword::K_dmat3:
    case eKeyword::K_dmat4x3:
        return &g_dvec3_type;
    case eKeyword::K_dmat2x4:
    case eKeyword::K_dmat3x4:
    case eKeyword::K_dmat4:
        return &g_dvec4_type;
    default:
        return nullptr;
    }
}

bool is_same_type(const ast_type *_type1, const ast_type *_type2) {
    if (_type1->builtin != _type2->builtin) {
        return false;
    }
    if (!_type1->builtin) {
        return _type1 == _type2;
    }
    const auto *type1 = static_cast<const ast_builtin *>(_type1);
    const auto *type2 = static_cast<const ast_builtin *>(_type2);
    return type1->type == type2->type;
}

bool is_compatible_type(const ast_type *_type1, const ast_type *_type2) {
    if (_type1->builtin != _type2->builtin) {
        return false;
    }
    if (!_type1->builtin) {
        return _type1 == _type2;
    }
    const auto *type1 = static_cast<const ast_builtin *>(_type1);
    const auto *type2 = static_cast<const ast_builtin *>(_type2);
    if (type1->type == type2->type) {
        return true;
    }

    for (int i = 0; i < std::size(g_convertible_types); ++i) {
        if (g_convertible_types[i][0] == type1->type) {
            for (int j = 1; j < std::size(g_convertible_types[0]); ++j) {
                if (g_convertible_types[i][j] == type2->type) {
                    return true;
                }
            }
        }
    }
    return false;
}
} // namespace glslx

const glslx::ast_type *glslx::Evaluate_ExpressionResultType(const TrUnit *tu, const ast_expression *expression,
                                                            int &array_dims) {
    array_dims = 0;
    switch (expression->type) {
    case eExprType::ShortConstant:
        return &g_short_type;
    case eExprType::UShortConstant:
        return &g_ushort_type;
    case eExprType::IntConstant:
        return &g_int_type;
    case eExprType::UIntConstant:
        return &g_uint_type;
    case eExprType::LongConstant:
        return &g_long_type;
    case eExprType::ULongConstant:
        return &g_ulong_type;
    case eExprType::HalfConstant:
        return &g_half_type;
    case eExprType::FloatConstant:
        return &g_float_type;
    case eExprType::DoubleConstant:
        return &g_double_type;
    case eExprType::BoolConstant:
        return &g_bool_type;
    case eExprType::VariableIdentifier: {
        const ast_variable *var = static_cast<const ast_variable_identifier *>(expression)->variable;
        array_dims = int(var->array_sizes.size());
        return var->base_type;
    }
    case eExprType::FieldOrSwizzle: {
        const auto *expr = static_cast<const ast_field_or_swizzle *>(expression);
        if (expr->field) {
            return Evaluate_ExpressionResultType(tu, expr->field, array_dims);
        } else {
            const ast_type *operand_type = Evaluate_ExpressionResultType(tu, expr->operand, array_dims);
            if (operand_type) {
                return to_vector_type(to_scalar_type(operand_type), int(strlen(expr->name)));
            }
        }
    } break;
    case eExprType::ArraySubscript: {
        const ast_type *operand_type = Evaluate_ExpressionResultType(
            tu, static_cast<const ast_array_subscript *>(expression)->operand, array_dims);
        if (array_dims > 0) {
            --array_dims;
            return operand_type;
        }
        if (operand_type && get_vector_size(operand_type) > 1) {
            return to_scalar_type(operand_type);
        }
        if (operand_type && is_matrix_type(operand_type)) {
            return to_matrix_subscript_type(operand_type);
        }
        return operand_type;
    }
    case eExprType::FunctionCall: {
        const auto *func_call = static_cast<const ast_function_call *>(expression);

        global_vector<const ast_type *> arg_types;
        for (int i = 0; i < int(func_call->parameters.size()); ++i) {
            int param_array_dims = 0;
            arg_types.push_back(Evaluate_ExpressionResultType(tu, func_call->parameters[i], param_array_dims));
            if (!arg_types.back()) {
                return nullptr;
            }
        }

        for (int i = 0; i < g_atomic_functions_count; ++i) {
            if (strcmp(func_call->name, g_atomic_functions[i]) == 0) {
                return arg_types[0];
            }
        }

        auto *p_find = tu->functions_by_name.Find(func_call->name);
        if (p_find) {
            for (ast_function *f : *p_find) {
                if (f->parameters.size() == arg_types.size()) {
                    bool args_match = true;
                    for (int j = 0; j < int(arg_types.size()) && args_match; ++j) {
                        const ast_type *arg_type = f->parameters[j]->base_type;
                        if (!arg_type) {
                            args_match = false;
                            break;
                        }
                        args_match &= is_same_type(arg_type, arg_types[j]);
                    }
                    if (args_match) {
                        return f->return_type;
                    }
                }
            }
        }

        return nullptr;
    } break;
    case eExprType::ConstructorCall: {
        return static_cast<const ast_constructor_call *>(expression)->type;
    } break;
    case eExprType::PostIncrement:
    case eExprType::PostDecrement:
    case eExprType::UnaryPlus:
    case eExprType::UnaryMinus:
    case eExprType::BitNot:
    case eExprType::LogicalNot:
    case eExprType::PrefixIncrement:
    case eExprType::PrefixDecrement:
        return Evaluate_ExpressionResultType(tu, static_cast<const ast_unary_expression *>(expression)->operand,
                                             array_dims);
    case eExprType::Assign:
        /*if (nested) {
            out_stream << "(";
        }
        Write_Assignment(static_cast<const ast_assignment_expression *>(expression), out_stream);
        if (nested) {
            out_stream << ")";
        }
        break;*/
        break;
    case eExprType::Sequence:
        return Evaluate_ExpressionResultType(tu, static_cast<const ast_sequence_expression *>(expression)->operand1,
                                             array_dims);
    case eExprType::Operation: {
        const auto *operation = static_cast<const ast_operation_expression *>(expression);
        const ast_type *op1_type = Evaluate_ExpressionResultType(tu, operation->operand1, array_dims);
        const ast_type *op2_type = Evaluate_ExpressionResultType(tu, operation->operand2, array_dims);
        if (!op1_type || !op2_type) {
            return nullptr;
        }

        if (is_same_type(op1_type, op2_type)) {
            return op1_type;
        }

        if (is_integer_type(to_scalar_type(op1_type)) && is_integer_type(to_scalar_type(op2_type))) {
            if (get_vector_size(op1_type) > get_vector_size(op2_type)) {
                return op1_type;
            } else if (get_vector_size(op2_type) > get_vector_size(op1_type)) {
                return op2_type;
            } else {
                if (is_unsigned_type(to_scalar_type(op1_type)) && !is_unsigned_type(to_scalar_type(op2_type))) {
                    return op1_type;
                } else {
                    return op2_type;
                }
            }
        } else if (is_integer_type(to_scalar_type(op1_type)) && is_float_type(to_scalar_type(op2_type))) {
            return op2_type;
        } else if (is_float_type(to_scalar_type(op1_type)) && is_integer_type(to_scalar_type(op2_type))) {
            return op1_type;
        }

        if (operation->oper == eOperator::multiply && is_matrix_type(op1_type) && get_vector_size(op2_type) > 1) {
        }

        // return Write_Operation(static_cast<const ast_operation_expression *>(expression), out_stream);
    } break;
    case eExprType::Ternary:
        // return Write_Ternary(static_cast<const ast_ternary_expression *>(expression), out_stream);
        break;
    case eExprType::ArraySpecifier:
        /*const ast_array_specifier *arr_specifier = static_cast<const ast_array_specifier *>(expression);
        out_stream << "{ ";
        for (int i = 0; i < int(arr_specifier->expressions.size()); ++i) {
            Write_Expression(arr_specifier->expressions[i], false, out_stream);
            if (i != int(arr_specifier->expressions.size()) - 1) {
                out_stream << ", ";
            }
        }
        out_stream << " }";
        break;*/
        break;
    }
    return nullptr;
}