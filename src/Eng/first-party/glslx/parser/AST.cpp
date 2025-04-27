#include "AST.h"

namespace glslx {
static const char *g_statement_names[] = {
    "invalid",     // Invalid
    "compound",    // Compound
    "empty",       // Empty
    "declaration", // Declaration
    "expression",  // Expression
    "if",          // If
    "switch",      // Switch
    "case label",  // CaseLabel
    "while",       // While
    "do",          // Do
    "for",         // For
    "continue",    // Continue
    "break",       // Break
    "return",      // Return
    "discard",     // Discard
    "ext jump"     // ExtJump
};
static_assert(std::size(g_statement_names) == int(eStatement::_Count));

int str_compare(const char *lhs, const char *rhs) {
    if (lhs == nullptr || rhs == nullptr) {
        if (lhs == rhs) {
            return 0;
        }
        return (lhs == nullptr) ? -1 : +1;
    }
    return strcmp(lhs, rhs);
}
} // namespace glslx

glslx::TrUnit::~TrUnit() {
    for (char *_str : str) {
        alloc.allocator.deallocate(_str, strlen(_str) + 1);
    }
    for (ast_memory &mem : alloc.allocations) {
        mem.destroy(alloc.allocator);
    }
}

char *glslx::TrUnit::makestr(const char *s) {
    if (!s) {
        return nullptr;
    }
    char **existing = str.Find(s);
    if (existing) {
        return *existing;
    }
    const size_t size = strlen(s) + 1;
    char *copy = alloc.allocator.allocate(size);
    if (!copy) {
        return nullptr;
    }
    memcpy(copy, s, size);
    str.Insert(copy);
    return copy;
}

glslx::ast_builtin *glslx::TrUnit::FindBuiltin(const eKeyword type) const {
    const int index = FindBuiltinIndex(type);
    if (index != -1) {
        return builtins[index];
    }
    return nullptr;
}

int glslx::TrUnit::FindBuiltinIndex(const eKeyword type) const {
    const auto it = std::lower_bound(std::begin(builtins), std::end(builtins), type,
                                     [](const ast_builtin *lhs, const eKeyword rhs) { return lhs->type < rhs; });
    if (it != std::end(builtins) && type == (*it)->type) {
        return int(std::distance(std::begin(builtins), it));
    }
    return -1;
}

glslx::ast_builtin *glslx::TrUnit::FindOrAddBuiltin(const eKeyword type) {
    const auto it = std::lower_bound(std::begin(builtins), std::end(builtins), type,
                                     [](const ast_builtin *lhs, const eKeyword rhs) { return lhs->type < rhs; });
    if (it != std::end(builtins) && type == (*it)->type) {
        return *it;
    }
    return *builtins.insert(it, make<ast_builtin>(type));
}

int glslx::Compare(const TrUnit *lhs, const TrUnit *rhs) {
    if (lhs == nullptr || rhs == nullptr) {
        if ((lhs == nullptr) != (rhs == nullptr)) {
            return rhs == nullptr ? -1 : +1;
        }
        return 0;
    }
    if (lhs->type != rhs->type) {
        return lhs->type < rhs->type ? -1 : +1;
    }
    const int version_cmp = Compare(lhs->version, rhs->version);
    if (version_cmp != 0) {
        return version_cmp;
    }
    const int ext_cmp = Compare_PointerSpans<ast_extension_directive *>(lhs->extensions, rhs->extensions);
    if (ext_cmp != 0) {
        return ext_cmp;
    }
    const int def_cmp = Compare_PointerSpans<ast_default_precision *>(lhs->default_precision, rhs->default_precision);
    if (def_cmp != 0) {
        return def_cmp;
    }
    const int inter_blocks_cmp =
        Compare_PointerSpans<ast_interface_block *>(lhs->interface_blocks, rhs->interface_blocks);
    if (inter_blocks_cmp != 0) {
        return inter_blocks_cmp;
    }
    const int builtins_cmp = Compare_PointerSpans<ast_builtin *>(lhs->builtins, rhs->builtins);
    if (builtins_cmp != 0) {
        return builtins_cmp;
    }
    const int structs_cmp = Compare_PointerSpans<ast_struct *>(lhs->structures, rhs->structures);
    if (structs_cmp != 0) {
        return structs_cmp;
    }
    const int globals_cmp = Compare_PointerSpans<ast_global_variable *>(lhs->globals, rhs->globals);
    if (globals_cmp != 0) {
        return structs_cmp;
    }
    const int functions_cmp = Compare_PointerSpans<ast_function *>(lhs->functions, rhs->functions);
    if (functions_cmp != 0) {
        return functions_cmp;
    }
    return 0;
}

