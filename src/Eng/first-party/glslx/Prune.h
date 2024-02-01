#pragma once

#include "parser/AST.h"

namespace glslx {
void Mark_Type(ast_type *type);
void Mark_VariableIdentifier(ast_variable_identifier *var);
void Mark_Variable(ast_variable *var);
void Mark_FunctionVariable(ast_function_variable *var);

void Mark_FunctionCall(ast_function_call *call);
void Mark_ConstructorCall(ast_constructor_call *call);

void Mark_Binary(ast_binary_expression *expr);
void Mark_Ternary(ast_ternary_expression *expr);
void Mark_Expression(ast_expression *expression);
void Mark_Statement(ast_statement *statement);

void Mark_Function(ast_function *func);

void Prune_Unreachable(TrUnit *tu);
} // namespace glslx