#include "WriterGLSL.h"

#include <ostream>

void glslx::WriterGLSL::Write_Expression(const ast_expression *expression, bool nested, std::ostream &out_stream) {
    switch (expression->type) {
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
    case eExprType::Operation:
        return Write_Operation(static_cast<const ast_operation_expression *>(expression), out_stream);
    case eExprType::Ternary:
        return Write_Ternary(static_cast<const ast_ternary_expression *>(expression), out_stream);
    case eExprType::ArraySpecifier:
        const ast_array_specifier *arr_specifier = static_cast<const ast_array_specifier *>(expression);
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

void glslx::WriterGLSL::Write_FunctionParameter(const ast_function_parameter *parameter, std::ostream &out_stream) {
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

void glslx::WriterGLSL::Write_Function(const ast_function *function, std::ostream &out_stream) {
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

void glslx::WriterGLSL::Write_SelectionAttributes(const Bitmask<eCtrlFlowAttribute> attributes,
                                                  std::ostream &out_stream) {
    if (!(attributes & SelectionAttributesMask)) {
        return;
    }
    out_stream << "[[";
    if (attributes & eCtrlFlowAttribute::Flatten) {
        out_stream << "flatten";
    } else if (attributes & eCtrlFlowAttribute::DontFlatten) {
        out_stream << "dont_flatten";
    }
    out_stream << "]] ";
}

void glslx::WriterGLSL::Write_LoopAttributes(ctrl_flow_params_t ctrl_flow, std::ostream &out_stream) {
    if (!(ctrl_flow.attributes & LoopAttributesMask)) {
        return;
    }
    out_stream << "[[";
    if (ctrl_flow.attributes & eCtrlFlowAttribute::Unroll) {
        out_stream << "unroll";
        ctrl_flow.attributes &= ~(Bitmask{eCtrlFlowAttribute::Unroll} | eCtrlFlowAttribute::DontUnroll);
        if (ctrl_flow.attributes) {
            out_stream << ", ";
        }
    } else if (ctrl_flow.attributes & eCtrlFlowAttribute::DontUnroll) {
        out_stream << "dont_unroll";
        ctrl_flow.attributes &= ~(Bitmask{eCtrlFlowAttribute::Unroll} | eCtrlFlowAttribute::DontUnroll);
        if (ctrl_flow.attributes) {
            out_stream << ", ";
        }
    }
    if (ctrl_flow.attributes & eCtrlFlowAttribute::DependencyInfinite) {
        out_stream << "dependency_infinite";
        ctrl_flow.attributes &= ~Bitmask{eCtrlFlowAttribute::DependencyInfinite};
        if (ctrl_flow.attributes) {
            out_stream << ", ";
        }
    }
    if (ctrl_flow.attributes & eCtrlFlowAttribute::DependencyLength) {
        out_stream << "dependency_length(" << ctrl_flow.dependency_length << ")";
        ctrl_flow.attributes &= ~Bitmask{eCtrlFlowAttribute::DependencyLength};
        if (ctrl_flow.attributes) {
            out_stream << ", ";
        }
    }
    if (ctrl_flow.attributes & eCtrlFlowAttribute::MinIterations) {
        out_stream << "min_iterations(" << ctrl_flow.min_iterations << ")";
        ctrl_flow.attributes &= ~Bitmask{eCtrlFlowAttribute::MinIterations};
        if (ctrl_flow.attributes) {
            out_stream << ", ";
        }
    }
    if (ctrl_flow.attributes & eCtrlFlowAttribute::MaxIterations) {
        out_stream << "max_iterations(" << ctrl_flow.max_iterations << ")";
        ctrl_flow.attributes &= ~Bitmask{eCtrlFlowAttribute::MaxIterations};
        if (ctrl_flow.attributes) {
            out_stream << ", ";
        }
    }
    if (ctrl_flow.attributes & eCtrlFlowAttribute::IterationMultiple) {
        out_stream << "iteration_multiple(" << ctrl_flow.iteration_multiple << ")";
        ctrl_flow.attributes &= ~Bitmask{eCtrlFlowAttribute::IterationMultiple};
        if (ctrl_flow.attributes) {
            out_stream << ", ";
        }
    }
    if (ctrl_flow.attributes & eCtrlFlowAttribute::PeelCount) {
        out_stream << "peel_count(" << ctrl_flow.peel_count << ")";
        ctrl_flow.attributes &= ~Bitmask{eCtrlFlowAttribute::PeelCount};
        if (ctrl_flow.attributes) {
            out_stream << ", ";
        }
    }
    if (ctrl_flow.attributes & eCtrlFlowAttribute::PartialCount) {
        out_stream << "partial_count(" << ctrl_flow.partial_count << ")";
        ctrl_flow.attributes &= ~Bitmask{eCtrlFlowAttribute::PartialCount};
        if (ctrl_flow.attributes) {
            out_stream << ", ";
        }
    }
    out_stream << "]] ";
}

void glslx::WriterGLSL::Write_FunctionVariable(const ast_function_variable *variable, std::ostream &out_stream,
                                               const Bitmask<eOutputFlags> output_flags) {
    if (variable->flags & eVariableFlags::Const) {
        out_stream << "const ";
    }
    Write_Variable(variable, out_stream);
    if (variable->initial_value) {
        out_stream << " = ";
        Write_Expression(variable->initial_value, false, out_stream);
    }
    if (output_flags & eOutputFlags::Semicolon) {
        out_stream << ";";
    }
    if (output_flags & eOutputFlags::NewLine) {
        out_stream << "\n";
    }
}

void glslx::WriterGLSL::Write_CompoundStatement(const ast_compound_statement *statement, std::ostream &out_stream,
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

void glslx::WriterGLSL::Write_EmptyStatement(const ast_empty_statement *statement, std::ostream &out_stream,
                                             Bitmask<eOutputFlags> output_flags) {
    out_stream << ";";
}

void glslx::WriterGLSL::Write_DeclarationStatement(const ast_declaration_statement *statement, std::ostream &out_stream,
                                                   const Bitmask<eOutputFlags> output_flags) {
    for (int i = 0; i < int(statement->variables.size()); ++i) {
        if (i > 0 && (output_flags & eOutputFlags::WriteTabs)) {
            Write_Tabs(out_stream);
        }
        Write_FunctionVariable(statement->variables[i], out_stream, output_flags);
    }
}

void glslx::WriterGLSL::Write_ExpressionStatement(const ast_expression_statement *statement, std::ostream &out_stream,
                                                  const Bitmask<eOutputFlags> output_flags) {
    Write_Expression(statement->expression, false, out_stream);
    if (output_flags & eOutputFlags::Semicolon) {
        out_stream << ";";
    }
    if (output_flags & eOutputFlags::NewLine) {
        out_stream << "\n";
    }
}

void glslx::WriterGLSL::Write_IfStatement(const ast_if_statement *statement, std::ostream &out_stream,
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

void glslx::WriterGLSL::Write_SwitchStatement(const ast_switch_statement *statement, std::ostream &out_stream,
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

void glslx::WriterGLSL::Write_CaseLabelStatement(const ast_case_label_statement *statement, std::ostream &out_stream,
                                                 Bitmask<eOutputFlags> output_flags) {
    if (statement->flags & eCaseLabelFlags::Default) {
        out_stream << "default";
    } else {
        out_stream << "case ";
        Write_Expression(statement->condition, false, out_stream);
    }
    out_stream << ":\n";
}

void glslx::WriterGLSL::Write_WhileStatement(const ast_while_statement *statement, std::ostream &out_stream,
                                             Bitmask<eOutputFlags> output_flags) {
    Write_LoopAttributes(statement->flow_params, out_stream);
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
        break;
    }
    out_stream << ")";
    Write_Statement(statement->body, out_stream, output_flags & ~Bitmask{eOutputFlags::WriteTabs});
}

void glslx::WriterGLSL::Write_DoStatement(const ast_do_statement *statement, std::ostream &out_stream,
                                          Bitmask<eOutputFlags> output_flags) {
    Write_LoopAttributes(statement->flow_params, out_stream);
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

void glslx::WriterGLSL::Write_ForStatement(const ast_for_statement *statement, std::ostream &out_stream,
                                           Bitmask<eOutputFlags> output_flags) {
    Write_LoopAttributes(statement->flow_params, out_stream);
    out_stream << "for (";
    if (statement->init) {
        switch (statement->init->type) {
        case eStatement::Declaration:
            Write_DeclarationStatement(static_cast<ast_declaration_statement *>(statement->init), out_stream,
                                       eOutputFlags::Semicolon);
            break;
        case eStatement::Expression:
            Write_ExpressionStatement(static_cast<ast_expression_statement *>(statement->init), out_stream,
                                      eOutputFlags::Semicolon);
            break;
        default:
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

void glslx::WriterGLSL::Write_ContinueStatement(const ast_continue_statement *statement, std::ostream &out_stream,
                                                Bitmask<eOutputFlags> output_flags) {
    out_stream << "continue;\n";
}

void glslx::WriterGLSL::Write_BreakStatement(const ast_break_statement *statement, std::ostream &out_stream,
                                             Bitmask<eOutputFlags> output_flags) {
    out_stream << "break;\n";
}

void glslx::WriterGLSL::Write_ReturnStatement(const ast_return_statement *statement, std::ostream &out_stream,
                                              Bitmask<eOutputFlags> output_flags) {
    if (statement->expression) {
        out_stream << "return ";
        Write_Expression(statement->expression, false, out_stream);
        out_stream << ";\n";
    } else {
        out_stream << "return;\n";
    }
}

void glslx::WriterGLSL::Write_DistardStatement(const ast_discard_statement *statement, std::ostream &out_stream,
                                               Bitmask<eOutputFlags> output_flags) {
    out_stream << "discard;\n";
}

void glslx::WriterGLSL::Write_ExtJumpStatement(const ast_ext_jump_statement *statement, std::ostream &out_stream,
                                               Bitmask<eOutputFlags> output_flags) {
    out_stream << g_keywords[int(statement->keyword)].name << ";\n";
}

void glslx::WriterGLSL::Write_Statement(const ast_statement *statement, std::ostream &out_stream,
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
    case eStatement::ExtJump:
        Write_ExtJumpStatement(static_cast<const ast_ext_jump_statement *>(statement), out_stream, out_flags);
        break;
    default:
        break;
    }
}

void glslx::WriterGLSL::Write_Builtin(const ast_builtin *builtin, std::ostream &out_stream) {
    out_stream << g_keywords[int(builtin->type)].name;
}

void glslx::WriterGLSL::Write_Type(const ast_type *type, std::ostream &out_stream) {
    if (type->builtin) {
        Write_Builtin(static_cast<const ast_builtin *>(type), out_stream);
    } else {
        out_stream << static_cast<const ast_struct *>(type)->name;
    }
}

void glslx::WriterGLSL::Write_ArraySize(Span<const ast_constant_expression *const> array_sizes,
                                        std::ostream &out_stream) {
    for (int i = 0; i < int(array_sizes.size()); ++i) {
        out_stream << "[";
        if (array_sizes[i]) {
            Write_Expression(array_sizes[i], false, out_stream);
        }
        out_stream << "]";
    }
}

void glslx::WriterGLSL::Write_Variable(const ast_variable *variable, std::ostream &out_stream, const bool name_only) {
    if (name_only) {
        out_stream << variable->name;
        return;
    }

    if (variable->flags & eVariableFlags::Precise) {
        out_stream << "precise ";
    }

    Write_Precision(variable->precision, out_stream);

    Write_Type(variable->base_type, out_stream);
    out_stream << " " << variable->name;

    if (variable->flags & eVariableFlags::Array) {
        Write_ArraySize(variable->array_sizes, out_stream);
    }
}

void glslx::WriterGLSL::Write_Layout(Span<const ast_layout_qualifier *const> qualifiers, std::ostream &out_stream) {
    if (qualifiers.empty()) {
        return;
    }
    out_stream << "layout(";
    for (int i = 0; i < int(qualifiers.size()); ++i) {
        const ast_layout_qualifier *qualifier = qualifiers[i];
        out_stream << qualifier->name;
        if (qualifier->initial_value) {
            out_stream << " = ";
            Write_Expression(qualifier->initial_value, false, out_stream);
        }
        if (i != int(qualifiers.size()) - 1) {
            out_stream << ", ";
        }
    }
    out_stream << ") ";
}

void glslx::WriterGLSL::Write_Storage(const eStorage storage, std::ostream &out_stream) {
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
    case eStorage::Uniform:
        out_stream << "uniform ";
        break;
    case eStorage::Varying:
        out_stream << "varying ";
        break;
    case eStorage::Buffer:
        out_stream << "buffer ";
        break;
    case eStorage::Shared:
        out_stream << "shared ";
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
        break;
    }
}

void glslx::WriterGLSL::Write_AuxStorage(eAuxStorage aux_storage, std::ostream &out_stream) {
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
        break;
    }
}

void glslx::WriterGLSL::Write_ParameterQualifiers(const Bitmask<eParamQualifier> qualifiers, std::ostream &out_stream) {
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

void glslx::WriterGLSL::Write_Memory(Bitmask<eMemory> memory, std::ostream &out_stream) {
    if (memory & eMemory::Coherent) {
        out_stream << "coherent ";
    }
    if (memory & eMemory::Volatile) {
        out_stream << "volatile ";
    }
    if (memory & eMemory::Restrict) {
        out_stream << "restrict ";
    }
    if (memory & eMemory::Readonly) {
        out_stream << "readonly ";
    }
    if (memory & eMemory::Writeonly) {
        out_stream << "writeonly ";
    }
}

void glslx::WriterGLSL::Write_Precision(ePrecision precision, std::ostream &out_stream) {
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
        break;
    }
}

void glslx::WriterGLSL::Write_GlobalVariable(const ast_global_variable *variable, std::ostream &out_stream) {
    if (written_globals_.Find(variable)) {
        return;
    }

    auto invariant_hidden = Bitmask{eVariableFlags::Invariant} | eVariableFlags::Hidden;
    if ((variable->flags & invariant_hidden) == invariant_hidden) {
        out_stream << "invariant " << variable->name << ";\n";
        return;
    }

    if (variable->flags & eVariableFlags::Const) {
        out_stream << "const ";
    }

    Write_Layout(variable->layout_qualifiers, out_stream);
    Write_Storage(variable->storage, out_stream);
    Write_AuxStorage(variable->aux_storage, out_stream);
    Write_Memory(variable->memory_flags, out_stream);

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
        break;
    }

    Write_Variable(variable, out_stream);

    if (variable->initial_value) {
        out_stream << " = ";
        Write_Expression(variable->initial_value, false, out_stream);
    }

    out_stream << ";\n";
}

void glslx::WriterGLSL::Write_VariableIdentifier(const ast_variable_identifier *expression, std::ostream &out_stream) {
    Write_Variable(expression->variable, out_stream, true /* name_only */);
}

void glslx::WriterGLSL::Write_FieldOrSwizzle(const ast_field_or_swizzle *expression, std::ostream &out_stream) {
    Write_Expression(expression->operand, false, out_stream);
    out_stream << ".";
    if (expression->field) {
        Write_VariableIdentifier(expression->field, out_stream);
    } else {
        out_stream << expression->name;
    }
}

void glslx::WriterGLSL::Write_ArraySubscript(const ast_array_subscript *expression, std::ostream &out_stream) {
    Write_Expression(expression->operand, false, out_stream);
    out_stream << "[";
    Write_Expression(expression->index, false, out_stream);
    out_stream << "]";
}

void glslx::WriterGLSL::Write_FunctionCall(const ast_function_call *expression, std::ostream &out_stream) {
    out_stream << expression->name << "(";
    for (int i = 0; i < int(expression->parameters.size()); ++i) {
        Write_Expression(expression->parameters[i], false, out_stream);
        if (i != int(expression->parameters.size()) - 1) {
            out_stream << ", ";
        }
    }
    out_stream << ")";
}

void glslx::WriterGLSL::Write_ConstructorCall(const ast_constructor_call *expression, std::ostream &out_stream) {
    Write_Type(expression->type, out_stream);
    out_stream << "(";
    for (int i = 0; i < int(expression->parameters.size()); ++i) {
        Write_Expression(expression->parameters[i], false, out_stream);
        if (i != int(expression->parameters.size()) - 1) {
            out_stream << ", ";
        }
    }
    out_stream << ")";
}

void glslx::WriterGLSL::Write_PostIncrement(const ast_post_increment_expression *expression, std::ostream &out_stream) {
    Write_Expression(expression->operand, false, out_stream);
    out_stream << "++";
}

void glslx::WriterGLSL::Write_PostDecrement(const ast_post_decrement_expression *expression, std::ostream &out_stream) {
    Write_Expression(expression->operand, false, out_stream);
    out_stream << "--";
}

void glslx::WriterGLSL::Write_UnaryPlus(const ast_unary_plus_expression *expression, std::ostream &out_stream) {
    out_stream << "+";
    Write_Expression(expression->operand, false, out_stream);
}

void glslx::WriterGLSL::Write_UnaryMinus(const ast_unary_minus_expression *expression, std::ostream &out_stream) {
    out_stream << "-";
    Write_Expression(expression->operand, false, out_stream);
}

void glslx::WriterGLSL::Write_UnaryBitNot(const ast_unary_bit_not_expression *expression, std::ostream &out_stream) {
    out_stream << "~";
    Write_Expression(expression->operand, false, out_stream);
}

void glslx::WriterGLSL::Write_UnaryLogicalNot(const ast_unary_logical_not_expression *expression,
                                              std::ostream &out_stream) {
    out_stream << "!";
    Write_Expression(expression->operand, false, out_stream);
}

void glslx::WriterGLSL::Write_PrefixIncrement(const ast_prefix_increment_expression *expression,
                                              std::ostream &out_stream) {
    out_stream << "++";
    Write_Expression(expression->operand, false, out_stream);
}

void glslx::WriterGLSL::Write_PrefixDecrement(const ast_prefix_decrement_expression *expression,
                                              std::ostream &out_stream) {
    out_stream << "--";
    Write_Expression(expression->operand, false, out_stream);
}

void glslx::WriterGLSL::Write_Assignment(const ast_assignment_expression *expression, std::ostream &out_stream) {
    Write_Expression(expression->operand1, false, out_stream);
    out_stream << " " << g_operators[int(expression->oper)].string << " ";
    Write_Expression(expression->operand2, false, out_stream);
}

void glslx::WriterGLSL::Write_Sequence(const ast_sequence_expression *expression, std::ostream &out_stream) {
    out_stream << "(";
    Write_Expression(expression->operand1, false, out_stream);
    out_stream << ", ";
    Write_Expression(expression->operand2, false, out_stream);
    out_stream << ")";
}

void glslx::WriterGLSL::Write_Operation(const ast_operation_expression *expression, std::ostream &out_stream) {
    out_stream << "(";
    Write_Expression(expression->operand1, true, out_stream);
    out_stream << " " << g_operators[int(expression->oper)].string << " ";
    Write_Expression(expression->operand2, true, out_stream);
    out_stream << ")";
}

void glslx::WriterGLSL::Write_Ternary(const ast_ternary_expression *expression, std::ostream &out_stream) {
    out_stream << "(";
    Write_Expression(expression->condition, true, out_stream);
    out_stream << " ? ";
    Write_Expression(expression->on_true, true, out_stream);
    out_stream << " : ";
    Write_Expression(expression->on_false, true, out_stream);
    out_stream << ")";
}

void glslx::WriterGLSL::Write_Structure(const ast_struct *structure, std::ostream &out_stream) {
    out_stream << "struct ";
    if (structure->name) {
        out_stream << structure->name << " ";
    }
    out_stream << "{\n";
    ++nest_level_;
    for (int i = 0; i < int(structure->fields.size()); ++i) {
        Write_Tabs(out_stream);
        Write_Variable(structure->fields[i], out_stream);
        out_stream << ";\n";
    }
    --nest_level_;
    out_stream << "};\n";
}

void glslx::WriterGLSL::Write_InterfaceBlock(const ast_interface_block *block, std::ostream &out_stream) {
    Write_Layout(block->layout_qualifiers, out_stream);
    Write_Memory(block->memory_flags, out_stream);
    Write_Storage(block->storage, out_stream);
    if (!block->name) {
        out_stream << ";\n";
        return;
    }
    out_stream << block->name << " {\n";
    ++nest_level_;
    for (int i = 0; i < int(block->fields.size()); ++i) {
        Write_Tabs(out_stream);
        Write_Variable(block->fields[i], out_stream);
        out_stream << ";\n";
    }
    --nest_level_;
    out_stream << "}";
    bool first = true;
    for (int i = 0; i < int(tu_->globals.size()); ++i) {
        if (tu_->globals[i]->base_type == block) {
            if (first) {
                out_stream << " " << tu_->globals[i]->name;
            } else {
                out_stream << ", " << tu_->globals[i]->name;
            }
            if (tu_->globals[i]->flags & eVariableFlags::Array) {
                Write_ArraySize(tu_->globals[i]->array_sizes, out_stream);
            }
            first = false;
            written_globals_.Insert(tu_->globals[i]);
        }
    }
    out_stream << ";\n";
}

void glslx::WriterGLSL::Write_VersionDirective(const ast_version_directive *version, std::ostream &out_stream) {
    out_stream << "#version " << version->number;
    switch (version->type) {
    case eVerType::Core:
        out_stream << " core\n";
        break;
    case eVerType::Compatibility:
        out_stream << " compatibility\n";
        break;
    case eVerType::ES:
        out_stream << " es\n";
        break;
    }
}

void glslx::WriterGLSL::Write_ExtensionDirective(const ast_extension_directive *extension, std::ostream &out_stream) {
    out_stream << "#extension " << extension->name << " : ";
    switch (extension->behavior) {
    case eExtBehavior::Enable:
        out_stream << "enable\n";
        break;
    case eExtBehavior::Require:
        out_stream << "require\n";
        break;
    case eExtBehavior::Warn:
        out_stream << "warn\n";
        break;
    case eExtBehavior::Disable:
        out_stream << "disable\n";
        break;
    default:
        break;
    }
}

void glslx::WriterGLSL::Write_DefaultPrecision(const ast_default_precision *precision, std::ostream &out_stream) {
    out_stream << "precision ";
    Write_Precision(precision->precision, out_stream);
    Write_Type(precision->type, out_stream);
    out_stream << ";\n";
}

void glslx::WriterGLSL::Write(const TrUnit *tu, std::ostream &out_stream) {
    tu_ = tu;

    if (tu->version) {
        Write_VersionDirective(tu->version, out_stream);
    }
    for (int i = 0; i < int(tu->extensions.size()); ++i) {
        Write_ExtensionDirective(tu->extensions[i], out_stream);
    }
    for (int i = 0; i < int(tu->default_precision.size()); ++i) {
        Write_DefaultPrecision(tu->default_precision[i], out_stream);
    }
    for (int i = 0; i < int(tu->globals.size()); ++i) {
        if (!tu->globals[i]->base_type->builtin) {
            continue;
        }
        if (!(tu->globals[i]->flags & eVariableFlags ::Hidden) || (tu->globals[i]->flags & eVariableFlags::Invariant) ||
            config_.write_hidden) {
            Write_GlobalVariable(tu->globals[i], out_stream);
        }
    }
    for (int i = 0; i < int(tu->structures.size()); ++i) {
        Write_Structure(tu->structures[i], out_stream);
    }
    for (int i = 0; i < int(tu->interface_blocks.size()); ++i) {
        Write_InterfaceBlock(tu->interface_blocks[i], out_stream);
    }
    for (int i = 0; i < int(tu->globals.size()); ++i) {
        if (tu->globals[i]->base_type->builtin) {
            continue;
        }
        if (!(tu->globals[i]->flags & eVariableFlags::Hidden) || config_.write_hidden) {
            Write_GlobalVariable(tu->globals[i], out_stream);
        }
    }
    for (int i = 0; i < int(tu->functions.size()); ++i) {
        Write_Function(tu->functions[i], out_stream);
    }
}