int glslx::Compare(const ast_type *lhs, const ast_type *rhs) {
    if (lhs == nullptr || rhs == nullptr) {
        if (lhs == rhs) {
            return 0;
        }
        return (lhs == nullptr) ? -1 : +1;
    }
    if (lhs->builtin != rhs->builtin) {
        return lhs->builtin ? -1 : +1;
    }
    if (lhs->builtin) {
        return Compare(static_cast<const ast_builtin *>(lhs), static_cast<const ast_builtin *>(rhs));
    }
    return Compare(static_cast<const ast_struct *>(lhs), static_cast<const ast_struct *>(rhs));
}

int glslx::Compare(const ast_builtin *lhs, const ast_builtin *rhs) {
    if (lhs == nullptr || rhs == nullptr) {
        if (lhs == rhs) {
            return 0;
        }
        return (lhs == nullptr) ? -1 : +1;
    }
    if (lhs->type != rhs->type) {
        return lhs->type < rhs->type ? -1 : +1;
    }
    return 0;
}

int glslx::Compare(const ast_struct *lhs, const ast_struct *rhs) {
    if (lhs == nullptr || rhs == nullptr) {
        if (lhs == rhs) {
            return 0;
        }
        return (lhs == nullptr) ? -1 : +1;
    }
    const int name_cmp = str_compare(lhs->name, rhs->name);
    if (name_cmp != 0) {
        return name_cmp;
    }
    return Compare_PointerSpans<ast_variable *>(lhs->fields, rhs->fields);
}

int glslx::Compare(const ast_interface_block *lhs, const ast_interface_block *rhs) {
    if (lhs == nullptr || rhs == nullptr) {
        if (lhs == rhs) {
            return 0;
        }
        return (lhs == nullptr) ? -1 : +1;
    }
    const int name_cmp = str_compare(lhs->name, rhs->name);
    if (name_cmp != 0) {
        return name_cmp;
    }
    const int fields_cmp = Compare_PointerSpans<ast_variable *>(lhs->fields, rhs->fields);
    if (fields_cmp != 0) {
        return fields_cmp;
    }
    if (lhs->storage != rhs->storage) {
        return lhs->storage < rhs->storage ? -1 : +1;
    }
    if (lhs->memory_flags != rhs->memory_flags) {
        return lhs->memory_flags < rhs->memory_flags ? -1 : +1;
    }
    return Compare_PointerSpans<ast_layout_qualifier *>(lhs->layout_qualifiers, rhs->layout_qualifiers);
}

int glslx::Compare(const ast_version_directive *lhs, const ast_version_directive *rhs) {
    if (lhs == nullptr || rhs == nullptr) {
        if (lhs == rhs) {
            return 0;
        }
        return (lhs == nullptr) ? -1 : +1;
    }
    if (lhs->type < rhs->type) {
        return true;
    } else if (lhs->type == rhs->type) {
        return lhs->number < rhs->number;
    }
    return 0;
}

int glslx::Compare(const ast_extension_directive *lhs, const ast_extension_directive *rhs) {
    if (lhs == nullptr || rhs == nullptr) {
        if (lhs == rhs) {
            return 0;
        }
        return (lhs == nullptr) ? -1 : +1;
    }
    if (lhs->behavior < rhs->behavior) {
        return true;
    } else if (lhs->behavior == rhs->behavior) {
        return str_compare(lhs->name, rhs->name);
    }
    return 0;
}

int glslx::Compare(const ast_default_precision *lhs, const ast_default_precision *rhs) {
    if (lhs == nullptr || rhs == nullptr) {
        if (lhs == rhs) {
            return 0;
        }
        return (lhs == nullptr) ? -1 : +1;
    }
    if (lhs->precision < rhs->precision) {
        return true;
    } else if (lhs->precision == rhs->precision) {
        return Compare(lhs->type, rhs->type);
    }
    return 0;
}

