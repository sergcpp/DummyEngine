#include "Prune.h"

void glslx::Mark_Type(ast_type *type) {
    type->gc = 1;
    if (!type->builtin) {
        auto *structure = static_cast<ast_struct *>(type);
        for (ast_variable *f : structure->fields) {
            Mark_Variable(f);
        }
    }
}

void glslx::Mark_VariableIdentifier(ast_variable_identifier *var) {
    var->gc = 1;
    Mark_Variable(var->variable);
}

void glslx::Mark_Variable(ast_variable *var) {
    var->gc = 1;
    Mark_Type(var->base_type);
    for (ast_constant_expression *sz : var->array_sizes) {
        if (sz) {
            Mark_Expression(sz);
        }
    }
    if (var->type == eVariableType::Global) {
        auto *global = static_cast<ast_global_variable *>(var);
        if (global->initial_value) {
            Mark_Expression(global->initial_value);
        }
        for (ast_layout_qualifier *l : global->layout_qualifiers) {
            l->gc = 1;
            if (l->initial_value) {
                Mark_Expression(l->initial_value);
            }
        }
    }
}

void glslx::Mark_FunctionVariable(ast_function_variable *var) {
    Mark_Variable(var);
    if (var->initial_value) {
        Mark_Expression(var->initial_value);
    }
}

void glslx::Mark_FunctionCall(ast_function_call *call) {
    call->gc = 1;
    if (call->func && !call->func->gc) {
        Mark_Function(call->func);
    }
    for (ast_expression *expr : call->parameters) {
        Mark_Expression(expr);
    }
}

void glslx::Mark_ConstructorCall(ast_constructor_call *call) {
    call->gc = 1;
    Mark_Type(call->type);
    for (ast_expression *expr : call->parameters) {
        Mark_Expression(expr);
    }
}

void glslx::Mark_Binary(ast_binary_expression *expr) {
    expr->gc = 1;
    Mark_Expression(expr->operand1);
    Mark_Expression(expr->operand2);
}

void glslx::Mark_Ternary(ast_ternary_expression *expr) {
    expr->gc = 1;
    Mark_Expression(expr->condition);
    Mark_Expression(expr->on_true);
    Mark_Expression(expr->on_false);
}

void glslx::Mark_Expression(ast_expression *expression) {
    expression->gc = 1;
    switch (expression->type) {
    case eExprType::VariableIdentifier:
        Mark_Variable(static_cast<ast_variable_identifier *>(expression)->variable);
        break;
    case eExprType::FieldOrSwizzle: {
        auto *field_or_swizzle = static_cast<ast_field_or_swizzle *>(expression);
        Mark_Expression(field_or_swizzle->operand);
        if (field_or_swizzle->field) {
            Mark_VariableIdentifier(field_or_swizzle->field);
        }
    } break;
    case eExprType::ArraySubscript: {
        auto *subscript = static_cast<ast_array_subscript *>(expression);
        Mark_Expression(subscript->operand);
        Mark_Expression(subscript->index);
    } break;
    case eExprType::FunctionCall:
        Mark_FunctionCall(static_cast<ast_function_call *>(expression));
        break;
    case eExprType::ConstructorCall:
        Mark_ConstructorCall(static_cast<ast_constructor_call *>(expression));
        break;
    case eExprType::PostIncrement:
    case eExprType::PostDecrement:
    case eExprType::UnaryPlus:
    case eExprType::UnaryMinus:
    case eExprType::BitNot:
    case eExprType::LogicalNot:
    case eExprType::PrefixIncrement:
    case eExprType::PrefixDecrement:
        Mark_Expression(static_cast<ast_unary_expression *>(expression)->operand);
        break;
    case eExprType::Assign:
    case eExprType::Sequence:
    case eExprType::Operation:
        Mark_Binary(static_cast<ast_binary_expression *>(expression));
        break;
    case eExprType::Ternary:
        Mark_Ternary(static_cast<ast_ternary_expression *>(expression));
        break;
    case eExprType::ArraySpecifier: {
        auto *arr_specifier = static_cast<ast_array_specifier *>(expression);
        for (int i = 0; i < int(arr_specifier->expressions.size()); ++i) {
            Mark_Expression(arr_specifier->expressions[i]);
        }
    } break;
    default:
        break;
    }
}

