#include "Serialize.h"

#include <istream>
#include <ostream>

namespace glslx {
const int32_t FormatVersion = 1;

const int32_t Block_Version = 0;
const int32_t Block_Strings = 1;
const int32_t Block_Extensions = 2;
const int32_t Block_Builtins = 3;
const int32_t Block_DefaultPrecision = 4;
const int32_t Block_Structures = 5;
const int32_t Block_InterfaceBlocks = 6;
const int32_t Block_Globals = 7;
const int32_t Block_Functions = 8;
} // namespace glslx

void glslx::Serialize::Serialize_VersionDirective(const ast_version_directive *version, std::ostream &out) {
    out.write((const char *)&version->type, sizeof(eVerType));
    out.write((const char *)&version->number, sizeof(int32_t));
}

glslx::ast_version_directive *glslx::Serialize::Deserialize_VersionDirective(std::istream &in) {
    ast_version_directive *ret = dst_->make<ast_version_directive>();
    in.read((char *)&ret->type, sizeof(eVerType));
    in.read((char *)&ret->number, sizeof(int32_t));
    return ret;
}

void glslx::Serialize::Serialize_ExtensionDirective(const ast_extension_directive *extension, std::ostream &out) {
    out.write((const char *)&extension->behavior, sizeof(eExtBehavior));
    const int32_t name_index = string_index_[extension->name];
    out.write((const char *)&name_index, sizeof(int32_t));
}

glslx::ast_extension_directive *glslx::Serialize::Deserialize_ExtensionDirective(std::istream &in) {
    ast_extension_directive *ret = dst_->make<ast_extension_directive>();
    in.read((char *)&ret->behavior, sizeof(eExtBehavior));
    int32_t name_index = -1;
    in.read((char *)&name_index, sizeof(int32_t));
    ret->name = strings_[name_index];
    return ret;
}

void glslx::Serialize::Serialize_Builtin(const ast_builtin *builtin, std::ostream &out) {
    out.write((const char *)&builtin->type, sizeof(eKeyword));
}

glslx::ast_builtin *glslx::Serialize::Deserialize_Builtin(std::istream &in) {
    eKeyword type = eKeyword::K__invalid;
    in.read((char *)&type, sizeof(eKeyword));
    return dst_->make<ast_builtin>(type);
}

void glslx::Serialize::Serialize_DefaultPrecision(const ast_default_precision *precision, std::ostream &out) {
    out.write((const char *)&precision->precision, sizeof(ePrecision));
    const int16_t builtin_index = int16_t(src_->FindBuiltinIndex(precision->type->type));
    assert(builtin_index != -1);
    out.write((const char *)&builtin_index, sizeof(int16_t));
}

glslx::ast_default_precision *glslx::Serialize::Deserialize_DefaultPrecision(std::istream &in) {
    ast_default_precision *ret = dst_->make<ast_default_precision>();
    in.read((char *)&ret->precision, sizeof(ePrecision));
    int16_t builtin_index = -1;
    in.read((char *)&builtin_index, sizeof(int16_t));
    ret->type = dst_->builtins[builtin_index];
    return ret;
}

void glslx::Serialize::Serialize_VariableIdentifier(const ast_variable_identifier *var, std::ostream &out) {
    int32_t var_index = -1;
    if (var) {
        var_index = variable_index_[var->variable];
    }
    out.write((const char *)&var_index, sizeof(int32_t));
}

glslx::ast_variable_identifier *glslx::Serialize::Deserialize_VariableIdentifier(std::istream &in) {
    int32_t var_index = -1;
    in.read((char *)&var_index, sizeof(int32_t));
    if (var_index == -1) {
        return nullptr;
    }
    return dst_->make<ast_variable_identifier>(variables_[var_index]);
}

void glslx::Serialize::Serialize_FieldOrSwizzle(const ast_field_or_swizzle *f, std::ostream &out) {
    Serialize_Expression(f->operand, out);
    Serialize_VariableIdentifier(f->field, out);
    const int32_t name_index = string_index_[f->name];
    out.write((const char *)&name_index, sizeof(int32_t));
}

glslx::ast_field_or_swizzle *glslx::Serialize::Deserialize_FieldOrSwizzle(std::istream &in) {
    ast_field_or_swizzle *ret = dst_->make<ast_field_or_swizzle>();
    ret->operand = Deserialize_Expression(in);
    ret->field = Deserialize_VariableIdentifier(in);
    int32_t name_index = -1;
    in.read((char *)&name_index, sizeof(int32_t));
    ret->name = strings_[name_index];
    return ret;
}

void glslx::Serialize::Serialize_ArraySubscript(const ast_array_subscript *s, std::ostream &out) {
    Serialize_Expression(s->operand, out);
    Serialize_Expression(s->index, out);
}

glslx::ast_array_subscript *glslx::Serialize::Deserialize_ArraySubscript(std::istream &in) {
    ast_array_subscript *ret = dst_->make<ast_array_subscript>();
    ret->operand = Deserialize_Expression(in);
    ret->index = Deserialize_Expression(in);
    return ret;
}

void glslx::Serialize::Serialize_FunctionCall(const ast_function_call *c, std::ostream &out) {
    const int32_t name_index = string_index_[c->name];
    out.write((const char *)&name_index, sizeof(int32_t));
    int32_t func_index = -1;
    if (c->func) {
        func_index = function_index_[c->func];
    }
    out.write((const char *)&func_index, sizeof(int32_t));
    const int32_t params_count = int32_t(c->parameters.size());
    out.write((const char *)&params_count, sizeof(int32_t));
    for (const ast_expression *p : c->parameters) {
        Serialize_Expression(p, out);
    }
}

glslx::ast_function_call *glslx::Serialize::Deserialize_FunctionCall(std::istream &in) {
    ast_function_call *ret = dst_->make<ast_function_call>(dst_->alloc.allocator);
    int32_t name_index = -1;
    in.read((char *)&name_index, sizeof(int32_t));
    ret->name = strings_[name_index];
    int32_t func_index = -1;
    in.read((char *)&func_index, sizeof(int32_t));
    if (func_index != -1) {
        ret->func = functions_[func_index];
    }
    int32_t params_count = -1;
    in.read((char *)&params_count, sizeof(int32_t));
    for (int32_t i = 0; i < params_count; ++i) {
        ret->parameters.push_back(Deserialize_Expression(in));
    }
    return ret;
}

