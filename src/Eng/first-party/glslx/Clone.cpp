#include "Clone.h"

glslx::ast_version_directive *glslx::Clone::Clone_VersionDirective(const ast_version_directive *in) {
    ast_version_directive *ret = dst_->make<ast_version_directive>();
    ret->type = in->type;
    ret->number = in->number;
    return ret;
}

glslx::ast_extension_directive *glslx::Clone::Clone_ExtensionDirective(const ast_extension_directive *in) {
    ast_extension_directive *ret = dst_->make<ast_extension_directive>();
    ret->behavior = in->behavior;
    ret->name = *dst_->str.Find(in->name);
    return ret;
}

glslx::ast_default_precision *glslx::Clone::Clone_DefaultPrecision(const ast_default_precision *in) {
    ast_default_precision *ret = dst_->make<ast_default_precision>();
    ret->precision = in->precision;
    ret->type = dst_->FindOrAddBuiltin(in->type->type);
    return ret;
}

glslx::ast_variable_identifier *glslx::Clone::Clone_VariableIdentifier(const ast_variable_identifier *in) {
    return dst_->make<ast_variable_identifier>(*variables_.Find(in->variable));
}

glslx::ast_field_or_swizzle *glslx::Clone::Clone_FieldOrSwizzle(const ast_field_or_swizzle *in) {
    ast_field_or_swizzle *ret = dst_->make<ast_field_or_swizzle>();
    ret->operand = Clone_Expression(in->operand);
    if (in->field) {
        ret->field = Clone_VariableIdentifier(in->field);
    }
    ret->name = *dst_->str.Find(in->name);
    return ret;
}

glslx::ast_array_subscript *glslx::Clone::Clone_ArraySubscript(const ast_array_subscript *in) {
    ast_array_subscript *ret = dst_->make<ast_array_subscript>();
    ret->operand = Clone_Expression(in->operand);
    ret->index = Clone_Expression(in->index);
    return ret;
}

glslx::ast_function_call *glslx::Clone::Clone_FunctionCall(const ast_function_call *in) {
    ast_function_call *ret = dst_->make<ast_function_call>(dst_->alloc.allocator);
    ret->name = *dst_->str.Find(in->name);
    if (in->func) {
        ret->func = *functions_.Find(in->func);
    }
    for (const ast_expression *expr : in->parameters) {
        ret->parameters.push_back(Clone_Expression(expr));
    }
    return ret;
}

glslx::ast_constructor_call *glslx::Clone::Clone_ConstructorCall(const ast_constructor_call *in) {
    ast_constructor_call *ret = dst_->make<ast_constructor_call>(dst_->alloc.allocator);
    if (in->type->builtin) {
        ret->type = FindOrAddBuiltin(static_cast<const ast_builtin *>(in->type)->type);
    } else {
        ret->type = *dst_->structures_by_name.Find(static_cast<const ast_struct *>(in->type)->name);
    }
    for (const ast_expression *expr : in->parameters) {
        ret->parameters.push_back(Clone_Expression(expr));
    }
    return ret;
}

glslx::ast_assignment_expression *glslx::Clone::Clone_Assignment(const ast_assignment_expression *in) {
    ast_assignment_expression *ret = dst_->make<ast_assignment_expression>(in->oper);
    ret->operand1 = Clone_Expression(in->operand1);
    ret->operand2 = Clone_Expression(in->operand2);
    return ret;
}

glslx::ast_sequence_expression *glslx::Clone::Clone_Sequence(const ast_sequence_expression *in) {
    ast_sequence_expression *ret = dst_->make<ast_sequence_expression>();
    ret->operand1 = Clone_Expression(in->operand1);
    ret->operand2 = Clone_Expression(in->operand2);
    return ret;
}

glslx::ast_operation_expression *glslx::Clone::Clone_Operation(const ast_operation_expression *in) {
    ast_operation_expression *ret = dst_->make<ast_operation_expression>(in->oper);
    ret->operand1 = Clone_Expression(in->operand1);
    ret->operand2 = Clone_Expression(in->operand2);
    return ret;
}

