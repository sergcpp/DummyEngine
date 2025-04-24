#pragma once

#include <memory>

#include "parser/AST.h"

namespace glslx {
class Clone {
    char *strnew(const char *str);
    template <class T, class... Args> T *astnew(Args &&...args) {
        return new (&dst_->alloc) T(std::forward<Args>(args)...);
    }

    ast_version_directive *Clone_VersionDirective(const ast_version_directive *in);
    ast_extension_directive *Clone_ExtensionDirective(const ast_extension_directive *in);
    ast_default_precision *Clone_DefaultPrecision(const ast_default_precision *in);

    template <typename T> T *Clone_Constant(const T *in) { return astnew<T>(in->value); }
    ast_variable_identifier *Clone_VariableIdentifier(const ast_variable_identifier *in);
    ast_field_or_swizzle *Clone_FieldOrSwizzle(const ast_field_or_swizzle *in);
    ast_array_subscript *Clone_ArraySubscript(const ast_array_subscript *in);
    ast_function_call *Clone_FunctionCall(const ast_function_call *in);
    ast_constructor_call *Clone_ConstructorCall(const ast_constructor_call *in);
    ast_assignment_expression *Clone_Assignment(const ast_assignment_expression *in);
    ast_sequence_expression *Clone_Sequence(const ast_sequence_expression *in);
    ast_operation_expression *Clone_Operation(const ast_operation_expression *in);
    ast_ternary_expression *Clone_Ternary(const ast_ternary_expression *in);
    ast_array_specifier *Clone_ArraySpecifier(const ast_array_specifier *in);
    ast_expression *Clone_Expression(const ast_expression *in);
    ast_type *Clone_Type(const ast_type *in);
    ast_struct *Clone_Structure(const ast_struct *in);
    ast_variable *Clone_Variable(const ast_variable *in);
    ast_global_variable *Clone_GlobalVariable(const ast_global_variable *in);

    ast_layout_qualifier *Clone_LayoutQualifier(const ast_layout_qualifier *in);
    ast_interface_block *Clone_InterfaceBlock(const ast_interface_block *in);

    // Statements
    ast_compound_statement *Clone_CompundStatement(const ast_compound_statement *in);
    ast_declaration_statement *Clone_DeclarationStatement(const ast_declaration_statement *in);
    ast_expression_statement *Clone_ExpressionStatement(const ast_expression_statement *in);
    ast_if_statement *Clone_IfStatement(const ast_if_statement *in);
    ast_switch_statement *Clone_SwitchStatement(const ast_switch_statement *in);
    ast_case_label_statement *Clone_CaseLabelStatement(const ast_case_label_statement *in);
    ast_while_statement *Clone_WhileStatement(const ast_while_statement *in);
    ast_do_statement *Clone_DoStatement(const ast_do_statement *in);
    ast_for_statement *Clone_ForStatement(const ast_for_statement *in);
    ast_return_statement *Clone_ReturnStatement(const ast_return_statement *in);
    ast_statement *Clone_Statement(const ast_statement *in);

    ast_function_parameter *Clone_FunctionParameter(const ast_function_parameter *in);
    ast_function_variable *Clone_FunctionVariable(const ast_function_variable *in);
    ast_function *Clone_Function(const ast_function *in);

    ast_builtin *FindOrAddBuiltin(eKeyword type);

    const TrUnit *src_ = nullptr;
    std::unique_ptr<TrUnit> dst_;

    std::vector<ast_builtin *> builtins_;
    HashMap32<const ast_interface_block *, ast_interface_block *> interface_blocks_;

  public:
    std::unique_ptr<TrUnit> CloneAST(const TrUnit *tu);
};
} // namespace glslx