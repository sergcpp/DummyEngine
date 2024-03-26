#pragma once

#include "WriterBase.h"
#include "parser/Parser.h"

namespace glslx {
class WriterHLSL : public WriterBase {
    const TrUnit *tu_ = nullptr;
    int compute_group_sizes_[3] = {1, 1, 1};
    int temp_var_index_ = 0;

    struct byteaddress_buf_t {
        const char *name;
        int size;
    };
    std::vector<byteaddress_buf_t> byteaddress_bufs_;
    struct atomic_operation_t {
        const ast_expression *expr = nullptr;
        std::string var_name;
    };
    std::string var_to_init_;
    std::vector<atomic_operation_t> atomic_operations_;

    void Write_Builtin(const ast_builtin *builtin, std::ostream &out_stream);
    void Write_Type(const ast_type *type, std::ostream &out_stream);
    void Write_ArraySize(Span<const ast_constant_expression *const> array_sizes, std::ostream &out_stream);
    void Write_Variable(const ast_variable *variable, Span<const ast_layout_qualifier *const> qualifiers,
                        std::ostream &out_stream, const bool name_only = false);
    void Write_Register(const ast_type *base_type, Span<const ast_layout_qualifier *const> qualifiers,
                        std::ostream &out_stream);
    void Write_Storage(eStorage storage, std::ostream &out_stream);
    void Write_AuxStorage(eAuxStorage aux_storage, std::ostream &out_stream);
    void Write_ParameterQualifiers(Bitmask<eParamQualifier> qualifiers, std::ostream &out_stream);
    void Write_Memory(Bitmask<eMemory> memory, std::ostream &out_stream);
    void Write_Precision(ePrecision precision, std::ostream &out_stream);
    void Write_GlobalVariable(const ast_global_variable *variable, std::ostream &out_stream);
    void Write_VariableIdentifier(const ast_variable_identifier *expression, std::ostream &out_stream);
    void Write_FieldOrSwizzle(const ast_field_or_swizzle *expression, std::ostream &out_stream);
    void Write_ArraySubscript(const ast_array_subscript *expression, std::ostream &out_stream);
    void Write_FunctionCall(const ast_function_call *expression, std::ostream &out_stream);
    void Write_ConstructorCall(const ast_constructor_call *expression, std::ostream &out_stream);
    void Write_PostIncrement(const ast_post_increment_expression *expression, std::ostream &out_stream);
    void Write_PostDecrement(const ast_post_decrement_expression *expression, std::ostream &out_stream);
    void Write_UnaryPlus(const ast_unary_plus_expression *expression, std::ostream &out_stream);
    void Write_UnaryMinus(const ast_unary_minus_expression *expression, std::ostream &out_stream);
    void Write_UnaryBitNot(const ast_unary_bit_not_expression *expression, std::ostream &out_stream);
    void Write_UnaryLogicalNot(const ast_unary_logical_not_expression *expression, std::ostream &out_stream);
    void Write_PrefixIncrement(const ast_prefix_increment_expression *expression, std::ostream &out_stream);
    void Write_PrefixDecrement(const ast_prefix_decrement_expression *expression, std::ostream &out_stream);
    void Write_Assignment(const ast_assignment_expression *expression, std::ostream &out_stream);
    void Write_Sequence(const ast_sequence_expression *expression, std::ostream &out_stream);
    void Write_Operation(const ast_operation_expression *expression, std::ostream &out_stream);
    void Write_Ternary(const ast_ternary_expression *expression, std::ostream &out_stream);
    void Write_Structure(const ast_struct *structure, std::ostream &out_stream);
    void Write_InterfaceBlock(const ast_interface_block *block, std::ostream &out_stream);
    void Write_Expression(const ast_expression *expression, bool nested, std::ostream &out_stream);
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

    // Byteaddress buffer access
    struct access_index_t {
        const ast_expression *index = nullptr;
        int multiplier = 0;
    };
    std::pair<int, int> Find_BufferAccessExpression(const ast_expression *expression, int offset,
                                                    std::vector<access_index_t> &out_indices);
    void Find_AtomicOperations(const ast_expression *expression, std::vector<atomic_operation_t> &out_operations);
    void Process_AtomicOperations(const ast_expression *expression, std::ostream &out_stream);
    int Calc_FieldOffset(const ast_type *type, const char *field_name);
    int Calc_TypeSize(const ast_type *type);
    int Calc_VariableSize(const ast_variable *type);
    int Write_ByteaddressBufLoads(const byteaddress_buf_t &buf, int offset, const std::string &prefix, int array_dim,
                                  const ast_variable *v, std::ostream &out_stream);
    int Write_ByteaddressBufStores(const byteaddress_buf_t &buf, int offset, const std::string &prefix, int array_dim,
                                   const ast_type *t, std::ostream &out_stream);
    int Write_ByteaddressBufStores(const byteaddress_buf_t &buf, int offset, const std::string &prefix, int array_dim,
                                   const ast_variable *v, std::ostream &out_stream);

  public:
    WriterHLSL(const writer_config_t &config = {}) : WriterBase(config) {}

    void Write(const TrUnit *tu, std::ostream &out_stream);
};
} // namespace glslx
