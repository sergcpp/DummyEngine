#pragma once

#include <memory>

#include "../Bitmask.h"
#include "AST.h"
#include "Lexer.h"

namespace glslx {
struct top_level_t {
    eStorage storage = eStorage::None;
    eAuxStorage aux_storage = eAuxStorage::None;
    Bitmask<eMemory> memory_flags;
    ePrecision precision = ePrecision::None;
    eInterpolation interpolation = eInterpolation::None;
    ast_type *type = nullptr;
    ast_constant_expression *initial_value = nullptr;
    std::vector<ast_constant_expression *> array_sizes;
    size_t array_on_type_offset = 0;
    std::vector<ast_layout_qualifier *> layout_qualifiers;
    bool is_invariant = false;
    bool is_precise = false;
    bool is_array = false;
    const char *name = nullptr;
};

class Parser {
    using scope = std::vector<ast_variable *>;

    std::unique_ptr<TrUnit> ast_;
    Lexer lexer_;
    std::string_view source_;
    token_t tok_;
    std::vector<scope> scopes_;
    std::vector<ast_builtin *> builtins_;
    const char *error_ = nullptr;
    const char *file_name_ = nullptr;

    std::vector<ast_function_call *> function_calls_;

    char *strnew(const char *str) {
        if (!str) {
            return nullptr;
        }
        const size_t size = strlen(str) + 1;
        char *copy = (char *)malloc(size);
        if (!copy) {
            return nullptr;
        }
        memcpy(copy, str, size);
        ast_->str.push_back(copy);
        return copy;
    }

    template <class T, class... Args> T *astnew(Args... args) {
        return new (&ast_->mem) T(std::forward<Args>(args)...);
    }

    static bool strnil(const char *str) { return !str || !*str; }

    enum class eEndCondition { Semicolon, Parenthesis, Bracket, Colon, Comma };

    template <typename T> T *ParseBlock(const char *type);

    bool ParseSource(std::string_view source);

  public:
    Parser(std::string_view source, const char *file_name);

    std::unique_ptr<TrUnit> Parse(eTrUnitType type);

    const char *error() const { return error_; }

  protected:
    bool next();
    bool expect(eTokType type);
    bool expect(eOperator op);

    ast_global_variable *AddHiddenGlobal(ast_builtin *type, const char *name, bool is_array = false,
                                         eStorage storage = eStorage::In,
                                         ePrecision precision = ePrecision::None);
    bool InitSpecialGlobals(eTrUnitType type);

    bool ParseTopLevel(std::vector<top_level_t> &items);
    bool ParseTopLevelItem(top_level_t &level, top_level_t *continuation = nullptr);

    bool ParseStorage(top_level_t &current);
    bool ParseAuxStorage(top_level_t &current);
    bool ParseInterpolation(top_level_t &current);
    bool ParsePrecision(ePrecision &precision);
    bool ParseInvariant(top_level_t &current);
    bool ParsePrecise(top_level_t &current);
    bool ParseMemoryFlags(top_level_t &current);
    bool ParseLayout(top_level_t &current);

    bool is_type(const eTokType type) const { return tok_.type == type; }
    bool is_keyword(const eKeyword keyword) const {
        return tok_.type == eTokType::Keyword && tok_.as_keyword == keyword;
    }
    static bool is_reserved_keyword(eKeyword keyword);
    static bool is_interface_block_storage(eStorage storage);
    static bool is_generic_type(eKeyword keyword);
    bool is_operator(const eOperator oper) const { return tok_.type == eTokType::Operator && tok_.as_operator == oper; }
    bool is_end_condition(Bitmask<eEndCondition> condition) const;
    bool is_builtin() const;

    static bool is_vector_type(const ast_type *type);
    static bool is_constant_value(const ast_expression *expression);
    static bool is_constant(const ast_expression *expression);

    void fatal(const char *fmt, ...);

    ast_constant_expression *Evaluate(ast_expression *expression);

    // Type parsers
    ast_builtin *ParseBuiltin();
    ast_builtin *FindOrAddBuiltin(eKeyword type);
    ast_struct *ParseStruct();
    ast_interface_block *ParseInterfaceBlock(eStorage storage);

    ast_function *ParseFunction(const top_level_t &parse);

    // Call parsers
    ast_constructor_call *ParseConstructorCall();
    ast_function_call *ParseFunctionCall();

    // Expression parsers
    ast_expression *ParseExpression(Bitmask<eEndCondition> condition);
    ast_expression *ParseUnary(Bitmask<eEndCondition> condition);
    ast_expression *ParseBinary(int lhs_precedence, ast_expression *lhs, Bitmask<eEndCondition> condition);
    ast_expression *ParseUnaryPrefix(Bitmask<eEndCondition> condition);
    ast_constant_expression *ParseArraySize();
    ast_expression *ParseArraySpecifier(Bitmask<eEndCondition> condition);

    // Statement parsers
    ast_statement *ParseStatement();
    ast_compound_statement *ParseCompoundStatement();
    ast_if_statement *ParseIfStatement(Bitmask<eCtrlFlowAttribute> attributes);
    ast_switch_statement *ParseSwitchStatement(Bitmask<eCtrlFlowAttribute> attributes);
    ast_case_label_statement *ParseCaseLabelStatement();
    ast_simple_statement *ParseDeclarationOrExpressionStatement(Bitmask<eEndCondition> condition);
    ast_declaration_statement *ParseDeclarationStatement(Bitmask<eEndCondition> condition);
    ast_expression_statement *ParseExpressionStatement(Bitmask<eEndCondition> condition);
    ast_continue_statement *ParseContinueStatement();
    ast_break_statement *ParseBreakStatement();
    ast_discard_statement *ParseDiscardStatement();
    ast_return_statement *ParseReturnStatement();
    ast_ext_jump_statement *ParseExtJumpStatement();
    ast_for_statement *ParseForStatement(const ctrl_flow_params_t &ctrl_flow);
    ast_do_statement *ParseDoStatement(const ctrl_flow_params_t &ctrl_flow);
    ast_while_statement *ParseWhileStatement(const ctrl_flow_params_t &ctrl_flow);

    ast_binary_expression *CreateExpression();

    ast_type *FindType(const char *name);
    ast_variable *FindVariable(const char *name);
    ast_type *GetType(ast_expression *expression);
};

int get_vector_size(eKeyword type);
int get_vector_size(const ast_type *type);

int is_matrix_type(const ast_type *type);

bool is_same_type(const ast_type *type1, const ast_type *type2);

const ast_type *Evaluate_ExpressionResultType(const TrUnit *tu, const ast_expression *expression, int &array_dims);
} // namespace glslx