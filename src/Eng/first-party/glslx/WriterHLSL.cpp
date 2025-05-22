#include "WriterHLSL.h"

#include <ostream>

namespace glslx {
#define X(_0, _1) #_0,
const char *g_hlsl_keywords[] = {
#include "KeywordsHLSL.inl"
};
#undef X

bool is_shadow_sampler(const ast_type *base_type) {
    if (!base_type->builtin) {
        return false;
    }
    const auto *type = static_cast<const ast_builtin *>(base_type);
    return type->type == eKeyword::K_sampler1DShadow || type->type == eKeyword::K_sampler1DArrayShadow ||
           type->type == eKeyword::K_sampler2DShadow || type->type == eKeyword::K_sampler2DArrayShadow ||
           type->type == eKeyword::K_sampler2DRectShadow || type->type == eKeyword::K_samplerCubeShadow ||
           type->type == eKeyword::K_samplerCubeArrayShadow || type->type == eKeyword::K_sampler1DArrayShadow ||
           type->type == eKeyword::K_sampler2DArrayShadow;
}

bool is_combined_texturesampler(const ast_type *base_type) {
    if (!base_type->builtin) {
        return false;
    }
    const eKeyword type = static_cast<const ast_builtin *>(base_type)->type;
    return type == eKeyword::K_sampler1D || type == eKeyword::K_sampler2D || type == eKeyword::K_sampler3D ||
           type == eKeyword::K_samplerCube || type == eKeyword::K_sampler1DArray ||
           type == eKeyword::K_sampler2DArray || type == eKeyword::K_sampler2DMS ||
           type == eKeyword::K_sampler2DMSArray || type == eKeyword::K_samplerCubeArray ||
           type == eKeyword::K_isampler1D || type == eKeyword::K_isampler2D || type == eKeyword::K_isampler3D ||
           type == eKeyword::K_isamplerCube || type == eKeyword::K_isampler1DArray ||
           type == eKeyword::K_isampler2DArray || type == eKeyword::K_isampler2DMS ||
           type == eKeyword::K_isampler2DMSArray || type == eKeyword::K_isamplerCubeArray ||
           type == eKeyword::K_usampler1D || type == eKeyword::K_usampler2D || type == eKeyword::K_usampler3D ||
           type == eKeyword::K_usamplerCube || type == eKeyword::K_usampler1DArray ||
           type == eKeyword::K_usampler2DArray || type == eKeyword::K_usampler2DMS ||
           type == eKeyword::K_usampler2DMSArray || type == eKeyword::K_usamplerCubeArray ||
           is_shadow_sampler(base_type);
}

bool is_int_image(const ast_type *base_type) {
    if (!base_type->builtin) {
        return false;
    }
    const eKeyword type = static_cast<const ast_builtin *>(base_type)->type;
    return type == eKeyword::K_isampler1D || type == eKeyword::K_isampler1DArray || type == eKeyword::K_isampler2D ||
           type == eKeyword::K_isampler2DArray || type == eKeyword::K_isampler2DRect ||
           type == eKeyword::K_isampler2DMS || type == eKeyword::K_isampler2DMSArray ||
           type == eKeyword::K_isampler3D || type == eKeyword::K_isamplerCube ||
           type == eKeyword::K_isamplerCubeArray || type == eKeyword::K_isamplerBuffer ||
           type == eKeyword::K_iimage1D || type == eKeyword::K_iimage1DArray || type == eKeyword::K_iimage2D ||
           type == eKeyword::K_iimage2DArray || type == eKeyword::K_iimage2DRect || type == eKeyword::K_iimage2DMS ||
           type == eKeyword::K_iimage2DMSArray || type == eKeyword::K_iimage3D || type == eKeyword::K_iimageCube ||
           type == eKeyword::K_iimageCubeArray || type == eKeyword::K_iimageBuffer || type == eKeyword::K_itexture1D ||
           type == eKeyword::K_itexture1DArray || type == eKeyword::K_itexture2D ||
           type == eKeyword::K_itexture2DArray || type == eKeyword::K_itexture2DRect ||
           type == eKeyword::K_itexture2DMS || type == eKeyword::K_itexture2DMSArray ||
           type == eKeyword::K_itexture3D || type == eKeyword::K_itextureCube ||
           type == eKeyword::K_itextureCubeArray || type == eKeyword::K_itextureBuffer;
}

bool is_uint_image(const ast_type *base_type) {
    if (!base_type->builtin) {
        return false;
    }
    const eKeyword type = static_cast<const ast_builtin *>(base_type)->type;
    return type == eKeyword::K_usampler1D || type == eKeyword::K_usampler1DArray || type == eKeyword::K_usampler2D ||
           type == eKeyword::K_usampler2DArray || type == eKeyword::K_usampler2DRect ||
           type == eKeyword::K_usampler2DMS || type == eKeyword::K_usampler2DMSArray ||
           type == eKeyword::K_usampler3D || type == eKeyword::K_usamplerCube ||
           type == eKeyword::K_usamplerCubeArray || type == eKeyword::K_usamplerBuffer ||
           type == eKeyword::K_uimage1D || type == eKeyword::K_uimage1DArray || type == eKeyword::K_uimage2D ||
           type == eKeyword::K_uimage2DArray || type == eKeyword::K_uimage2DRect || type == eKeyword::K_uimage2DMS ||
           type == eKeyword::K_uimage2DMSArray || type == eKeyword::K_uimage3D || type == eKeyword::K_uimageCube ||
           type == eKeyword::K_uimageCubeArray || type == eKeyword::K_uimageBuffer || type == eKeyword::K_utexture1D ||
           type == eKeyword::K_utexture2D;
}

bool requires_template_argument(const ast_type *base_type) {
    if (!base_type->builtin) {
        return false;
    }
    const eKeyword type = static_cast<const ast_builtin *>(base_type)->type;
    return type == eKeyword::K_sampler1D || type == eKeyword::K_sampler2D || type == eKeyword::K_sampler3D ||
           type == eKeyword::K_samplerCube || type == eKeyword::K_sampler1DArray ||
           type == eKeyword::K_sampler2DArray || type == eKeyword::K_samplerBuffer || type == eKeyword::K_sampler2DMS ||
           type == eKeyword::K_sampler2DMSArray || type == eKeyword::K_samplerCubeArray ||
           type == eKeyword::K_isampler1D || type == eKeyword::K_isampler2D || type == eKeyword::K_sampler1DShadow ||
           type == eKeyword::K_image1D || type == eKeyword::K_image2D || type == eKeyword::K_image3D ||
           type == eKeyword::K_image1DArray || type == eKeyword::K_image2DArray || type == eKeyword::K_imageBuffer ||
           type == eKeyword::K_image2DMS || type == eKeyword::K_image2DMSArray || type == eKeyword::K_iimage1D ||
           type == eKeyword::K_iimage2D || type == eKeyword::K_isampler3D || type == eKeyword::K_iimage3D ||
           type == eKeyword::K_isampler1DArray || type == eKeyword::K_iimage1DArray ||
           type == eKeyword::K_isampler2DArray || type == eKeyword::K_iimage2DArray ||
           type == eKeyword::K_isamplerBuffer || type == eKeyword::K_iimageBuffer || type == eKeyword::K_iimage2DMS ||
           type == eKeyword::K_isampler2DMSArray || type == eKeyword::K_iimage2DMSArray ||
           type == eKeyword::K_isamplerCubeArray || type == eKeyword::K_usampler1D || type == eKeyword::K_uimage1D ||
           type == eKeyword::K_usampler2D || type == eKeyword::K_uimage2D || type == eKeyword::K_usampler3D ||
           type == eKeyword::K_uimage3D || type == eKeyword::K_uimageCube || type == eKeyword::K_usampler1DArray ||
           type == eKeyword::K_uimage1DArray || type == eKeyword::K_usampler2DArray ||
           type == eKeyword::K_uimage2DArray || type == eKeyword::K_usamplerBuffer ||
           type == eKeyword::K_uimageBuffer || type == eKeyword::K_usampler2DMS || type == eKeyword::K_uimage2DMS ||
           type == eKeyword::K_usampler2DMSArray || type == eKeyword::K_uimage2DMSArray ||
           type == eKeyword::K_usamplerCubeArray || type == eKeyword::K_texture1D || type == eKeyword::K_itexture1D ||
           type == eKeyword::K_utexture1D || type == eKeyword::K_texture2D || type == eKeyword::K_itexture2D ||
           type == eKeyword::K_utexture2D || is_combined_texturesampler(base_type);
}

bool is_scalar_type(const eKeyword type) {
    switch (type) {
    case eKeyword::K_int:
    case eKeyword::K_uint:
    case eKeyword::K_float:
    case eKeyword::K_double:
    case eKeyword::K_bool:
        return true;
    default:
        return false;
    }
}

bool is_scalar_type(const ast_type *_type) {
    if (!_type->builtin) {
        return false;
    }
    return is_scalar_type(static_cast<const ast_builtin *>(_type)->type);
}

int get_variable_size(const ast_variable *v, const int array_dim) {
    if ((v->flags & eVariableFlags::Array) && int(v->array_sizes.size()) > array_dim && v->array_sizes[array_dim]) {
        int count = 0;
        if (v->array_sizes[array_dim]->type == eExprType::IntConstant) {
            count = static_cast<ast_int_constant *>(v->array_sizes[array_dim])->value;
        } else if (v->array_sizes[array_dim]->type == eExprType::UIntConstant) {
            count = static_cast<ast_uint_constant *>(v->array_sizes[array_dim])->value;
        } else {
            assert(false);
        }
        int size = 0;
        for (int i = 0; i < count; ++i) {
            size += get_variable_size(v, array_dim + 1);
        }
        return size;
    }

    if (v->base_type->builtin) {
        const auto *type = static_cast<const ast_builtin *>(v->base_type);
        switch (type->type) {
        case eKeyword::K_int:
        case eKeyword::K_uint:
        case eKeyword::K_float:
            return 4;
        case eKeyword::K_double:
        case eKeyword::K_int64_t:
        case eKeyword::K_uint64_t:
            return 8;
        case eKeyword::K_bool:
            return 1;
        case eKeyword::K_float16_t:
            return 2;
        case eKeyword::K_ivec2:
        case eKeyword::K_uvec2:
        case eKeyword::K_vec2:
            return 2 * 4;
        case eKeyword::K_ivec3:
        case eKeyword::K_uvec3:
        case eKeyword::K_vec3:
            return 3 * 4;
        case eKeyword::K_ivec4:
        case eKeyword::K_uvec4:
        case eKeyword::K_vec4:
            return 4 * 4;
        case eKeyword::K_mat4x4:
        case eKeyword::K_mat4:
            return 16 * 4;
        default:
            return -1;
        }
    } else {
        int ret = 0;
        const auto *type = static_cast<const ast_struct *>(v->base_type);
        for (const ast_variable *_v : type->fields) {
            ret += get_variable_size(_v, array_dim);
        }
        return ret;
    }
}

std::pair<int, int> get_binding_and_set(Span<const ast_layout_qualifier *const> qualifiers) {
    int binding = -1, set = 0;
    for (int i = 0; i < int(qualifiers.size()); ++i) {
        const ast_layout_qualifier *qualifier = qualifiers[i];
        if (strcmp(qualifier->name, "binding") == 0) {
            if (qualifier->initial_value->type == eExprType::IntConstant) {
                binding = static_cast<const ast_int_constant *>(qualifier->initial_value)->value;
            } else if (qualifier->initial_value->type == eExprType::UIntConstant) {
                binding = static_cast<const ast_uint_constant *>(qualifier->initial_value)->value;
            }
        } else if (strcmp(qualifier->name, "set") == 0) {
            if (qualifier->initial_value->type == eExprType::IntConstant) {
                set = static_cast<const ast_int_constant *>(qualifier->initial_value)->value;
            } else if (qualifier->initial_value->type == eExprType::UIntConstant) {
                set = static_cast<const ast_uint_constant *>(qualifier->initial_value)->value;
            }
        }
    }
    return std::make_pair(binding, set);
}

extern const HashSet32<const char *> g_atomic_functions{
    {"atomicAdd", "atomicAnd", "atomicOr", "atomicXor", "atomicMin", "atomicMax", "atomicCompSwap", "atomicExchange"}};

const HashMap32<std::string, std::string> g_hlsl_function_mapping{{"intBitsToFloat", "asfloat"},
                                                                  {"uintBitsToFloat", "asfloat"},
                                                                  {"floatBitsToInt", "asint"},
                                                                  {"floatBitsToUint", "asuint"},
                                                                  {"fract", "frac"},
                                                                  {"fma", "mad"},
                                                                  {"inversesqrt", "rsqrt"},
                                                                  {"mix", "lerp"},
                                                                  {"barrier", "GroupMemoryBarrierWithGroupSync"},
                                                                  {"groupMemoryBarrier", "AllMemoryBarrier"},
                                                                  {"atomicAdd", "InterlockedAdd"},
                                                                  {"atomicAnd", "InterlockedAnd"},
                                                                  {"atomicOr", "InterlockedOr"},
                                                                  {"atomicXor", "InterlockedXor"},
                                                                  {"atomicMin", "InterlockedMin"},
                                                                  {"atomicMax", "InterlockedMax"},
                                                                  {"atomicExchange", "InterlockedExchange"},
                                                                  {"atomicCompSwap", "InterlockedCompareExchange"},
                                                                  {"subgroupAll", "WaveActiveAllTrue"},
                                                                  {"subgroupAny", "WaveActiveAnyTrue"},
                                                                  {"subgroupAdd", "WaveActiveSum"},
                                                                  {"subgroupElect", "WaveIsFirstLane"},
                                                                  {"subgroupExclusiveAdd", "WavePrefixSum"},
                                                                  {"bitCount", "countbits"},
                                                                  {"nonuniformEXT", "NonUniformResourceIndex"}};
} // namespace glslx