glslx::ast_ternary_expression *glslx::Clone::Clone_Ternary(const ast_ternary_expression *in) {
    ast_ternary_expression *ret = dst_->make<ast_ternary_expression>();
    ret->condition = Clone_Expression(in->condition);
    ret->on_true = Clone_Expression(in->on_true);
    ret->on_false = Clone_Expression(in->on_false);
    return ret;
}

glslx::ast_array_specifier *glslx::Clone::Clone_ArraySpecifier(const ast_array_specifier *in) {
    ast_array_specifier *ret = dst_->make<ast_array_specifier>(dst_->alloc.allocator);
    for (const ast_expression *expr : in->expressions) {
        ret->expressions.push_back(Clone_Expression(expr));
    }
    return ret;
}

glslx::ast_expression *glslx::Clone::Clone_Expression(const ast_expression *in) {
    switch (in->type) {
    case eExprType::ShortConstant:
        return Clone_Constant(static_cast<const ast_short_constant *>(in));
    case eExprType::UShortConstant:
        return Clone_Constant(static_cast<const ast_ushort_constant *>(in));
    case eExprType::IntConstant:
        return Clone_Constant(static_cast<const ast_int_constant *>(in));
    case eExprType::UIntConstant:
        return Clone_Constant(static_cast<const ast_uint_constant *>(in));
    case eExprType::LongConstant:
        return Clone_Constant(static_cast<const ast_long_constant *>(in));
    case eExprType::ULongConstant:
        return Clone_Constant(static_cast<const ast_ulong_constant *>(in));
    case eExprType::HalfConstant:
        return Clone_Constant(static_cast<const ast_half_constant *>(in));
    case eExprType::FloatConstant:
        return Clone_Constant(static_cast<const ast_float_constant *>(in));
    case eExprType::DoubleConstant:
        return Clone_Constant(static_cast<const ast_double_constant *>(in));
    case eExprType::BoolConstant:
        return Clone_Constant(static_cast<const ast_bool_constant *>(in));
    case eExprType::VariableIdentifier:
        return Clone_VariableIdentifier(static_cast<const ast_variable_identifier *>(in));
    case eExprType::FieldOrSwizzle:
        return Clone_FieldOrSwizzle(static_cast<const ast_field_or_swizzle *>(in));
    case eExprType::ArraySubscript:
        return Clone_ArraySubscript(static_cast<const ast_array_subscript *>(in));
    case eExprType::FunctionCall:
        return Clone_FunctionCall(static_cast<const ast_function_call *>(in));
    case eExprType::ConstructorCall:
        return Clone_ConstructorCall(static_cast<const ast_constructor_call *>(in));
    case eExprType::PostIncrement:
        return dst_->make<ast_post_increment_expression>(
            Clone_Expression(static_cast<const ast_post_increment_expression *>(in)->operand));
    case eExprType::PostDecrement:
        return dst_->make<ast_post_decrement_expression>(
            Clone_Expression(static_cast<const ast_post_decrement_expression *>(in)->operand));
    case eExprType::UnaryPlus:
        return dst_->make<ast_unary_plus_expression>(
            Clone_Expression(static_cast<const ast_unary_plus_expression *>(in)->operand));
    case eExprType::UnaryMinus:
        return dst_->make<ast_unary_minus_expression>(
            Clone_Expression(static_cast<const ast_unary_minus_expression *>(in)->operand));
    case eExprType::BitNot:
        return dst_->make<ast_unary_bit_not_expression>(
            Clone_Expression(static_cast<const ast_unary_bit_not_expression *>(in)->operand));
    case eExprType::LogicalNot:
        return dst_->make<ast_unary_logical_not_expression>(
            Clone_Expression(static_cast<const ast_unary_logical_not_expression *>(in)->operand));
    case eExprType::PrefixIncrement:
        return dst_->make<ast_prefix_increment_expression>(
            Clone_Expression(static_cast<const ast_prefix_increment_expression *>(in)->operand));
    case eExprType::PrefixDecrement:
        return dst_->make<ast_prefix_decrement_expression>(
            Clone_Expression(static_cast<const ast_prefix_decrement_expression *>(in)->operand));
    case eExprType::Assign:
        return Clone_Assignment(static_cast<const ast_assignment_expression *>(in));
    case eExprType::Sequence:
        return Clone_Sequence(static_cast<const ast_sequence_expression *>(in));
    case eExprType::Operation:
        return Clone_Operation(static_cast<const ast_operation_expression *>(in));
    case eExprType::Ternary:
        return Clone_Ternary(static_cast<const ast_ternary_expression *>(in));
    case eExprType::ArraySpecifier:
        return Clone_ArraySpecifier(static_cast<const ast_array_specifier *>(in));
    }
    return nullptr;
}