void glslx::Serialize::Serialize_ConstructorCall(const ast_constructor_call *c, std::ostream &out) {
    const int32_t type_index = type_index_[c->type];
    out.write((const char *)&type_index, sizeof(int32_t));
    const int32_t params_count = int32_t(c->parameters.size());
    out.write((const char *)&params_count, sizeof(int32_t));
    for (const ast_expression *p : c->parameters) {
        Serialize_Expression(p, out);
    }
}

glslx::ast_constructor_call *glslx::Serialize::Deserialize_ConstructorCall(std::istream &in) {
    ast_constructor_call *ret = dst_->make<ast_constructor_call>(dst_->alloc.allocator);
    int32_t type_index = -1;
    in.read((char *)&type_index, sizeof(int32_t));
    ret->type = types_[type_index];
    int32_t params_count = -1;
    in.read((char *)&params_count, sizeof(int32_t));
    for (int32_t i = 0; i < params_count; ++i) {
        ret->parameters.push_back(Deserialize_Expression(in));
    }
    return ret;
}

void glslx::Serialize::Serialize_Assignment(const ast_assignment_expression *a, std::ostream &out) {
    out.write((const char *)&a->oper, sizeof(eOperator));
    Serialize_Expression(a->operand1, out);
    Serialize_Expression(a->operand2, out);
}

glslx::ast_assignment_expression *glslx::Serialize::Deserialize_Assignment(std::istream &in) {
    eOperator oper;
    in.read((char *)&oper, sizeof(eOperator));
    ast_assignment_expression *ret = dst_->make<ast_assignment_expression>(oper);
    ret->operand1 = Deserialize_Expression(in);
    ret->operand2 = Deserialize_Expression(in);
    return ret;
}

void glslx::Serialize::Serialize_Sequence(const ast_sequence_expression *s, std::ostream &out) {
    Serialize_Expression(s->operand1, out);
    Serialize_Expression(s->operand2, out);
}

glslx::ast_sequence_expression *glslx::Serialize::Deserialize_Sequence(std::istream &in) {
    ast_sequence_expression *ret = dst_->make<ast_sequence_expression>();
    ret->operand1 = Deserialize_Expression(in);
    ret->operand2 = Deserialize_Expression(in);
    return ret;
}

void glslx::Serialize::Serialize_Operation(const ast_operation_expression *o, std::ostream &out) {
    out.write((const char *)&o->oper, sizeof(eOperator));
    Serialize_Expression(o->operand1, out);
    Serialize_Expression(o->operand2, out);
}

glslx::ast_operation_expression *glslx::Serialize::Deserialize_Operation(std::istream &in) {
    eOperator oper;
    in.read((char *)&oper, sizeof(eOperator));
    ast_operation_expression *ret = dst_->make<ast_operation_expression>(oper);
    ret->operand1 = Deserialize_Expression(in);
    ret->operand2 = Deserialize_Expression(in);
    return ret;
}

void glslx::Serialize::Serialize_Ternary(const ast_ternary_expression *t, std::ostream &out) {
    Serialize_Expression(t->condition, out);
    Serialize_Expression(t->on_true, out);
    Serialize_Expression(t->on_false, out);
}

glslx::ast_ternary_expression *glslx::Serialize::Deserialize_Ternary(std::istream &in) {
    ast_ternary_expression *ret = dst_->make<ast_ternary_expression>();
    ret->condition = Deserialize_Expression(in);
    ret->on_true = Deserialize_Expression(in);
    ret->on_false = Deserialize_Expression(in);
    return ret;
}

void glslx::Serialize::Serialize_ArraySpecifier(const ast_array_specifier *s, std::ostream &out) {
    const int32_t expr_count = int32_t(s->expressions.size());
    out.write((const char *)&expr_count, sizeof(int32_t));
    for (const ast_expression *e : s->expressions) {
        Serialize_Expression(e, out);
    }
}

glslx::ast_array_specifier *glslx::Serialize::Deserialize_ArraySpecifier(std::istream &in) {
    ast_array_specifier *ret = dst_->make<ast_array_specifier>(dst_->alloc.allocator);
    int32_t expr_count = -1;
    in.read((char *)&expr_count, sizeof(int32_t));
    for (int32_t i = 0; i < expr_count; ++i) {
        ret->expressions.push_back(Deserialize_Expression(in));
    }
    return ret;
}

