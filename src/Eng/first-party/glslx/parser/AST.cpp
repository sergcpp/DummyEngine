#include "AST.h"

namespace glslx {
const char *g_statement_names[] = {
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
static_assert(sizeof(g_statement_names) / sizeof(g_statement_names[0]) == int(eStatement::_Count), "!");
} // namespace glslx

glslx::TrUnit::~TrUnit() {
    for (char *_str : str) {
        alloc.allocator.deallocate(_str, strlen(_str) + 1);
    }
    for (ast_memory &mem : alloc.allocations) {
        mem.destroy(alloc.allocator);
    }
}

bool glslx::IsConstantValue(const ast_expression *expression) {
    return expression->type == eExprType::IntConstant || expression->type == eExprType::UIntConstant ||
           expression->type == eExprType::FloatConstant || expression->type == eExprType::DoubleConstant ||
           expression->type == eExprType::BoolConstant;
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
            if (static_cast<const ast_function_variable *>(reference)->is_const) {
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