void glslx::WriterHLSL::Write_Expression(const ast_expression *expression, bool nested, std::ostream &out_stream) {
    switch (expression->type) {
    case eExprType::Undefined:
        assert(false);
        return;
    case eExprType::ShortConstant:
        return Write_Constant(static_cast<const ast_short_constant *>(expression), out_stream);
    case eExprType::UShortConstant:
        return Write_Constant(static_cast<const ast_ushort_constant *>(expression), out_stream);
    case eExprType::IntConstant:
        return Write_Constant(static_cast<const ast_int_constant *>(expression), out_stream);
    case eExprType::UIntConstant:
        return Write_Constant(static_cast<const ast_uint_constant *>(expression), out_stream);
    case eExprType::LongConstant:
        return Write_Constant(static_cast<const ast_long_constant *>(expression), out_stream);
    case eExprType::ULongConstant:
        return Write_Constant(static_cast<const ast_ulong_constant *>(expression), out_stream);
    case eExprType::HalfConstant:
        return Write_Constant(static_cast<const ast_half_constant *>(expression), out_stream);
    case eExprType::FloatConstant:
        return Write_Constant(static_cast<const ast_float_constant *>(expression), out_stream);
    case eExprType::DoubleConstant:
        return Write_Constant(static_cast<const ast_double_constant *>(expression), out_stream);
    case eExprType::BoolConstant:
        return Write_Constant(static_cast<const ast_bool_constant *>(expression), out_stream);
    case eExprType::VariableIdentifier:
        return Write_VariableIdentifier(static_cast<const ast_variable_identifier *>(expression), out_stream);
    case eExprType::FieldOrSwizzle:
        return Write_FieldOrSwizzle(static_cast<const ast_field_or_swizzle *>(expression), out_stream);
    case eExprType::ArraySubscript:
        return Write_ArraySubscript(static_cast<const ast_array_subscript *>(expression), out_stream);
    case eExprType::FunctionCall:
        return Write_FunctionCall(static_cast<const ast_function_call *>(expression), out_stream);
    case eExprType::ConstructorCall:
        return Write_ConstructorCall(static_cast<const ast_constructor_call *>(expression), out_stream);
    case eExprType::PostIncrement:
        return Write_PostIncrement(static_cast<const ast_post_increment_expression *>(expression), out_stream);
    case eExprType::PostDecrement:
        return Write_PostDecrement(static_cast<const ast_post_decrement_expression *>(expression), out_stream);
    case eExprType::UnaryPlus:
        return Write_UnaryPlus(static_cast<const ast_unary_plus_expression *>(expression), out_stream);
    case eExprType::UnaryMinus:
        return Write_UnaryMinus(static_cast<const ast_unary_minus_expression *>(expression), out_stream);
    case eExprType::BitNot:
        return Write_UnaryBitNot(static_cast<const ast_unary_bit_not_expression *>(expression), out_stream);
    case eExprType::LogicalNot:
        return Write_UnaryLogicalNot(static_cast<const ast_unary_logical_not_expression *>(expression), out_stream);
    case eExprType::PrefixIncrement:
        return Write_PrefixIncrement(static_cast<const ast_prefix_increment_expression *>(expression), out_stream);
    case eExprType::PrefixDecrement:
        return Write_PrefixDecrement(static_cast<const ast_prefix_decrement_expression *>(expression), out_stream);
    case eExprType::Assign:
        if (nested) {
            out_stream << "(";
        }
        Write_Assignment(static_cast<const ast_assignment_expression *>(expression), out_stream);
        if (nested) {
            out_stream << ")";
        }
        break;
    case eExprType::Sequence:
        return Write_Sequence(static_cast<const ast_sequence_expression *>(expression), out_stream);
    case eExprType::Operation: {
        const auto *operation = static_cast<const ast_operation_expression *>(expression);
        int array_dims = 0;
        const ast_type *op1_type = Evaluate_ExpressionResultType(tu_, operation->operand1, array_dims);
        const ast_type *op2_type = Evaluate_ExpressionResultType(tu_, operation->operand2, array_dims);

        if (op1_type && op2_type) {
            if (operation->oper == eOperator::multiply && is_matrix_type(op1_type) && get_vector_size(op2_type) > 1) {
                ast_function_call func_call(tu_->alloc.allocator);
                func_call.name = tu_->makestr("mul");
                func_call.parameters.push_back(operation->operand2);
                func_call.parameters.push_back(operation->operand1);
                return Write_FunctionCall(&func_call, out_stream);
            }
        }

        return Write_Operation(static_cast<const ast_operation_expression *>(expression), out_stream);
    }
    case eExprType::Ternary:
        return Write_Ternary(static_cast<const ast_ternary_expression *>(expression), out_stream);
    case eExprType::ArraySpecifier:
        const auto *arr_specifier = static_cast<const ast_array_specifier *>(expression);
        out_stream << "{ ";
        for (int i = 0; i < int(arr_specifier->expressions.size()); ++i) {
            Write_Expression(arr_specifier->expressions[i], false, out_stream);
            if (i != int(arr_specifier->expressions.size()) - 1) {
                out_stream << ", ";
            }
        }
        out_stream << " }";
        break;
    }
}

void glslx::WriterHLSL::Write_FunctionParameter(const ast_function_parameter *parameter, std::ostream &out_stream) {
    Write_ParameterQualifiers(parameter->qualifiers, out_stream);
    Write_Precision(parameter->precision, out_stream);
    Write_Type(parameter->base_type, out_stream);
    if (parameter->name) {
        out_stream << " " << parameter->name;
    }
    if (parameter->flags & eVariableFlags::Array) {
        Write_ArraySize(parameter->array_sizes, out_stream);
    }
}

void glslx::WriterHLSL::Write_Function(const ast_function *function, std::ostream &out_stream) {
    if (function->attributes & eFunctionAttribute::Builtin) {
        return;
    }

    Write_Type(function->return_type, out_stream);
    out_stream << " " << function->name << "(";
    if (tu_->type == eTrUnitType::Compute && strcmp(function->name, "main") == 0) {
        out_stream << "GLSLX_Input glslx_input";
        if (!function->parameters.empty()) {
            out_stream << ", ";
        }
    }
    for (int i = 0; i < int(function->parameters.size()); ++i) {
        Write_FunctionParameter(function->parameters[i], out_stream);
        if (is_combined_texturesampler(function->parameters[i]->base_type)) {
            out_stream << ", SamplerState ";
            out_stream << function->parameters[i]->name;
            out_stream << "_sampler";
        }
        if (i != int(function->parameters.size()) - 1) {
            out_stream << ", ";
        }
    }
    out_stream << ")";
    if (function->attributes & eFunctionAttribute::Prototype) {
        out_stream << ";\n";
        return;
    }
    out_stream << " {\n";
    ++nest_level_;
    if (strcmp(function->name, "main") == 0) {
        if (tu_->type == eTrUnitType::Compute) {
            Write_Tabs(out_stream);
            out_stream << "gl_WorkGroupID = glslx_input.gl_WorkGroupID;\n";
            Write_Tabs(out_stream);
            out_stream << "gl_LocalInvocationID = glslx_input.gl_LocalInvocationID;\n";
            Write_Tabs(out_stream);
            out_stream << "gl_GlobalInvocationID = glslx_input.gl_GlobalInvocationID;\n";
            Write_Tabs(out_stream);
            out_stream << "gl_LocalInvocationIndex = glslx_input.gl_LocalInvocationIndex;\n";
            Write_Tabs(out_stream);
            out_stream << "gl_SubgroupSize = WaveGetLaneCount();\n";
            Write_Tabs(out_stream);
            out_stream << "gl_SubgroupInvocationID = WaveGetLaneIndex();\n";
        }
    }
    for (const ast_statement *statement : function->statements) {
        Write_Statement(statement, out_stream, DefaultOutputFlags);
    }
    --nest_level_;
    out_stream << "}\n";
}

void glslx::WriterHLSL::Write_SelectionAttributes(const Bitmask<eCtrlFlowAttribute> attributes,
                                                  std::ostream &out_stream) {
    if (!(attributes & SelectionAttributesMask)) {
        return;
    }
    out_stream << "[";
    if (attributes & eCtrlFlowAttribute::Flatten) {
        out_stream << "flatten";
    } else if (attributes & eCtrlFlowAttribute::DontFlatten) {
        out_stream << "branch";
    }
    out_stream << "] ";
}

void glslx::WriterHLSL::Write_LoopAttributes(ctrl_flow_params_t ctrl_flow, std::ostream &out_stream) {
    if (!(ctrl_flow.attributes & LoopAttributesMask)) {
        return;
    }
    out_stream << "[";
    if (ctrl_flow.attributes & eCtrlFlowAttribute::Unroll) {
        out_stream << "unroll";
        ctrl_flow.attributes &= ~(Bitmask{eCtrlFlowAttribute::Unroll} | eCtrlFlowAttribute::DontUnroll);
    } else if (ctrl_flow.attributes & eCtrlFlowAttribute::DontUnroll) {
        out_stream << "loop";
        ctrl_flow.attributes &= ~(Bitmask{eCtrlFlowAttribute::Unroll} | eCtrlFlowAttribute::DontUnroll);
    }
    out_stream << "] ";
}