void glslx::Serialize::Serialize_Expression(const ast_expression *e, std::ostream &out) {
    const eExprType type = e ? e->type : eExprType::Undefined;
    out.write((const char *)&type, sizeof(eExprType));
    switch (type) {
    case eExprType::Undefined:
        return;
    case eExprType::ShortConstant:
        return Serialize_Constant(static_cast<const ast_short_constant *>(e), out);
    case eExprType::UShortConstant:
        return Serialize_Constant(static_cast<const ast_ushort_constant *>(e), out);
    case eExprType::IntConstant:
        return Serialize_Constant(static_cast<const ast_int_constant *>(e), out);
    case eExprType::UIntConstant:
        return Serialize_Constant(static_cast<const ast_uint_constant *>(e), out);
    case eExprType::LongConstant:
        return Serialize_Constant(static_cast<const ast_long_constant *>(e), out);
    case eExprType::ULongConstant:
        return Serialize_Constant(static_cast<const ast_ulong_constant *>(e), out);
    case eExprType::HalfConstant:
        return Serialize_Constant(static_cast<const ast_half_constant *>(e), out);
    case eExprType::FloatConstant:
        return Serialize_Constant(static_cast<const ast_float_constant *>(e), out);
    case eExprType::DoubleConstant:
        return Serialize_Constant(static_cast<const ast_double_constant *>(e), out);
    case eExprType::BoolConstant:
        return Serialize_Constant(static_cast<const ast_bool_constant *>(e), out);
    case eExprType::VariableIdentifier:
        return Serialize_VariableIdentifier(static_cast<const ast_variable_identifier *>(e), out);
    case eExprType::FieldOrSwizzle:
        return Serialize_FieldOrSwizzle(static_cast<const ast_field_or_swizzle *>(e), out);
    case eExprType::ArraySubscript:
        return Serialize_ArraySubscript(static_cast<const ast_array_subscript *>(e), out);
    case eExprType::FunctionCall:
        return Serialize_FunctionCall(static_cast<const ast_function_call *>(e), out);
    case eExprType::ConstructorCall:
        return Serialize_ConstructorCall(static_cast<const ast_constructor_call *>(e), out);
    case eExprType::PostIncrement:
        return Serialize_Expression(static_cast<const ast_post_increment_expression *>(e)->operand, out);
    case eExprType::PostDecrement:
        return Serialize_Expression(static_cast<const ast_post_decrement_expression *>(e)->operand, out);
    case eExprType::UnaryPlus:
        return Serialize_Expression(static_cast<const ast_unary_plus_expression *>(e)->operand, out);
    case eExprType::UnaryMinus:
        return Serialize_Expression(static_cast<const ast_unary_minus_expression *>(e)->operand, out);
    case eExprType::BitNot:
        return Serialize_Expression(static_cast<const ast_unary_bit_not_expression *>(e)->operand, out);
    case eExprType::LogicalNot:
        return Serialize_Expression(static_cast<const ast_unary_logical_not_expression *>(e)->operand, out);
    case eExprType::PrefixIncrement:
        return Serialize_Expression(static_cast<const ast_prefix_increment_expression *>(e)->operand, out);
    case eExprType::PrefixDecrement:
        return Serialize_Expression(static_cast<const ast_prefix_decrement_expression *>(e)->operand, out);
    case eExprType::Assign:
        return Serialize_Assignment(static_cast<const ast_assignment_expression *>(e), out);
    case eExprType::Sequence:
        return Serialize_Sequence(static_cast<const ast_sequence_expression *>(e), out);
    case eExprType::Operation:
        return Serialize_Operation(static_cast<const ast_operation_expression *>(e), out);
    case eExprType::Ternary:
        return Serialize_Ternary(static_cast<const ast_ternary_expression *>(e), out);
    case eExprType::ArraySpecifier:
        return Serialize_ArraySpecifier(static_cast<const ast_array_specifier *>(e), out);
    }
}

glslx::ast_expression *glslx::Serialize::Deserialize_Expression(std::istream &in) {
    eExprType type = eExprType::Undefined;
    in.read((char *)&type, sizeof(eExprType));
    switch (type) {
    case eExprType::Undefined:
        return nullptr;
    case eExprType::ShortConstant:
        return Deserialize_Constant<ast_short_constant>(in);
    case eExprType::UShortConstant:
        return Deserialize_Constant<ast_ushort_constant>(in);
    case eExprType::IntConstant:
        return Deserialize_Constant<ast_int_constant>(in);
    case eExprType::UIntConstant:
        return Deserialize_Constant<ast_uint_constant>(in);
    case eExprType::LongConstant:
        return Deserialize_Constant<ast_long_constant>(in);
    case eExprType::ULongConstant:
        return Deserialize_Constant<ast_ulong_constant>(in);
    case eExprType::HalfConstant:
        return Deserialize_Constant<ast_half_constant>(in);
    case eExprType::FloatConstant:
        return Deserialize_Constant<ast_float_constant>(in);
    case eExprType::DoubleConstant:
        return Deserialize_Constant<ast_double_constant>(in);
    case eExprType::BoolConstant:
        return Deserialize_Constant<ast_bool_constant>(in);
    case eExprType::VariableIdentifier:
        return Deserialize_VariableIdentifier(in);
    case eExprType::FieldOrSwizzle:
        return Deserialize_FieldOrSwizzle(in);
    case eExprType::ArraySubscript:
        return Deserialize_ArraySubscript(in);
    case eExprType::FunctionCall:
        return Deserialize_FunctionCall(in);
    case eExprType::ConstructorCall:
        return Deserialize_ConstructorCall(in);
    case eExprType::PostIncrement:
        return dst_->make<ast_post_increment_expression>(Deserialize_Expression(in));
    case eExprType::PostDecrement:
        return dst_->make<ast_post_decrement_expression>(Deserialize_Expression(in));
    case eExprType::UnaryPlus:
        return dst_->make<ast_unary_plus_expression>(Deserialize_Expression(in));
    case eExprType::UnaryMinus:
        return dst_->make<ast_unary_minus_expression>(Deserialize_Expression(in));
    case eExprType::BitNot:
        return dst_->make<ast_unary_bit_not_expression>(Deserialize_Expression(in));
    case eExprType::LogicalNot:
        return dst_->make<ast_unary_logical_not_expression>(Deserialize_Expression(in));
    case eExprType::PrefixIncrement:
        return dst_->make<ast_prefix_increment_expression>(Deserialize_Expression(in));
    case eExprType::PrefixDecrement:
        return dst_->make<ast_prefix_decrement_expression>(Deserialize_Expression(in));
    case eExprType::Assign:
        return Deserialize_Assignment(in);
    case eExprType::Sequence:
        return Deserialize_Sequence(in);
    case eExprType::Operation:
        return Deserialize_Operation(in);
    case eExprType::Ternary:
        return Deserialize_Ternary(in);
    case eExprType::ArraySpecifier:
        return Deserialize_ArraySpecifier(in);
    }
    return nullptr;
}

void glslx::Serialize::Serialize_CompoundStatement(const ast_compound_statement *s, std::ostream &out) {
    const int32_t statement_count = int32_t(s->statements.size());
    out.write((const char *)&statement_count, sizeof(int32_t));
    for (const ast_statement *st : s->statements) {
        Serialize_Statement(st, out);
    }
}

glslx::ast_compound_statement *glslx::Serialize::Deserialize_CompoundStatement(std::istream &in) {
    ast_compound_statement *ret = dst_->make<ast_compound_statement>(dst_->alloc.allocator);
    int32_t statement_count = -1;
    in.read((char *)&statement_count, sizeof(int32_t));
    for (int32_t i = 0; i < statement_count; ++i) {
        ret->statements.push_back(Deserialize_Statement(in));
    }
    return ret;
}