glslx::ast_type *glslx::Clone::Clone_Type(const ast_type *in) {
    if (in->builtin) {
        return dst_->make<ast_builtin>(static_cast<const ast_builtin *>(in)->type);
    } else {
        return nullptr;
    }
}

glslx::ast_struct *glslx::Clone::Clone_Structure(const ast_struct *in) {
    ast_struct *ret = dst_->make<ast_struct>(dst_->alloc.allocator);
    if (in->name) {
        ret->name = *dst_->str.Find(in->name);
    }
    for (const ast_variable *f : in->fields) {
        ret->fields.push_back(Clone_Variable(f));
        variables_.Insert(f, ret->fields.back());
    }
    return ret;
}

glslx::ast_variable *glslx::Clone::Clone_Variable(const ast_variable *in) {
    if (in->type == eVariableType::Function) {
        return Clone_FunctionVariable(static_cast<const ast_function_variable *>(in));
    } else if (in->type == eVariableType::Parameter) {
        return Clone_FunctionParameter(static_cast<const ast_function_parameter *>(in));
    } else if (in->type == eVariableType::Global) {
        return Clone_GlobalVariable(static_cast<const ast_global_variable *>(in));
    }
    ast_variable *ret = dst_->make<ast_variable>(in->type, dst_->alloc.allocator);
    ret->type = in->type;
    ret->flags = in->flags;
    ret->precision = in->precision;
    ret->name = *dst_->str.Find(in->name);
    if (in->base_type->builtin) {
        ret->base_type = FindOrAddBuiltin(static_cast<const ast_builtin *>(in->base_type)->type);
    } else {
        ast_struct **type = dst_->structures_by_name.Find(static_cast<const ast_struct *>(in->base_type)->name);
        if (type) {
            ret->base_type = *type;
        } else {
            ret->base_type = *interface_blocks_.Find(static_cast<const ast_interface_block *>(in->base_type));
        }
    }
    for (const ast_constant_expression *expr : in->array_sizes) {
        ast_constant_expression *new_expr = nullptr;
        if (expr) {
            new_expr = Clone_Expression(expr);
        }
        ret->array_sizes.push_back(new_expr);
    }
    return ret;
}

glslx::ast_function_parameter *glslx::Clone::Clone_FunctionParameter(const ast_function_parameter *in) {
    ast_function_parameter *ret = dst_->make<ast_function_parameter>(dst_->alloc.allocator);
    ret->type = in->type;
    ret->flags = in->flags;
    ret->precision = in->precision;
    if (in->name) {
        ret->name = *dst_->str.Find(in->name);
    }
    if (in->base_type->builtin) {
        ret->base_type = FindOrAddBuiltin(static_cast<const ast_builtin *>(in->base_type)->type);
    } else {
        ret->base_type = *dst_->structures_by_name.Find(static_cast<const ast_struct *>(in->base_type)->name);
    }
    for (const ast_constant_expression *expr : in->array_sizes) {
        ast_constant_expression *new_expr = nullptr;
        if (expr) {
            new_expr = Clone_Expression(expr);
        }
        ret->array_sizes.push_back(new_expr);
    }
    ret->qualifiers = in->qualifiers;
    return ret;
}

glslx::ast_function_variable *glslx::Clone::Clone_FunctionVariable(const ast_function_variable *in) {
    ast_function_variable *ret = dst_->make<ast_function_variable>(dst_->alloc.allocator);
    ret->type = in->type;
    ret->flags = in->flags;
    ret->precision = in->precision;
    ret->name = *dst_->str.Find(in->name);
    if (in->base_type->builtin) {
        ret->base_type = FindOrAddBuiltin(static_cast<const ast_builtin *>(in->base_type)->type);
    } else {
        ret->base_type = *dst_->structures_by_name.Find(static_cast<const ast_struct *>(in->base_type)->name);
    }
    for (const ast_constant_expression *expr : in->array_sizes) {
        ast_constant_expression *new_expr = nullptr;
        if (expr) {
            new_expr = Clone_Expression(expr);
        }
        ret->array_sizes.push_back(new_expr);
    }
    if (in->initial_value) {
        ret->initial_value = Clone_Expression(in->initial_value);
    }
    return ret;
}