void glslx::WriterHLSL::Write_FunctionVariable(const ast_function_variable *variable, std::ostream &out_stream,
                                               const Bitmask<eOutputFlags> output_flags) {
    if (variable->initial_value) {
        Process_AtomicOperations(variable->initial_value, out_stream);
    }

    if (variable->flags & eVariableFlags::Const) {
        out_stream << "const ";
    }
    Write_Variable(variable, {}, out_stream, output_flags);
    if (variable->initial_value) {
        out_stream << " = ";
        Write_Expression(variable->initial_value, false, out_stream);
    }
    [[maybe_unused]] static const auto ComaOrSemicolon = Bitmask{eOutputFlags::Coma} | eOutputFlags::Semicolon;
    assert((output_flags & ComaOrSemicolon) != ComaOrSemicolon);
    if (output_flags & eOutputFlags::Semicolon) {
        out_stream << ";";
    } else if (output_flags & eOutputFlags::Coma) {
        out_stream << ", ";
    }
    if (output_flags & eOutputFlags::NewLine) {
        out_stream << "\n";
    }
}

void glslx::WriterHLSL::Write_CompoundStatement(const ast_compound_statement *statement, std::ostream &out_stream,
                                                Bitmask<eOutputFlags> output_flags) {
    out_stream << " {\n";
    ++nest_level_;
    for (int i = 0; i < int(statement->statements.size()); ++i) {
        Write_Statement(statement->statements[i], out_stream, DefaultOutputFlags);
    }
    --nest_level_;
    Write_Tabs(out_stream);
    out_stream << "}";
    if (output_flags & eOutputFlags::NewLine) {
        out_stream << "\n";
    }
}

void glslx::WriterHLSL::Write_EmptyStatement(const ast_empty_statement *statement, std::ostream &out_stream,
                                             Bitmask<eOutputFlags> output_flags) {
    out_stream << ";";
}

void glslx::WriterHLSL::Write_DeclarationStatement(const ast_declaration_statement *statement, std::ostream &out_stream,
                                                   const Bitmask<eOutputFlags> _output_flags) {
    for (int i = 0; i < int(statement->variables.size()); ++i) {
        if (i > 0 && (_output_flags & eOutputFlags::WriteTabs)) {
            Write_Tabs(out_stream);
        }
        static const auto ComaOrSemicolon = Bitmask{eOutputFlags::Coma} | eOutputFlags::Semicolon;
        Bitmask<eOutputFlags> output_flags = _output_flags;
        if ((output_flags & ComaOrSemicolon) == ComaOrSemicolon) {
            if (i != int(statement->variables.size()) - 1) {
                output_flags &= ~Bitmask{eOutputFlags::Semicolon};
            } else {
                output_flags &= ~Bitmask{eOutputFlags::Coma};
            }
            if (i != 0) {
                output_flags &= ~Bitmask{eOutputFlags::VarType};
            }
        }
        Write_FunctionVariable(statement->variables[i], out_stream, output_flags);
    }
}

void glslx::WriterHLSL::Write_ExpressionStatement(const ast_expression_statement *statement, std::ostream &out_stream,
                                                  const Bitmask<eOutputFlags> output_flags) {
    Process_AtomicOperations(statement->expression, out_stream);
    Write_Expression(statement->expression, false, out_stream);
    if (output_flags & eOutputFlags::Semicolon) {
        out_stream << ";";
    }
    if (output_flags & eOutputFlags::NewLine) {
        out_stream << "\n";
    }
}

void glslx::WriterHLSL::Write_IfStatement(const ast_if_statement *statement, std::ostream &out_stream,
                                          Bitmask<eOutputFlags> output_flags) {
    Write_SelectionAttributes(statement->attributes, out_stream);
    out_stream << "if (";
    Write_Expression(statement->condition, false, out_stream);
    out_stream << ")";
    if (statement->else_statement) {
        Write_Statement(statement->then_statement, out_stream,
                        DefaultOutputFlags & ~(Bitmask{eOutputFlags::WriteTabs} | eOutputFlags::NewLine));
        out_stream << " else";
        if (statement->else_statement->type == eStatement::If) {
            out_stream << " ";
        }
        Write_Statement(statement->else_statement, out_stream, DefaultOutputFlags & ~Bitmask{eOutputFlags::WriteTabs});
    } else {
        Write_Statement(statement->then_statement, out_stream,
                        DefaultOutputFlags & ~(Bitmask{eOutputFlags::WriteTabs}));
    }
}

void glslx::WriterHLSL::Write_SwitchStatement(const ast_switch_statement *statement, std::ostream &out_stream,
                                              Bitmask<eOutputFlags> output_flags) {
    Write_SelectionAttributes(statement->attributes, out_stream);
    out_stream << "switch (";
    Write_Expression(statement->expression, false, out_stream);
    out_stream << ") {\n";
    ++nest_level_;
    for (int i = 0; i < int(statement->statements.size()); ++i) {
        Write_Statement(statement->statements[i], out_stream, DefaultOutputFlags);
    }
    --nest_level_;
    Write_Tabs(out_stream);
    out_stream << "}";
    if (output_flags & eOutputFlags::NewLine) {
        out_stream << "\n";
    }
}

void glslx::WriterHLSL::Write_CaseLabelStatement(const ast_case_label_statement *statement, std::ostream &out_stream,
                                                 Bitmask<eOutputFlags> output_flags) {
    if (statement->flags & eCaseLabelFlags::Default) {
        out_stream << "default";
    } else {
        out_stream << "case ";
        Write_Expression(statement->condition, false, out_stream);
    }
    out_stream << ":\n";
}

void glslx::WriterHLSL::Write_WhileStatement(const ast_while_statement *statement, std::ostream &out_stream,
                                             Bitmask<eOutputFlags> output_flags) {
    out_stream << "while (";
    switch (statement->condition->type) {
    case eStatement::Declaration:
        Write_DeclarationStatement(static_cast<const ast_declaration_statement *>(statement->condition), out_stream,
                                   {});
        break;
    case eStatement::Expression:
        Write_ExpressionStatement(static_cast<const ast_expression_statement *>(statement->condition), out_stream, {});
        break;
    default:
        // TODO: report error
        break;
    }
    out_stream << ")";
    Write_Statement(statement->body, out_stream, output_flags & ~Bitmask<eOutputFlags>{eOutputFlags::WriteTabs});
}

void glslx::WriterHLSL::Write_DoStatement(const ast_do_statement *statement, std::ostream &out_stream,
                                          Bitmask<eOutputFlags> output_flags) {
    out_stream << "do";
    if (statement->body->type != eStatement::Compound) {
        out_stream << " ";
    }
    Write_Statement(statement->body, out_stream, DefaultOutputFlags & ~Bitmask{eOutputFlags::WriteTabs});
    Write_Tabs(out_stream);
    out_stream << "while (";
    Write_Expression(statement->condition, false, out_stream);
    out_stream << ");";
    if (output_flags & eOutputFlags::NewLine) {
        out_stream << "\n";
    }
}

void glslx::WriterHLSL::Write_ForStatement(const ast_for_statement *statement, std::ostream &out_stream,
                                           Bitmask<eOutputFlags> output_flags) {
    Write_LoopAttributes(statement->flow_params, out_stream);
    out_stream << "for (";
    if (statement->init) {
        static const auto OutputFlags =
            Bitmask{eOutputFlags::VarType} | eOutputFlags::VarName | eOutputFlags::VarArrSize | eOutputFlags::Semicolon;
        switch (statement->init->type) {
        case eStatement::Declaration:
            Write_DeclarationStatement(static_cast<ast_declaration_statement *>(statement->init), out_stream,
                                       OutputFlags | eOutputFlags::Coma);
            break;
        case eStatement::Expression:
            Write_ExpressionStatement(static_cast<ast_expression_statement *>(statement->init), out_stream,
                                      OutputFlags);
            break;
        default:
            // TODO: report error
            break;
        }
    } else {
        out_stream << ";";
    }
    if (statement->condition) {
        out_stream << " ";
        Write_Expression(statement->condition, false, out_stream);
    }
    out_stream << ";";
    if (statement->loop) {
        out_stream << " ";
        Write_Expression(statement->loop, false, out_stream);
    }
    out_stream << ")";
    Write_Statement(statement->body, out_stream, DefaultOutputFlags & ~Bitmask{eOutputFlags::WriteTabs});
}

void glslx::WriterHLSL::Write_ContinueStatement(const ast_continue_statement *statement, std::ostream &out_stream,
                                                Bitmask<eOutputFlags> output_flags) {
    out_stream << "continue;\n";
}

void glslx::WriterHLSL::Write_BreakStatement(const ast_break_statement *statement, std::ostream &out_stream,
                                             Bitmask<eOutputFlags> output_flags) {
    out_stream << "break;\n";
}

void glslx::WriterHLSL::Write_ReturnStatement(const ast_return_statement *statement, std::ostream &out_stream,
                                              Bitmask<eOutputFlags> output_flags) {
    if (statement->expression) {
        out_stream << "return ";
        Write_Expression(statement->expression, false, out_stream);
        out_stream << ";\n";
    } else {
        out_stream << "return;\n";
    }
}

void glslx::WriterHLSL::Write_DistardStatement(const ast_discard_statement *statement, std::ostream &out_stream,
                                               Bitmask<eOutputFlags> output_flags) {
    out_stream << "discard;\n";
}

void glslx::WriterHLSL::Write_ExtJumpStatement(const ast_ext_jump_statement *statement, std::ostream &out_stream,
                                               Bitmask<eOutputFlags> output_flags) {
    out_stream << g_hlsl_keywords[int(statement->keyword)] << "();\n";
}

void glslx::WriterHLSL::Write_Statement(const ast_statement *statement, std::ostream &out_stream,
                                        Bitmask<eOutputFlags> out_flags) {
    if (out_flags & eOutputFlags::WriteTabs) {
        Write_Tabs(out_stream);
    }
    switch (statement->type) {
    case eStatement::Compound:
        Write_CompoundStatement(static_cast<const ast_compound_statement *>(statement), out_stream, out_flags);
        break;
    case eStatement::Empty:
        Write_EmptyStatement(static_cast<const ast_empty_statement *>(statement), out_stream, out_flags);
        break;
    case eStatement::Declaration:
        Write_DeclarationStatement(static_cast<const ast_declaration_statement *>(statement), out_stream, out_flags);
        break;
    case eStatement::Expression:
        Write_ExpressionStatement(static_cast<const ast_expression_statement *>(statement), out_stream, out_flags);
        break;
    case eStatement::If:
        Write_IfStatement(static_cast<const ast_if_statement *>(statement), out_stream, out_flags);
        break;
    case eStatement::Switch:
        Write_SwitchStatement(static_cast<const ast_switch_statement *>(statement), out_stream, out_flags);
        break;
    case eStatement::CaseLabel:
        Write_CaseLabelStatement(static_cast<const ast_case_label_statement *>(statement), out_stream, out_flags);
        break;
    case eStatement::While:
        Write_WhileStatement(static_cast<const ast_while_statement *>(statement), out_stream, out_flags);
        break;
    case eStatement::Do:
        Write_DoStatement(static_cast<const ast_do_statement *>(statement), out_stream, out_flags);
        break;
    case eStatement::For:
        Write_ForStatement(static_cast<const ast_for_statement *>(statement), out_stream, out_flags);
        break;
    case eStatement::Continue:
        Write_ContinueStatement(static_cast<const ast_continue_statement *>(statement), out_stream, out_flags);
        break;
    case eStatement::Break:
        Write_BreakStatement(static_cast<const ast_break_statement *>(statement), out_stream, out_flags);
        break;
    case eStatement::Return:
        Write_ReturnStatement(static_cast<const ast_return_statement *>(statement), out_stream, out_flags);
        break;
    case eStatement::Discard:
        Write_DistardStatement(static_cast<const ast_discard_statement *>(statement), out_stream, out_flags);
        break;
    default:
        // TODO: report error
        break;
    }
}