void glslx::Serialize::Serialize_DeclarationStatement(const ast_declaration_statement *s, std::ostream &out) {
    const int32_t variables_count = int32_t(s->variables.size());
    out.write((const char *)&variables_count, sizeof(int32_t));
    for (const ast_function_variable *var : s->variables) {
        Serialize_Variable(var, out);
        variable_index_.Insert(var, variable_index_.size());
    }
}

glslx::ast_declaration_statement *glslx::Serialize::Deserialize_DeclarationStatement(std::istream &in) {
    ast_declaration_statement *ret = dst_->make<ast_declaration_statement>(dst_->alloc.allocator);
    int32_t variables_count = -1;
    in.read((char *)&variables_count, sizeof(int32_t));
    for (int32_t i = 0; i < variables_count; ++i) {
        ast_variable *var = Deserialize_Variable(in);
        assert(var->type == eVariableType::Function);
        ret->variables.push_back(static_cast<ast_function_variable *>(var));
        variables_.push_back(var);
    }
    return ret;
}

void glslx::Serialize::Serialize_ExpressionStatement(const ast_expression_statement *s, std::ostream &out) {
    Serialize_Expression(s->expression, out);
}

glslx::ast_expression_statement *glslx::Serialize::Deserialize_ExpressionStatement(std::istream &in) {
    return dst_->make<ast_expression_statement>(Deserialize_Expression(in));
}

void glslx::Serialize::Serialize_IfStatement(const ast_if_statement *s, std::ostream &out) {
    out.write((const char *)&s->attributes, sizeof(Bitmask<eCtrlFlowAttribute>));
    Serialize_Expression(s->condition, out);
    Serialize_Statement(s->then_statement, out);
    Serialize_Statement(s->else_statement, out);
}

glslx::ast_if_statement *glslx::Serialize::Deserialize_IfStatement(std::istream &in) {
    Bitmask<eCtrlFlowAttribute> attributes;
    in.read((char *)&attributes, sizeof(Bitmask<eCtrlFlowAttribute>));
    ast_if_statement *ret = dst_->make<ast_if_statement>(attributes);
    ret->condition = Deserialize_Expression(in);
    ret->then_statement = Deserialize_Statement(in);
    ret->else_statement = Deserialize_Statement(in);
    return ret;
}

void glslx::Serialize::Serialize_SwitchStatement(const ast_switch_statement *s, std::ostream &out) {
    out.write((const char *)&s->attributes, sizeof(Bitmask<eCtrlFlowAttribute>));
    Serialize_Expression(s->expression, out);
    const int32_t statements_count = int32_t(s->statements.size());
    out.write((const char *)&statements_count, sizeof(int32_t));
    for (const ast_statement *st : s->statements) {
        Serialize_Statement(st, out);
    }
}

glslx::ast_switch_statement *glslx::Serialize::Deserialize_SwitchStatement(std::istream &in) {
    Bitmask<eCtrlFlowAttribute> attributes;
    in.read((char *)&attributes, sizeof(Bitmask<eCtrlFlowAttribute>));
    ast_switch_statement *ret = dst_->make<ast_switch_statement>(dst_->alloc.allocator, attributes);
    ret->expression = Deserialize_Expression(in);
    int32_t statements_count = -1;
    in.read((char *)&statements_count, sizeof(int32_t));
    for (int32_t i = 0; i < statements_count; ++i) {
        ret->statements.push_back(Deserialize_Statement(in));
    }
    return ret;
}

void glslx::Serialize::Serialize_CaseLabelStatement(const ast_case_label_statement *s, std::ostream &out) {
    out.write((const char *)&s->flags, sizeof(Bitmask<eCaseLabelFlags>));
    Serialize_Expression(s->condition, out);
}

glslx::ast_case_label_statement *glslx::Serialize::Deserialize_CaseLabelStatement(std::istream &in) {
    ast_case_label_statement *ret = dst_->make<ast_case_label_statement>();
    in.read((char *)&ret->flags, sizeof(Bitmask<eCaseLabelFlags>));
    ret->condition = Deserialize_Expression(in);
    return ret;
}

void glslx::Serialize::Serialize_WhileStatement(const ast_while_statement *s, std::ostream &out) {
    out.write((const char *)&s->flow_params, sizeof(ctrl_flow_params_t));
    Serialize_Statement(s->condition, out);
    Serialize_Statement(s->body, out);
}

glslx::ast_while_statement *glslx::Serialize::Deserialize_WhileStatement(std::istream &in) {
    ctrl_flow_params_t flow_params;
    in.read((char *)&flow_params, sizeof(ctrl_flow_params_t));
    ast_while_statement *ret = dst_->make<ast_while_statement>(flow_params);
    ret->condition = static_cast<ast_simple_statement *>(Deserialize_Statement(in));
    ret->body = Deserialize_Statement(in);
    return ret;
}

void glslx::Serialize::Serialize_DoStatement(const ast_do_statement *s, std::ostream &out) {
    out.write((const char *)&s->flow_params, sizeof(ctrl_flow_params_t));
    Serialize_Expression(s->condition, out);
    Serialize_Statement(s->body, out);
}

glslx::ast_do_statement *glslx::Serialize::Deserialize_DoStatement(std::istream &in) {
    ctrl_flow_params_t flow_params;
    in.read((char *)&flow_params, sizeof(ctrl_flow_params_t));
    ast_do_statement *ret = dst_->make<ast_do_statement>(flow_params);
    ret->condition = Deserialize_Expression(in);
    ret->body = Deserialize_Statement(in);
    return ret;
}

void glslx::Serialize::Serialize_ForStatement(const ast_for_statement *s, std::ostream &out) {
    out.write((const char *)&s->flow_params, sizeof(ctrl_flow_params_t));
    Serialize_Statement(s->init, out);
    Serialize_Expression(s->condition, out);
    Serialize_Expression(s->loop, out);
    Serialize_Statement(s->body, out);
}

glslx::ast_for_statement *glslx::Serialize::Deserialize_ForStatement(std::istream &in) {
    ctrl_flow_params_t flow_params;
    in.read((char *)&flow_params, sizeof(ctrl_flow_params_t));
    ast_for_statement *ret = dst_->make<ast_for_statement>(flow_params);
    ret->init = static_cast<ast_simple_statement *>(Deserialize_Statement(in));
    ret->condition = Deserialize_Expression(in);
    ret->loop = Deserialize_Expression(in);
    ret->body = Deserialize_Statement(in);
    return ret;
}