glslx::ast_global_variable *glslx::Clone::Clone_GlobalVariable(const ast_global_variable *in) {
    ast_global_variable *ret = dst_->make<ast_global_variable>(dst_->alloc.allocator);
    ret->type = in->type;
    ret->flags = in->flags;
    ret->precision = in->precision;
    ret->name = *dst_->str.Find(in->name);
    if (in->base_type->builtin) {
        ret->base_type = FindOrAddBuiltin(static_cast<const ast_builtin *>(in->base_type)->type);
    } else {
        ast_struct **type = dst_->structures_by_name.Find(static_cast<const ast_struct *>(in->base_type)->name);
        if (type) {
            ret->base_type = *type;
        } else {
            ret->base_type = *interface_blocks_.Find(static_cast<const ast_interface_block *>(in->base_type));
        }
    }
    for (const ast_constant_expression *expr : in->array_sizes) {
        ast_constant_expression *new_expr = nullptr;
        if (expr) {
            new_expr = Clone_Expression(expr);
        }
        ret->array_sizes.push_back(new_expr);
    }
    ret->storage = in->storage;
    ret->aux_storage = in->aux_storage;
    ret->memory_flags = in->memory_flags;
    ret->interpolation = in->interpolation;
    if (in->initial_value) {
        ret->initial_value = Clone_Expression(in->initial_value);
    }
    for (const ast_layout_qualifier *lq : in->layout_qualifiers) {
        ret->layout_qualifiers.push_back(Clone_LayoutQualifier(lq));
    }
    return ret;
}

glslx::ast_layout_qualifier *glslx::Clone::Clone_LayoutQualifier(const ast_layout_qualifier *in) {
    ast_layout_qualifier *ret = dst_->make<ast_layout_qualifier>();
    ret->name = *dst_->str.Find(in->name);
    if (in->initial_value) {
        ret->initial_value = Clone_Expression(in->initial_value);
    }
    return ret;
}

glslx::ast_interface_block *glslx::Clone::Clone_InterfaceBlock(const ast_interface_block *in) {
    ast_interface_block *ret = dst_->make<ast_interface_block>(dst_->alloc.allocator);
    if (in->name) {
        ret->name = *dst_->str.Find(in->name);
    }
    for (const ast_variable *f : in->fields) {
        ret->fields.push_back(Clone_Variable(f));
        variables_.Insert(f, ret->fields.back());
    }
    ret->storage = in->storage;
    ret->memory_flags = in->memory_flags;
    for (const ast_layout_qualifier *lq : in->layout_qualifiers) {
        ret->layout_qualifiers.push_back(Clone_LayoutQualifier(lq));
    }
    return ret;
}

glslx::ast_compound_statement *glslx::Clone::Clone_CompundStatement(const ast_compound_statement *in) {
    ast_compound_statement *ret = dst_->make<ast_compound_statement>(dst_->alloc.allocator);
    for (const ast_statement *s : in->statements) {
        ret->statements.push_back(Clone_Statement(s));
    }
    return ret;
}

glslx::ast_declaration_statement *glslx::Clone::Clone_DeclarationStatement(const ast_declaration_statement *in) {
    ast_declaration_statement *ret = dst_->make<ast_declaration_statement>(dst_->alloc.allocator);
    for (const ast_function_variable *var : in->variables) {
        ret->variables.push_back(Clone_FunctionVariable(var));
        variables_.Insert(var, ret->variables.back());
    }
    return ret;
}

glslx::ast_expression_statement *glslx::Clone::Clone_ExpressionStatement(const ast_expression_statement *in) {
    return dst_->make<ast_expression_statement>(Clone_Expression(in->expression));
}

