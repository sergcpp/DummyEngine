#include "WriterHLSL.h"

#include <ostream>

#include "Clone.h"
#include "LowerHLSL.h"

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
           type->type == eKeyword::K_samplerCubeArrayShadow;
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

bool is_combined_sampler_argument(const ast_expression *expr) {
    if (expr->type == eExprType::ArraySubscript) {
        expr = static_cast<const ast_array_subscript *>(expr)->operand;
    }
    return expr->type == eExprType::VariableIdentifier &&
           is_combined_texturesampler(static_cast<const ast_variable_identifier *>(expr)->variable->base_type);
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
} // namespace glslx

void glslx::WriterHLSL::Write_Expression(const ast_expression *expression, const assoc_t parent,
                                         std::ostream &out_stream) {
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
        return Write_FieldOrSwizzle(static_cast<const ast_field_or_swizzle *>(expression), parent, out_stream);
    case eExprType::ArraySubscript:
        return Write_ArraySubscript(static_cast<const ast_array_subscript *>(expression), parent, out_stream);
    case eExprType::FunctionCall:
        return Write_FunctionCall(static_cast<const ast_function_call *>(expression), parent, out_stream);
    case eExprType::MethodCall:
        return Write_MethodCall(static_cast<const ast_method_call *>(expression), parent, out_stream);
    case eExprType::ConstructorCall:
        return Write_ConstructorCall(static_cast<const ast_constructor_call *>(expression), parent, out_stream);
    case eExprType::PostIncrement:
        return Write_PostIncrement(static_cast<const ast_post_increment_expression *>(expression), parent, out_stream);
    case eExprType::PostDecrement:
        return Write_PostDecrement(static_cast<const ast_post_decrement_expression *>(expression), parent, out_stream);
    case eExprType::UnaryPlus:
        return Write_UnaryPlus(static_cast<const ast_unary_plus_expression *>(expression), parent, out_stream);
    case eExprType::UnaryMinus:
        return Write_UnaryMinus(static_cast<const ast_unary_minus_expression *>(expression), parent, out_stream);
    case eExprType::BitNot:
        return Write_UnaryBitNot(static_cast<const ast_unary_bit_not_expression *>(expression), parent, out_stream);
    case eExprType::LogicalNot:
        return Write_UnaryLogicalNot(static_cast<const ast_unary_logical_not_expression *>(expression), parent,
                                     out_stream);
    case eExprType::PrefixIncrement:
        return Write_PrefixIncrement(static_cast<const ast_prefix_increment_expression *>(expression), parent,
                                     out_stream);
    case eExprType::PrefixDecrement:
        return Write_PrefixDecrement(static_cast<const ast_prefix_decrement_expression *>(expression), parent,
                                     out_stream);
    case eExprType::Assign:
        return Write_Assignment(static_cast<const ast_assignment_expression *>(expression), parent, out_stream);
    case eExprType::Sequence:
        return Write_Sequence(static_cast<const ast_sequence_expression *>(expression), parent, out_stream);
    case eExprType::Operation:
        return Write_Operation(static_cast<const ast_operation_expression *>(expression), parent, out_stream);
    case eExprType::Ternary:
        return Write_Ternary(static_cast<const ast_ternary_expression *>(expression), parent, out_stream);
    case eExprType::ArraySpecifier:
        const auto *arr_specifier = static_cast<const ast_array_specifier *>(expression);
        out_stream << "{ ";
        for (int i = 0; i < int(arr_specifier->expressions.size()); ++i) {
            Write_Expression(arr_specifier->expressions[i], {}, out_stream);
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
    for (int i = 0; i < int(function->parameters.size()); ++i) {
        Write_FunctionParameter(function->parameters[i], out_stream);
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
    if (variable->flags & eVariableFlags::Const) {
        out_stream << "const ";
    }
    Write_Variable(variable, {}, out_stream, output_flags);
    if (variable->initial_value) {
        out_stream << " = ";
        const int prec = g_operators[int(eOperator::assign)].precedence;
        Write_Expression(variable->initial_value, {prec, eAssoc::Right, eOperator::assign, true}, out_stream);
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
    Write_Expression(statement->expression, {}, out_stream);
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
    Write_Expression(statement->condition, {}, out_stream);
    out_stream << ")";
    if (statement->then_statement->type != eStatement::Compound) {
        out_stream << " ";
    }
    if (statement->else_statement) {
        Write_Statement(statement->then_statement, out_stream,
                        DefaultOutputFlags & ~(Bitmask{eOutputFlags::WriteTabs} | eOutputFlags::NewLine));
        out_stream << " else";
        if (statement->else_statement->type != eStatement::Compound) {
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
    Write_Expression(statement->expression, {}, out_stream);
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
        Write_Expression(statement->condition, {}, out_stream);
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
    Write_Expression(statement->condition, {}, out_stream);
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
        Write_Expression(statement->condition, {}, out_stream);
    }
    out_stream << ";";
    if (statement->loop) {
        out_stream << " ";
        Write_Expression(statement->loop, {}, out_stream);
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
        Write_Expression(statement->expression, {}, out_stream);
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
            Write_Expression(array_sizes[i], {}, out_stream);
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
    } else if (type->type == eKeyword::K_sampler || type->type == eKeyword::K_samplerShadow) {
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
    if ((variable->storage == eStorage::None && variable->layout_qualifiers.empty()) ||
        variable->storage == eStorage::Const) {
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
        const int prec = g_operators[int(eOperator::assign)].precedence;
        Write_Expression(variable->initial_value, {prec, eAssoc::Right, eOperator::assign, true}, out_stream);
    }

    out_stream << ";\n";
}

void glslx::WriterHLSL::Write_VariableIdentifier(const ast_variable_identifier *expression, std::ostream &out_stream) {
    Write_Variable(expression->variable, {}, out_stream, eOutputFlags::VarName /* name_only */);
}

void glslx::WriterHLSL::Write_FieldOrSwizzle(const ast_field_or_swizzle *expression, const assoc_t parent,
                                             std::ostream &out_stream) {
    const int prec = g_operators[int(eOperator::dot)].precedence;
    const bool need_parens = parentheses_required(prec, eAssoc::Left, parent);

    if (need_parens) {
        out_stream << "(";
    }
    Write_Expression(expression->operand, {prec, eAssoc::Left}, out_stream);
    out_stream << ".";
    if (expression->field) {
        Write_VariableIdentifier(expression->field, out_stream);
    } else {
        out_stream << expression->name;
    }
    if (need_parens) {
        out_stream << ")";
    }
}

void glslx::WriterHLSL::Write_ArraySubscript(const ast_array_subscript *expression, const assoc_t parent,
                                             std::ostream &out_stream) {
    const int prec = g_operators[int(eOperator::bracket_begin)].precedence;
    const bool need_parens = parentheses_required(prec, eAssoc::Left, parent);

    if (need_parens) {
        out_stream << "(";
    }
    Write_Expression(expression->operand, {prec, eAssoc::Left}, out_stream);
    out_stream << "[";
    Write_Expression(expression->index, {}, out_stream);
    out_stream << "]";
    if (need_parens) {
        out_stream << ")";
    }
}

void glslx::WriterHLSL::Write_FunctionCall(const ast_function_call *expression, const assoc_t parent,
                                           std::ostream &out_stream) {
    out_stream << expression->name << "(";
    for (int i = 0; i < int(expression->parameters.size()); ++i) {
        // Only user functions may take a combined sampler here; builtins must be lowered to method calls.
        assert((expression->func && !(expression->func->attributes & eFunctionAttribute::Builtin)) ||
               !is_combined_sampler_argument(expression->parameters[i]));
        Write_Expression(expression->parameters[i], {}, out_stream);
        if (i != int(expression->parameters.size()) - 1) {
            out_stream << ", ";
        }
    }
    out_stream << ")";
}

void glslx::WriterHLSL::Write_MethodCall(const ast_method_call *expression, const assoc_t parent,
                                         std::ostream &out_stream) {
    const int prec = g_operators[int(eOperator::dot)].precedence;
    const bool need_parens = parentheses_required(prec, eAssoc::Left, parent);

    if (need_parens) {
        out_stream << "(";
    }
    Write_Expression(expression->object, {prec, eAssoc::Left}, out_stream);
    out_stream << "." << expression->name;
    if (expression->template_arg) {
        out_stream << "<";
        Write_Type(expression->template_arg, out_stream);
        out_stream << ">";
    }
    out_stream << "(";
    for (int i = 0; i < int(expression->parameters.size()); ++i) {
        Write_Expression(expression->parameters[i], {}, out_stream);
        if (i != int(expression->parameters.size()) - 1) {
            out_stream << ", ";
        }
    }
    out_stream << ")";
    if (need_parens) {
        out_stream << ")";
    }
}

void glslx::WriterHLSL::Write_ConstructorCall(const ast_constructor_call *expression, const assoc_t parent,
                                              std::ostream &out_stream) {
    const int prec = g_operators[int(eOperator::parenthesis_begin)].precedence;
    const bool need_parens = parentheses_required(prec, eAssoc::Left, parent);

    if (need_parens) {
        out_stream << "(";
    }

    // An inline `sampler2D(tex, samp)` constructor is always consumed by LowerHLSL's
    // Resolve_TextureArg/Rewrite_SampleCall and must never reach the writer.
    assert(!is_combined_texturesampler(expression->type) && "combined-sampler ctor must be lowered");
    if (!expression->type->builtin) {
        out_stream << "(";
        Write_Type(expression->type, out_stream);
        out_stream << ")";
    } else {
        // `row_major float3x2` is not a valid ctor prefix. The plain ctor fills elements in row-major
        // order, matching the row_major tag's convention; the receiving variable carries the tag.
        const char *name = g_hlsl_keywords[int(static_cast<const ast_builtin *>(expression->type)->type)];
        const char *space = strchr(name, ' ');
        out_stream << (space ? space + 1 : name);
    }
    out_stream << "(";
    for (int i = 0; i < int(expression->parameters.size()); ++i) {
        Write_Expression(expression->parameters[i], {}, out_stream);
        if (i != int(expression->parameters.size()) - 1) {
            out_stream << ", ";
        }
    }
    out_stream << ")";

    if (need_parens) {
        out_stream << ")";
    }
}

void glslx::WriterHLSL::Write_PostIncrement(const ast_post_increment_expression *expression, const assoc_t parent,
                                            std::ostream &out_stream) {
    const int prec = g_operators[int(eOperator::increment)].precedence + 1;
    const bool need_parens = parentheses_required(prec, eAssoc::Left, parent);

    if (need_parens) {
        out_stream << "(";
    }
    Write_Expression(expression->operand, {prec, eAssoc::Left}, out_stream);
    out_stream << "++";
    if (need_parens) {
        out_stream << ")";
    }
}

void glslx::WriterHLSL::Write_PostDecrement(const ast_post_decrement_expression *expression, const assoc_t parent,
                                            std::ostream &out_stream) {
    const int prec = g_operators[int(eOperator::decrement)].precedence + 1;
    const bool need_parens = parentheses_required(prec, eAssoc::Left, parent);

    if (need_parens) {
        out_stream << "(";
    }
    Write_Expression(expression->operand, {prec, eAssoc::Left}, out_stream);
    out_stream << "--";
    if (need_parens) {
        out_stream << ")";
    }
}

void glslx::WriterHLSL::Write_UnaryPlus(const ast_unary_plus_expression *expression, const assoc_t parent,
                                        std::ostream &out_stream) {
    const int prec = g_operators[int(eOperator::plus)].precedence + 2;
    const bool need_parens = parentheses_required(prec, eAssoc::Right, parent);

    if (need_parens) {
        out_stream << "(";
    }
    out_stream << "+";
    Write_Expression(expression->operand, {prec, eAssoc::Right}, out_stream);
    if (need_parens) {
        out_stream << ")";
    }
}

void glslx::WriterHLSL::Write_UnaryMinus(const ast_unary_minus_expression *expression, const assoc_t parent,
                                         std::ostream &out_stream) {
    const int prec = g_operators[int(eOperator::minus)].precedence + 2;
    const bool need_parens = parentheses_required(prec, eAssoc::Right, parent);

    if (need_parens) {
        out_stream << "(";
    }
    out_stream << "-";
    Write_Expression(expression->operand, {prec, eAssoc::Right}, out_stream);
    if (need_parens) {
        out_stream << ")";
    }
}

void glslx::WriterHLSL::Write_UnaryBitNot(const ast_unary_bit_not_expression *expression, const assoc_t parent,
                                          std::ostream &out_stream) {
    const int prec = g_operators[int(eOperator::bit_not)].precedence;
    const bool need_parens = parentheses_required(prec, eAssoc::Right, parent);

    if (need_parens) {
        out_stream << "(";
    }
    out_stream << "~";
    Write_Expression(expression->operand, {prec, eAssoc::Right}, out_stream);
    if (need_parens) {
        out_stream << ")";
    }
}

void glslx::WriterHLSL::Write_UnaryLogicalNot(const ast_unary_logical_not_expression *expression, const assoc_t parent,
                                              std::ostream &out_stream) {
    const int prec = g_operators[int(eOperator::logital_not)].precedence;
    const bool need_parens = parentheses_required(prec, eAssoc::Right, parent);

    if (need_parens) {
        out_stream << "(";
    }
    out_stream << "!";
    Write_Expression(expression->operand, {prec, eAssoc::Right}, out_stream);
    if (need_parens) {
        out_stream << ")";
    }
}

void glslx::WriterHLSL::Write_PrefixIncrement(const ast_prefix_increment_expression *expression, const assoc_t parent,
                                              std::ostream &out_stream) {
    const int prec = g_operators[int(eOperator::increment)].precedence;
    const bool need_parens = parentheses_required(prec, eAssoc::Right, parent);

    if (need_parens) {
        out_stream << "(";
    }
    out_stream << "++";
    Write_Expression(expression->operand, {prec, eAssoc::Right}, out_stream);
    if (need_parens) {
        out_stream << ")";
    }
}

void glslx::WriterHLSL::Write_PrefixDecrement(const ast_prefix_decrement_expression *expression, const assoc_t parent,
                                              std::ostream &out_stream) {
    const int prec = g_operators[int(eOperator::decrement)].precedence;
    const bool need_parens = parentheses_required(prec, eAssoc::Right, parent);

    if (need_parens) {
        out_stream << "(";
    }
    out_stream << "--";
    Write_Expression(expression->operand, {prec, eAssoc::Right}, out_stream);
    if (need_parens) {
        out_stream << ")";
    }
}

void glslx::WriterHLSL::Write_Assignment(const ast_assignment_expression *expression, const assoc_t parent,
                                         std::ostream &out_stream) {
    const int prec = g_operators[int(eOperator::assign)].precedence;
    const bool need_parens = parentheses_required(prec, eAssoc::Right, parent);

    if (need_parens) {
        out_stream << "(";
    }
    Write_Expression(expression->operand1, {prec, eAssoc::Right, expression->oper, false}, out_stream);
    out_stream << " " << g_operators[int(expression->oper)].string << " ";
    Write_Expression(expression->operand2, {prec, eAssoc::Right, expression->oper, true}, out_stream);
    if (need_parens) {
        out_stream << ")";
    }
}

void glslx::WriterHLSL::Write_Sequence(const ast_sequence_expression *expression, const assoc_t parent,
                                       std::ostream &out_stream) {
    const int prec = g_operators[int(eOperator::comma)].precedence;
    const bool need_parens = parentheses_required(prec, eAssoc::Left, parent);

    if (need_parens) {
        out_stream << "(";
    }
    Write_Expression(expression->operand1, {prec, eAssoc::Left, eOperator::none, false}, out_stream);
    out_stream << ", ";
    Write_Expression(expression->operand2, {prec, eAssoc::Left, eOperator::none, true}, out_stream);
    if (need_parens) {
        out_stream << ")";
    }
}

void glslx::WriterHLSL::Write_Operation(const ast_operation_expression *expression, const assoc_t parent,
                                        std::ostream &out_stream) {
    const int prec = g_operators[int(expression->oper)].precedence;
    bool need_parens = parentheses_required(prec, eAssoc::Left, parent);
    if (config_.insert_adviseable_parentheses) {
        need_parens |= parentheses_adviseable(expression->oper, parent);
    }

    if (need_parens) {
        out_stream << "(";
    }
    Write_Expression(expression->operand1, {prec, eAssoc::Left, expression->oper, false}, out_stream);
    out_stream << " " << g_operators[int(expression->oper)].string << " ";
    Write_Expression(expression->operand2, {prec, eAssoc::Left, expression->oper, true}, out_stream);
    if (need_parens) {
        out_stream << ")";
    }
}

void glslx::WriterHLSL::Write_Ternary(const ast_ternary_expression *expression, const assoc_t parent,
                                      std::ostream &out_stream) {
    const int prec = g_operators[int(eOperator::questionmark)].precedence;
    const bool insert_parens = parentheses_required(prec, eAssoc::Right, parent);

    if (insert_parens) {
        out_stream << "(";
    }
    Write_Expression(expression->condition, {prec, eAssoc::Right, eOperator::questionmark, false}, out_stream);
    out_stream << " ? ";
    Write_Expression(expression->on_true, {prec, eAssoc::Right, eOperator::questionmark, false}, out_stream);
    out_stream << " : ";
    Write_Expression(expression->on_false, {prec, eAssoc::Right, eOperator::questionmark, true}, out_stream);
    if (insert_parens) {
        out_stream << ")";
    }
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
        if (structure->fields[i]->semantic) {
            out_stream << " : " << structure->fields[i]->semantic;
        }
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

void glslx::WriterHLSL::Write(const TrUnit *tu, std::ostream &out_stream) {
    tu_ = tu;
    ast_function *main_function = nullptr;
    auto *p_find = tu->functions_by_name.Find("main");
    if (p_find) {
        assert(p_find->size() == 1);
        main_function = (*p_find)[0];
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

void glslx::Write_HLSL(const TrUnit *tu, std::ostream &out_stream, writer_config_t config) {
    std::unique_ptr<TrUnit> owned = Clone().CloneAST(tu);
    LowerHLSL().Apply(owned.get());
    WriterHLSL(std::move(config)).Write(owned.get(), out_stream);
}