void glslx::WriterHLSL::Write_Builtin(const ast_builtin *builtin, std::ostream &out_stream) {
    out_stream << g_hlsl_keywords[int(builtin->type)];
}

void glslx::WriterHLSL::Write_Type(const ast_type *type, std::ostream &out_stream) {
    if (type->builtin) {
        Write_Builtin(static_cast<const ast_builtin *>(type), out_stream);
    } else {
        out_stream << static_cast<const ast_struct *>(type)->name;
    }
}

void glslx::WriterHLSL::Write_ArraySize(Span<const ast_constant_expression *const> array_sizes,
                                        std::ostream &out_stream) {
    for (int i = 0; i < int(array_sizes.size()); ++i) {
        out_stream << "[";
        if (array_sizes[i]) {
            Write_Expression(array_sizes[i], false, out_stream);
        }
        out_stream << "]";
    }
}

void glslx::WriterHLSL::Write_Variable(const ast_variable *variable, Span<const ast_layout_qualifier *const> qualifiers,
                                       std::ostream &out_stream, const Bitmask<eOutputFlags> output_flags) {
    // if (variable->is_precise) {
    //     out_stream << "precise ";
    // }

    if (output_flags & eOutputFlags::VarType) {
        Write_Type(variable->base_type, out_stream);
        if (requires_template_argument(variable->base_type)) {
            bool found = false;
            for (const ast_layout_qualifier *q : qualifiers) {
                if (q->initial_value) {
                    continue;
                }
                if (strcmp(q->name, "rgba32f") == 0 || strcmp(q->name, "rgba16f") == 0) {
                    out_stream << "<float4>";
                    found = true;
                } else if (strcmp(q->name, "rgba16") == 0 || strcmp(q->name, "rgb10_a2") == 0 ||
                           strcmp(q->name, "rgba8") == 0) {
                    out_stream << "<unorm float4>";
                    found = true;
                } else if (strcmp(q->name, "rgba16_snorm") == 0 || strcmp(q->name, "rgba8_snorm") == 0) {
                    out_stream << "<snorm float4>";
                    found = true;
                } else if (strcmp(q->name, "rgba32i") == 0 || strcmp(q->name, "rgba16i") == 0 ||
                           strcmp(q->name, "rgba8i") == 0) {
                    out_stream << "<int4>";
                    found = true;
                } else if (strcmp(q->name, "rgba32ui") == 0 || strcmp(q->name, "rgba16ui") == 0 ||
                           strcmp(q->name, "rgb10_a2ui") == 0 || strcmp(q->name, "rgba8ui") == 0) {
                    out_stream << "<uint4>";
                    found = true;
                } else if (strcmp(q->name, "r11f_g11f_b10f") == 0) {
                    out_stream << "<float3>";
                    found = true;
                } else if (strcmp(q->name, "rg32f") == 0 || strcmp(q->name, "rg16f") == 0) {
                    out_stream << "<float2>";
                    found = true;
                } else if (strcmp(q->name, "rg16") == 0 || strcmp(q->name, "rg8") == 0) {
                    out_stream << "<unorm float2>";
                    found = true;
                } else if (strcmp(q->name, "rg16_snorm") == 0 || strcmp(q->name, "rg8_snorm") == 0) {
                    out_stream << "<snorm float2>";
                    found = true;
                } else if (strcmp(q->name, "rg32i") == 0 || strcmp(q->name, "rg16i") == 0 ||
                           strcmp(q->name, "rg8i") == 0) {
                    out_stream << "<int2>";
                    found = true;
                } else if (strcmp(q->name, "rg32ui") == 0 || strcmp(q->name, "rg16ui") == 0 ||
                           strcmp(q->name, "rg8ui") == 0) {
                    out_stream << "<uint2>";
                    found = true;
                } else if (strcmp(q->name, "r32f") == 0 || strcmp(q->name, "r16f") == 0) {
                    out_stream << "<float>";
                    found = true;
                } else if (strcmp(q->name, "r16") == 0 || strcmp(q->name, "r8") == 0) {
                    out_stream << "<unorm float>";
                    found = true;
                } else if (strcmp(q->name, "r16_snorm") == 0 || strcmp(q->name, "r8_snorm") == 0) {
                    out_stream << "<snorm float>";
                    found = true;
                } else if (strcmp(q->name, "r32i") == 0 || strcmp(q->name, "r16i") == 0 ||
                           strcmp(q->name, "r8i") == 0) {
                    out_stream << "<int>";
                    found = true;
                } else if (strcmp(q->name, "r32ui") == 0 || strcmp(q->name, "r16ui") == 0 ||
                           strcmp(q->name, "r8ui") == 0) {
                    out_stream << "<uint>";
                    found = true;
                }
                if (found) {
                    break;
                }
            }
            if (!found) {
                if (is_int_image(variable->base_type)) {
                    out_stream << "<int4>";
                } else if (is_uint_image(variable->base_type)) {
                    out_stream << "<uint4>";
                } else {
                    out_stream << "<float4>";
                }
            }
        }
        out_stream << " ";
    }
    if (output_flags & eOutputFlags::VarName) {
        out_stream << variable->name;
    }
    if (output_flags & eOutputFlags::VarArrSize) {
        if (variable->flags & eVariableFlags::Array) {
            Write_ArraySize(variable->array_sizes, out_stream);
        }
    }
}

void glslx::WriterHLSL::Write_Register(const ast_type *base_type, Span<const ast_layout_qualifier *const> qualifiers,
                                       std::ostream &out_stream) {
    if (qualifiers.empty() || !base_type->builtin) {
        return;
    }

    out_stream << " : register(";

    const auto *type = static_cast<const ast_builtin *>(base_type);
    if (type->type == eKeyword::K_sampler1D || type->type == eKeyword::K_sampler2D ||
        type->type == eKeyword::K_sampler3D || type->type == eKeyword::K_samplerCube ||
        type->type == eKeyword::K_sampler1DArray || type->type == eKeyword::K_sampler2DArray ||
        type->type == eKeyword::K_samplerBuffer || type->type == eKeyword::K_sampler2DMS ||
        type->type == eKeyword::K_sampler2DMSArray || type->type == eKeyword::K_samplerCubeArray ||
        type->type == eKeyword::K_isampler1D || type->type == eKeyword::K_isampler2D ||
        type->type == eKeyword::K_isampler3D || type->type == eKeyword::K_isamplerCube ||
        type->type == eKeyword::K_isampler1DArray || type->type == eKeyword::K_isampler2DArray ||
        type->type == eKeyword::K_isamplerBuffer || type->type == eKeyword::K_isampler2DMS ||
        type->type == eKeyword::K_isampler2DMSArray || type->type == eKeyword::K_isamplerCubeArray ||
        type->type == eKeyword::K_usampler1D || type->type == eKeyword::K_usampler2D ||
        type->type == eKeyword::K_usampler3D || type->type == eKeyword::K_usamplerCube ||
        type->type == eKeyword::K_usampler1DArray || type->type == eKeyword::K_usampler2DArray ||
        type->type == eKeyword::K_usamplerBuffer || type->type == eKeyword::K_usampler2DMS ||
        type->type == eKeyword::K_usampler2DMSArray || type->type == eKeyword::K_usamplerCubeArray ||
        type->type == eKeyword::K_texture1D || type->type == eKeyword::K_itexture1D ||
        type->type == eKeyword::K_utexture1D || type->type == eKeyword::K_texture2D ||
        type->type == eKeyword::K_itexture2D || type->type == eKeyword::K_utexture2D ||
        type->type == eKeyword::K_accelerationStructureEXT || is_shadow_sampler(base_type)) {
        out_stream << 't';
    } else if (type->type == eKeyword::K_sampler) {
        out_stream << 's';
    } else if (type->type == eKeyword::K_image1D || type->type == eKeyword::K_image2D ||
               type->type == eKeyword::K_image3D || type->type == eKeyword::K_image1DArray ||
               type->type == eKeyword::K_image2DArray || type->type == eKeyword::K_imageBuffer ||
               type->type == eKeyword::K_image2DMS || type->type == eKeyword::K_image2DMSArray ||
               type->type == eKeyword::K_iimage1D || type->type == eKeyword::K_iimage2D ||
               type->type == eKeyword::K_iimage3D || type->type == eKeyword::K_iimage1DArray ||
               type->type == eKeyword::K_iimage2DArray || type->type == eKeyword::K_iimageBuffer ||
               type->type == eKeyword::K_iimage2DMS || type->type == eKeyword::K_iimage2DMSArray ||
               type->type == eKeyword::K_uimage1D || type->type == eKeyword::K_uimage2D ||
               type->type == eKeyword::K_uimage3D || type->type == eKeyword::K_uimageCube ||
               type->type == eKeyword::K_uimage1DArray || type->type == eKeyword::K_uimage2DArray ||
               type->type == eKeyword::K_uimageBuffer || type->type == eKeyword::K_uimage2DMS ||
               type->type == eKeyword::K_uimage2DMSArray) {
        out_stream << 'u';
    }

    const auto [binding, set] = get_binding_and_set(qualifiers);

    out_stream << binding;
    out_stream << ", space";
    out_stream << set;
    out_stream << ")";
}

void glslx::WriterHLSL::Write_Storage(const eStorage storage, std::ostream &out_stream) {
    switch (storage) {
    case eStorage::Const:
        out_stream << "const ";
        break;
    case eStorage::In:
        out_stream << "in ";
        break;
    case eStorage::Out:
        out_stream << "out ";
        break;
    case eStorage::Attribute:
        out_stream << "attribute ";
        break;
    // case eStorage::Uniform:
    //     out_stream << "uniform ";
    //     break;
    case eStorage::Varying:
        out_stream << "varying ";
        break;
    case eStorage::Buffer:
        out_stream << "buffer ";
        break;
    case eStorage::Shared:
        out_stream << "groupshared ";
        break;
    case eStorage::RayPayload:
        out_stream << "rayPayloadEXT ";
        break;
    case eStorage::RayPayloadIn:
        out_stream << "rayPayloadInEXT ";
        break;
    case eStorage::HitAttribute:
        out_stream << "hitAttributeEXT ";
        break;
    case eStorage::CallableData:
        out_stream << "callableDataEXT ";
        break;
    case eStorage::CallableDataIn:
        out_stream << "callableDataInEXT ";
        break;
    default:
        // TODO: report error
        break;
    }
}

void glslx::WriterHLSL::Write_AuxStorage(eAuxStorage aux_storage, std::ostream &out_stream) {
    switch (aux_storage) {
    case eAuxStorage::Centroid:
        out_stream << "centroid ";
        break;
    case eAuxStorage::Sample:
        out_stream << "sample ";
        break;
    case eAuxStorage::Patch:
        out_stream << "patch ";
        break;
    default:
        // TODO: report error
        break;
    }
}