int glslx::Compare(const ast_variable *lhs, const ast_variable *rhs) {
    if (lhs == nullptr || rhs == nullptr) {
        if (lhs == rhs) {
            return 0;
        }
        return (lhs == nullptr) ? -1 : +1;
    }
    if (lhs->type != rhs->type) {
        return lhs->type < rhs->type ? -1 : +1;
    }
    if (lhs->flags != rhs->flags) {
        return lhs->flags < rhs->flags ? -1 : +1;
    }
    if (lhs->precision != rhs->precision) {
        return lhs->precision < rhs->precision ? -1 : +1;
    }
    const int name_cmp = str_compare(lhs->name, rhs->name);
    if (name_cmp != 0) {
        return name_cmp;
    }
    const int type_cmp = Compare(lhs->base_type, rhs->base_type);
    if (type_cmp != 0) {
        return type_cmp;
    }
    const int arr_sizes_cmp = Compare_PointerSpans<ast_constant_expression *>(lhs->array_sizes, rhs->array_sizes);
    if (arr_sizes_cmp != 0) {
        return arr_sizes_cmp;
    }
    if (lhs->type == eVariableType::Function) {
        const auto *_lhs = static_cast<const ast_function_variable *>(lhs),
                   *_rhs = static_cast<const ast_function_variable *>(rhs);
        return Compare(_lhs->initial_value, _rhs->initial_value);
    } else if (lhs->type == eVariableType::Parameter) {
        const auto *_lhs = static_cast<const ast_function_parameter *>(lhs),
                   *_rhs = static_cast<const ast_function_parameter *>(rhs);
        return _lhs->qualifiers < _rhs->qualifiers;
    } else if (lhs->type == eVariableType::Global) {
        const auto *_lhs = static_cast<const ast_global_variable *>(lhs),
                   *_rhs = static_cast<const ast_global_variable *>(rhs);
        if (_lhs->storage != _rhs->storage) {
            return _lhs->storage < _rhs->storage ? -1 : +1;
        }
        if (_lhs->aux_storage != _rhs->aux_storage) {
            return _lhs->aux_storage < _rhs->aux_storage ? -1 : +1;
        }
        if (_lhs->memory_flags != _rhs->memory_flags) {
            return _lhs->memory_flags < _rhs->memory_flags ? -1 : +1;
        }
        if (_lhs->interpolation != _rhs->interpolation) {
            return _lhs->interpolation < _rhs->interpolation ? -1 : +1;
        }
        const int init_cmp = Compare(_lhs->initial_value, _rhs->initial_value);
        if (init_cmp != 0) {
            return init_cmp;
        }
        return Compare_PointerSpans<ast_layout_qualifier *>(_lhs->layout_qualifiers, _rhs->layout_qualifiers);
    }
    return 0;
}

int glslx::Compare(const ast_layout_qualifier *lhs, const ast_layout_qualifier *rhs) {
    if (lhs == nullptr || rhs == nullptr) {
        if (lhs == rhs) {
            return 0;
        }
        return (lhs == nullptr) ? -1 : +1;
    }
    const int name_cmp = str_compare(lhs->name, rhs->name);
    if (name_cmp != 0) {
        return name_cmp;
    }
    return Compare(lhs->initial_value, rhs->initial_value);
}

int glslx::Compare(const ast_function *lhs, const ast_function *rhs) {
    if (lhs == nullptr || rhs == nullptr) {
        if (lhs == rhs) {
            return 0;
        }
        return (lhs == nullptr) ? -1 : +1;
    }
    if (lhs->attributes != rhs->attributes) {
        return lhs->attributes < rhs->attributes ? -1 : +1;
    }
    const int return_type_cmp = Compare(lhs->return_type, rhs->return_type);
    if (return_type_cmp != 0) {
        return return_type_cmp;
    }
    const int name_cmp = str_compare(lhs->name, rhs->name);
    if (name_cmp != 0) {
        return name_cmp;
    }
    const int params_cmp = Compare_PointerSpans<ast_function_parameter *>(lhs->parameters, rhs->parameters);
    if (params_cmp != 0) {
        return params_cmp;
    }
    const int statements_cmp = Compare_PointerSpans<ast_statement *>(lhs->statements, rhs->statements);
    if (statements_cmp != 0) {
        return statements_cmp;
    }
    return 0;
}