void glslx::Mark_Statement(ast_statement *statement) {
    statement->gc = 1;
    switch (statement->type) {
    case eStatement::Compound: {
        auto *compound = static_cast<ast_compound_statement *>(statement);
        for (ast_statement *st : compound->statements) {
            Mark_Statement(st);
        }
    } break;
    case eStatement::Declaration: {
        auto *declaration = static_cast<ast_declaration_statement *>(statement);
        for (ast_function_variable *var : declaration->variables) {
            Mark_FunctionVariable(var);
        }
    } break;
    case eStatement::Expression:
        Mark_Expression(static_cast<ast_expression_statement *>(statement)->expression);
        break;
    case eStatement::If: {
        auto *if_statement = static_cast<ast_if_statement *>(statement);
        Mark_Expression(if_statement->condition);
        if (if_statement->then_statement) {
            Mark_Statement(if_statement->then_statement);
        }
        if (if_statement->else_statement) {
            Mark_Statement(if_statement->else_statement);
        }
    } break;
    case eStatement::Switch: {
        auto *switch_statement = static_cast<ast_switch_statement *>(statement);
        Mark_Expression(switch_statement->expression);
        for (ast_statement *st : switch_statement->statements) {
            Mark_Statement(st);
        }
    } break;
    case eStatement::CaseLabel: {
        ast_case_label_statement *_case = static_cast<ast_case_label_statement *>(statement);
        if (!(_case->flags & eCaseLabelFlags::Default)) {
            Mark_Expression(_case->condition);
        }
    } break;
    case eStatement::While: {
        auto *while_statement = static_cast<ast_while_statement *>(statement);
        Mark_Statement(while_statement->condition);
        Mark_Statement(while_statement->body);
    } break;
    case eStatement::Do: {
        auto *do_statement = static_cast<ast_do_statement *>(statement);
        Mark_Statement(do_statement->body);
        Mark_Expression(do_statement->condition);
    } break;
    case eStatement::For: {
        auto *for_statement = static_cast<ast_for_statement *>(statement);
        if (for_statement->init) {
            Mark_Statement(for_statement->init);
        }
        if (for_statement->condition) {
            Mark_Expression(for_statement->condition);
        }
        if (for_statement->loop) {
            Mark_Expression(for_statement->loop);
        }
        Mark_Statement(for_statement->body);
    } break;
    case eStatement::Return: {
        auto *return_statement = static_cast<ast_return_statement *>(statement);
        if (return_statement->expression) {
            Mark_Expression(return_statement->expression);
        }
    } break;
    default:
        break;
    }
}

void glslx::Mark_Function(ast_function *func) {
    func->gc = 1;
    Mark_Type(func->return_type);
    for (ast_function_parameter *param : func->parameters) {
        Mark_Variable(param);
    }
    for (ast_statement *statement : func->statements) {
        Mark_Statement(statement);
    }
    if (func->prototype) {
        Mark_Function(func->prototype);
    }
}

void glslx::Prune_Unreachable(TrUnit *tu) {
    if (tu->version) {
        tu->version->gc = 1;
    }
    for (ast_extension_directive *ext : tu->extensions) {
        ext->gc = 1;
    }
    for (ast_default_precision *pre : tu->default_precision) {
        pre->gc = 1;
        Mark_Type(pre->type);
    }

    for (ast_interface_block *block : tu->interface_blocks) {
        block->gc = 1;
        for (ast_variable *f : block->fields) {
            Mark_Variable(f);
        }
        for (ast_layout_qualifier *l : block->layout_qualifiers) {
            l->gc = 1;
            if (l->initial_value) {
                Mark_Expression(l->initial_value);
            }
        }
    }

    // keep all uniforms
    for (ast_global_variable *var : tu->globals) {
        if (var->storage == eStorage::In || var->storage == eStorage::Out || var->storage == eStorage::Uniform) {
            Mark_Variable(var);
        }
    }

    ast_function *main_function = nullptr;
    auto *p_main = tu->functions_by_name.Find("main");
    if (p_main) {
        main_function = (*p_main)[0];
    }

    if (main_function) {
        Mark_Function(main_function);
        for (auto it = std::begin(tu->functions); it != std::end(tu->functions);) {
            if ((*it)->gc) {
                ++it;
            } else {
                auto &functions = *tu->functions_by_name.Find((*it)->name);
                auto it2 = std::remove(std::begin(functions), std::end(functions), *it);
                functions.erase(it2, std::end(functions));
                it = tu->functions.erase(it);
            }
        }
    } else {
        // keep all functions
        for (ast_function *func : tu->functions) {
            Mark_Function(func);
        }
    }

    for (auto it = tu->globals.begin(); it != tu->globals.end();) {
        if ((*it)->gc) {
            ++it;
        } else {
            it = tu->globals.erase(it);
        }
    }

    for (auto it = tu->structures.begin(); it != tu->structures.end();) {
        if ((*it)->gc) {
            ++it;
        } else {
            tu->structures_by_name.Erase((*it)->name);
            it = tu->structures.erase(it);
        }
    }
    assert(tu->structures.size() == tu->structures_by_name.size());

    // changes the order, but we don't care
    for (size_t i = 0; i < tu->alloc.allocations.size();) {
        if (tu->alloc.allocations[i].data->gc) {
            tu->alloc.allocations[i].data->gc = 0;
            ++i;
        } else {
            tu->alloc.allocations[i].destroy(tu->alloc.allocator);
            tu->alloc.allocations[i] = tu->alloc.allocations.back();
            tu->alloc.allocations.pop_back();
        }
    }
}
