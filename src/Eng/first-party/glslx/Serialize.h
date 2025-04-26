#pragma once

#include "HashMap32.h"
#include "Span.h"
#include "parser/AST.h"

namespace glslx {
class Serialize {
    void Serialize_VersionDirective(const ast_version_directive *version, std::ostream &out);
    ast_version_directive *Deserialize_VersionDirective(std::istream &in);

    void Serialize_ExtensionDirective(const ast_extension_directive *extension, std::ostream &out);
    ast_extension_directive *Deserialize_ExtensionDirective(std::istream &in);

    void Serialize_Builtin(const ast_builtin *builtin, std::ostream &out);
    ast_builtin *Deserialize_Builtin(std::istream &in);

    void Serialize_DefaultPrecision(const ast_default_precision *precision, std::ostream &out);
    ast_default_precision *Deserialize_DefaultPrecision(std::istream &in);

    template <typename T> void Serialize_Constant(const T *e, std::ostream &out) {
        out.write((const char *)&e->value, sizeof(e->value));
    }
    template <typename T> T *Deserialize_Constant(std::istream &in) {
        decltype(T::value) val;
        in.read((char *)&val, sizeof(val));
        return dst_->make<T>(val);
    }

    // Expression
    void Serialize_VariableIdentifier(const ast_variable_identifier *var, std::ostream &out);
    ast_variable_identifier *Deserialize_VariableIdentifier(std::istream &in);
    void Serialize_FieldOrSwizzle(const ast_field_or_swizzle *f, std::ostream &out);
    ast_field_or_swizzle *Deserialize_FieldOrSwizzle(std::istream &in);
    void Serialize_ArraySubscript(const ast_array_subscript *s, std::ostream &out);
    ast_array_subscript *Deserialize_ArraySubscript(std::istream &in);
    void Serialize_FunctionCall(const ast_function_call *c, std::ostream &out);
    ast_function_call *Deserialize_FunctionCall(std::istream &in);
    void Serialize_ConstructorCall(const ast_constructor_call *c, std::ostream &out);
    ast_constructor_call *Deserialize_ConstructorCall(std::istream &in);
    void Serialize_Assignment(const ast_assignment_expression *a, std::ostream &out);
    ast_assignment_expression *Deserialize_Assignment(std::istream &in);
    void Serialize_Sequence(const ast_sequence_expression *s, std::ostream &out);
    ast_sequence_expression *Deserialize_Sequence(std::istream &in);
    void Serialize_Operation(const ast_operation_expression *o, std::ostream &out);
    ast_operation_expression *Deserialize_Operation(std::istream &in);
    void Serialize_Ternary(const ast_ternary_expression *t, std::ostream &out);
    ast_ternary_expression *Deserialize_Ternary(std::istream &in);
    void Serialize_ArraySpecifier(const ast_array_specifier *s, std::ostream &out);
    ast_array_specifier *Deserialize_ArraySpecifier(std::istream &in);
    void Serialize_Expression(const ast_expression *expression, std::ostream &out);
    ast_expression *Deserialize_Expression(std::istream &in);

    // Statement
    void Serialize_CompoundStatement(const ast_compound_statement *s, std::ostream &out);
    ast_compound_statement *Deserialize_CompoundStatement(std::istream &in);
    void Serialize_DeclarationStatement(const ast_declaration_statement *s, std::ostream &out);
    ast_declaration_statement *Deserialize_DeclarationStatement(std::istream &in);
    void Serialize_ExpressionStatement(const ast_expression_statement *s, std::ostream &out);
    ast_expression_statement *Deserialize_ExpressionStatement(std::istream &in);
    void Serialize_IfStatement(const ast_if_statement *s, std::ostream &out);
    ast_if_statement *Deserialize_IfStatement(std::istream &in);
    void Serialize_SwitchStatement(const ast_switch_statement *s, std::ostream &out);
    ast_switch_statement *Deserialize_SwitchStatement(std::istream &in);
    void Serialize_CaseLabelStatement(const ast_case_label_statement *s, std::ostream &out);
    ast_case_label_statement *Deserialize_CaseLabelStatement(std::istream &in);
    void Serialize_WhileStatement(const ast_while_statement *s, std::ostream &out);
    ast_while_statement *Deserialize_WhileStatement(std::istream &in);
    void Serialize_DoStatement(const ast_do_statement *s, std::ostream &out);
    ast_do_statement *Deserialize_DoStatement(std::istream &in);
    void Serialize_ForStatement(const ast_for_statement *s, std::ostream &out);
    ast_for_statement *Deserialize_ForStatement(std::istream &in);
    void Serialize_ReturnStatement(const ast_return_statement *s, std::ostream &out);
    ast_return_statement *Deserialize_ReturnStatement(std::istream &in);
    void Serialize_ExtJumpStatement(const ast_ext_jump_statement *s, std::ostream &out);
    ast_ext_jump_statement *Deserialize_ExtJumpStatement(std::istream &in);
    void Serialize_Statement(const ast_statement *statement, std::ostream &out);
    ast_statement *Deserialize_Statement(std::istream &in);

    void Serialize_LayoutQualifier(const ast_layout_qualifier *q, std::ostream &out);
    ast_layout_qualifier *Deserialize_LayoutQualifier(std::istream &in);

    void Serialize_Variable(const ast_variable *variable, std::ostream &out);
    ast_variable *Deserialize_Variable(std::istream &in);

    void Serialize_Structure(const ast_struct *structure, std::ostream &out);
    ast_struct *Deserialize_Structure(std::istream &in);
    void Serialize_InterfaceBlock(const ast_interface_block *block, std::ostream &out);
    ast_interface_block *Deserialize_InterfaceBlock(std::istream &in);

    void Serialize_Function(const ast_function *function, std::ostream &out);
    ast_function *Deserialize_Function(std::istream &in);

    const TrUnit *src_ = nullptr;
    TrUnit *dst_ = nullptr;

    HashMap32<void *, int32_t> string_index_;
    global_vector<char *> strings_;

    HashMap32<const ast_type *, int32_t> type_index_;
    global_vector<ast_type *> types_;

    HashMap32<const ast_variable *, int32_t> variable_index_;
    global_vector<ast_variable *> variables_;

    HashMap32<const ast_function *, int32_t> function_index_;
    global_vector<ast_function *> functions_;

  public:
    void SerializeAST(const TrUnit *tu, std::ostream &out);
    bool DeserializeAST(TrUnit *tu, std::istream &in);
};
} // namespace glslx