int glslx::Compare(const ast_statement *lhs, const ast_statement *rhs) {
    if (lhs == nullptr || rhs == nullptr) {
        if (lhs == rhs) {
            return 0;
        }
        return (lhs == nullptr) ? -1 : +1;
    }
    if (lhs->type != rhs->type) {
        return lhs->type < rhs->type ? -1 : +1;
    }
    switch (lhs->type) {
    case eStatement::Compound: {
        const auto *_lhs = static_cast<const ast_compound_statement *>(lhs),
                   *_rhs = static_cast<const ast_compound_statement *>(rhs);
        return Compare_PointerSpans<ast_statement *>(_lhs->statements, _rhs->statements);
    }
    case eStatement::Empty:
        break;
    case eStatement::Declaration: {
        const auto *_lhs = static_cast<const ast_declaration_statement *>(lhs),
                   *_rhs = static_cast<const ast_declaration_statement *>(rhs);
        return Compare_PointerSpans<ast_function_variable *>(_lhs->variables, _rhs->variables);
    }
    case eStatement::Expression: {
        const auto *_lhs = static_cast<const ast_expression_statement *>(lhs),
                   *_rhs = static_cast<const ast_expression_statement *>(rhs);
        return Compare(_lhs->expression, _rhs->expression);
    }
    case eStatement::If: {
        const auto *_lhs = static_cast<const ast_if_statement *>(lhs),
                   *_rhs = static_cast<const ast_if_statement *>(rhs);
        if (_lhs->attributes != _rhs->attributes) {
            return _lhs->attributes < _rhs->attributes ? -1 : +1;
        }
        const int cond_cmp = Compare(_lhs->condition, _rhs->condition);
        if (cond_cmp != 0) {
            return cond_cmp;
        }
        const int then_cmp = Compare(_lhs->then_statement, _rhs->then_statement);
        if (then_cmp != 0) {
            return then_cmp;
        }
        return Compare(_lhs->else_statement, _rhs->else_statement);
    }
    case eStatement::Switch: {
        const auto *_lhs = static_cast<const ast_switch_statement *>(lhs),
                   *_rhs = static_cast<const ast_switch_statement *>(rhs);
        if (_lhs->attributes != _rhs->attributes) {
            return _lhs->attributes < _rhs->attributes ? -1 : +1;
        }
        const int expr_cmp = Compare(_lhs->expression, _rhs->expression);
        if (expr_cmp != 0) {
            return expr_cmp;
        }
        return Compare_PointerSpans<ast_statement *>(_lhs->statements, _rhs->statements);
    }
    case eStatement::CaseLabel: {
        const auto *_lhs = static_cast<const ast_case_label_statement *>(lhs),
                   *_rhs = static_cast<const ast_case_label_statement *>(rhs);
        if (_lhs->flags != _rhs->flags) {
            return _lhs->flags < _rhs->flags ? -1 : +1;
        }
        return Compare(_lhs->condition, _rhs->condition);
    }
    case eStatement::While:
    case eStatement::Do:
    case eStatement::For:
        return Compare(static_cast<const ast_loop_statement *>(lhs), static_cast<const ast_loop_statement *>(rhs));
    case eStatement::ExtJump: {
        const auto *_lhs = static_cast<const ast_ext_jump_statement *>(lhs),
                   *_rhs = static_cast<const ast_ext_jump_statement *>(rhs);
        if (_lhs->keyword != _rhs->keyword) {
            return _lhs->keyword < _rhs->keyword ? -1 : +1;
        }
        break;
    }
    default:
        break;
    }
    return 0;
}