void glslx::Serialize::Serialize_ReturnStatement(const ast_return_statement *s, std::ostream &out) {
    Serialize_Expression(s->expression, out);
}

glslx::ast_return_statement *glslx::Serialize::Deserialize_ReturnStatement(std::istream &in) {
    ast_return_statement *ret = dst_->make<ast_return_statement>();
    ret->expression = Deserialize_Expression(in);
    return ret;
}

void glslx::Serialize::Serialize_ExtJumpStatement(const ast_ext_jump_statement *s, std::ostream &out) {
    out.write((const char *)&s->keyword, sizeof(eKeyword));
}

glslx::ast_ext_jump_statement *glslx::Serialize::Deserialize_ExtJumpStatement(std::istream &in) {
    eKeyword keyword = eKeyword::K__invalid;
    in.read((char *)&keyword, sizeof(eKeyword));
    return dst_->make<ast_ext_jump_statement>(keyword);
}

void glslx::Serialize::Serialize_Statement(const ast_statement *s, std::ostream &out) {
    eStatement type = eStatement::Invalid;
    if (s) {
        type = s->type;
    }
    out.write((const char *)&type, sizeof(eStatement));
    switch (type) {
    case eStatement::Compound:
        return Serialize_CompoundStatement(static_cast<const ast_compound_statement *>(s), out);
    case eStatement::Declaration:
        return Serialize_DeclarationStatement(static_cast<const ast_declaration_statement *>(s), out);
    case eStatement::Expression:
        return Serialize_ExpressionStatement(static_cast<const ast_expression_statement *>(s), out);
    case eStatement::If:
        return Serialize_IfStatement(static_cast<const ast_if_statement *>(s), out);
    case eStatement::Switch:
        return Serialize_SwitchStatement(static_cast<const ast_switch_statement *>(s), out);
    case eStatement::CaseLabel:
        return Serialize_CaseLabelStatement(static_cast<const ast_case_label_statement *>(s), out);
    case eStatement::While:
        return Serialize_WhileStatement(static_cast<const ast_while_statement *>(s), out);
    case eStatement::Do:
        return Serialize_DoStatement(static_cast<const ast_do_statement *>(s), out);
    case eStatement::For:
        return Serialize_ForStatement(static_cast<const ast_for_statement *>(s), out);
    case eStatement::Return:
        return Serialize_ReturnStatement(static_cast<const ast_return_statement *>(s), out);
    case eStatement::ExtJump:
        return Serialize_ExtJumpStatement(static_cast<const ast_ext_jump_statement *>(s), out);
    default:
        break;
    }
}

glslx::ast_statement *glslx::Serialize::Deserialize_Statement(std::istream &in) {
    eStatement type;
    in.read((char *)&type, sizeof(eStatement));
    switch (type) {
    case eStatement::Invalid:
        return nullptr;
    case eStatement::Compound:
        return Deserialize_CompoundStatement(in);
    case eStatement::Empty:
        return dst_->make<ast_empty_statement>();
    case eStatement::Declaration:
        return Deserialize_DeclarationStatement(in);
    case eStatement::Expression:
        return Deserialize_ExpressionStatement(in);
    case eStatement::If:
        return Deserialize_IfStatement(in);
    case eStatement::Switch:
        return Deserialize_SwitchStatement(in);
    case eStatement::CaseLabel:
        return Deserialize_CaseLabelStatement(in);
    case eStatement::While:
        return Deserialize_WhileStatement(in);
    case eStatement::Do:
        return Deserialize_DoStatement(in);
    case eStatement::For:
        return Deserialize_ForStatement(in);
    case eStatement::Continue:
        return dst_->make<ast_continue_statement>();
    case eStatement::Break:
        return dst_->make<ast_break_statement>();
    case eStatement::Return:
        return Deserialize_ReturnStatement(in);
    case eStatement::Discard:
        return dst_->make<ast_discard_statement>();
    case eStatement::ExtJump:
        return Deserialize_ExtJumpStatement(in);
    default:
        break;
    }
    return nullptr;
}

void glslx::Serialize::Serialize_LayoutQualifier(const ast_layout_qualifier *q, std::ostream &out) {
    const int32_t name_index = string_index_[q->name];
    out.write((const char *)&name_index, sizeof(int32_t));
    Serialize_Expression(q->initial_value, out);
}

glslx::ast_layout_qualifier *glslx::Serialize::Deserialize_LayoutQualifier(std::istream &in) {
    ast_layout_qualifier *ret = dst_->make<ast_layout_qualifier>();
    int32_t name_index = -1;
    in.read((char *)&name_index, sizeof(int32_t));
    ret->name = strings_[name_index];
    ret->initial_value = Deserialize_Expression(in);
    return ret;
}

void glslx::Serialize::Serialize_Variable(const ast_variable *variable, std::ostream &out) {
    out.write((const char *)&variable->type, sizeof(eVariableType));
    if (variable->type == eVariableType::Function) {
        const ast_function_variable *var = static_cast<const ast_function_variable *>(variable);
        Serialize_Expression(var->initial_value, out);
    } else if (variable->type == eVariableType::Parameter) {
        const ast_function_parameter *par = static_cast<const ast_function_parameter *>(variable);
        out.write((const char *)&par->qualifiers, sizeof(Bitmask<eParamQualifier>));
    } else if (variable->type == eVariableType::Global) {
        const ast_global_variable *glob = static_cast<const ast_global_variable *>(variable);
        out.write((const char *)&glob->storage, sizeof(eStorage));
        out.write((const char *)&glob->aux_storage, sizeof(eAuxStorage));
        out.write((const char *)&glob->memory_flags, sizeof(Bitmask<eMemory>));
        out.write((const char *)&glob->interpolation, sizeof(eInterpolation));
        Serialize_Expression(glob->initial_value, out);
        const int32_t qualifiers_count = int32_t(glob->layout_qualifiers.size());
        out.write((const char *)&qualifiers_count, sizeof(int32_t));
        for (int32_t i = 0; i < qualifiers_count; ++i) {
            Serialize_LayoutQualifier(glob->layout_qualifiers[i], out);
        }
    }
    out.write((const char *)&variable->flags, sizeof(Bitmask<eVariableFlags>));
    out.write((const char *)&variable->precision, sizeof(ePrecision));
    int32_t name_index = -1;
    if (variable->name) {
        name_index = string_index_[variable->name];
    }
    out.write((const char *)&name_index, sizeof(int32_t));
    const int32_t type_index = type_index_[variable->base_type];
    out.write((const char *)&type_index, sizeof(int32_t));
    const int32_t array_sizes_count = int32_t(variable->array_sizes.size());
    out.write((const char *)&array_sizes_count, sizeof(int32_t));
    for (int32_t i = 0; i < array_sizes_count; ++i) {
        Serialize_Expression(variable->array_sizes[i], out);
    }
}

