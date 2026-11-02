#pragma once

#include "Span.h"
#include "parser/AST.h"

namespace glslx {
struct byteaddress_buf_t {
    const char *name;
};

int Calc_TypeAlignment(const ast_type *type);
int Calc_TypeSize(const ast_type *type);
int Calc_ArrayStride(const ast_type *type);
int Calc_FieldOffset(const ast_type *type, const char *field_name);
int Calc_VariableSize(const ast_variable *v, int array_dim = 0);

global_vector<byteaddress_buf_t> Collect_ByteaddressBuffers(const TrUnit *tu);

// Result type of the sampling/gathering method calls the HLSL lowering pass creates. Its parser-side
// evaluator hands those post-lowering nodes over here.
const ast_type *Evaluate_HLSLMethodResultType(const TrUnit *tu, const ast_method_call *call);

class LowerHLSL {
    TrUnit *tu_ = nullptr;
    global_vector<byteaddress_buf_t> bufs_;
    int temp_index_ = 0;
    int local_size_[3] = { 1, 1, 1 };
    bool local_size_scanned_ = false;
    global_vector<ast_function_call *> pending_renames_;
    global_vector<ast_function_call *> pending_sampler_calls_;
    global_vector<ast_statement *> *hoist_ = nullptr;

    struct access_index_t {
        ast_expression *index = nullptr;
        int multiplier = 0;
    };

    struct buf_access_t {
        int buf_index = -1;
        int offset = -1;
        ast_variable *var = nullptr;
    };

    struct shared_coord_t {
        ast_function_variable *var = nullptr;
        ast_expression *coord = nullptr;
    };
    global_vector<shared_coord_t> shared_coords_;

    buf_access_t Find_BufferAccessExpression(const ast_expression *expression, int offset,
                                             global_vector<access_index_t> &out_indices);

    ast_expression *Build_OffsetSum(const global_vector<access_index_t> &indices, int final_offset);
    ast_expression *Build_Load(ast_variable *buf_var, ast_expression *offset_expr, const ast_type *leaf_type);

    int Build_Stores(ast_variable *buf_var, int offset, ast_expression *value, int array_dim, const ast_type *t,
                     ast_variable *offset_var, global_vector<ast_statement *> &out_statements);
    int Build_Stores(ast_variable *buf_var, int offset, ast_expression *value, int array_dim, const ast_variable *v,
                     ast_variable *offset_var, global_vector<ast_statement *> &out_statements);

    int Build_Loads(ast_variable *buf_var, int offset, ast_expression *target, int array_dim, const ast_type *t,
                    ast_variable *offset_var, global_vector<ast_statement *> &out_statements);
    int Build_Loads(ast_variable *buf_var, int offset, ast_expression *target, int array_dim, const ast_variable *v,
                    ast_variable *offset_var, global_vector<ast_statement *> &out_statements);
    ast_expression *Hoist_BufferRead(const buf_access_t &access, global_vector<access_index_t> &indices,
                                     const ast_type *type);
    ast_expression *Rewrite_Hoisting(ast_expression *expr, global_vector<ast_statement *> &pre);

    ast_statement *Build_ExitCheck(ast_expression *cond);
    void Build_SkipFirst(global_vector<ast_statement *> &decls, global_vector<ast_statement *> &guarded,
                         global_vector<ast_statement *> &out);
    ast_statement *Wrap_LoopBody(global_vector<ast_statement *> &pre, ast_expression *cond, ast_statement *body);

    bool Try_Lower_Store(ast_expression_statement *statement, global_vector<ast_statement *> &out_statements);
    bool Try_Lower_RayQueryInit(ast_expression_statement *statement, global_vector<ast_statement *> &out_statements);
    ast_expression *Lower_Atomics(ast_expression *expr, global_vector<ast_statement *> &pre);
    ast_expression *Lower_LogicalOperation(ast_operation_expression *operation, global_vector<ast_statement *> &pre);
    ast_expression *Lower_GuardedTernary(ast_ternary_expression *ternary, global_vector<ast_statement *> &pre,
                                         uint32_t true_mark, uint32_t false_mark);
    ast_compound_statement *Take_Block(global_vector<ast_statement *> &pre, uint32_t from, ast_statement *tail);
    ast_expression_statement *Build_Assignment(ast_variable *target, ast_expression *value);
    ast_expression *Hoist_Atomic(ast_function_call *call, global_vector<ast_statement *> &pre);
    ast_expression *Hoist_TextureSize(ast_function_call *call, global_vector<ast_statement *> &pre);
    ast_expression *Hoist_SharedCoord(ast_expression *coord);
    ast_expression *Hoist_MatrixScalar(ast_expression *scalar, const ast_type *scalar_type);
    void Flush_SharedCoord(const ast_variable *var, global_vector<ast_statement *> &pre);
    int Image_ComponentCount(const ast_variable *v);
    ast_expression *Rewrite_TextureImageCall(ast_function_call *call);
    ast_expression *Rewrite_SampleCall(ast_function_call *call);
    ast_expression *Rewrite_RayQueryCall(ast_function_call *call);
    ast_expression *Rewrite_GlSpecial(const char *var_name);
    ast_expression *Rewrite_Expression(ast_expression *expr);
    void Rewrite_SimpleStatement(ast_simple_statement *stmt, global_vector<ast_statement *> &pre);
    void Lower_StatementReplacements(ast_statement *statement, global_vector<ast_statement *> &out_statements);
    void Lower_StatementList(local_vector<ast_statement *> &statements);
    void Lower_Slot(ast_statement *&slot);
    void Lower_ComputePrologue(TrUnit *tu);

  public:
    void Apply(TrUnit *tu);
};
} // namespace glslx
