#pragma once

#include <memory>

#if defined(_MSC_VER)
#include <sal.h>
#define CHECK_FORMAT_STRING(format_string_index_one_based, vargs_index_one_based)
#else
#define _Printf_format_string_
#define CHECK_FORMAT_STRING(format_string_index_one_based, vargs_index_one_based)                                      \
    __attribute__((format(printf, format_string_index_one_based, vargs_index_one_based)))
#endif

#include "../Bitmask.h"
#include "AST.h"
#include "Lexer.h"

namespace glslx {
class Parser {
    using scope = global_vector<ast_variable *>;

    std::unique_ptr<TrUnit> ast_;
    MultiPoolAllocator<char> allocator_ = MultiPoolAllocator<char>(8, 128);
    Lexer lexer_;
    std::string_view source_;
    token_t tok_;
    global_vector<scope> scopes_;
    const char *error_ = nullptr;
    const char *file_name_ = nullptr;

    global_vector<ast_function_call *> function_calls_;

    static bool strnil(const char *str) { return !str || !*str; }

    enum class eEndCondition { Semicolon, Parenthesis, Bracket, Colon, Comma };

    template <typename T> T *ParseBlock(const char *type);

    bool ParseSource(std::string_view source);

    std::string DumpASTBlob();

  public:
    Parser(std::string_view source, const char *file_name);

    std::unique_ptr<TrUnit> Parse(eTrUnitType type);

    [[nodiscard]] const char *error() const { return error_; }

  protected:
    struct top_level_t {
        eStorage storage = eStorage::None;
        eAuxStorage aux_storage = eAuxStorage::None;
        Bitmask<eMemory> memory_flags;
        ePrecision precision = ePrecision::None;
        eInterpolation interpolation = eInterpolation::None;
        ast_type *type = nullptr;
        ast_constant_expression *initial_value = nullptr;
        local_vector<ast_constant_expression *> array_sizes;
        size_t array_on_type_offset = 0;
        local_vector<ast_layout_qualifier *> layout_qualifiers;
        bool is_invariant = false;
        bool is_precise = false;
        bool is_array = false;
        const char *name = nullptr;

        explicit top_level_t(MultiPoolAllocator<char> &alloc) : array_sizes(alloc), layout_qualifiers(alloc) {}
    };

    bool next();
    bool expect(eTokType type);
    bool expect(eOperator op);

    ast_global_variable *AddHiddenGlobal(eKeyword type, const char *name, Bitmask<eVariableFlags> flags = {},
                                         eStorage storage = eStorage::In, ePrecision precision = ePrecision::None);
    bool InitSpecialGlobals(eTrUnitType type);

    bool ParseTopLevel(global_vector<top_level_t> &items);
    bool ParseTopLevelItem(top_level_t &level, top_level_t *continuation = nullptr);

    bool ParseStorage(top_level_t &current);
    bool ParseAuxStorage(top_level_t &current);
    bool ParseInterpolation(top_level_t &current);
    bool ParsePrecision(ePrecision &precision);
    bool ParseInvariant(top_level_t &current);
    bool ParsePrecise(top_level_t &current);
    bool ParseMemoryFlags(top_level_t &current);
    bool ParseLayout(top_level_t &current);

    [[nodiscard]] bool is_type(const eTokType type) const { return tok_.type == type; }
    [[nodiscard]] bool is_keyword(const eKeyword keyword) const {
        return tok_.type == eTokType::Keyword && tok_.as_keyword == keyword;
    }
    static bool is_reserved_keyword(eKeyword keyword);
    static bool is_interface_block_storage(eStorage storage);
    static bool is_generic_type(eKeyword keyword);
    [[nodiscard]] bool is_operator(const eOperator oper) const {
        return tok_.type == eTokType::Operator && tok_.as_operator == oper;
    }
    [[nodiscard]] bool is_end_condition(Bitmask<eEndCondition> condition) const;
    [[nodiscard]] bool is_builtin() const;

    static bool is_vector_type(const ast_type *type);

    void fatal(_Printf_format_string_ const char *fmt, ...) CHECK_FORMAT_STRING(2, 3);

    ast_constant_expression *Evaluate(ast_expression *expression);

    // Type parsers
    ast_builtin *ParseBuiltin();
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

extern const char g_builtin_impl[];

int get_vector_size(eKeyword type);
int get_vector_size(const ast_type *type);

int is_matrix_type(const ast_type *type);

bool is_same_type(const ast_type *type1, const ast_type *type2);
bool is_compatible_type(const ast_type *_type1, const ast_type *_type2);

const ast_type *Evaluate_ExpressionResultType(const TrUnit *tu, const ast_expression *expression, int &array_dims);
} // namespace glslx