int glslx::Compare(const ast_loop_statement *lhs, const ast_loop_statement *rhs) {
    if (lhs == nullptr || rhs == nullptr) {
        if (lhs == rhs) {
            return 0;
        }
        return (lhs == nullptr) ? -1 : +1;
    }
    if (lhs->flow_params != rhs->flow_params) {
        return lhs->flow_params < rhs->flow_params ? -1 : +1;
    }
    switch (lhs->type) {
    case eStatement::While: {
        const auto *_lhs = static_cast<const ast_while_statement *>(lhs),
                   *_rhs = static_cast<const ast_while_statement *>(rhs);
        const int cond_cmp = Compare(_lhs->condition, _rhs->condition);
        if (cond_cmp != 0) {
            return cond_cmp;
        }
        return Compare(_lhs->body, _rhs->body);
    }
    case eStatement::Do: {
        const auto *_lhs = static_cast<const ast_do_statement *>(lhs),
                   *_rhs = static_cast<const ast_do_statement *>(rhs);
        const int cond_cmp = Compare(_lhs->condition, _rhs->condition);
        if (cond_cmp != 0) {
            return cond_cmp;
        }
        return Compare(_lhs->body, _rhs->body);
    }
    case eStatement::For: {
        const auto *_lhs = static_cast<const ast_for_statement *>(lhs),
                   *_rhs = static_cast<const ast_for_statement *>(rhs);
        const int init_cmp = Compare(_lhs->init, _rhs->init);
        if (init_cmp != 0) {
            return init_cmp;
        }
        const int cond_cmp = Compare(_lhs->condition, _rhs->condition);
        if (cond_cmp != 0) {
            return cond_cmp;
        }
        const int loop_cmp = Compare(_lhs->loop, _rhs->loop);
        if (loop_cmp != 0) {
            return loop_cmp;
        }
        return Compare(_lhs->body, _rhs->body);
    }
    default:
        break;
    }
    return 0;
}