glslx::ast_if_statement *glslx::Clone::Clone_IfStatement(const ast_if_statement *in) {
    ast_if_statement *ret = dst_->make<ast_if_statement>(in->attributes);
    ret->condition = Clone_Expression(in->condition);
    if (in->then_statement) {
        ret->then_statement = Clone_Statement(in->then_statement);
    }
    if (in->else_statement) {
        ret->else_statement = Clone_Statement(in->else_statement);
    }
    return ret;
}

glslx::ast_switch_statement *glslx::Clone::Clone_SwitchStatement(const ast_switch_statement *in) {
    ast_switch_statement *ret = dst_->make<ast_switch_statement>(dst_->alloc.allocator, in->attributes);
    ret->expression = Clone_Expression(in->expression);
    for (const ast_statement *s : in->statements) {
        ret->statements.push_back(Clone_Statement(s));
    }
    return ret;
}

glslx::ast_case_label_statement *glslx::Clone::Clone_CaseLabelStatement(const ast_case_label_statement *in) {
    ast_case_label_statement *ret = dst_->make<ast_case_label_statement>();
    ret->flags = in->flags;
    if (in->condition) {
        ret->condition = Clone_Expression(in->condition);
    }
    return ret;
}

glslx::ast_while_statement *glslx::Clone::Clone_WhileStatement(const ast_while_statement *in) {
    ast_while_statement *ret = dst_->make<ast_while_statement>(in->flow_params);
    switch (in->condition->type) {
    case eStatement::Declaration:
        ret->condition = Clone_DeclarationStatement(static_cast<const ast_declaration_statement *>(in->condition));
        break;
    case eStatement::Expression:
        ret->condition = Clone_ExpressionStatement(static_cast<const ast_expression_statement *>(in->condition));
        break;
    default:
        break;
    }
    if (in->body) {
        ret->body = Clone_Statement(in->body);
    }
    return ret;
}

glslx::ast_do_statement *glslx::Clone::Clone_DoStatement(const ast_do_statement *in) {
    ast_do_statement *ret = dst_->make<ast_do_statement>(in->flow_params);
    ret->body = Clone_Statement(in->body);
    ret->condition = Clone_Expression(in->condition);
    return ret;
}

glslx::ast_for_statement *glslx::Clone::Clone_ForStatement(const ast_for_statement *in) {
    ast_for_statement *ret = dst_->make<ast_for_statement>(in->flow_params);
    if (in->init) {
        switch (in->init->type) {
        case eStatement::Declaration:
            ret->init = Clone_DeclarationStatement(static_cast<ast_declaration_statement *>(in->init));
            break;
        case eStatement::Expression:
            ret->init = Clone_ExpressionStatement(static_cast<ast_expression_statement *>(in->init));
            break;
        default:
            break;
        }
    }
    if (in->condition) {
        ret->condition = Clone_Expression(in->condition);
    }
    if (in->loop) {
        ret->loop = Clone_Expression(in->loop);
    }
    ret->body = Clone_Statement(in->body);
    return ret;
}

glslx::ast_return_statement *glslx::Clone::Clone_ReturnStatement(const ast_return_statement *in) {
    ast_return_statement *ret = dst_->make<ast_return_statement>();
    if (in->expression) {
        ret->expression = Clone_Expression(in->expression);
    }
    return ret;
}

