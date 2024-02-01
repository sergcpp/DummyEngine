#include "Fixup.h"

#include <cstdio>

void glslx::Fixup::Visit_Statement(ast_statement *statement) {
    switch (statement->type) {
    case eStatement::Compound: {
        auto *compound = static_cast<ast_compound_statement *>(statement);
        for (ast_statement *st : compound->statements) {
            Visit_Statement(st);
        }
    } break;
    case eStatement::If: {
        auto *if_statement = static_cast<ast_if_statement *>(statement);
        //Visit_Expression(if_statement->condition);
        if (if_statement->then_statement) {
            Visit_Statement(if_statement->then_statement);
        }
        if (if_statement->else_statement) {
            Visit_Statement(if_statement->else_statement);
        }
    } break;
    case eStatement::Switch: {
        auto *switch_statement = static_cast<ast_switch_statement *>(statement);
        //Mark_Expression(switch_statement->expression);
        for (ast_statement *st : switch_statement->statements) {
            Visit_Statement(st);
        }
    } break;
    case eStatement::CaseLabel:
        //Mark_Expression(static_cast<ast_case_label_statement *>(statement)->condition);
        break;
    case eStatement::While: {
        auto *while_statement = static_cast<ast_while_statement *>(statement);
        Visit_Statement(while_statement->condition);
        Visit_Statement(while_statement->body);
    } break;
    case eStatement::Do: {
        auto *do_statement = static_cast<ast_do_statement *>(statement);
        Visit_Statement(do_statement->body);
        //Visit_Expression(do_statement->condition);
    } break;
    case eStatement::For: {
        auto *for_statement = static_cast<ast_for_statement *>(statement);
        if (for_statement->init->type == eStatement::Declaration) {
            auto *declaration = static_cast<ast_declaration_statement *>(for_statement->init);
            for (ast_function_variable *var : declaration->variables) {
                char temp[16];
                snprintf(temp, sizeof(temp), "_%i", next_counter_++);
                const int temp_len = int(strlen(temp));

                const int size = int(strlen(var->name));

                char *copy = (char *)malloc(size + temp_len + 1);
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
            //Mark_Expression(return_statement->expression);
        }
    } break;
    }
}

void glslx::Fixup::Visit_Function(ast_function *func) {
    for (ast_statement *statement : func->statements) {
        Visit_Statement(statement);
    }
}

void glslx::Fixup::Apply(TrUnit *tu) {
    tu_ = tu;
    for (ast_function *func : tu->functions) {
        next_counter_ = 0;
        Visit_Function(func);
    }
}