int glslx::Compare(const ast_expression *lhs, const ast_expression *rhs) {
    if (lhs == nullptr || rhs == nullptr) {
        if (lhs == rhs) {
            return 0;
        }
        return (lhs == nullptr) ? -1 : +1;
    }
    if (lhs->type != rhs->type) {
        return lhs->type < rhs->type ? -1 : +1;
    }
    switch (lhs->type) {
    case eExprType::Undefined:
        assert(false);
        break;
    case eExprType::ShortConstant: {
        const auto *_lhs = static_cast<const ast_short_constant *>(lhs),
                   *_rhs = static_cast<const ast_short_constant *>(rhs);
        if (_lhs->value != _rhs->value) {
            return _lhs->value < _rhs->value ? -1 : +1;
        }
        break;
    }
    case eExprType::UShortConstant: {
        const auto *_lhs = static_cast<const ast_ushort_constant *>(lhs),
                   *_rhs = static_cast<const ast_ushort_constant *>(rhs);
        if (_lhs->value != _rhs->value) {
            return _lhs->value < _rhs->value ? -1 : +1;
        }
        break;
    }
    case eExprType::IntConstant: {
        const auto *_lhs = static_cast<const ast_int_constant *>(lhs),
                   *_rhs = static_cast<const ast_int_constant *>(rhs);
        if (_lhs->value != _rhs->value) {
            return _lhs->value < _rhs->value ? -1 : +1;
        }
        break;
    }
    case eExprType::UIntConstant: {
        const auto *_lhs = static_cast<const ast_uint_constant *>(lhs),
                   *_rhs = static_cast<const ast_uint_constant *>(rhs);
        if (_lhs->value != _rhs->value) {
            return _lhs->value < _rhs->value ? -1 : +1;
        }
        break;
    }
    case eExprType::LongConstant: {
        const auto *_lhs = static_cast<const ast_long_constant *>(lhs),
                   *_rhs = static_cast<const ast_long_constant *>(rhs);
        if (_lhs->value != _rhs->value) {
            return _lhs->value < _rhs->value ? -1 : +1;
        }
        break;
    }
    case eExprType::ULongConstant: {
        const auto *_lhs = static_cast<const ast_ulong_constant *>(lhs),
                   *_rhs = static_cast<const ast_ulong_constant *>(rhs);
        if (_lhs->value != _rhs->value) {
            return _lhs->value < _rhs->value ? -1 : +1;
        }
        break;
    }
    case eExprType::HalfConstant: {
        const auto *_lhs = static_cast<const ast_half_constant *>(lhs),
                   *_rhs = static_cast<const ast_half_constant *>(rhs);
        if (_lhs->value != _rhs->value) {
            return _lhs->value < _rhs->value ? -1 : +1;
        }
        break;
    }
    case eExprType::FloatConstant: {
        const auto *_lhs = static_cast<const ast_float_constant *>(lhs),
                   *_rhs = static_cast<const ast_float_constant *>(rhs);
        if (_lhs->value != _rhs->value) {
            return _lhs->value < _rhs->value ? -1 : +1;
        }
        break;
    }
    case eExprType::DoubleConstant: {
        const auto *_lhs = static_cast<const ast_double_constant *>(lhs),
                   *_rhs = static_cast<const ast_double_constant *>(rhs);
        if (_lhs->value != _rhs->value) {
            return _lhs->value < _rhs->value ? -1 : +1;
        }
        break;
    }
    case eExprType::BoolConstant: {
        const auto *_lhs = static_cast<const ast_bool_constant *>(lhs),
                   *_rhs = static_cast<const ast_bool_constant *>(rhs);
        if (_lhs->value != _rhs->value) {
            return _lhs->value < _rhs->value ? -1 : +1;
        }
        break;
    }
    case eExprType::VariableIdentifier: {
        const auto *_lhs = static_cast<const ast_variable_identifier *>(lhs),
                   *_rhs = static_cast<const ast_variable_identifier *>(rhs);
        return Compare(_lhs->variable, _rhs->variable);
    }
    case eExprType::FieldOrSwizzle: {
        const auto *_lhs = static_cast<const ast_field_or_swizzle *>(lhs),
                   *_rhs = static_cast<const ast_field_or_swizzle *>(rhs);
        const int operand_cmp = Compare(_lhs->operand, _rhs->operand);
        if (operand_cmp != 0) {
            return operand_cmp;
        }
        if (_lhs->field == nullptr || _rhs->field == nullptr) {
            if (_lhs->field == _rhs->field) {
                return 0;
            }
            return (_lhs->field == nullptr) ? -1 : +1;
        }
        const int field_cmp = Compare(_lhs->field->variable, _rhs->field->variable);
        if (field_cmp != 0) {
            return field_cmp;
        }
        return str_compare(_lhs->name, _rhs->name);
    }
    case eExprType::ArraySubscript: {
        const auto *_lhs = static_cast<const ast_array_subscript *>(lhs),
                   *_rhs = static_cast<const ast_array_subscript *>(rhs);
        const int operand_cmp = Compare(_lhs->operand, _rhs->operand);
        if (operand_cmp != 0) {
            return operand_cmp;
        }
        return Compare(_lhs->index, _rhs->index);
    }
    case eExprType::FunctionCall: {
        const auto *_lhs = static_cast<const ast_function_call *>(lhs),
                   *_rhs = static_cast<const ast_function_call *>(rhs);
        const int name_cmp = str_compare(_lhs->name, _rhs->name);
        if (name_cmp != 0) {
            return name_cmp;
        }
        return Compare_PointerSpans<ast_expression *>(_lhs->parameters, _rhs->parameters);
    }
    case eExprType::ConstructorCall: {
        const auto *_lhs = static_cast<const ast_constructor_call *>(lhs),
                   *_rhs = static_cast<const ast_constructor_call *>(rhs);
        const int type_cmp = Compare(_lhs->type, _rhs->type);
        if (type_cmp != 0) {
            return type_cmp;
        }
        return Compare_PointerSpans<ast_expression *>(_lhs->parameters, _rhs->parameters);
    }
    case eExprType::PostIncrement:
    case eExprType::PostDecrement:
    case eExprType::UnaryPlus:
    case eExprType::UnaryMinus:
    case eExprType::BitNot:
    case eExprType::LogicalNot:
    case eExprType::PrefixIncrement:
    case eExprType::PrefixDecrement: {
        const auto *_lhs = static_cast<const ast_unary_expression *>(lhs),
                   *_rhs = static_cast<const ast_unary_expression *>(rhs);
        return Compare(_lhs->operand, _rhs->operand);
    }
    case eExprType::Assign: {
        const auto *_lhs = static_cast<const ast_assignment_expression *>(lhs),
                   *_rhs = static_cast<const ast_assignment_expression *>(rhs);
        const int op1_cmp = Compare(_lhs->operand1, _rhs->operand1);
        if (op1_cmp != 0) {
            return op1_cmp;
        }
        const int op2_cmp = Compare(_lhs->operand2, _rhs->operand2);
        if (op2_cmp != 0) {
            return op2_cmp;
        }
        if (_lhs->oper != _rhs->oper) {
            return _lhs->oper < _rhs->oper ? -1 : +1;
        }
        break;
    }
    case eExprType::Sequence: {
        const auto *_lhs = static_cast<const ast_sequence_expression *>(lhs),
                   *_rhs = static_cast<const ast_sequence_expression *>(rhs);
        const int op1_cmp = Compare(_lhs->operand1, _rhs->operand1);
        if (op1_cmp != 0) {
            return op1_cmp;
        }
        return Compare(_lhs->operand2, _rhs->operand2);
    }
    case eExprType::Operation: {
        const auto *_lhs = static_cast<const ast_operation_expression *>(lhs),
                   *_rhs = static_cast<const ast_operation_expression *>(rhs);
        const int op1_cmp = Compare(_lhs->operand1, _rhs->operand1);
        if (op1_cmp != 0) {
            return op1_cmp;
        }
        const int op2_cmp = Compare(_lhs->operand2, _rhs->operand2);
        if (op2_cmp != 0) {
            return op2_cmp;
        }
        if (_lhs->oper != _rhs->oper) {
            return _lhs->oper < _rhs->oper ? -1 : +1;
        }
        break;
    }
    case eExprType::Ternary: {
        const auto *_lhs = static_cast<const ast_ternary_expression *>(lhs),
                   *_rhs = static_cast<const ast_ternary_expression *>(rhs);
        const int cond_cmp = Compare(_lhs->condition, _rhs->condition);
        if (cond_cmp != 0) {
            return cond_cmp;
        }
        const int true_cmp = Compare(_lhs->on_true, _rhs->on_true);
        if (true_cmp != 0) {
            return true_cmp;
        }
        return Compare(_lhs->on_false, _rhs->on_false);
    }
    case eExprType::ArraySpecifier: {
        const auto *_lhs = static_cast<const ast_array_specifier *>(lhs),
                   *_rhs = static_cast<const ast_array_specifier *>(rhs);
        return Compare_PointerSpans<ast_expression *>(_lhs->expressions, _rhs->expressions);
    }
    }
    return 0;
}