void glslx::WriterHLSL::Write_ParameterQualifiers(const Bitmask<eParamQualifier> qualifiers, std::ostream &out_stream) {
    if (qualifiers & eParamQualifier::Const) {
        out_stream << "const ";
    }
    if (qualifiers & eParamQualifier::In) {
        out_stream << "in ";
    }
    if (qualifiers & eParamQualifier::Out) {
        out_stream << "out ";
    }
    if (qualifiers & eParamQualifier::Inout) {
        out_stream << "inout ";
    }
}

void glslx::WriterHLSL::Write_Memory(Bitmask<eMemory> memory, std::ostream &out_stream) {
    if (memory & eMemory::Coherent) {
        out_stream << "coherent ";
    }
    if (memory & eMemory::Volatile) {
        out_stream << "volatile ";
    }
    if (memory & eMemory::Restrict) {
        out_stream << "restrict ";
    }
    // if (memory & eMemory::Readonly) {
    //     out_stream << "readonly ";
    // }
    // if (memory & eMemory::Writeonly) {
    //     out_stream << "writeonly ";
    // }
}

void glslx::WriterHLSL::Write_Precision(ePrecision precision, std::ostream &out_stream) {
    switch (precision) {
    case ePrecision::Lowp:
        out_stream << "lowp ";
        break;
    case ePrecision::Mediump:
        out_stream << "mediump ";
        break;
    case ePrecision::Highp:
        out_stream << "highp ";
        break;
    default:
        // TODO: report error
        break;
    }
}

void glslx::WriterHLSL::Write_GlobalVariable(const ast_global_variable *variable, std::ostream &out_stream) {
    if (variable->storage == eStorage::Const) {
        out_stream << "static ";
    }

    Write_Storage(variable->storage, out_stream);
    Write_AuxStorage(variable->aux_storage, out_stream);
    // Write_Memory(variable->memory_flags, out_stream);
    Write_Precision(variable->precision, out_stream);

    if (variable->flags & eVariableFlags::Invariant) {
        out_stream << "invariant ";
    }

    switch (variable->interpolation) {
    case eInterpolation::Smooth:
        out_stream << "smooth ";
        break;
    case eInterpolation::Flat:
        out_stream << "flat ";
        break;
    case eInterpolation::Noperspective:
        out_stream << "noperspective ";
        break;
    default:
        // TODO: report error
        break;
    }

    Write_Variable(variable, variable->layout_qualifiers, out_stream);
    Write_Register(variable->base_type, variable->layout_qualifiers, out_stream);

    if (variable->initial_value) {
        out_stream << " = ";
        Write_Expression(variable->initial_value, false, out_stream);
    }

    out_stream << ";\n";

    if (is_combined_texturesampler(variable->base_type)) {
        if (is_shadow_sampler(variable->base_type)) {
            out_stream << "SamplerComparisonState ";
        } else {
            out_stream << "SamplerState ";
        }
        out_stream << variable->name;
        out_stream << "_sampler";
        if (variable->flags & eVariableFlags::Array) {
            for (int i = 0; i < int(variable->array_sizes.size()); ++i) {
                out_stream << "[";
                if (variable->array_sizes[i]) {
                    Write_Expression(variable->array_sizes[i], false, out_stream);
                }
                out_stream << "]";
            }
        }

        ast_builtin temp_type(eKeyword::K_sampler);
        Write_Register(&temp_type, variable->layout_qualifiers, out_stream);

        out_stream << ";\n";
    }
}

void glslx::WriterHLSL::Write_VariableIdentifier(const ast_variable_identifier *expression, std::ostream &out_stream) {
    Write_Variable(expression->variable, {}, out_stream, eOutputFlags::VarName /* name_only */);
}

void glslx::WriterHLSL::Write_FieldOrSwizzle(const ast_field_or_swizzle *expression, std::ostream &out_stream) {
    Write_Expression(expression->operand, false, out_stream);
    out_stream << ".";
    if (expression->field) {
        Write_VariableIdentifier(expression->field, out_stream);
    } else {
        out_stream << expression->name;
    }
}

void glslx::WriterHLSL::Write_ArraySubscript(const ast_array_subscript *expression, std::ostream &out_stream) {
    if (expression->operand->type == eExprType::VariableIdentifier) {
        const auto *var = static_cast<const ast_variable_identifier *>(expression->operand);
        auto it = std::find_if(byteaddress_bufs_.begin(), byteaddress_bufs_.end(), [var](const byteaddress_buf_t &buf) {
            return strcmp(buf.name, var->variable->name) == 0;
        });
        if (it != byteaddress_bufs_.end()) {
            out_stream << "__load_" << it->name;
            out_stream << "(";
            Write_Expression(expression->index, false, out_stream);
            out_stream << ")";
            return;
        }
    }
    Write_Expression(expression->operand, false, out_stream);
    out_stream << "[";
    Write_Expression(expression->index, false, out_stream);
    out_stream << "]";
}

void glslx::WriterHLSL::Write_FunctionCall(const ast_function_call *expression, std::ostream &out_stream) {
    bool skip_call = false;
    bool is_atomic = false;

    { // Capture atomic operation
        const auto *p_find = g_atomic_functions.Find(expression->name);
        if (p_find) {
            is_atomic = true;
            if (!atomic_operations_.empty()) {
                out_stream << atomic_operations_.front().var_name;
                atomic_operations_.erase(atomic_operations_.begin());
                return;
            }
        }
    }

    if (!skip_call) {
        auto *p_find = g_hlsl_function_mapping.Find(expression->name);
        if (p_find) {
            out_stream << *p_find;
        } else {
            out_stream << expression->name;
        }
    }
    out_stream << "(";
    for (int i = 0; i < int(expression->parameters.size()); ++i) {
        Write_Expression(expression->parameters[i], false, out_stream);
        if (expression->parameters[i]->type == eExprType::VariableIdentifier) {
            const auto *tex_arg = static_cast<ast_variable_identifier *>(expression->parameters[i]);
            if (is_combined_texturesampler(tex_arg->variable->base_type)) {
                out_stream << ", ";
                out_stream << tex_arg->variable->name;
                out_stream << "_sampler";
            }
        } else if (expression->parameters[i]->type == eExprType::ArraySubscript) {
            const auto *subscript = static_cast<ast_array_subscript *>(expression->parameters[i]);
            if (subscript->operand->type == eExprType::VariableIdentifier) {
                const auto *tex_arg = static_cast<ast_variable_identifier *>(subscript->operand);
                if (is_combined_texturesampler(tex_arg->variable->base_type)) {
                    out_stream << ", ";
                    out_stream << tex_arg->variable->name;
                    out_stream << "_sampler";
                    out_stream << "[";
                    Write_Expression(subscript->index, false, out_stream);
                    out_stream << "]";
                }
            }
        }
        if (i != int(expression->parameters.size()) - 1) {
            out_stream << ", ";
        }
    }
    if (is_atomic) {
        out_stream << ", " << var_to_init_;
    }
    out_stream << ")";
}

void glslx::WriterHLSL::Write_ConstructorCall(const ast_constructor_call *expression, std::ostream &out_stream) {
    const bool combined_texturesampler = is_combined_texturesampler(expression->type);
    if (!combined_texturesampler) {
        if (!expression->type->builtin) {
            out_stream << "(";
        }
        Write_Type(expression->type, out_stream);
        if (!expression->type->builtin) {
            out_stream << ")";
        }
        out_stream << "(";
    }
    if (expression->parameters.size() == 1) {
        if (expression->type->builtin) {
            const auto *expr_type = static_cast<const ast_builtin *>(expression->type);
            int array_dims = 0;
            const ast_type *res_type = Evaluate_ExpressionResultType(tu_, expression->parameters[0], array_dims);
            if (res_type) {
                const int vec_size = get_vector_size(expr_type->type);
                if (is_scalar_type(res_type) && vec_size > 1) {
                    Write_Expression(expression->parameters[0], false, out_stream);
                    out_stream << ".";
                    for (int i = 0; i < vec_size; ++i) {
                        out_stream << "x";
                    }
                    out_stream << ")";
                    return;
                }
            }
        }
    }
    for (int i = 0; i < int(expression->parameters.size()); ++i) {
        Write_Expression(expression->parameters[i], false, out_stream);
        if (i != int(expression->parameters.size()) - 1) {
            out_stream << ", ";
        }
    }
    if (!combined_texturesampler) {
        out_stream << ")";
    }
}

void glslx::WriterHLSL::Write_PostIncrement(const ast_post_increment_expression *expression, std::ostream &out_stream) {
    Write_Expression(expression->operand, false, out_stream);
    out_stream << "++";
}

void glslx::WriterHLSL::Write_PostDecrement(const ast_post_decrement_expression *expression, std::ostream &out_stream) {
    Write_Expression(expression->operand, false, out_stream);
    out_stream << "--";
}

void glslx::WriterHLSL::Write_UnaryPlus(const ast_unary_plus_expression *expression, std::ostream &out_stream) {
    out_stream << "+";
    Write_Expression(expression->operand, false, out_stream);
}

void glslx::WriterHLSL::Write_UnaryMinus(const ast_unary_minus_expression *expression, std::ostream &out_stream) {
    out_stream << "(-";
    Write_Expression(expression->operand, false, out_stream);
    out_stream << ")";
}

void glslx::WriterHLSL::Write_UnaryBitNot(const ast_unary_bit_not_expression *expression, std::ostream &out_stream) {
    out_stream << "~";
    Write_Expression(expression->operand, false, out_stream);
}

void glslx::WriterHLSL::Write_UnaryLogicalNot(const ast_unary_logical_not_expression *expression,
                                              std::ostream &out_stream) {
    out_stream << "!";
    Write_Expression(expression->operand, false, out_stream);
}

void glslx::WriterHLSL::Write_PrefixIncrement(const ast_prefix_increment_expression *expression,
                                              std::ostream &out_stream) {
    out_stream << "++";
    Write_Expression(expression->operand, false, out_stream);
}

void glslx::WriterHLSL::Write_PrefixDecrement(const ast_prefix_decrement_expression *expression,
                                              std::ostream &out_stream) {
    out_stream << "--";
    Write_Expression(expression->operand, false, out_stream);
}

void glslx::WriterHLSL::Write_Assignment(const ast_assignment_expression *expression, std::ostream &out_stream) {
    global_vector<access_index_t> indices;
    const auto [buf_index, buf_offset] = Find_BufferAccessExpression(expression->operand1, 0, indices);
    if (buf_index != -1) {
        int array_dims = 0;
        const ast_type *var_type = Evaluate_ExpressionResultType(tu_, expression->operand2, array_dims);
        Write_Type(var_type, out_stream);
        out_stream << " __temp" + std::to_string(temp_var_index_) + " = ";
        Write_Expression(expression->operand2, false, out_stream);
        out_stream << ";\n";
        Write_Tabs(out_stream);
        out_stream << "uint __offset" + std::to_string(temp_var_index_) + " = ";
        for (int i = 0; i < int(indices.size()); ++i) {
            Write_Expression(indices[i].index, false, out_stream);
            out_stream << " * " << indices[i].multiplier;
            out_stream << " + ";
        }
        out_stream << buf_offset;
        out_stream << ";\n";

        const byteaddress_buf_t &buf = byteaddress_bufs_[buf_index];
        Write_ByteaddressBufStores(buf, 0, "__temp" + std::to_string(temp_var_index_), 0, var_type, out_stream);

        // erase the last ";\n"
        out_stream.seekp(-2, std::ios::cur);

        ++temp_var_index_;
        return;
    }

    Write_Expression(expression->operand1, false, out_stream);
    out_stream << " " << g_operators[int(expression->oper)].string << " ";
    Write_Expression(expression->operand2, false, out_stream);
}