glslx::ast_variable *glslx::Serialize::Deserialize_Variable(std::istream &in) {
    eVariableType type;
    in.read((char *)&type, sizeof(eVariableType));
    ast_variable *ret = nullptr;
    if (type == eVariableType::Function) {
        ast_function_variable *var = dst_->make<ast_function_variable>(dst_->alloc.allocator);
        var->initial_value = Deserialize_Expression(in);
        ret = var;
    } else if (type == eVariableType::Parameter) {
        ast_function_parameter *par = dst_->make<ast_function_parameter>(dst_->alloc.allocator);
        in.read((char *)&par->qualifiers, sizeof(Bitmask<eParamQualifier>));
        ret = par;
    } else if (type == eVariableType::Global) {
        ast_global_variable *glob = dst_->make<ast_global_variable>(dst_->alloc.allocator);
        in.read((char *)&glob->storage, sizeof(eStorage));
        in.read((char *)&glob->aux_storage, sizeof(eAuxStorage));
        in.read((char *)&glob->memory_flags, sizeof(Bitmask<eMemory>));
        in.read((char *)&glob->interpolation, sizeof(eInterpolation));
        glob->initial_value = Deserialize_Expression(in);
        int32_t qualifiers_count = -1;
        in.read((char *)&qualifiers_count, sizeof(int32_t));
        for (int32_t i = 0; i < qualifiers_count; ++i) {
            glob->layout_qualifiers.push_back(Deserialize_LayoutQualifier(in));
        }
        ret = glob;
    } else {
        assert(type == eVariableType::Field);
        ret = dst_->make<ast_variable>(type, dst_->alloc.allocator);
    }
    in.read((char *)&ret->flags, sizeof(Bitmask<eVariableFlags>));
    in.read((char *)&ret->precision, sizeof(ePrecision));
    int32_t name_index = -1;
    in.read((char *)&name_index, sizeof(int32_t));
    if (name_index != -1) {
        ret->name = strings_[name_index];
    }
    int32_t type_index = -1;
    in.read((char *)&type_index, sizeof(int32_t));
    ret->base_type = types_[type_index];
    int32_t array_sizes_count = -1;
    in.read((char *)&array_sizes_count, sizeof(int32_t));
    for (int32_t i = 0; i < array_sizes_count; ++i) {
        ret->array_sizes.push_back(Deserialize_Expression(in));
    }
    return ret;
}

void glslx::Serialize::Serialize_Structure(const ast_struct *structure, std::ostream &out) {
    int32_t name_index = -1;
    if (structure->name) {
        name_index = string_index_[structure->name];
    }
    out.write((const char *)&name_index, sizeof(int32_t));
    const int32_t fields_count = int32_t(structure->fields.size());
    out.write((const char *)&fields_count, sizeof(int32_t));
    for (int32_t i = 0; i < fields_count; ++i) {
        Serialize_Variable(structure->fields[i], out);
        variable_index_.Insert(structure->fields[i], variable_index_.size());
    }
}

glslx::ast_struct *glslx::Serialize::Deserialize_Structure(std::istream &in) {
    ast_struct *ret = dst_->make<ast_struct>(dst_->alloc.allocator);
    int32_t name_index = -1;
    in.read((char *)&name_index, sizeof(int32_t));
    if (name_index != -1) {
        ret->name = strings_[name_index];
    }
    int32_t fields_count = -1;
    in.read((char *)&fields_count, sizeof(int32_t));
    for (int32_t i = 0; i < fields_count; ++i) {
        ret->fields.push_back(Deserialize_Variable(in));
        variables_.push_back(ret->fields.back());
    }
    return ret;
}

void glslx::Serialize::Serialize_InterfaceBlock(const ast_interface_block *block, std::ostream &out) {
    out.write((const char *)&block->storage, sizeof(eStorage));
    out.write((const char *)&block->memory_flags, sizeof(Bitmask<eMemory>));
    const int32_t qualifiers_count = int32_t(block->layout_qualifiers.size());
    out.write((const char *)&qualifiers_count, sizeof(int32_t));
    for (int32_t i = 0; i < qualifiers_count; ++i) {
        Serialize_LayoutQualifier(block->layout_qualifiers[i], out);
    }
    Serialize_Structure(block, out);
}

glslx::ast_interface_block *glslx::Serialize::Deserialize_InterfaceBlock(std::istream &in) {
    ast_interface_block *ret = dst_->make<ast_interface_block>(dst_->alloc.allocator);
    in.read((char *)&ret->storage, sizeof(eStorage));
    in.read((char *)&ret->memory_flags, sizeof(Bitmask<eMemory>));
    int32_t qualifiers_count = -1;
    in.read((char *)&qualifiers_count, sizeof(int32_t));
    for (int32_t i = 0; i < qualifiers_count; ++i) {
        ret->layout_qualifiers.push_back(Deserialize_LayoutQualifier(in));
    }
    ////
    int32_t name_index = -1;
    in.read((char *)&name_index, sizeof(int32_t));
    if (name_index != -1) {
        ret->name = strings_[name_index];
    }
    int32_t fields_count = -1;
    in.read((char *)&fields_count, sizeof(int32_t));
    for (int32_t i = 0; i < fields_count; ++i) {
        ret->fields.push_back(Deserialize_Variable(in));
        variables_.push_back(ret->fields.back());
    }
    return ret;
}