bool glslx::IsConstantValue(const ast_expression *expression) {
    return expression->type == eExprType::ShortConstant || expression->type == eExprType::UShortConstant ||
           expression->type == eExprType::IntConstant || expression->type == eExprType::UIntConstant ||
           expression->type == eExprType::LongConstant || expression->type == eExprType::ULongConstant ||
           expression->type == eExprType::HalfConstant || expression->type == eExprType::FloatConstant ||
           expression->type == eExprType::DoubleConstant || expression->type == eExprType::BoolConstant;
}

bool glslx::IsConstant(const ast_expression *expression) {
    if (IsConstantValue(expression)) {
        return true;
    } else if (expression->type == eExprType::VariableIdentifier) {
        const ast_variable *reference = static_cast<const ast_variable_identifier *>(expression)->variable;
        const ast_expression *initial_value = nullptr;
        if (reference->type == eVariableType::Global) {
            initial_value = static_cast<const ast_global_variable *>(reference)->initial_value;
        } else if (reference->type == eVariableType::Function) {
            if (reference->flags & eVariableFlags::Const) {
                initial_value = static_cast<const ast_function_variable *>(reference)->initial_value;
            }
        }
        if (!initial_value) {
            return false;
        }
        return IsConstant(initial_value);
    } else if (expression->type == eExprType::UnaryMinus || expression->type == eExprType::UnaryPlus ||
               expression->type == eExprType::BitNot) {
        return IsConstant(static_cast<const ast_unary_expression *>(expression)->operand);
    } else if (expression->type == eExprType::ConstructorCall) {
        const ast_constructor_call *call = static_cast<const ast_constructor_call *>(expression);
        if (!call->type->builtin) {
            return false;
        }
        bool ret = true;
        for (int i = 0; i < int(call->parameters.size()); ++i) {
            ret &= IsConstant(call->parameters[i]);
        }
        return ret;
    } else if (expression->type == eExprType::Operation) {
        const auto *operation = static_cast<const ast_operation_expression *>(expression);
        return IsConstant(operation->operand1) && IsConstant(operation->operand2);
    } else if (expression->type == eExprType::Sequence) {
        const auto *sequence = static_cast<const ast_sequence_expression *>(expression);
        return IsConstant(sequence->operand1) && IsConstant(sequence->operand2);
    } else if (expression->type == eExprType::ArraySpecifier) {
        const auto *arr_specifier = static_cast<const ast_array_specifier *>(expression);
        bool ret = true;
        for (int i = 0; i < int(arr_specifier->expressions.size()); ++i) {
            ret &= IsConstant(arr_specifier->expressions[i]);
        }
        return ret;
    }
    return false;
}