void glslx::WriterHLSL::Write_Sequence(const ast_sequence_expression *expression, std::ostream &out_stream) {
    out_stream << "(";
    Write_Expression(expression->operand1, false, out_stream);
    out_stream << ", ";
    Write_Expression(expression->operand2, false, out_stream);
    out_stream << ")";
}

void glslx::WriterHLSL::Write_Operation(const ast_operation_expression *expression, std::ostream &out_stream) {
    out_stream << "(";
    Write_Expression(expression->operand1, true, out_stream);
    out_stream << " " << g_operators[int(expression->oper)].string << " ";
    Write_Expression(expression->operand2, true, out_stream);
    out_stream << ")";
}

void glslx::WriterHLSL::Write_Ternary(const ast_ternary_expression *expression, std::ostream &out_stream) {
    out_stream << "(";
    Write_Expression(expression->condition, true, out_stream);
    out_stream << " ? ";
    Write_Expression(expression->on_true, true, out_stream);
    out_stream << " : ";
    Write_Expression(expression->on_false, true, out_stream);
    out_stream << ")";
}

void glslx::WriterHLSL::Write_Structure(const ast_struct *structure, std::ostream &out_stream) {
    out_stream << "struct ";
    if (structure->name) {
        out_stream << structure->name << " ";
    }
    out_stream << "{\n";
    ++nest_level_;
    for (int i = 0; i < int(structure->fields.size()); ++i) {
        Write_Tabs(out_stream);
        Write_Variable(structure->fields[i], {}, out_stream);
        out_stream << ";\n";
    }
    --nest_level_;
    out_stream << "};\n";
}

void glslx::WriterHLSL::Write_InterfaceBlock(const ast_interface_block *block, std::ostream &out_stream) {
    enum class eBlockKind { Unknown, ROBuffer, RWBuffer, PushConstant, UniformBuffer } block_kind = eBlockKind::Unknown;

    if (block->storage == eStorage::Buffer) {
        if (block->memory_flags & eMemory::Readonly) {
            block_kind = eBlockKind::ROBuffer;
        } else {
            block_kind = eBlockKind::RWBuffer;
        }
    } else if (block->storage == eStorage::Uniform) {
        block_kind = eBlockKind::UniformBuffer;
        for (int i = 0; i < int(block->layout_qualifiers.size()); ++i) {
            if (strcmp(block->layout_qualifiers[i]->name, "push_constant") == 0) {
                block_kind = eBlockKind::PushConstant;
                break;
            }
        }
    }

    if (block_kind == eBlockKind::ROBuffer || block_kind == eBlockKind::RWBuffer) {
        const char *name = block->name;
        if (block->fields.size() == 1) {
            name = block->fields[0]->name;
        }

        if (block_kind == eBlockKind::ROBuffer) {
            out_stream << "ByteAddressBuffer " << name;
        } else {
            out_stream << "RWByteAddressBuffer " << name;
        }
        out_stream << " : register(";
        if (block_kind == eBlockKind::ROBuffer) {
            out_stream << "t";
        } else {
            out_stream << "u";
        }

        const auto [binding, set] = get_binding_and_set(block->layout_qualifiers);

        out_stream << binding;
        out_stream << ", space";
        out_stream << set;
        out_stream << ");\n";

        int total_size = 0;
        for (const ast_variable *f : block->fields) {
            total_size += get_variable_size(f, 0);
        }
        // assert(total_size % 4 == 0);

        byteaddress_buf_t &new_buf = byteaddress_bufs_.emplace_back();
        new_buf.name = name;
        new_buf.size = total_size;

        // write accessor functions
        int offset = 0;
        for (const ast_variable *f : block->fields) {
            Write_Type(f->base_type, out_stream);
            out_stream << " __load_" << name << "(int index) {\n";
            ++nest_level_;
            Write_Tabs(out_stream);
            Write_Type(f->base_type, out_stream);
            out_stream << " ret;\n";
            const int size = Write_ByteaddressBufLoads(new_buf, offset, "ret", 0, f, out_stream);
            Write_Tabs(out_stream);
            out_stream << "return ret;\n";
            out_stream << "}\n";
            --nest_level_;
            /*if (block_kind == eBlockKind::RWBuffer) {
                out_stream << "void __store_" << name << "(int index, ";
                Write_Type(f->base_type, out_stream);
                out_stream << " val) {\n";
                ++nest_level_;
                const int size2 = Write_ByteaddressBufStores(new_buf, offset, "val", 0, f, out_stream);
                assert(size2 == size);
                --nest_level_;
                out_stream << "}\n";
            }*/
            offset += size;
        }
        return;
    } else if (block_kind == eBlockKind::PushConstant || block_kind == eBlockKind::UniformBuffer) {
        out_stream << "cbuffer ";
    }

    // Write_Layout(block->layout_qualifiers, out_stream);
    // Write_Memory(block->memory_flags, out_stream);
    // Write_Storage(block->storage, out_stream);
    if (!block->name) {
        out_stream << ";\n";
        return;
    }
    out_stream << block->name;
    if (block_kind == eBlockKind::UniformBuffer) {
        const auto [binding, set] = get_binding_and_set(block->layout_qualifiers);
        out_stream << " : register(b" << binding << ")";
    }
    out_stream << " {\n";
    ++nest_level_;
    for (int i = 0; i < int(block->fields.size()); ++i) {
        Write_Tabs(out_stream);
        Write_Variable(block->fields[i], {}, out_stream);
        if (i == 0 && (block_kind == eBlockKind::PushConstant || block_kind == eBlockKind::UniformBuffer)) {
            out_stream << " : packoffset(c0)";
        }
        out_stream << ";\n";
    }
    --nest_level_;
    out_stream << "};\n";
}