glslx::ast_statement *glslx::Clone::Clone_Statement(const ast_statement *in) {
    switch (in->type) {
    case eStatement::Compound:
        return Clone_CompundStatement(static_cast<const ast_compound_statement *>(in));
    case eStatement::Empty:
        return dst_->make<ast_empty_statement>();
    case eStatement::Declaration:
        return Clone_DeclarationStatement(static_cast<const ast_declaration_statement *>(in));
    case eStatement::Expression:
        return Clone_ExpressionStatement(static_cast<const ast_expression_statement *>(in));
    case eStatement::If:
        return Clone_IfStatement(static_cast<const ast_if_statement *>(in));
    case eStatement::Switch:
        return Clone_SwitchStatement(static_cast<const ast_switch_statement *>(in));
    case eStatement::CaseLabel:
        return Clone_CaseLabelStatement(static_cast<const ast_case_label_statement *>(in));
    case eStatement::While:
        return Clone_WhileStatement(static_cast<const ast_while_statement *>(in));
    case eStatement::Do:
        return Clone_DoStatement(static_cast<const ast_do_statement *>(in));
    case eStatement::For:
        return Clone_ForStatement(static_cast<const ast_for_statement *>(in));
    case eStatement::Continue:
        return dst_->make<ast_continue_statement>();
    case eStatement::Break:
        return dst_->make<ast_break_statement>();
    case eStatement::Return:
        return Clone_ReturnStatement(static_cast<const ast_return_statement *>(in));
    case eStatement::Discard:
        return dst_->make<ast_discard_statement>();
    case eStatement::ExtJump:
        return dst_->make<ast_ext_jump_statement>(static_cast<const ast_ext_jump_statement *>(in)->keyword);
    default:
        break;
    }
    return nullptr;
}

glslx::ast_function *glslx::Clone::Clone_Function(const ast_function *in) {
    ast_function *ret = dst_->make<ast_function>(dst_->alloc.allocator);
    ret->attributes = in->attributes;
    if (in->return_type->builtin) {
        ret->return_type = FindOrAddBuiltin(static_cast<const ast_builtin *>(in->return_type)->type);
    } else {
        ret->return_type = *dst_->structures_by_name.Find(static_cast<const ast_struct *>(in->return_type)->name);
    }
    ret->name = *dst_->str.Find(in->name);
    for (const ast_function_parameter *p : in->parameters) {
        ret->parameters.push_back(Clone_FunctionParameter(p));
        variables_.Insert(p, ret->parameters.back());
    }
    for (const ast_statement *s : in->statements) {
        ret->statements.push_back(Clone_Statement(s));
    }
    if (in->prototype) {
        ret->prototype = *functions_.Find(in->prototype);
    }
    return ret;
}

glslx::ast_builtin *glslx::Clone::FindOrAddBuiltin(const eKeyword type) {
    const auto it = std::lower_bound(std::begin(dst_->builtins), std::end(dst_->builtins), type,
                                     [](const ast_builtin *lhs, const eKeyword rhs) { return lhs->type < rhs; });
    if (it != std::end(dst_->builtins) && type == (*it)->type) {
        return *it;
    }
    return *dst_->builtins.insert(it, dst_->make<ast_builtin>(type));
}

std::unique_ptr<glslx::TrUnit> glslx::Clone::CloneAST(const TrUnit *tu) {
    if (!tu) {
        return {};
    }
    src_ = tu;
    dst_ = std::make_unique<TrUnit>();
    dst_->type = tu->type;
    if (tu->version) {
        dst_->version = Clone_VersionDirective(tu->version);
    }
    for (const char *str : tu->str) {
        dst_->str.Insert(dst_->makestr(str));
    }
    for (const ast_extension_directive *extension : tu->extensions) {
        dst_->extensions.push_back(Clone_ExtensionDirective(extension));
    }
    for (const ast_default_precision *precision : tu->default_precision) {
        dst_->default_precision.push_back(Clone_DefaultPrecision(precision));
    }
    for (const ast_struct *structure : tu->structures) {
        dst_->structures.push_back(Clone_Structure(structure));
        dst_->structures_by_name.Insert(dst_->structures.back()->name, dst_->structures.back());
    }
    for (const ast_interface_block *block : tu->interface_blocks) {
        dst_->interface_blocks.push_back(Clone_InterfaceBlock(block));
        interface_blocks_.Insert(block, dst_->interface_blocks.back());
    }
    for (const ast_global_variable *var : tu->globals) {
        dst_->globals.push_back(Clone_GlobalVariable(var));
        variables_.Insert(var, dst_->globals.back());
    }
    for (const ast_function *f : tu->functions) {
        dst_->functions.push_back(Clone_Function(f));
        functions_.Insert(f, dst_->functions.back());
        auto *p_find = dst_->functions_by_name.Find(f->name);
        if (p_find) {
            p_find->push_back(dst_->functions.back());
        } else {
            dst_->functions_by_name.Insert(dst_->functions.back()->name, {dst_->functions.back()});
        }
    }
    return std::move(dst_);
}