void glslx::Serialize::Serialize_Function(const ast_function *function, std::ostream &out) {
    out.write((const char *)&function->attributes, sizeof(Bitmask<eFunctionAttribute>));
    const int32_t type_index = type_index_[function->return_type];
    out.write((const char *)&type_index, sizeof(int32_t));
    const int32_t name_index = string_index_[function->name];
    out.write((const char *)&name_index, sizeof(int32_t));
    const int32_t params_count = int32_t(function->parameters.size());
    out.write((const char *)&params_count, sizeof(int32_t));
    for (int32_t i = 0; i < params_count; ++i) {
        Serialize_Variable(function->parameters[i], out);
        variable_index_.Insert(function->parameters[i], variable_index_.size());
    }
    const int32_t statements_count = int32_t(function->statements.size());
    out.write((const char *)&statements_count, sizeof(int32_t));
    for (int32_t i = 0; i < statements_count; ++i) {
        Serialize_Statement(function->statements[i], out);
    }
    int32_t prototype_index = -1;
    if (function->prototype) {
        prototype_index = function_index_[function->prototype];
    }
    out.write((const char *)&prototype_index, sizeof(int32_t));
}

glslx::ast_function *glslx::Serialize::Deserialize_Function(std::istream &in) {
    ast_function *ret = dst_->make<ast_function>(dst_->alloc.allocator);
    in.read((char *)&ret->attributes, sizeof(Bitmask<eFunctionAttribute>));
    int32_t type_index = -1;
    in.read((char *)&type_index, sizeof(int32_t));
    ret->return_type = types_[type_index];
    int32_t name_index = -1;
    in.read((char *)&name_index, sizeof(int32_t));
    ret->name = strings_[name_index];
    int32_t params_count = -1;
    in.read((char *)&params_count, sizeof(int32_t));
    for (int32_t i = 0; i < params_count; ++i) {
        ast_variable *var = Deserialize_Variable(in);
        assert(var->type == eVariableType::Parameter);
        ret->parameters.push_back(static_cast<ast_function_parameter *>(var));
        variables_.push_back(var);
    }
    int32_t statements_count = -1;
    in.read((char *)&statements_count, sizeof(int32_t));
    for (int32_t i = 0; i < statements_count; ++i) {
        ret->statements.push_back(Deserialize_Statement(in));
    }
    int32_t prototype_index = -1;
    in.read((char *)&prototype_index, sizeof(int32_t));
    if (prototype_index != -1) {
        ret->prototype = functions_[prototype_index];
    }
    return ret;
}

void glslx::Serialize::SerializeAST(const TrUnit *tu, std::ostream &out) {
    string_index_.clear();
    src_ = tu;
    dst_ = nullptr;

    if (!tu) {
        return;
    }

    out.write((char *)&FormatVersion, sizeof(int32_t));
    out.write((char *)&tu->type, sizeof(eTrUnitType));
    if (tu->version) {
        out.write((char *)&Block_Version, sizeof(int32_t));
        const int32_t version_block_size = sizeof(eVerType) + sizeof(int32_t);
        out.write((char *)&version_block_size, sizeof(int32_t));
        Serialize_VersionDirective(tu->version, out);
    }
    if (!tu->str.empty()) {
        out.write((char *)&Block_Strings, sizeof(int32_t));
        const std::streampos block_size_pos = out.tellp();
        const int32_t temp_block_size = 0;
        out.write((char *)&temp_block_size, sizeof(int32_t));
        for (const char *str : tu->str) {
            string_index_.Insert((void *)str, int32_t(string_index_.size()));
            const int32_t string_len = int32_t(strlen(str));
            out.write((char *)&string_len, sizeof(int32_t));
            out.write(str, string_len);
        }
        const std::streampos strings_end_pos = out.tellp();
        out.seekp(block_size_pos, std::ios::beg);
        const int32_t block_size = int32_t(strings_end_pos - block_size_pos) - sizeof(int32_t);
        out.write((char *)&block_size, sizeof(int32_t));
        out.seekp(strings_end_pos, std::ios::beg);
    }
    if (!tu->extensions.empty()) {
        out.write((char *)&Block_Extensions, sizeof(int32_t));
        const int32_t block_size = int32_t(tu->extensions.size() * (sizeof(eExtBehavior) + sizeof(int32_t)));
        out.write((char *)&block_size, sizeof(int32_t));
        const std::streampos extensions_beg = out.tellp();
        for (const ast_extension_directive *extension : tu->extensions) {
            Serialize_ExtensionDirective(extension, out);
        }
        const std::streampos extensions_end = out.tellp();
        assert(int32_t(extensions_end - extensions_beg) == block_size);
    }
    if (!tu->builtins.empty()) {
        out.write((char *)&Block_Builtins, sizeof(int32_t));
        const int32_t block_size = tu->builtins.size() * sizeof(eKeyword);
        out.write((char *)&block_size, sizeof(int32_t));
        const std::streampos builtins_beg = out.tellp();
        for (const ast_builtin *builtin : tu->builtins) {
            Serialize_Builtin(builtin, out);
            type_index_.Insert(builtin, int32_t(type_index_.size()));
        }
        const std::streampos builtins_end = out.tellp();
        assert(int32_t(builtins_end - builtins_beg) == block_size);
    }
    if (!tu->default_precision.empty()) {
        out.write((char *)&Block_DefaultPrecision, sizeof(int32_t));
        const int32_t block_size = tu->default_precision.size() * (sizeof(ePrecision) + sizeof(int16_t));
        out.write((char *)&block_size, sizeof(int32_t));
        const std::streampos precision_beg = out.tellp();
        for (const ast_default_precision *precision : tu->default_precision) {
            Serialize_DefaultPrecision(precision, out);
        }
        const std::streampos precision_end = out.tellp();
        assert(int32_t(precision_end - precision_beg) == block_size);
    }
    if (!tu->structures.empty()) {
        out.write((char *)&Block_Structures, sizeof(int32_t));
        const std::streampos block_size_pos = out.tellp();
        const int32_t temp_block_size = 0;
        out.write((char *)&temp_block_size, sizeof(int32_t));
        for (const ast_struct *structure : tu->structures) {
            Serialize_Structure(structure, out);
            type_index_.Insert(structure, int32_t(type_index_.size()));
        }
        const std::streampos structs_end_pos = out.tellp();
        out.seekp(block_size_pos, std::ios::beg);
        const int32_t block_size = int32_t(structs_end_pos - block_size_pos) - sizeof(int32_t);
        out.write((char *)&block_size, sizeof(int32_t));
        out.seekp(structs_end_pos, std::ios::beg);
    }
    if (!tu->interface_blocks.empty()) {
        out.write((char *)&Block_InterfaceBlocks, sizeof(int32_t));
        const std::streampos block_size_pos = out.tellp();
        const int32_t temp_block_size = 0;
        out.write((char *)&temp_block_size, sizeof(int32_t));
        for (const ast_interface_block *block : tu->interface_blocks) {
            Serialize_InterfaceBlock(block, out);
            type_index_.Insert(block, int32_t(type_index_.size()));
        }
        const std::streampos inter_blocks_end_pos = out.tellp();
        out.seekp(block_size_pos, std::ios::beg);
        const int32_t block_size = int32_t(inter_blocks_end_pos - block_size_pos) - sizeof(int32_t);
        out.write((char *)&block_size, sizeof(int32_t));
        out.seekp(inter_blocks_end_pos, std::ios::beg);
    }
    if (!tu->globals.empty()) {
        out.write((char *)&Block_Globals, sizeof(int32_t));
        const std::streampos block_size_pos = out.tellp();
        const int32_t temp_block_size = 0;
        out.write((char *)&temp_block_size, sizeof(int32_t));
        for (const ast_global_variable *var : tu->globals) {
            Serialize_Variable(var, out);
            variable_index_.Insert(var, variable_index_.size());
        }
        const std::streampos globals_end_pos = out.tellp();
        out.seekp(block_size_pos, std::ios::beg);
        const int32_t block_size = int32_t(globals_end_pos - block_size_pos) - sizeof(int32_t);
        out.write((char *)&block_size, sizeof(int32_t));
        out.seekp(globals_end_pos, std::ios::beg);
    }
    if (!tu->functions.empty()) {
        out.write((char *)&Block_Functions, sizeof(int32_t));
        const std::streampos block_size_pos = out.tellp();
        const int32_t temp_block_size = 0;
        out.write((char *)&temp_block_size, sizeof(int32_t));
        for (const ast_function *f : tu->functions) {
            Serialize_Function(f, out);
            function_index_.Insert(f, function_index_.size());
        }
        const std::streampos functions_end_pos = out.tellp();
        out.seekp(block_size_pos, std::ios::beg);
        const int32_t block_size = int32_t(functions_end_pos - block_size_pos) - sizeof(int32_t);
        out.write((char *)&block_size, sizeof(int32_t));
        out.seekp(functions_end_pos, std::ios::beg);
    }
}