void glslx::WriterHLSL::Write(TrUnit *tu, std::ostream &out_stream) {
    tu_ = tu;
    ast_function *main_function = nullptr;
    auto *p_find = tu->functions_by_name.Find("main");
    if (p_find) {
        assert(p_find->size() == 1);
        main_function = (*p_find)[0];
    }
    if (main_function) {
        // TODO: Remove unused
        out_stream
            << "uint4 texelFetch(Texture2D<uint4> t, int2 P, int lod) {\n"
               "    return t.Load(int3(P, lod));\n"
               "}\n"
               "float4 texelFetch(Texture2D<float4> t, int2 P, int lod) {\n"
               "    return t.Load(int3(P, lod));\n"
               "}\n"
               "float4 texelFetch(Texture2D<float4> t, SamplerState s, int2 P, int lod) {\n"
               "    return t.Load(int3(P, lod));\n"
               "}\n"
               "float4 texelFetch(Texture2DArray<float4> t, SamplerState s, int3 P, int lod) {\n"
               "    return t.Load(int4(P, lod));\n"
               "}\n"
               "float4 texelFetch(Texture3D<float4> t, SamplerState s, int3 P, int lod) {\n"
               "    return t.Load(int4(P, lod));\n"
               "}\n"
               "float4 textureLod(Texture2D<float4> t, SamplerState s, float2 P, float lod) {\n"
               "    return t.SampleLevel(s, P, lod);\n"
               "}\n"
               "float4 textureLod(Texture3D<float4> t, SamplerState s, float3 P, float lod) {\n"
               "    return t.SampleLevel(s, P, lod);\n"
               "}\n"
               "float4 textureLod(Texture2DArray<float4> t, SamplerState s, float3 P, float lod) {\n"
               "    return t.SampleLevel(s, P, lod);\n"
               "}\n"
               "float4 textureLodOffset(Texture2D<float4> t, SamplerState s, float2 P, float lod, int2 offset) {\n"
               "    return t.SampleLevel(s, P, lod, offset);\n"
               "}\n"
               "int2 textureSize(Texture2D<float4> t, int lod) {\n"
               "    uint2 ret;\n"
               "    uint NumberOfLevels;\n"
               "    t.GetDimensions(lod, ret.x, ret.y, NumberOfLevels);\n"
               "    return ret;\n"
               "}\n"
               "float4 imageLoad(RWTexture2D<float> image, int2 P) { return float4(image[P], 0, 0, 0); }\n"
               "float4 imageLoad(RWTexture2D<float2> image, int2 P) { return float4(image[P], 0, 0); }\n"
               "float4 imageLoad(RWTexture2D<float4> image, int2 P) { return image[P]; }\n"
               "int4 imageLoad(RWTexture2D<int> image, int2 P) { return int4(image[P], 0, 0, 0); }\n"
               "int4 imageLoad(RWTexture2D<int2> image, int2 P) { return int4(image[P], 0, 0); }\n"
               "int4 imageLoad(RWTexture2D<int4> image, int2 P) { return image[P]; }\n"
               "uint4 imageLoad(RWTexture2D<uint> image, int2 P) { return uint4(image[P], 0, 0, 0); }\n"
               "uint4 imageLoad(RWTexture2D<uint2> image, int2 P) { return uint4(image[P], 0, 0); }\n"
               "uint4 imageLoad(RWTexture2D<uint4> image, int2 P) { return image[P]; }\n"
               "void imageStore(RWTexture2D<float> image, int2 P, float4 data) { image[P] = data.x; }\n"
               "void imageStore(RWTexture2D<float2> image, int2 P, float4 data) { image[P] = data.xy; }\n"
               "void imageStore(RWTexture2D<float4> image, int2 P, float4 data) { image[P] = data; }\n"
               "void imageStore(RWTexture2D<int> image, int2 P, int4 data) { image[P] = data.x; }\n"
               "void imageStore(RWTexture2D<int2> image, int2 P, int4 data) { image[P] = data.xy; }\n"
               "void imageStore(RWTexture2D<int4> image, int2 P, int4 data) { image[P] = data; }\n"
               "void imageStore(RWTexture2D<uint> image, int2 P, uint4 data) { image[P] = data.x; }\n"
               "void imageStore(RWTexture2D<uint2> image, int2 P, uint4 data) { image[P] = data.xy; }\n"
               "void imageStore(RWTexture2D<uint4> image, int2 P, uint4 data) { image[P] = data; }\n";
        if (tu->type == eTrUnitType::Compute) {
            out_stream
                << "static const uint gl_RayQueryCandidateIntersectionTriangleEXT = CANDIDATE_NON_OPAQUE_TRIANGLE;\n"
                   "static const uint gl_RayQueryCandidateIntersectionAABBEXT = CANDIDATE_PROCEDURAL_PRIMITIVE;\n"
                   "static const uint gl_RayQueryCommittedIntersectionNoneEXT = COMMITTED_NOTHING;\n"
                   "static const uint gl_RayQueryCommittedIntersectionTriangleEXT = COMMITTED_TRIANGLE_HIT;\n"
                   "static const uint gl_RayQueryCommittedIntersectionGeneratedEXT = "
                   "COMMITTED_PROCEDURAL_PRIMITIVE_HIT;\n"
                   "void rayQueryInitializeEXT(RayQuery<RAY_FLAG_NONE> rayQuery,\n"
                   "                           RaytracingAccelerationStructure topLevel,\n"
                   "                           uint rayFlags, uint cullMask, float3 origin, float tMin,\n"
                   "                           float3 direction, float tMax) {\n"
                   "    RayDesc desc = {origin, tMin, direction, tMax};\n"
                   "    rayQuery.TraceRayInline(topLevel, rayFlags, cullMask, desc);\n"
                   "}\n"
                   "bool rayQueryProceedEXT(RayQuery<RAY_FLAG_NONE> q) {\n"
                   "    return q.Proceed();\n"
                   "}\n"
                   "uint rayQueryGetIntersectionTypeEXT(RayQuery<RAY_FLAG_NONE> q, bool committed) {\n"
                   "    if (committed) {\n"
                   "        return q.CommittedStatus();\n"
                   "    } else {\n"
                   "        return q.CandidateType();\n"
                   "    }\n"
                   "}\n"
                   "void rayQueryConfirmIntersectionEXT(RayQuery<RAY_FLAG_NONE> q) {\n"
                   "    q.CommitNonOpaqueTriangleHit();\n"
                   "}\n"
                   "int rayQueryGetIntersectionInstanceCustomIndexEXT(RayQuery<RAY_FLAG_NONE> q, bool committed) {\n"
                   "    if (committed) {\n"
                   "        return q.CommittedInstanceID();\n"
                   "    } else {\n"
                   "        return q.CandidateInstanceID();\n"
                   "    }\n"
                   "}\n"
                   "int rayQueryGetIntersectionInstanceIdEXT(RayQuery<RAY_FLAG_NONE> q, bool committed) {\n"
                   "    if (committed) {\n"
                   "        return q.CommittedInstanceIndex();\n"
                   "    } else {\n"
                   "        return q.CandidateInstanceIndex();\n"
                   "    }\n"
                   "}\n"
                   "int rayQueryGetIntersectionPrimitiveIndexEXT(RayQuery<RAY_FLAG_NONE> q, bool committed) {\n"
                   "    if (committed) {\n"
                   "        return q.CommittedPrimitiveIndex();\n"
                   "    } else {\n"
                   "        return q.CandidatePrimitiveIndex();\n"
                   "    }\n"
                   "}\n"
                   "bool rayQueryGetIntersectionFrontFaceEXT(RayQuery<RAY_FLAG_NONE> q, bool committed) {\n"
                   "    if (committed) {\n"
                   "        return q.CommittedTriangleFrontFace();\n"
                   "    } else {\n"
                   "        return q.CandidateTriangleFrontFace();\n"
                   "    }\n"
                   "}\n"
                   "float2 rayQueryGetIntersectionBarycentricsEXT(RayQuery<RAY_FLAG_NONE> q, bool committed) {\n"
                   "    if (committed) {\n"
                   "        return q.CommittedTriangleBarycentrics();\n"
                   "    } else {\n"
                   "        return q.CandidateTriangleBarycentrics();\n"
                   "    }\n"
                   "}\n"
                   "float rayQueryGetIntersectionTEXT(RayQuery<RAY_FLAG_NONE> q, bool committed) {\n"
                   "    if (committed) {\n"
                   "        return q.CommittedRayT();\n"
                   "    } else {\n"
                   "        return q.CandidateTriangleRayT();\n"
                   "    }\n"
                   "}\n";
        }
    }

    for (int i = 0; i < int(tu->globals.size()); ++i) {
        if (!tu->globals[i]->base_type->builtin) {
            continue;
        }
        if (!(tu->globals[i]->flags & eVariableFlags::Hidden) || config_.write_hidden) {
            Write_GlobalVariable(tu->globals[i], out_stream);
        }
    }
    for (int i = 0; i < int(tu->structures.size()); ++i) {
        Write_Structure(tu->structures[i], out_stream);
    }
    if (main_function && tu->type == eTrUnitType::Compute) {
        out_stream << "static uint3 gl_WorkGroupID;\n";
        out_stream << "static uint3 gl_LocalInvocationID;\n";
        out_stream << "static uint3 gl_GlobalInvocationID;\n";
        out_stream << "static uint gl_LocalInvocationIndex;\n";
        out_stream << "static uint gl_SubgroupSize;\n";
        out_stream << "static uint gl_SubgroupInvocationID;\n";
        out_stream << "struct GLSLX_Input {\n";
        out_stream << "    uint3 gl_WorkGroupID : SV_GroupID;\n";
        out_stream << "    uint3 gl_LocalInvocationID : SV_GroupThreadID;\n";
        out_stream << "    uint3 gl_GlobalInvocationID : SV_DispatchThreadID;\n";
        out_stream << "    uint gl_LocalInvocationIndex : SV_GroupIndex;\n";
        out_stream << "};\n";
    }
    for (int i = 0; i < int(tu->interface_blocks.size()); ++i) {
        const ast_interface_block *block = tu->interface_blocks[i];

        bool skip = false;
        for (int j = 0; j < int(block->layout_qualifiers.size()); ++j) {
            int assign_index = -1;
            if (strcmp(block->layout_qualifiers[j]->name, "local_size_x") == 0) {
                assign_index = 0;
            } else if (strcmp(block->layout_qualifiers[j]->name, "local_size_y") == 0) {
                assign_index = 1;
            } else if (strcmp(block->layout_qualifiers[j]->name, "local_size_z") == 0) {
                assign_index = 2;
            }

            if (assign_index != -1) {
                const ast_constant_expression *value = block->layout_qualifiers[j]->initial_value;
                if (value->type == eExprType::IntConstant) {
                    compute_group_sizes_[assign_index] = static_cast<const ast_int_constant *>(value)->value;
                } else if (value->type == eExprType::UIntConstant) {
                    compute_group_sizes_[assign_index] = static_cast<const ast_uint_constant *>(value)->value;
                }
                skip = true;
            }
        }

        if (!skip) {
            Write_InterfaceBlock(block, out_stream);
        }
    }
    for (int i = 0; i < int(tu->globals.size()); ++i) {
        if (tu->globals[i]->base_type->builtin) {
            continue;
        }
        if (!(tu->globals[i]->flags & eVariableFlags::Hidden) || config_.write_hidden) {
            Write_GlobalVariable(tu->globals[i], out_stream);
        }
    }

    for (const ast_function *f : tu->functions) {
        if (tu->type == eTrUnitType::Compute && f == main_function) {
            out_stream << "[numthreads(";
            out_stream << compute_group_sizes_[0] << ", ";
            out_stream << compute_group_sizes_[1] << ", ";
            out_stream << compute_group_sizes_[2] << ")]\n";
        }
        Write_Function(f, out_stream);
    }
}

std::pair<int, int> glslx::WriterHLSL::Find_BufferAccessExpression(const ast_expression *expression, const int offset,
                                                                   global_vector<access_index_t> &out_indices) {
    if (expression->type == eExprType::VariableIdentifier) {
        const auto *var = static_cast<const ast_variable_identifier *>(expression);
        auto it = std::find_if(byteaddress_bufs_.begin(), byteaddress_bufs_.end(), [var](const byteaddress_buf_t &buf) {
            return strcmp(var->variable->name, buf.name) == 0;
        });
        if (it == byteaddress_bufs_.end()) {
            return {-1, -1};
        }
        return {int(std::distance(byteaddress_bufs_.begin(), it)), offset};
    } else if (expression->type == eExprType::ArraySubscript) {
        const auto *subscript = static_cast<const ast_array_subscript *>(expression);
        int array_dims = 0;
        const ast_type *operand_type = Evaluate_ExpressionResultType(tu_, subscript->operand, array_dims);
        const int operand_size = Calc_TypeSize(operand_type);
        out_indices.push_back({subscript->index, operand_size});
        return Find_BufferAccessExpression(subscript->operand, offset, out_indices);
    } else if (expression->type == eExprType::FieldOrSwizzle) {
        const auto *field = static_cast<const ast_field_or_swizzle *>(expression);
        int array_dims = 0;
        const ast_type *operand_type = Evaluate_ExpressionResultType(tu_, field->operand, array_dims);
        const int field_offset = Calc_FieldOffset(operand_type, field->name);
        return Find_BufferAccessExpression(field->operand, field_offset, out_indices);
    }
    return {-1, -1};
}

void glslx::WriterHLSL::Find_AtomicOperations(const ast_expression *expression,
                                              global_vector<atomic_operation_t> &out_operations) {
    switch (expression->type) {
    case eExprType::FunctionCall: {
        const auto *call = static_cast<const ast_function_call *>(expression);
        const auto *p_find = g_atomic_functions.Find(call->name);
        if (p_find) {
            out_operations.push_back({expression});
        }
    } break;
    case eExprType::Assign:
        return Find_AtomicOperations(static_cast<const ast_assignment_expression *>(expression)->operand2,
                                     out_operations);
    case eExprType::Sequence: {
        const auto *sequence = static_cast<const ast_sequence_expression *>(expression);
        Find_AtomicOperations(sequence->operand1, out_operations);
        Find_AtomicOperations(sequence->operand2, out_operations);
    } break;
    case eExprType::Operation: {
        const auto *operation = static_cast<const ast_operation_expression *>(expression);
        Find_AtomicOperations(operation->operand1, out_operations);
        Find_AtomicOperations(operation->operand2, out_operations);
        break;
    }
    case eExprType::Ternary: {
        const auto *ternary = static_cast<const ast_ternary_expression *>(expression);
        Find_AtomicOperations(ternary->condition, out_operations);
        Find_AtomicOperations(ternary->on_true, out_operations);
        Find_AtomicOperations(ternary->on_false, out_operations);
    } break;
    case eExprType::ArraySpecifier: {
        const ast_array_specifier *arr_specifier = static_cast<const ast_array_specifier *>(expression);
        for (int i = 0; i < int(arr_specifier->expressions.size()); ++i) {
            Find_AtomicOperations(arr_specifier->expressions[i], out_operations);
        }
    } break;
    default:
        // TODO: report error
        break;
    }
}

void glslx::WriterHLSL::Process_AtomicOperations(const ast_expression *expression, std::ostream &out_stream) {
    global_vector<atomic_operation_t> atomic_operations;
    Find_AtomicOperations(expression, atomic_operations);

    for (int i = 0; i < int(atomic_operations.size()); ++i) {
        int array_dims = 0;
        const ast_type *temp_type = Evaluate_ExpressionResultType(tu_, atomic_operations[i].expr, array_dims);
        Write_Type(temp_type, out_stream);
        atomic_operations[i].var_name = "__temp" + std::to_string(temp_var_index_++ + i);
        out_stream << " " << atomic_operations[i].var_name << ";\n";
        Write_Tabs(out_stream);
        assert(atomic_operations[i].expr->type == eExprType::FunctionCall);
        const auto *call = static_cast<const ast_function_call *>(atomic_operations[i].expr);
        if (call->parameters[0]->type == eExprType::ArraySubscript) {
            const auto *subscript = static_cast<const ast_array_subscript *>(call->parameters[0]);
            assert(subscript->operand->type == eExprType::VariableIdentifier);
            const auto *var = static_cast<const ast_variable_identifier *>(subscript->operand);
            auto buf_it =
                std::find_if(byteaddress_bufs_.begin(), byteaddress_bufs_.end(), [var](const byteaddress_buf_t &buf) {
                    return strcmp(buf.name, var->variable->name) == 0;
                });
            if (buf_it != byteaddress_bufs_.end()) {
                const char *func_name = call->name;
                auto *p_find = g_hlsl_function_mapping.Find(call->name);
                if (p_find) {
                    func_name = p_find->c_str();
                }
                if (strcmp(func_name, "InterlockedCompareExchange") == 0 && temp_type->builtin &&
                    static_cast<const ast_builtin *>(temp_type)->type == eKeyword::K_uint64_t) {
                    func_name = "InterlockedCompareExchange64";
                }
                out_stream << var->variable->name << "." << func_name << "(";
                out_stream << buf_it->size << " * ";
                Write_Expression(subscript->index, false, out_stream);
                out_stream << ", ";
                for (int j = 1; j < int(call->parameters.size()); ++j) {
                    Write_Expression(call->parameters[j], false, out_stream);
                    out_stream << ", ";
                }
                out_stream << atomic_operations[i].var_name;
                out_stream << ")";
            } else {
                var_to_init_ = atomic_operations[i].var_name;
                Write_Expression(atomic_operations[i].expr, false, out_stream);
            }
        } else {
            var_to_init_ = atomic_operations[i].var_name;
            Write_Expression(atomic_operations[i].expr, false, out_stream);
        }
        out_stream << ";\n";
        Write_Tabs(out_stream);
    }

    atomic_operations_.insert(atomic_operations_.end(), atomic_operations.begin(), atomic_operations.end());
}

