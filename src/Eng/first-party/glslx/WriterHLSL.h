#pragma once

#include "Span.h"
#include "WriterBase.h"
#include "parser/Parser.h"

namespace glslx {
class WriterHLSL : public WriterBase {
    const TrUnit *tu_ = nullptr;
    int compute_group_sizes_[3] = {1, 1, 1};

    void Write_Builtin(const ast_builtin *builtin, std::ostream &out_stream);
    void Write_Type(const ast_type *type, std::ostream &out_stream);
    void Write_ArraySize(Span<const ast_constant_expression *const> array_sizes, std::ostream &out_stream);
    void Write_Variable(const ast_variable *variable, Span<const ast_layout_qualifier *const> qualifiers,
                        std::ostream &out_stream, Bitmask<eOutputFlags> output_flags = DefaultOutputFlags);
    void Write_Register(const ast_type *base_type, Span<const ast_layout_qualifier *const> qualifiers,
                        std::ostream &out_stream);
    void Write_Storage(eStorage storage, std::ostream &out_stream);
    void Write_AuxStorage(eAuxStorage aux_storage, std::ostream &out_stream);
    void Write_ParameterQualifiers(Bitmask<eParamQualifier> qualifiers, std::ostream &out_stream);
    void Write_Memory(Bitmask<eMemory> memory, std::ostream &out_stream);
    void Write_Precision(ePrecision precision, std::ostream &out_stream);
    void Write_GlobalVariable(const ast_global_variable *variable, std::ostream &out_stream);
    void Write_VariableIdentifier(const ast_variable_identifier *expression, std::ostream &out_stream);
    void Write_FieldOrSwizzle(const ast_field_or_swizzle *expression, assoc_t parent, std::ostream &out_stream);
    void Write_ArraySubscript(const ast_array_subscript *expression, assoc_t parent, std::ostream &out_stream);
    void Write_FunctionCall(const ast_function_call *expression, assoc_t parent, std::ostream &out_stream);
    void Write_MethodCall(const ast_method_call *expression, assoc_t parent, std::ostream &out_stream);
    void Write_ConstructorCall(const ast_constructor_call *expression, assoc_t parent, std::ostream &out_stream);
    void Write_PostIncrement(const ast_post_increment_expression *expression, assoc_t parent, std::ostream &out_stream);
    void Write_PostDecrement(const ast_post_decrement_expression *expression, assoc_t parent, std::ostream &out_stream);
    void Write_UnaryPlus(const ast_unary_plus_expression *expression, assoc_t parent, std::ostream &out_stream);
    void Write_UnaryMinus(const ast_unary_minus_expression *expression, assoc_t parent, std::ostream &out_stream);
    void Write_UnaryBitNot(const ast_unary_bit_not_expression *expression, assoc_t parent, std::ostream &out_stream);
    void Write_UnaryLogicalNot(const ast_unary_logical_not_expression *expression, assoc_t parent,
                               std::ostream &out_stream);
    void Write_PrefixIncrement(const ast_prefix_increment_expression *expression, assoc_t parent,
                               std::ostream &out_stream);
    void Write_PrefixDecrement(const ast_prefix_decrement_expression *expression, assoc_t parent,
                               std::ostream &out_stream);
    void Write_Assignment(const ast_assignment_expression *expression, assoc_t parent, std::ostream &out_stream);
    void Write_Sequence(const ast_sequence_expression *expression, assoc_t parent, std::ostream &out_stream);
    void Write_Operation(const ast_operation_expression *expression, assoc_t parent, std::ostream &out_stream);
    void Write_Ternary(const ast_ternary_expression *expression, assoc_t parent, std::ostream &out_stream);
    void Write_Structure(const ast_struct *structure, std::ostream &out_stream);
    void Write_InterfaceBlock(const ast_interface_block *block, std::ostream &out_stream);
    void Write_Expression(const ast_expression *expression, assoc_t parent, std::ostream &out_stream);
    void Write_FunctionParameter(const ast_function_parameter *parameter, std::ostream &out_stream);
    void Write_Function(const ast_function *function, std::ostream &out_stream);
    void Write_SelectionAttributes(Bitmask<eCtrlFlowAttribute> attributes, std::ostream &out_stream);
    void Write_LoopAttributes(ctrl_flow_params_t ctrl_flow, std::ostream &out_stream);

    // Statements
    void Write_FunctionVariable(const ast_function_variable *variable, std::ostream &out_stream,
                                Bitmask<eOutputFlags> output_flags = DefaultOutputFlags);
    void Write_CompoundStatement(const ast_compound_statement *statement, std::ostream &out_stream,
                                 Bitmask<eOutputFlags> output_flags);
    void Write_EmptyStatement(const ast_empty_statement *statement, std::ostream &out_stream,
                              Bitmask<eOutputFlags> output_flags);
    void Write_DeclarationStatement(const ast_declaration_statement *statement, std::ostream &out_stream,
                                    Bitmask<eOutputFlags> output_flags);
    void Write_ExpressionStatement(const ast_expression_statement *statement, std::ostream &out_stream,
                                   Bitmask<eOutputFlags> output_flags);
    void Write_IfStatement(const ast_if_statement *statement, std::ostream &out_stream,
                           Bitmask<eOutputFlags> output_flags);
    void Write_SwitchStatement(const ast_switch_statement *statement, std::ostream &out_stream,
                               Bitmask<eOutputFlags> output_flags);
    void Write_CaseLabelStatement(const ast_case_label_statement *statement, std::ostream &out_stream,
                                  Bitmask<eOutputFlags> output_flags);
    void Write_WhileStatement(const ast_while_statement *statement, std::ostream &out_stream,
                              Bitmask<eOutputFlags> output_flags);
    void Write_DoStatement(const ast_do_statement *statement, std::ostream &out_stream,
                           Bitmask<eOutputFlags> output_flags);
    void Write_ForStatement(const ast_for_statement *statement, std::ostream &out_stream,
                            Bitmask<eOutputFlags> output_flags);
    void Write_ContinueStatement(const ast_continue_statement *statement, std::ostream &out_stream,
                                 Bitmask<eOutputFlags> output_flags);
    void Write_BreakStatement(const ast_break_statement *statement, std::ostream &out_stream,
                              Bitmask<eOutputFlags> output_flags);
    void Write_ReturnStatement(const ast_return_statement *statement, std::ostream &out_stream,
                               Bitmask<eOutputFlags> output_flags);
    void Write_DistardStatement(const ast_discard_statement *statement, std::ostream &out_stream,
                                Bitmask<eOutputFlags> output_flags);
    void Write_ExtJumpStatement(const ast_ext_jump_statement *statement, std::ostream &out_stream,
                                Bitmask<eOutputFlags> output_flags);
    void Write_Statement(const ast_statement *statement, std::ostream &out_stream, Bitmask<eOutputFlags> output_flags);

  public:
    explicit WriterHLSL(writer_config_t config = {}) : WriterBase(std::move(config)) {}

    void Write(const TrUnit *tu, std::ostream &out_stream);
};

// Runs the LowerHLSL AST pass on a clone of `tu` and prints the result with WriterHLSL.
// Use this (not WriterHLSL::Write directly) whenever the tree may contain byteaddress-buffer
// stores: WriterHLSL itself is a near-1:1 printer and no longer rewrites those while printing.
void Write_HLSL(const TrUnit *tu, std::ostream &out_stream, writer_config_t config = {});
} // namespace glslx
