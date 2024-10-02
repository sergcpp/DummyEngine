#include "Fixup.h"

#include <cstdio>

void glslx::Fixup::Visit_Statement(ast_statement *statement) {
    switch (statement->type) {
    case eStatement::Declaration: {
        auto *declaration = static_cast<ast_declaration_statement *>(statement);
        for (int i = 0; i < int(declaration->variables.size()); ++i) {
            if (!declaration->variables[i]->initial_value) {
                continue;
            }
            if (config_.remove_const && !IsConstant(declaration->variables[i]->initial_value)) {
                declaration->variables[i]->is_const = false;
            }
        }
    } break;
    case eStatement::Compound: {
        auto *compound = static_cast<ast_compound_statement *>(statement);
        for (ast_statement *st : compound->statements) {
            Visit_Statement(st);
        }
    } break;
    case eStatement::If: {
        auto *if_statement = static_cast<ast_if_statement *>(statement);
        if (config_.remove_ctrl_flow_attributes) {
            if_statement->attributes = {};
        }
        // Visit_Expression(if_statement->condition);
        if (if_statement->then_statement) {
            Visit_Statement(if_statement->then_statement);
        }
        if (if_statement->else_statement) {
            Visit_Statement(if_statement->else_statement);
        }
    } break;
    case eStatement::Switch: {
        auto *switch_statement = static_cast<ast_switch_statement *>(statement);
        if (config_.remove_ctrl_flow_attributes) {
            switch_statement->attributes = {};
        }
        // Mark_Expression(switch_statement->expression);
        for (ast_statement *st : switch_statement->statements) {
            Visit_Statement(st);
        }
    } break;
    case eStatement::CaseLabel:
        // Mark_Expression(static_cast<ast_case_label_statement *>(statement)->condition);
        break;
    case eStatement::While: {
        auto *while_statement = static_cast<ast_while_statement *>(statement);
        if (config_.remove_ctrl_flow_attributes) {
            while_statement->flow_params.attributes = {};
        }
        Visit_Statement(while_statement->condition);
        Visit_Statement(while_statement->body);
    } break;
    case eStatement::Do: {
        auto *do_statement = static_cast<ast_do_statement *>(statement);
        if (config_.remove_ctrl_flow_attributes) {
            do_statement->flow_params.attributes = {};
        }
        Visit_Statement(do_statement->body);
        // Visit_Expression(do_statement->condition);
    } break;
    case eStatement::For: {
        auto *for_statement = static_cast<ast_for_statement *>(statement);
        if (config_.remove_ctrl_flow_attributes) {
            for_statement->flow_params.attributes = {};
        }
        if (config_.randomize_loop_counters && for_statement->init &&
            for_statement->init->type == eStatement::Declaration) {
            auto *declaration = static_cast<ast_declaration_statement *>(for_statement->init);
            for (ast_function_variable *var : declaration->variables) {
                char temp[16];
                snprintf(temp, sizeof(temp), "_%i", next_counter_++);
                const int temp_len = int(strlen(temp));

                const int size = int(strlen(var->name));

                char *copy = tu_->alloc.allocator.allocate(size + temp_len + 1);
                if (copy) {
                    memcpy(copy, var->name, size);
                    memcpy(copy + size, temp, temp_len);
                    copy[size + temp_len] = '\0';
                }

                var->name = copy;
                tu_->str.push_back(copy);
            }
        }
        Visit_Statement(for_statement->body);
    } break;
    case eStatement::Return: {
        auto *return_statement = static_cast<ast_return_statement *>(statement);
        if (return_statement->expression) {
            // Mark_Expression(return_statement->expression);
        }
    } break;
    default:
        break;
    }
}

void glslx::Fixup::Visit_FunctionParameter(ast_function_parameter *parameter) {
    if (config_.remove_const) {
        parameter->qualifiers &= ~Bitmask{eParamQualifier::Const};
    }
}

void glslx::Fixup::Visit_Function(ast_function *func) {
    for (ast_function_parameter *parameter : func->parameters) {
        Visit_FunctionParameter(parameter);
    }
    for (ast_statement *statement : func->statements) {
        Visit_Statement(statement);
    }
}

void glslx::Fixup::Apply(TrUnit *tu) {
    tu_ = tu;
    if (config_.force_version != -1 && tu->version) {
        tu->version->number = config_.force_version;
        tu->version->type = config_.force_version_type;
    }
    for (auto it = begin(tu->extensions); it != end(tu->extensions);) {
        if (config_.remove_ctrl_flow_attributes && strcmp((*it)->name, "GL_EXT_control_flow_attributes") == 0) {
            it = tu->extensions.erase(it);
        } else {
            ++it;
        }
    }
    for (int i = 0; i < int(tu->globals.size()); ++i) {
        if (config_.remove_inout_layout &&
            (tu->globals[i]->storage == eStorage::In || tu->globals[i]->storage == eStorage::Out)) {
            tu->globals[i]->layout_qualifiers.clear();
        }
    }
    for (ast_function *func : tu->functions) {
        next_counter_ = 0;
        Visit_Function(func);
    }
    if (config_.flip_vertex_y && tu->type == eTrUnitType::Vertex) {
        ast_function *main_function = nullptr;
        for (ast_function *func : tu->functions) {
            if (strcmp(func->name, "main") == 0) {
                main_function = func;
            }
        }
        ast_global_variable *gl_Position = nullptr;
        for (ast_global_variable *var : tu->globals) {
            if (strcmp(var->name, "gl_Position") == 0) {
                gl_Position = var;
                break;
            }
        }
        if (main_function && gl_Position) {
            auto *variable_identifier = new (&tu->alloc) ast_variable_identifier(gl_Position);

            auto *lhs = new (&tu->alloc) ast_field_or_swizzle();
            lhs->operand = variable_identifier;
            lhs->name = "y";

            auto *y_field = new (&tu->alloc) ast_field_or_swizzle();
            y_field->operand = variable_identifier;
            y_field->name = "y";
            auto *rhs = new (&tu->alloc) ast_unary_expression(eExprType::UnaryMinus, y_field);

            auto *expr = new (&tu->alloc) ast_assignment_expression(eOperator::assign);
            expr->operand1 = lhs;
            expr->operand2 = rhs;

            auto *statement = new (&tu->alloc) ast_expression_statement(expr);

            main_function->statements.push_back(statement);
        }
    }
}