int glslx::WriterHLSL::Calc_FieldOffset(const ast_type *type, const char *field_name) {
    if (type->builtin) {
        // TODO: fix this
        if (field_name[0] == 'x') {
            return 0;
        } else if (field_name[1] == 'y') {
            return 4;
        } else if (field_name[2] == 'z') {
            return 8;
        } else {
            return 12;
        }
    } else {
        const auto *struct_type = static_cast<const ast_struct *>(type);
        int offset = 0;
        for (const ast_variable *v : struct_type->fields) {
            if (strcmp(v->name, field_name) == 0) {
                return offset;
            }
            offset += Calc_VariableSize(v);
        }
        return -1;
    }
}

int glslx::WriterHLSL::Calc_TypeSize(const ast_type *type) {
    int total_size = 0;
    if (type->builtin) {
        const auto *builtin_type = static_cast<const ast_builtin *>(type);
        switch (builtin_type->type) {
        case eKeyword::K_int:
        case eKeyword::K_uint:
        case eKeyword::K_float:
            total_size = 4;
            break;
        case eKeyword::K_int64_t:
        case eKeyword::K_uint64_t:
        case eKeyword::K_double:
            total_size = 8;
            break;
        case eKeyword::K_float16_t:
            total_size = 2;
            break;
        case eKeyword::K_ivec2:
        case eKeyword::K_uvec2:
        case eKeyword::K_vec2:
            total_size = 8;
            break;
        case eKeyword::K_ivec4:
        case eKeyword::K_uvec4:
        case eKeyword::K_vec4:
            total_size = 16;
            break;
        case eKeyword::K_mat4x4:
        case eKeyword::K_mat4:
            total_size = 16 * 4;
            break;
        default:
            // TODO: report error
            break;
        }
    } else {
        const auto *struct_type = static_cast<const ast_struct *>(type);
        for (const ast_variable *v : struct_type->fields) {
            total_size += Calc_VariableSize(v);
        }
    }
    return total_size;
}

int glslx::WriterHLSL::Calc_VariableSize(const ast_variable *v) {
    int total_size = Calc_TypeSize(v->base_type);
    if (v->flags & eVariableFlags::Array) {
        for (int i = 0; i < int(v->array_sizes.size()) && v->array_sizes[i]; ++i) {
            const ast_constant_expression *sz = v->array_sizes[i];
            if (sz->type == eExprType::IntConstant) {
                total_size *= static_cast<const ast_int_constant *>(sz)->value;
            } else if (sz->type == eExprType::UIntConstant) {
                total_size *= static_cast<const ast_uint_constant *>(sz)->value;
            }
        }
    }
    return total_size;
}

int glslx::WriterHLSL::Write_ByteaddressBufLoads(const byteaddress_buf_t &buf, const int offset,
                                                 const std::string &prefix, int array_dim, const ast_variable *v,
                                                 std::ostream &out_stream) {
    if ((v->flags & eVariableFlags::Array) && int(v->array_sizes.size()) > array_dim && v->array_sizes[array_dim]) {
        int count = 0;
        if (v->array_sizes[array_dim]->type == eExprType::IntConstant) {
            count = static_cast<ast_int_constant *>(v->array_sizes[array_dim])->value;
        } else if (v->array_sizes[array_dim]->type == eExprType::UIntConstant) {
            count = static_cast<ast_uint_constant *>(v->array_sizes[array_dim])->value;
        } else {
            assert(false);
        }
        int size = 0;
        for (int i = 0; i < count; ++i) {
            const std::string new_prefix = prefix + "[" + std::to_string(i) + "]";
            size += Write_ByteaddressBufLoads(buf, offset + size, new_prefix, array_dim + 1, v, out_stream);
        }
        return size;
    }

    if (v->base_type->builtin) {
        const auto *type = static_cast<const ast_builtin *>(v->base_type);
        switch (type->type) {
        case eKeyword::K_int:
        case eKeyword::K_uint:
            Write_Tabs(out_stream);
            out_stream << prefix << " = ";
            out_stream << buf.name << ".Load(index * " << buf.size << " + " << offset << ");\n";
            return 4;
        case eKeyword::K_int64_t:
            Write_Tabs(out_stream);
            out_stream << prefix << " = ";
            out_stream << buf.name << ".Load<int64_t>(index * " << buf.size << " + " << offset << ");\n";
            return 8;
        case eKeyword::K_uint64_t:
            Write_Tabs(out_stream);
            out_stream << prefix << " = ";
            out_stream << buf.name << ".Load<uint64_t>(index * " << buf.size << " + " << offset << ");\n";
            return 8;
        case eKeyword::K_float:
            Write_Tabs(out_stream);
            out_stream << prefix << " = asfloat(";
            out_stream << buf.name << ".Load(index * " << buf.size << " + " << offset << "));\n";
            return 4;
        case eKeyword::K_double:
            assert(false);
            return 8;
        case eKeyword::K_float16_t:
            Write_Tabs(out_stream);
            out_stream << prefix << " = ";
            out_stream << buf.name << ".Load<half>(index * " << buf.size << " + " << offset << ");\n";
            return 2;
        case eKeyword::K_bool:
            assert(false);
            return 1;
        case eKeyword::K_ivec2:
        case eKeyword::K_uvec2:
        case eKeyword::K_vec2: {
            Write_Tabs(out_stream);
            out_stream << prefix << " = ";
            if (type->type == eKeyword::K_vec2) {
                out_stream << "asfloat(";
            }
            out_stream << buf.name << ".Load2(index * " << buf.size << " + " << offset << ")";
            if (type->type == eKeyword::K_vec2) {
                out_stream << ")";
            }
            out_stream << ";\n";
            return 2 * 4;
        }
        case eKeyword::K_ivec4:
        case eKeyword::K_uvec4:
        case eKeyword::K_vec4: {
            Write_Tabs(out_stream);
            out_stream << prefix << " = ";
            if (type->type == eKeyword::K_vec4) {
                out_stream << "asfloat(";
            }
            out_stream << buf.name << ".Load4(index * " << buf.size << " + " << offset << ")";
            if (type->type == eKeyword::K_vec4) {
                out_stream << ")";
            }
            out_stream << ";\n";
            return 4 * 4;
        }
        case eKeyword::K_mat4x4:
        case eKeyword::K_mat4: {
            for (int i = 0; i < 4; ++i) {
                Write_Tabs(out_stream);
                out_stream << prefix << "[" << i << "] = ";
                out_stream << "asfloat(";
                out_stream << buf.name << ".Load4(index * " << buf.size << " + " << offset + 16 * i << ")";
                out_stream << ")";
                out_stream << ";\n";
            }
            return 16 * 4;
        }
        default:
            return -1;
        }
    } else {
        int size = 0;
        const auto *type = static_cast<const ast_struct *>(v->base_type);
        for (const ast_variable *sub_v : type->fields) {
            const std::string new_prefix = prefix + "." + sub_v->name;
            size += Write_ByteaddressBufLoads(buf, offset + size, new_prefix, array_dim, sub_v, out_stream);
        }
        return size;
    }
}

int glslx::WriterHLSL::Write_ByteaddressBufStores(const byteaddress_buf_t &buf, const int offset,
                                                  const std::string &prefix, int array_dim, const ast_type *t,
                                                  std::ostream &out_stream) {
    if (t->builtin) {
        const auto *type = static_cast<const ast_builtin *>(t);
        switch (type->type) {
        case eKeyword::K_int:
        case eKeyword::K_uint:
            Write_Tabs(out_stream);
            out_stream << buf.name << ".Store(__offset" << temp_var_index_ << " + " << offset << ", " << prefix
                       << ");\n";
            return 4;
        case eKeyword::K_ivec2:
        case eKeyword::K_uvec2:
            Write_Tabs(out_stream);
            out_stream << buf.name << ".Store2(__offset" << temp_var_index_ << " + " << offset << ", " << prefix
                       << ");\n";
            return 8;
        case eKeyword::K_ivec4:
        case eKeyword::K_uvec4:
            Write_Tabs(out_stream);
            out_stream << buf.name << ".Store4(__offset" << temp_var_index_ << " + " << offset << ", " << prefix
                       << ");\n";
            return 16;
        case eKeyword::K_int64_t:
            Write_Tabs(out_stream);
            out_stream << buf.name << ".Store<int64_t>(__offset" << temp_var_index_ << " + " << offset << ", " << prefix
                       << ");\n";
            return 8;
        case eKeyword::K_uint64_t:
            Write_Tabs(out_stream);
            out_stream << buf.name << ".Store<uint64_t>(__offset" << temp_var_index_ << " + " << offset << ", "
                       << prefix << ");\n";
            return 8;
        case eKeyword::K_float:
            Write_Tabs(out_stream);
            out_stream << buf.name << ".Store(__offset" << temp_var_index_ << " + " << offset << ", asuint(" << prefix
                       << "));\n";
            return 4;
        case eKeyword::K_double:
            assert(false);
            return 8;
        case eKeyword::K_bool:
            assert(false);
            return 1;
        case eKeyword::K_float16_t:
            Write_Tabs(out_stream);
            out_stream << buf.name << ".Store<half>(__offset" << temp_var_index_ << " + " << offset << ", " << prefix
                       << ");\n";
            return 2;
        default:
            return -1;
        }
    } else {
        int size = 0;
        const auto *type = static_cast<const ast_struct *>(t);
        for (const ast_variable *sub_v : type->fields) {
            const std::string new_prefix = prefix + "." + sub_v->name;
            size += Write_ByteaddressBufStores(buf, offset + size, new_prefix, array_dim, sub_v, out_stream);
        }
        return size;
    }
}

int glslx::WriterHLSL::Write_ByteaddressBufStores(const byteaddress_buf_t &buf, const int offset,
                                                  const std::string &prefix, int array_dim, const ast_variable *v,
                                                  std::ostream &out_stream) {
    if ((v->flags & eVariableFlags::Array) && int(v->array_sizes.size()) > array_dim && v->array_sizes[array_dim]) {
        int count = 0;
        if (v->array_sizes[array_dim]->type == eExprType::IntConstant) {
            count = static_cast<ast_int_constant *>(v->array_sizes[array_dim])->value;
        } else if (v->array_sizes[array_dim]->type == eExprType::UIntConstant) {
            count = static_cast<ast_uint_constant *>(v->array_sizes[array_dim])->value;
        } else {
            assert(false);
        }
        int size = 0;
        for (int i = 0; i < count; ++i) {
            const std::string new_prefix = prefix + "[" + std::to_string(i) + "]";
            size += Write_ByteaddressBufStores(buf, offset + size, new_prefix, array_dim + 1, v, out_stream);
        }
        return size;
    }

    return Write_ByteaddressBufStores(buf, offset, prefix, array_dim, v->base_type, out_stream);
}