bool glslx::Serialize::DeserializeAST(TrUnit *tu, std::istream &in) {
    strings_.clear();
    src_ = nullptr;
    dst_ = tu;

    int32_t format_version = -1;
    in.read((char *)&format_version, sizeof(int32_t));
    if (format_version != FormatVersion) {
        return false;
    }
    in.read((char *)&tu->type, sizeof(eTrUnitType));
    while (in.peek() != std::ios::traits_type::eof()) {
        int32_t block_id = -1, block_size = 0;
        in.read((char *)&block_id, sizeof(int32_t));
        in.read((char *)&block_size, sizeof(int32_t));
        if (block_id == Block_Version) {
            tu->version = Deserialize_VersionDirective(in);
        } else if (block_id == Block_Strings) {
            const std::streampos block_end = in.tellg() + std::streampos(block_size);
            while (in.tellg() < block_end) {
                int32_t string_len = -1;
                in.read((char *)&string_len, sizeof(int32_t));
                char *new_str = tu->alloc.allocator.allocate(string_len + 1);
                in.read(new_str, string_len);
                new_str[string_len] = '\0';
                tu->str.Insert(new_str);
                strings_.push_back(new_str);
            }
        } else if (block_id == Block_Extensions) {
            const std::streampos block_end = in.tellg() + std::streampos(block_size);
            while (in.tellg() < block_end) {
                tu->extensions.push_back(Deserialize_ExtensionDirective(in));
            }
        } else if (block_id == Block_Builtins) {
            const std::streampos block_end = in.tellg() + std::streampos(block_size);
            while (in.tellg() < block_end) {
                tu->builtins.push_back(Deserialize_Builtin(in));
                types_.push_back(tu->builtins.back());
            }
#ifndef NDEBUG
            // Ensure it's sorted
            for (int i = 0; i < int(tu->builtins.size()) - 1; ++i) {
                assert(tu->builtins[i]->type < tu->builtins[i + 1]->type);
            }
#endif
        } else if (block_id == Block_DefaultPrecision) {
            const std::streampos block_end = in.tellg() + std::streampos(block_size);
            while (in.tellg() < block_end) {
                tu->default_precision.push_back(Deserialize_DefaultPrecision(in));
            }
        } else if (block_id == Block_Structures) {
            const std::streampos block_end = in.tellg() + std::streampos(block_size);
            while (in.tellg() < block_end) {
                tu->structures.push_back(Deserialize_Structure(in));
                tu->structures_by_name.Insert(tu->structures.back()->name, tu->structures.back());
                types_.push_back(tu->structures.back());
            }
        } else if (block_id == Block_InterfaceBlocks) {
            const std::streampos block_end = in.tellg() + std::streampos(block_size);
            while (in.tellg() < block_end) {
                tu->interface_blocks.push_back(Deserialize_InterfaceBlock(in));
                types_.push_back(tu->interface_blocks.back());
            }
        } else if (block_id == Block_Globals) {
            const std::streampos block_end = in.tellg() + std::streampos(block_size);
            while (in.tellg() < block_end) {
                ast_variable *var = Deserialize_Variable(in);
                assert(var->type == eVariableType::Global);
                tu->globals.push_back(static_cast<ast_global_variable *>(var));
                variables_.push_back(var);
            }
        } else if (block_id == Block_Functions) {
            const std::streampos block_end = in.tellg() + std::streampos(block_size);
            while (in.tellg() < block_end) {
                tu->functions.push_back(Deserialize_Function(in));
                functions_.push_back(tu->functions.back());
            }
            in.seekg(block_end, std::ios::beg);
        } else {
            return false;
        }
    }
    return true;
}