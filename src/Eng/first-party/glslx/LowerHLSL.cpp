#include "LowerHLSL.h"

#include <algorithm>
#include <cassert>
#include <cstring>
#include <string>

#include "parser/Parser.h"

namespace glslx {
bool is_int_image(const ast_type *base_type);
bool is_uint_image(const ast_type *base_type);
bool is_combined_sampler_argument(const ast_expression *expr);
bool is_combined_texturesampler(const ast_type *base_type);

extern const HashSet32<const char *> g_atomic_functions{
    {"atomicAdd", "atomicAnd", "atomicOr", "atomicXor", "atomicMin", "atomicMax", "atomicCompSwap", "atomicExchange"}};
extern const HashMap32<std::string, std::string> g_hlsl_function_mapping{
    {"intBitsToFloat", "asfloat"},
    {"uintBitsToFloat", "asfloat"},
    {"floatBitsToInt", "asint"},
    {"floatBitsToUint", "asuint"},
    {"fract", "frac"},
    {"fma", "mad"},
    {"inversesqrt", "rsqrt"},
    {"mix", "lerp"},
    {"barrier", "GroupMemoryBarrierWithGroupSync"},
    {"groupMemoryBarrier", "AllMemoryBarrier"},
    {"atomicAdd", "InterlockedAdd"},
    {"atomicAnd", "InterlockedAnd"},
    {"atomicOr", "InterlockedOr"},
    {"atomicXor", "InterlockedXor"},
    {"atomicMin", "InterlockedMin"},
    {"atomicMax", "InterlockedMax"},
    {"atomicExchange", "InterlockedExchange"},
    {"atomicCompSwap", "InterlockedCompareExchange"},
    {"subgroupAll", "WaveActiveAllTrue"},
    {"subgroupAny", "WaveActiveAnyTrue"},
    {"subgroupAdd", "WaveActiveSum"},
    {"subgroupElect", "WaveIsFirstLane"},
    {"subgroupExclusiveAdd", "WavePrefixSum"},
    {"bitCount", "countbits"},
    {"findLSB", "firstbitlow"},
    {"findMSB", "firstbithigh"},
    {"nonuniformEXT", "NonUniformResourceIndex"}};

static int Align_To(const int value, const int alignment) { return (value + alignment - 1) / alignment * alignment; }

static bool Get_MatrixShape(const ast_type *type, int &cols, int &rows, bool &is_double) {
    if (!type->builtin) {
        return false;
    }
    is_double = false;
    switch (static_cast<const ast_builtin *>(type)->type) {
    case eKeyword::K_mat2:
    case eKeyword::K_mat2x2:
        cols = 2, rows = 2;
        return true;
    case eKeyword::K_mat2x3:
        cols = 2, rows = 3;
        return true;
    case eKeyword::K_mat2x4:
        cols = 2, rows = 4;
        return true;
    case eKeyword::K_mat3x2:
        cols = 3, rows = 2;
        return true;
    case eKeyword::K_mat3:
    case eKeyword::K_mat3x3:
        cols = 3, rows = 3;
        return true;
    case eKeyword::K_mat3x4:
        cols = 3, rows = 4;
        return true;
    case eKeyword::K_mat4x2:
        cols = 4, rows = 2;
        return true;
    case eKeyword::K_mat4x3:
        cols = 4, rows = 3;
        return true;
    case eKeyword::K_mat4:
    case eKeyword::K_mat4x4:
        cols = 4, rows = 4;
        return true;
    case eKeyword::K_dmat2:
    case eKeyword::K_dmat2x2:
        cols = 2, rows = 2, is_double = true;
        return true;
    case eKeyword::K_dmat2x3:
        cols = 2, rows = 3, is_double = true;
        return true;
    case eKeyword::K_dmat2x4:
        cols = 2, rows = 4, is_double = true;
        return true;
    case eKeyword::K_dmat3x2:
        cols = 3, rows = 2, is_double = true;
        return true;
    case eKeyword::K_dmat3:
    case eKeyword::K_dmat3x3:
        cols = 3, rows = 3, is_double = true;
        return true;
    case eKeyword::K_dmat3x4:
        cols = 3, rows = 4, is_double = true;
        return true;
    case eKeyword::K_dmat4x2:
        cols = 4, rows = 2, is_double = true;
        return true;
    case eKeyword::K_dmat4x3:
        cols = 4, rows = 3, is_double = true;
        return true;
    case eKeyword::K_dmat4:
    case eKeyword::K_dmat4x4:
        cols = 4, rows = 4, is_double = true;
        return true;
    default:
        return false;
    }
}

static bool Get_MatrixShape(const ast_type *type, int &cols, int &rows) {
    bool is_double = false;
    return Get_MatrixShape(type, cols, rows, is_double);
}

// std430 column layout: each column is a vector of the scalar type, and the stride is the column
// vector's size rounded up to its alignment.
static int Calc_MatrixColumnStride(const int rows, const bool is_double) {
    return is_double ? (rows == 2 ? 16 : 32) : (rows == 2 ? 8 : 16);
}

static int Calc_MatrixColumnAlignment(const int rows, const bool is_double) {
    return is_double ? 16 : Calc_MatrixColumnStride(rows, false);
}

static eKeyword Get_MatrixColumnKeyword(const int rows, const bool is_double) {
    if (is_double) {
        return rows == 2 ? eKeyword::K_dvec2 : (rows == 3 ? eKeyword::K_dvec3 : eKeyword::K_dvec4);
    }
    return rows == 2 ? eKeyword::K_vec2 : (rows == 3 ? eKeyword::K_vec3 : eKeyword::K_vec4);
}

// dvec, bvec, 64-bit integer and 16-bit vectors have no single-expression load (LoadN + asfloat only
// covers 32-bit-component vectors), so whole ones decompose into scalar component accesses.
static bool Is_DecomposableVector(const eKeyword type) {
    switch (type) {
    case eKeyword::K_dvec2:
    case eKeyword::K_dvec3:
    case eKeyword::K_dvec4:
    case eKeyword::K_bvec2:
    case eKeyword::K_bvec3:
    case eKeyword::K_bvec4:
    case eKeyword::K_i64vec2:
    case eKeyword::K_i64vec3:
    case eKeyword::K_i64vec4:
    case eKeyword::K_u64vec2:
    case eKeyword::K_u64vec3:
    case eKeyword::K_u64vec4:
    case eKeyword::K_i16vec2:
    case eKeyword::K_i16vec3:
    case eKeyword::K_i16vec4:
    case eKeyword::K_u16vec2:
    case eKeyword::K_u16vec3:
    case eKeyword::K_u16vec4:
    case eKeyword::K_f16vec2:
    case eKeyword::K_f16vec3:
    case eKeyword::K_f16vec4:
        return true;
    default:
        return false;
    }
}

static int Calc_ComponentSize(const eKeyword type) {
    switch (type) {
    case eKeyword::K_double:
    case eKeyword::K_dvec2:
    case eKeyword::K_dvec3:
    case eKeyword::K_dvec4:
    case eKeyword::K_int64_t:
    case eKeyword::K_uint64_t:
    case eKeyword::K_i64vec2:
    case eKeyword::K_i64vec3:
    case eKeyword::K_i64vec4:
    case eKeyword::K_u64vec2:
    case eKeyword::K_u64vec3:
    case eKeyword::K_u64vec4:
        return 8;
    case eKeyword::K_float16_t:
    case eKeyword::K_int16_t:
    case eKeyword::K_uint16_t:
    case eKeyword::K_i16vec2:
    case eKeyword::K_i16vec3:
    case eKeyword::K_i16vec4:
    case eKeyword::K_u16vec2:
    case eKeyword::K_u16vec3:
    case eKeyword::K_u16vec4:
    case eKeyword::K_f16vec2:
    case eKeyword::K_f16vec3:
    case eKeyword::K_f16vec4:
        return 2;
    default:
        return 4;
    }
}

static eKeyword Calc_ComponentKeyword(const eKeyword type) {
    switch (type) {
    case eKeyword::K_dvec2:
    case eKeyword::K_dvec3:
    case eKeyword::K_dvec4:
        return eKeyword::K_double;
    case eKeyword::K_i64vec2:
    case eKeyword::K_i64vec3:
    case eKeyword::K_i64vec4:
        return eKeyword::K_int64_t;
    case eKeyword::K_u64vec2:
    case eKeyword::K_u64vec3:
    case eKeyword::K_u64vec4:
        return eKeyword::K_uint64_t;
    case eKeyword::K_i16vec2:
    case eKeyword::K_i16vec3:
    case eKeyword::K_i16vec4:
        return eKeyword::K_int16_t;
    case eKeyword::K_u16vec2:
    case eKeyword::K_u16vec3:
    case eKeyword::K_u16vec4:
        return eKeyword::K_uint16_t;
    case eKeyword::K_f16vec2:
    case eKeyword::K_f16vec3:
    case eKeyword::K_f16vec4:
        return eKeyword::K_float16_t;
    default:
        return eKeyword::K_bool;
    }
}

static int Calc_ArrayCount(const ast_expression *size) {
    if (size->type == eExprType::IntConstant) {
        return static_cast<const ast_int_constant *>(size)->value;
    } else if (size->type == eExprType::UIntConstant) {
        return static_cast<const ast_uint_constant *>(size)->value;
    }
    assert(false);
    return 0;
}

int Calc_TypeAlignment(const ast_type *type) {
    int cols = 0, rows = 0;
    bool is_double = false;
    if (Get_MatrixShape(type, cols, rows, is_double)) {
        return Calc_MatrixColumnAlignment(rows, is_double);
    }
    if (type->builtin) {
        switch (static_cast<const ast_builtin *>(type)->type) {
        case eKeyword::K_float16_t:
        case eKeyword::K_int16_t:
        case eKeyword::K_uint16_t:
            return 2;
        case eKeyword::K_int64_t:
        case eKeyword::K_uint64_t:
        case eKeyword::K_double:
        case eKeyword::K_ivec2:
        case eKeyword::K_uvec2:
        case eKeyword::K_vec2:
            return 8;
        case eKeyword::K_ivec3:
        case eKeyword::K_uvec3:
        case eKeyword::K_vec3:
        case eKeyword::K_ivec4:
        case eKeyword::K_uvec4:
        case eKeyword::K_vec4:
            return 16;
        case eKeyword::K_bvec2:
            return 8;
        case eKeyword::K_bvec3:
        case eKeyword::K_bvec4:
        case eKeyword::K_dvec2:
        case eKeyword::K_dvec3:
        case eKeyword::K_dvec4:
        case eKeyword::K_i64vec2:
        case eKeyword::K_i64vec3:
        case eKeyword::K_i64vec4:
        case eKeyword::K_u64vec2:
        case eKeyword::K_u64vec3:
        case eKeyword::K_u64vec4:
            return 16;
        case eKeyword::K_i16vec2:
        case eKeyword::K_u16vec2:
        case eKeyword::K_f16vec2:
            return 4;
        case eKeyword::K_i16vec3:
        case eKeyword::K_i16vec4:
        case eKeyword::K_u16vec3:
        case eKeyword::K_u16vec4:
        case eKeyword::K_f16vec3:
        case eKeyword::K_f16vec4:
            return 8;
        default:
            return 4;
        }
    }
    int alignment = 1;
    for (const ast_variable *v : static_cast<const ast_struct *>(type)->fields) {
        alignment = std::max(alignment, Calc_TypeAlignment(v->base_type));
    }
    return alignment;
}

int Calc_TypeSize(const ast_type *type) {
    int cols = 0, rows = 0;
    bool is_double = false;
    if (Get_MatrixShape(type, cols, rows, is_double)) {
        return cols * Calc_MatrixColumnStride(rows, is_double);
    }
    if (type->builtin) {
        switch (static_cast<const ast_builtin *>(type)->type) {
        case eKeyword::K_int:
        case eKeyword::K_uint:
        case eKeyword::K_float:
        case eKeyword::K_bool:
            return 4;
        case eKeyword::K_int64_t:
        case eKeyword::K_uint64_t:
        case eKeyword::K_double:
            return 8;
        case eKeyword::K_float16_t:
        case eKeyword::K_int16_t:
        case eKeyword::K_uint16_t:
            return 2;
        case eKeyword::K_ivec2:
        case eKeyword::K_uvec2:
        case eKeyword::K_vec2:
        case eKeyword::K_bvec2:
            return 8;
        case eKeyword::K_ivec3:
        case eKeyword::K_uvec3:
        case eKeyword::K_vec3:
        case eKeyword::K_bvec3:
            return 12;
        case eKeyword::K_ivec4:
        case eKeyword::K_uvec4:
        case eKeyword::K_vec4:
        case eKeyword::K_bvec4:
            return 16;
        case eKeyword::K_dvec2:
        case eKeyword::K_i64vec2:
        case eKeyword::K_u64vec2:
            return 16;
        case eKeyword::K_dvec3:
        case eKeyword::K_i64vec3:
        case eKeyword::K_u64vec3:
            return 24;
        case eKeyword::K_dvec4:
        case eKeyword::K_i64vec4:
        case eKeyword::K_u64vec4:
            return 32;
        case eKeyword::K_i16vec2:
        case eKeyword::K_u16vec2:
        case eKeyword::K_f16vec2:
            return 4;
        case eKeyword::K_i16vec3:
        case eKeyword::K_u16vec3:
        case eKeyword::K_f16vec3:
            return 6;
        case eKeyword::K_i16vec4:
        case eKeyword::K_u16vec4:
        case eKeyword::K_f16vec4:
            return 8;
        default:
            return 0;
        }
    }
    int offset = 0;
    const auto *struct_type = static_cast<const ast_struct *>(type);
    for (const ast_variable *v : struct_type->fields) {
        offset = Align_To(offset, Calc_TypeAlignment(v->base_type)) + Calc_VariableSize(v);
    }
    return Align_To(offset, Calc_TypeAlignment(type));
}

int Calc_ArrayStride(const ast_type *type) { return Align_To(Calc_TypeSize(type), Calc_TypeAlignment(type)); }

int Calc_VariableSize(const ast_variable *v, const int array_dim) {
    if ((v->flags & eVariableFlags::Array) == 0) {
        return Calc_TypeSize(v->base_type);
    }
    if (int(v->array_sizes.size()) > array_dim && v->array_sizes[array_dim]) {
        return Calc_ArrayCount(v->array_sizes[array_dim]) * Calc_VariableSize(v, array_dim + 1);
    }
    return Calc_ArrayStride(v->base_type);
}

int Calc_FieldOffset(const ast_type *type, const char *field_name) {
    if (type->builtin) {
        int stride = Calc_ComponentSize(static_cast<const ast_builtin *>(type)->type);
        int cols = 0, rows = 0;
        bool is_double = false;
        if (Get_MatrixShape(type, cols, rows, is_double)) {
            stride = Calc_MatrixColumnStride(rows, is_double);
        }
        if (field_name[0] == 'x') {
            return 0;
        } else if (field_name[0] == 'y') {
            return stride;
        } else if (field_name[0] == 'z') {
            return stride * 2;
        } else {
            return stride * 3;
        }
    } else {
        const auto *struct_type = static_cast<const ast_struct *>(type);
        int offset = 0;
        for (const ast_variable *v : struct_type->fields) {
            offset = Align_To(offset, Calc_TypeAlignment(v->base_type));
            if (strcmp(v->name, field_name) == 0) {
                return offset;
            }
            offset += Calc_VariableSize(v);
        }
        return -1;
    }
}

global_vector<byteaddress_buf_t> Collect_ByteaddressBuffers(const TrUnit *tu) {
    global_vector<byteaddress_buf_t> ret;
    for (const ast_interface_block *block : tu->interface_blocks) {
        if (block->storage != eStorage::Buffer) {
            continue;
        }
        const char *name = block->name;
        if (block->fields.size() == 1) {
            name = block->fields[0]->name;
        }
        ret.push_back({name});
    }
    return ret;
}

static bool Is_InlineableLeaf(const ast_type *type, const int array_dims) {
    if (array_dims != 0 || !type->builtin) {
        return false;
    }
    switch (static_cast<const ast_builtin *>(type)->type) {
    case eKeyword::K_int:
    case eKeyword::K_uint:
    case eKeyword::K_bool:
    case eKeyword::K_int64_t:
    case eKeyword::K_uint64_t:
    case eKeyword::K_float:
    case eKeyword::K_float16_t:
    case eKeyword::K_int16_t:
    case eKeyword::K_uint16_t:
    case eKeyword::K_ivec2:
    case eKeyword::K_uvec2:
    case eKeyword::K_vec2:
    case eKeyword::K_ivec3:
    case eKeyword::K_uvec3:
    case eKeyword::K_vec3:
    case eKeyword::K_ivec4:
    case eKeyword::K_uvec4:
    case eKeyword::K_vec4:
    case eKeyword::K_double:
        return true;
    default:
        return false;
    }
}

static bool Is_AggregateType(const ast_type *type, const int array_dims) {
    if (array_dims != 0) {
        return false;
    }
    if (!type->builtin) {
        return true;
    }
    if (Is_DecomposableVector(static_cast<const ast_builtin *>(type)->type)) {
        return true;
    }
    int cols = 0, rows = 0;
    return Get_MatrixShape(type, cols, rows);
}

// Types carry no array sizes, so the sizes of a subscript run's deeper dims come from the
// array variable the chain bottoms out at (identifier or resolved array field).
static const ast_variable *Resolve_ArrayBaseVariable(const ast_expression *expression) {
    switch (expression->type) {
    case eExprType::VariableIdentifier:
        return static_cast<const ast_variable_identifier *>(expression)->variable;
    case eExprType::ArraySubscript:
        return Resolve_ArrayBaseVariable(static_cast<const ast_array_subscript *>(expression)->operand);
    case eExprType::FieldOrSwizzle: {
        const auto *field = static_cast<const ast_field_or_swizzle *>(expression);
        if (field->field && (field->field->variable->flags & eVariableFlags::Array) != 0) {
            return field->field->variable;
        }
        return Resolve_ArrayBaseVariable(field->operand);
    }
    default:
        return nullptr;
    }
}
} // namespace glslx

glslx::LowerHLSL::buf_access_t
glslx::LowerHLSL::Find_BufferAccessExpression(const ast_expression *expression, const int offset,
                                              global_vector<access_index_t> &out_indices) {
    if (expression->type == eExprType::VariableIdentifier) {
        const auto *var = static_cast<const ast_variable_identifier *>(expression);
        auto it = std::find_if(bufs_.begin(), bufs_.end(), [var](const byteaddress_buf_t &buf) {
            return strcmp(var->variable->name, buf.name) == 0;
        });
        if (it == bufs_.end()) {
            return {};
        }
        return {int(std::distance(bufs_.begin(), it)), offset, var->variable};
    } else if (expression->type == eExprType::ArraySubscript) {
        const auto *subscript = static_cast<const ast_array_subscript *>(expression);
        int array_dims = 0;
        const ast_type *operand_type = Evaluate_ExpressionResultType(tu_, subscript->operand, array_dims);
        if (!operand_type) {
            // Operand type could not be resolved (e.g. a ternary/method-call/unmatched function
            // call), so this cannot be a buffer-access chain.
            return {};
        }
        int operand_size = Calc_ArrayStride(operand_type);
        if (array_dims == 0 && operand_type->builtin) {
            // Subscripting a matrix selects a column, subscripting a vector selects a component.
            int cols = 0, rows = 0;
            bool is_double = false;
            if (Get_MatrixShape(operand_type, cols, rows, is_double)) {
                operand_size = Calc_MatrixColumnStride(rows, is_double);
            } else {
                operand_size = Calc_ComponentSize(static_cast<const ast_builtin *>(operand_type)->type);
            }
        } else if (array_dims > 1) {
            // The indexed element still carries the deeper dims of a multi-dim array.
            const ast_variable *base = Resolve_ArrayBaseVariable(subscript->operand);
            assert(base && int(base->array_sizes.size()) >= array_dims);
            const int dim = int(base->array_sizes.size()) - array_dims;
            for (int k = dim + 1; k < int(base->array_sizes.size()); ++k) {
                assert(base->array_sizes[k] && "only the leftmost array dimension may be unsized");
                operand_size *= Calc_ArrayCount(base->array_sizes[k]);
            }
        }
        out_indices.push_back({subscript->index, operand_size});
        return Find_BufferAccessExpression(subscript->operand, offset, out_indices);
    } else if (expression->type == eExprType::FieldOrSwizzle) {
        const auto *field = static_cast<const ast_field_or_swizzle *>(expression);
        int array_dims = 0;
        const ast_type *operand_type = Evaluate_ExpressionResultType(tu_, field->operand, array_dims);
        if (!operand_type) {
            return {};
        }
        const int field_offset = Calc_FieldOffset(operand_type, field->name);
        return Find_BufferAccessExpression(field->operand, offset + field_offset, out_indices);
    }
    return {};
}

glslx::ast_expression *glslx::LowerHLSL::Build_OffsetSum(const global_vector<access_index_t> &indices,
                                                         const int final_offset) {
    ast_expression *offset_sum = nullptr;
    for (const access_index_t &idx : indices) {
        auto *term = tu_->make<ast_operation_expression>(eOperator::multiply);
        term->operand1 = idx.index;
        term->operand2 = tu_->make<ast_int_constant>(idx.multiplier);
        if (!offset_sum) {
            offset_sum = term;
        } else {
            auto *sum = tu_->make<ast_operation_expression>(eOperator::plus);
            sum->operand1 = offset_sum;
            sum->operand2 = term;
            offset_sum = sum;
        }
    }
    if (!offset_sum) {
        return tu_->make<ast_int_constant>(final_offset);
    }
    auto *sum = tu_->make<ast_operation_expression>(eOperator::plus);
    sum->operand1 = offset_sum;
    sum->operand2 = tu_->make<ast_int_constant>(final_offset);
    return sum;
}

glslx::ast_expression *glslx::LowerHLSL::Build_Load(ast_variable *buf_var, ast_expression *offset_expr,
                                                    const ast_type *leaf_type) {
    const auto *type = static_cast<const ast_builtin *>(leaf_type);

    const char *method_name = nullptr;
    bool has_template_arg = false;
    eKeyword template_arg_kw = eKeyword::K_int;
    bool wrap_asfloat = false;
    bool compare_nonzero = false;

    switch (type->type) {
    case eKeyword::K_int:
    case eKeyword::K_uint:
        method_name = "Load";
        break;
    case eKeyword::K_bool:
        method_name = "Load";
        compare_nonzero = true;
        break;
    case eKeyword::K_int64_t:
        method_name = "Load";
        has_template_arg = true;
        template_arg_kw = eKeyword::K_int64_t;
        break;
    case eKeyword::K_uint64_t:
        method_name = "Load";
        has_template_arg = true;
        template_arg_kw = eKeyword::K_uint64_t;
        break;
    case eKeyword::K_double:
        method_name = "Load";
        has_template_arg = true;
        template_arg_kw = eKeyword::K_double;
        break;
    case eKeyword::K_float:
        method_name = "Load";
        wrap_asfloat = true;
        break;
    case eKeyword::K_float16_t:
        method_name = "Load";
        has_template_arg = true;
        template_arg_kw = eKeyword::K_float16_t;
        break;
    case eKeyword::K_int16_t:
        method_name = "Load";
        has_template_arg = true;
        template_arg_kw = eKeyword::K_int16_t;
        break;
    case eKeyword::K_uint16_t:
        method_name = "Load";
        has_template_arg = true;
        template_arg_kw = eKeyword::K_uint16_t;
        break;
    case eKeyword::K_ivec2:
    case eKeyword::K_uvec2:
        method_name = "Load2";
        break;
    case eKeyword::K_vec2:
        method_name = "Load2";
        wrap_asfloat = true;
        break;
    case eKeyword::K_ivec3:
    case eKeyword::K_uvec3:
        method_name = "Load3";
        break;
    case eKeyword::K_vec3:
        method_name = "Load3";
        wrap_asfloat = true;
        break;
    case eKeyword::K_ivec4:
    case eKeyword::K_uvec4:
        method_name = "Load4";
        break;
    case eKeyword::K_vec4:
        method_name = "Load4";
        wrap_asfloat = true;
        break;
    default:
        assert(false);
        return nullptr;
    }

    auto *call = tu_->make<ast_method_call>(tu_->alloc.allocator);
    call->object = tu_->make<ast_variable_identifier>(buf_var);
    call->name = tu_->makestr(method_name);
    if (has_template_arg) {
        call->template_arg = tu_->FindOrAddBuiltin(template_arg_kw);
    }
    call->parameters.push_back(offset_expr);

    if (wrap_asfloat) {
        auto *asfloat_call = tu_->make<ast_function_call>(tu_->alloc.allocator);
        asfloat_call->name = tu_->makestr("asfloat");
        asfloat_call->parameters.push_back(call);
        return asfloat_call;
    }
    if (compare_nonzero) {
        auto *not_equal = tu_->make<ast_operation_expression>(eOperator::not_equal);
        not_equal->operand1 = call;
        not_equal->operand2 = tu_->make<ast_int_constant>(0);
        return not_equal;
    }
    return call;
}

int glslx::LowerHLSL::Build_Stores(ast_variable *buf_var, const int offset, ast_expression *value, int array_dim,
                                   const ast_type *t, ast_variable *offset_var,
                                   global_vector<ast_statement *> &out_statements) {
    if (t->builtin) {
        const auto *type = static_cast<const ast_builtin *>(t);

        int cols = 0, rows = 0;
        bool is_double = false;
        if (Get_MatrixShape(t, cols, rows, is_double)) {
            const ast_type *column_type = tu_->FindOrAddBuiltin(Get_MatrixColumnKeyword(rows, is_double));
            const int stride = Calc_MatrixColumnStride(rows, is_double);
            for (int i = 0; i < cols; ++i) {
                auto *column = tu_->make<ast_array_subscript>();
                column->operand = value;
                column->index = tu_->make<ast_int_constant>(i);
                Build_Stores(buf_var, offset + stride * i, column, array_dim, column_type, offset_var, out_statements);
            }
            return cols * stride;
        }

        if (Is_DecomposableVector(type->type)) {
            const ast_type *component_type = tu_->FindOrAddBuiltin(Calc_ComponentKeyword(type->type));
            const int stride = Calc_ComponentSize(type->type);
            for (int i = 0; i < get_vector_size(type->type); ++i) {
                auto *component = tu_->make<ast_array_subscript>();
                component->operand = value;
                component->index = tu_->make<ast_int_constant>(i);
                Build_Stores(buf_var, offset + stride * i, component, array_dim, component_type, offset_var,
                             out_statements);
            }
            return Calc_TypeSize(t);
        }

        const char *method_name = nullptr;
        bool has_template_arg = false;
        eKeyword template_arg_kw = eKeyword::K_int;
        bool wrap_asuint = false;
        bool wrap_uint_cast = false;
        int size = 0;

        switch (type->type) {
        case eKeyword::K_int:
        case eKeyword::K_uint:
            method_name = "Store";
            size = 4;
            break;
        case eKeyword::K_ivec2:
        case eKeyword::K_uvec2:
            method_name = "Store2";
            size = 8;
            break;
        case eKeyword::K_ivec3:
        case eKeyword::K_uvec3:
            method_name = "Store3";
            size = 12;
            break;
        case eKeyword::K_ivec4:
        case eKeyword::K_uvec4:
            method_name = "Store4";
            size = 16;
            break;
        case eKeyword::K_vec3:
            method_name = "Store3";
            wrap_asuint = true;
            size = 12;
            break;
        case eKeyword::K_vec2:
            method_name = "Store2";
            wrap_asuint = true;
            size = 8;
            break;
        case eKeyword::K_vec4:
            method_name = "Store4";
            wrap_asuint = true;
            size = 16;
            break;
        case eKeyword::K_int64_t:
            method_name = "Store";
            has_template_arg = true;
            template_arg_kw = eKeyword::K_int64_t;
            size = 8;
            break;
        case eKeyword::K_uint64_t:
            method_name = "Store";
            has_template_arg = true;
            template_arg_kw = eKeyword::K_uint64_t;
            size = 8;
            break;
        case eKeyword::K_float:
            method_name = "Store";
            wrap_asuint = true;
            size = 4;
            break;
        case eKeyword::K_double:
            method_name = "Store";
            has_template_arg = true;
            template_arg_kw = eKeyword::K_double;
            size = 8;
            break;
        case eKeyword::K_bool:
            method_name = "Store";
            wrap_uint_cast = true;
            size = 4;
            break;
        case eKeyword::K_float16_t:
            method_name = "Store";
            has_template_arg = true;
            template_arg_kw = eKeyword::K_float16_t;
            size = 2;
            break;
        case eKeyword::K_int16_t:
            method_name = "Store";
            has_template_arg = true;
            template_arg_kw = eKeyword::K_int16_t;
            size = 2;
            break;
        case eKeyword::K_uint16_t:
            method_name = "Store";
            has_template_arg = true;
            template_arg_kw = eKeyword::K_uint16_t;
            size = 2;
            break;
        default:
            return -1;
        }

        auto *call = tu_->make<ast_method_call>(tu_->alloc.allocator);
        call->object = tu_->make<ast_variable_identifier>(buf_var);
        call->name = tu_->makestr(method_name);
        if (has_template_arg) {
            call->template_arg = tu_->FindOrAddBuiltin(template_arg_kw);
        }

        auto *store_offset = tu_->make<ast_operation_expression>(eOperator::plus);
        store_offset->operand1 = tu_->make<ast_variable_identifier>(offset_var);
        store_offset->operand2 = tu_->make<ast_int_constant>(offset);
        call->parameters.push_back(store_offset);

        if (wrap_asuint || wrap_uint_cast) {
            auto *wrap_call = tu_->make<ast_function_call>(tu_->alloc.allocator);
            wrap_call->name = tu_->makestr(wrap_asuint ? "asuint" : "uint");
            wrap_call->parameters.push_back(value);
            call->parameters.push_back(wrap_call);
        } else {
            call->parameters.push_back(value);
        }

        out_statements.push_back(tu_->make<ast_expression_statement>(call));
        return size;
    } else {
        int size = 0;
        const auto *type = static_cast<const ast_struct *>(t);
        for (const ast_variable *sub_v : type->fields) {
            auto *field_access = tu_->make<ast_field_or_swizzle>();
            field_access->operand = value;
            field_access->name = sub_v->name;
            size = Align_To(size, Calc_TypeAlignment(sub_v->base_type));
            size += Build_Stores(buf_var, offset + size, field_access, array_dim, sub_v, offset_var, out_statements);
        }
        return Align_To(size, Calc_TypeAlignment(type));
    }
}

int glslx::LowerHLSL::Build_Stores(ast_variable *buf_var, const int offset, ast_expression *value, int array_dim,
                                   const ast_variable *v, ast_variable *offset_var,
                                   global_vector<ast_statement *> &out_statements) {
    if ((v->flags & eVariableFlags::Array) && int(v->array_sizes.size()) > array_dim && v->array_sizes[array_dim]) {
        const int count = Calc_ArrayCount(v->array_sizes[array_dim]);
        const int stride = Calc_VariableSize(v, array_dim + 1);
        for (int i = 0; i < count; ++i) {
            auto *elem_access = tu_->make<ast_array_subscript>();
            elem_access->operand = value;
            elem_access->index = tu_->make<ast_int_constant>(i);
            Build_Stores(buf_var, offset + i * stride, elem_access, array_dim + 1, v, offset_var, out_statements);
        }
        return count * stride;
    }

    return Build_Stores(buf_var, offset, value, array_dim, v->base_type, offset_var, out_statements);
}

int glslx::LowerHLSL::Build_Loads(ast_variable *buf_var, const int offset, ast_expression *target, const int array_dim,
                                  const ast_type *t, ast_variable *offset_var,
                                  global_vector<ast_statement *> &out_statements) {
    auto make_load_assign = [&](ast_expression *lhs, const int load_offset, const ast_type *leaf_type) {
        auto *load_offset_expr = tu_->make<ast_operation_expression>(eOperator::plus);
        load_offset_expr->operand1 = tu_->make<ast_variable_identifier>(offset_var);
        load_offset_expr->operand2 = tu_->make<ast_int_constant>(load_offset);

        auto *assign = tu_->make<ast_assignment_expression>(eOperator::assign);
        assign->operand1 = lhs;
        assign->operand2 = Build_Load(buf_var, load_offset_expr, leaf_type);
        out_statements.push_back(tu_->make<ast_expression_statement>(assign));
    };

    if (!t->builtin) {
        int size = 0;
        const auto *type = static_cast<const ast_struct *>(t);
        for (const ast_variable *sub_v : type->fields) {
            auto *field_access = tu_->make<ast_field_or_swizzle>();
            field_access->operand = target;
            field_access->name = sub_v->name;
            size = Align_To(size, Calc_TypeAlignment(sub_v->base_type));
            size += Build_Loads(buf_var, offset + size, field_access, array_dim, sub_v, offset_var, out_statements);
        }
        return Align_To(size, Calc_TypeAlignment(type));
    }

    int cols = 0, rows = 0;
    bool is_double = false;
    if (Get_MatrixShape(t, cols, rows, is_double)) {
        const ast_type *column_type = tu_->FindOrAddBuiltin(Get_MatrixColumnKeyword(rows, is_double));
        const int stride = Calc_MatrixColumnStride(rows, is_double);
        for (int i = 0; i < cols; ++i) {
            auto *column = tu_->make<ast_array_subscript>();
            column->operand = target;
            column->index = tu_->make<ast_int_constant>(i);
            // Recurse (not Build_Load): double columns are decomposable vectors, not loadable leaves.
            Build_Loads(buf_var, offset + stride * i, column, 0, column_type, offset_var, out_statements);
        }
        return cols * stride;
    }

    const auto *leaf_kw = static_cast<const ast_builtin *>(t);
    if (Is_DecomposableVector(leaf_kw->type)) {
        const ast_type *component_type = tu_->FindOrAddBuiltin(Calc_ComponentKeyword(leaf_kw->type));
        const int stride = Calc_ComponentSize(leaf_kw->type);
        for (int i = 0; i < get_vector_size(leaf_kw->type); ++i) {
            auto *component = tu_->make<ast_array_subscript>();
            component->operand = target;
            component->index = tu_->make<ast_int_constant>(i);
            make_load_assign(component, offset + stride * i, component_type);
        }
        return Calc_TypeSize(t);
    }

    assert(Is_InlineableLeaf(t, 0) && "unsupported buffer member type");
    make_load_assign(target, offset, t);
    return Calc_TypeSize(t);
}

int glslx::LowerHLSL::Build_Loads(ast_variable *buf_var, const int offset, ast_expression *target, const int array_dim,
                                  const ast_variable *v, ast_variable *offset_var,
                                  global_vector<ast_statement *> &out_statements) {
    if ((v->flags & eVariableFlags::Array) && int(v->array_sizes.size()) > array_dim && v->array_sizes[array_dim]) {
        const int count = Calc_ArrayCount(v->array_sizes[array_dim]);
        const int stride = Calc_VariableSize(v, array_dim + 1);
        for (int i = 0; i < count; ++i) {
            auto *elem_access = tu_->make<ast_array_subscript>();
            elem_access->operand = target;
            elem_access->index = tu_->make<ast_int_constant>(i);
            Build_Loads(buf_var, offset + i * stride, elem_access, array_dim + 1, v, offset_var, out_statements);
        }
        return count * stride;
    }

    return Build_Loads(buf_var, offset, target, array_dim, v->base_type, offset_var, out_statements);
}

glslx::ast_expression *glslx::LowerHLSL::Hoist_BufferRead(const buf_access_t &access,
                                                          global_vector<access_index_t> &indices,
                                                          const ast_type *type) {
    for (access_index_t &idx : indices) {
        idx.index = Rewrite_Expression(idx.index);
    }
    const int index = temp_index_++;

    auto *offset_var = tu_->make<ast_function_variable>(tu_->alloc.allocator);
    offset_var->base_type = tu_->FindOrAddBuiltin(eKeyword::K_uint);
    offset_var->name = tu_->makestr(("__offset" + std::to_string(index)).c_str());
    offset_var->initial_value = Build_OffsetSum(indices, access.offset);

    auto *offset_decl = tu_->make<ast_declaration_statement>(tu_->alloc.allocator);
    offset_decl->variables.push_back(offset_var);
    hoist_->push_back(offset_decl);

    auto *load_var = tu_->make<ast_function_variable>(tu_->alloc.allocator);
    load_var->base_type = const_cast<ast_type *>(type);
    load_var->name = tu_->makestr(("__load" + std::to_string(index)).c_str());

    auto *load_decl = tu_->make<ast_declaration_statement>(tu_->alloc.allocator);
    load_decl->variables.push_back(load_var);
    hoist_->push_back(load_decl);

    Build_Loads(access.var, 0, tu_->make<ast_variable_identifier>(load_var), 0, type, offset_var, *hoist_);
    return tu_->make<ast_variable_identifier>(load_var);
}

glslx::ast_expression *glslx::LowerHLSL::Rewrite_Hoisting(ast_expression *expr, global_vector<ast_statement *> &pre) {
    hoist_ = &pre;
    expr = Rewrite_Expression(expr);
    hoist_ = nullptr;
    return Lower_Atomics(expr, pre);
}

bool glslx::LowerHLSL::Try_Lower_Store(ast_expression_statement *statement,
                                       global_vector<ast_statement *> &out_statements) {
    if (!statement->expression || statement->expression->type != eExprType::Assign) {
        return false;
    }
    auto *assign = static_cast<ast_assignment_expression *>(statement->expression);

    global_vector<access_index_t> indices;
    const buf_access_t access = Find_BufferAccessExpression(assign->operand1, 0, indices);
    if (access.buf_index == -1) {
        return false;
    }

    int array_dims = 0;
    auto *var_type = const_cast<ast_type *>(Evaluate_ExpressionResultType(tu_, assign->operand2, array_dims));
    if (!var_type) {
        assert(false && "cannot type the value stored into a ByteAddressBuffer member");
        return false;
    }

    const int index = temp_index_++;

    // Rewrite nested buffer reads on the LHS subscript indices and on the store RHS now, while
    // the result type above is still known for the untouched operand2.
    for (access_index_t &idx : indices) {
        idx.index = Rewrite_Expression(idx.index);
    }
    assign->operand2 = Rewrite_Expression(assign->operand2);
    for (access_index_t &idx : indices) {
        idx.index = Lower_Atomics(idx.index, out_statements);
    }
    assign->operand2 = Lower_Atomics(assign->operand2, out_statements);

    auto *temp_var = tu_->make<ast_function_variable>(tu_->alloc.allocator);
    temp_var->base_type = var_type;
    temp_var->name = tu_->makestr(("__temp" + std::to_string(index)).c_str());
    temp_var->initial_value = assign->operand2;

    auto *temp_decl = tu_->make<ast_declaration_statement>(tu_->alloc.allocator);
    temp_decl->variables.push_back(temp_var);
    out_statements.push_back(temp_decl);

    auto *offset_var = tu_->make<ast_function_variable>(tu_->alloc.allocator);
    offset_var->base_type = tu_->FindOrAddBuiltin(eKeyword::K_uint);
    offset_var->name = tu_->makestr(("__offset" + std::to_string(index)).c_str());
    offset_var->initial_value = Build_OffsetSum(indices, access.offset);

    auto *offset_decl = tu_->make<ast_declaration_statement>(tu_->alloc.allocator);
    offset_decl->variables.push_back(offset_var);
    out_statements.push_back(offset_decl);

    ast_expression *temp_ident = tu_->make<ast_variable_identifier>(temp_var);
    Build_Stores(access.var, 0, temp_ident, 0, var_type, offset_var, out_statements);

    return true;
}

// rayQueryInitializeEXT(rayQuery, topLevel, rayFlags, cullMask, origin, tMin, direction, tMax) is
// void and has no HLSL equivalent as a single call -- expand it to the two statements it has always
// stood for:
//   RayDesc __raydescN = { origin, tMin, direction, tMax };
//   rayQuery.TraceRayInline(topLevel, rayFlags, cullMask, __raydescN);
bool glslx::LowerHLSL::Try_Lower_RayQueryInit(ast_expression_statement *statement,
                                              global_vector<ast_statement *> &out_statements) {
    if (!statement->expression || statement->expression->type != eExprType::FunctionCall) {
        return false;
    }
    auto *call = static_cast<ast_function_call *>(statement->expression);
    if (strcmp(call->name, "rayQueryInitializeEXT") != 0 || call->parameters.size() != 8) {
        return false;
    }

    for (int i = 0; i < int(call->parameters.size()); ++i) {
        call->parameters[i] = Lower_Atomics(Rewrite_Expression(call->parameters[i]), out_statements);
    }

    const int index = temp_index_++;

    // Synthetic "RayDesc" struct, never registered in tu->structures -- Write_Type's non-builtin
    // path prints just the name, so no `struct RayDesc {...};` is ever emitted.
    auto *raydesc_type = tu_->make<ast_struct>(tu_->alloc.allocator);
    raydesc_type->name = tu_->makestr("RayDesc");

    auto *raydesc_var = tu_->make<ast_function_variable>(tu_->alloc.allocator);
    raydesc_var->base_type = raydesc_type;
    raydesc_var->name = tu_->makestr(("__raydesc" + std::to_string(index)).c_str());

    auto *init = tu_->make<ast_array_specifier>(tu_->alloc.allocator);
    init->expressions.push_back(call->parameters[4]); // origin
    init->expressions.push_back(call->parameters[5]); // tMin
    init->expressions.push_back(call->parameters[6]); // direction
    init->expressions.push_back(call->parameters[7]); // tMax
    raydesc_var->initial_value = init;

    auto *raydesc_decl = tu_->make<ast_declaration_statement>(tu_->alloc.allocator);
    raydesc_decl->variables.push_back(raydesc_var);
    out_statements.push_back(raydesc_decl);

    auto *trace_call = tu_->make<ast_method_call>(tu_->alloc.allocator);
    trace_call->object = call->parameters[0]; // rayQuery
    trace_call->name = tu_->makestr("TraceRayInline");
    trace_call->parameters.push_back(call->parameters[1]); // topLevel
    trace_call->parameters.push_back(call->parameters[2]); // rayFlags
    trace_call->parameters.push_back(call->parameters[3]); // cullMask
    trace_call->parameters.push_back(tu_->make<ast_variable_identifier>(raydesc_var));
    out_statements.push_back(tu_->make<ast_expression_statement>(trace_call));

    return true;
}

int glslx::LowerHLSL::Image_ComponentCount(const ast_variable *v) {
    if (v->type != eVariableType::Global) {
        return 4;
    }
    const auto *gv = static_cast<const ast_global_variable *>(v);
    for (const ast_layout_qualifier *q : gv->layout_qualifiers) {
        if (q->initial_value) {
            continue;
        }
        if (strcmp(q->name, "rgba32f") == 0 || strcmp(q->name, "rgba16f") == 0 || strcmp(q->name, "rgba16") == 0 ||
            strcmp(q->name, "rgb10_a2") == 0 || strcmp(q->name, "rgba8") == 0 || strcmp(q->name, "rgba16_snorm") == 0 ||
            strcmp(q->name, "rgba8_snorm") == 0 || strcmp(q->name, "rgba32i") == 0 || strcmp(q->name, "rgba16i") == 0 ||
            strcmp(q->name, "rgba8i") == 0 || strcmp(q->name, "rgba32ui") == 0 || strcmp(q->name, "rgba16ui") == 0 ||
            strcmp(q->name, "rgb10_a2ui") == 0 || strcmp(q->name, "rgba8ui") == 0) {
            return 4;
        }
        if (strcmp(q->name, "r11f_g11f_b10f") == 0) {
            return 3;
        }
        if (strcmp(q->name, "rg32f") == 0 || strcmp(q->name, "rg16f") == 0 || strcmp(q->name, "rg16") == 0 ||
            strcmp(q->name, "rg8") == 0 || strcmp(q->name, "rg16_snorm") == 0 || strcmp(q->name, "rg8_snorm") == 0 ||
            strcmp(q->name, "rg32i") == 0 || strcmp(q->name, "rg16i") == 0 || strcmp(q->name, "rg8i") == 0 ||
            strcmp(q->name, "rg32ui") == 0 || strcmp(q->name, "rg16ui") == 0 || strcmp(q->name, "rg8ui") == 0) {
            return 2;
        }
        if (strcmp(q->name, "r32f") == 0 || strcmp(q->name, "r16f") == 0 || strcmp(q->name, "r16") == 0 ||
            strcmp(q->name, "r8") == 0 || strcmp(q->name, "r16_snorm") == 0 || strcmp(q->name, "r8_snorm") == 0 ||
            strcmp(q->name, "r32i") == 0 || strcmp(q->name, "r16i") == 0 || strcmp(q->name, "r8i") == 0 ||
            strcmp(q->name, "r32ui") == 0 || strcmp(q->name, "r16ui") == 0 || strcmp(q->name, "r8ui") == 0) {
            return 1;
        }
    }
    return 4;
}

namespace glslx {
// Finds the sampler/image variable a (possibly array-subscripted) texture/image argument refers to.
const ast_variable *Find_TextureImageVariable(const ast_expression *expr) {
    if (expr->type == eExprType::VariableIdentifier) {
        return static_cast<const ast_variable_identifier *>(expr)->variable;
    }
    if (expr->type == eExprType::ArraySubscript) {
        return Find_TextureImageVariable(static_cast<const ast_array_subscript *>(expr)->operand);
    }
    return nullptr;
}

// True for the shadow-sampler keywords Rewrite_SampleCall handles.
bool is_shadow_sampler_kw(const eKeyword kw) {
    return kw == eKeyword::K_sampler1DShadow || kw == eKeyword::K_sampler1DArrayShadow ||
           kw == eKeyword::K_sampler2DShadow || kw == eKeyword::K_sampler2DArrayShadow ||
           kw == eKeyword::K_samplerCubeShadow || kw == eKeyword::K_samplerCubeArrayShadow;
}

eKeyword Companion_SamplerKeyword(const ast_type *tex_type) {
    const bool shadow = tex_type->builtin && is_shadow_sampler_kw(static_cast<const ast_builtin *>(tex_type)->type);
    return shadow ? eKeyword::K_samplerShadow : eKeyword::K_sampler;
}

// Builds a reference to the synthetic "<tex>_sampler" SamplerState companion that WriterHLSL emits
// next to every combined-texture-sampler global (see Write_GlobalVariable). The returned variable is
// tree-detached (not part of any declaration list) -- only its name is ever printed.
ast_expression *Build_SamplerRef(TrUnit *tu, ast_expression *tex_expr) {
    const ast_variable *tex_var = nullptr;
    ast_expression *index_expr = nullptr;
    if (tex_expr->type == eExprType::VariableIdentifier) {
        tex_var = static_cast<const ast_variable_identifier *>(tex_expr)->variable;
    } else if (tex_expr->type == eExprType::ArraySubscript) {
        auto *subscript = static_cast<ast_array_subscript *>(tex_expr);
        if (subscript->operand->type != eExprType::VariableIdentifier) {
            return nullptr;
        }
        tex_var = static_cast<const ast_variable_identifier *>(subscript->operand)->variable;
        index_expr = subscript->index;
    } else {
        return nullptr;
    }

    auto *sampler_var = tu->make<ast_function_variable>(tu->alloc.allocator);
    sampler_var->base_type = tu->FindOrAddBuiltin(Companion_SamplerKeyword(tex_var->base_type));
    sampler_var->name = tu->makestr((std::string(tex_var->name) + "_sampler").c_str());

    ast_expression *sampler_ident = tu->make<ast_variable_identifier>(sampler_var);
    if (index_expr) {
        auto *subscript = tu->make<ast_array_subscript>();
        subscript->operand = sampler_ident;
        subscript->index = index_expr;
        return subscript;
    }
    return sampler_ident;
}

// True for 1D/2D/3D/Cube/array samplers (incl. int/uint and shadow); Rect/MS/Buffer fall through to an assert.
bool is_combined_sampler_keyword(const eKeyword kw) {
    switch (kw) {
    case eKeyword::K_sampler1D:
    case eKeyword::K_isampler1D:
    case eKeyword::K_usampler1D:
    case eKeyword::K_sampler1DArray:
    case eKeyword::K_isampler1DArray:
    case eKeyword::K_usampler1DArray:
    case eKeyword::K_sampler2D:
    case eKeyword::K_isampler2D:
    case eKeyword::K_usampler2D:
    case eKeyword::K_sampler2DArray:
    case eKeyword::K_isampler2DArray:
    case eKeyword::K_usampler2DArray:
    case eKeyword::K_sampler3D:
    case eKeyword::K_isampler3D:
    case eKeyword::K_usampler3D:
    case eKeyword::K_samplerCube:
    case eKeyword::K_isamplerCube:
    case eKeyword::K_usamplerCube:
    case eKeyword::K_samplerCubeArray:
    case eKeyword::K_isamplerCubeArray:
    case eKeyword::K_usamplerCubeArray:
    case eKeyword::K_sampler1DShadow:
    case eKeyword::K_sampler1DArrayShadow:
    case eKeyword::K_sampler2DShadow:
    case eKeyword::K_sampler2DArrayShadow:
    case eKeyword::K_samplerCubeShadow:
    case eKeyword::K_samplerCubeArrayShadow:
        return true;
    default:
        return false;
    }
}

// GetDimensions shape for a sampler kind: the number of size components textureSize returns (its HLSL
// out-parameter run is `width [, height [, depthOrArraySize]]` followed by the mip-level count), and
// 0 for kinds this pass does not support. `out_mip_level` is false for the MS kinds, whose HLSL overload
// takes no lod and ends with the sample count instead.
int GetDimensions_Components(const eKeyword kw, bool &out_mip_level) {
    out_mip_level = true;
    switch (kw) {
    case eKeyword::K_sampler1D:
    case eKeyword::K_isampler1D:
    case eKeyword::K_usampler1D:
    case eKeyword::K_texture1D:
    case eKeyword::K_itexture1D:
    case eKeyword::K_utexture1D:
    case eKeyword::K_sampler1DShadow:
        return 1;
    case eKeyword::K_sampler1DArray:
    case eKeyword::K_isampler1DArray:
    case eKeyword::K_usampler1DArray:
    case eKeyword::K_sampler1DArrayShadow:
    case eKeyword::K_sampler2D:
    case eKeyword::K_isampler2D:
    case eKeyword::K_usampler2D:
    case eKeyword::K_texture2D:
    case eKeyword::K_itexture2D:
    case eKeyword::K_utexture2D:
    case eKeyword::K_sampler2DShadow:
    case eKeyword::K_samplerCube:
    case eKeyword::K_isamplerCube:
    case eKeyword::K_usamplerCube:
    case eKeyword::K_textureCube:
    case eKeyword::K_itextureCube:
    case eKeyword::K_utextureCube:
    case eKeyword::K_samplerCubeShadow:
        return 2;
    case eKeyword::K_sampler2DArray:
    case eKeyword::K_isampler2DArray:
    case eKeyword::K_usampler2DArray:
    case eKeyword::K_texture2DArray:
    case eKeyword::K_itexture2DArray:
    case eKeyword::K_utexture2DArray:
    case eKeyword::K_sampler2DArrayShadow:
    case eKeyword::K_sampler3D:
    case eKeyword::K_isampler3D:
    case eKeyword::K_usampler3D:
    case eKeyword::K_texture3D:
    case eKeyword::K_itexture3D:
    case eKeyword::K_utexture3D:
    case eKeyword::K_samplerCubeArray:
    case eKeyword::K_isamplerCubeArray:
    case eKeyword::K_usamplerCubeArray:
    case eKeyword::K_textureCubeArray:
    case eKeyword::K_itextureCubeArray:
    case eKeyword::K_utextureCubeArray:
    case eKeyword::K_samplerCubeArrayShadow:
        return 3;
    case eKeyword::K_sampler2DMS:
    case eKeyword::K_isampler2DMS:
    case eKeyword::K_usampler2DMS:
    case eKeyword::K_texture2DMS:
    case eKeyword::K_itexture2DMS:
    case eKeyword::K_utexture2DMS:
        out_mip_level = false;
        return 2;
    case eKeyword::K_sampler2DMSArray:
    case eKeyword::K_isampler2DMSArray:
    case eKeyword::K_usampler2DMSArray:
    case eKeyword::K_texture2DMSArray:
    case eKeyword::K_itexture2DMSArray:
    case eKeyword::K_utexture2DMSArray:
        out_mip_level = false;
        return 3;
    default:
        return 0;
    }
}

// uint / int vector keyword for `components` components, with the scalar itself for 1 (which is what
// GetDimensions hands out for 1D textures).
eKeyword Dims_VectorKeyword(const bool is_unsigned, const int components) {
    static const eKeyword k_uvecs[] = {eKeyword::K_uint, eKeyword::K_uvec2, eKeyword::K_uvec3};
    static const eKeyword k_ivecs[] = {eKeyword::K_int, eKeyword::K_ivec2, eKeyword::K_ivec3};
    assert(components >= 1 && components <= 3);
    return (is_unsigned ? k_uvecs : k_ivecs)[components - 1];
}

// Resolved first argument of a texelFetch/textureLod/textureLodOffset call: the texture object to
// call the HLSL method on, an explicit sampler (if the GLSL source combined a separate texture and
// sampler inline via a `sampler2D(tex, samp)` constructor), and the keyword driving overload
// selection.
struct texture_arg_t {
    ast_expression *object = nullptr;
    ast_expression *sampler = nullptr;
    eKeyword kw = eKeyword::K_void;
};

texture_arg_t Resolve_TextureArg(const ast_function_call *call) {
    ast_expression *arg0 = call->parameters[0];
    if (arg0->type == eExprType::ConstructorCall) {
        auto *ctor = static_cast<ast_constructor_call *>(arg0);
        if (ctor->type->builtin && ctor->parameters.size() == 2) {
            const eKeyword ckw = static_cast<const ast_builtin *>(ctor->type)->type;
            if (is_combined_sampler_keyword(ckw)) {
                return {ctor->parameters[0], ctor->parameters[1], ckw};
            }
        }
    }
    eKeyword kw = eKeyword::K_void;
    if (call->func && !call->func->parameters.empty() && call->func->parameters[0]->base_type->builtin) {
        kw = static_cast<const ast_builtin *>(call->func->parameters[0]->base_type)->type;
    } else {
        const ast_variable *v = Find_TextureImageVariable(arg0);
        if (v && v->base_type->builtin) {
            kw = static_cast<const ast_builtin *>(v->base_type)->type;
        }
    }
    return {arg0, nullptr, kw};
}

bool is_sample_call_name(const char *name) {
    static const char *const names[] = {"texture",           "textureOffset",       "textureGrad",
                                        "textureGradOffset", "textureProj",         "textureProjLod",
                                        "textureProjOffset", "textureProjGrad",     "textureProjGradOffset",
                                        "textureGather",     "textureGatherOffset", "textureGatherOffsets"};
    for (const char *n : names) {
        if (strcmp(name, n) == 0) {
            return true;
        }
    }
    return false;
}

// Result type of the sampling/gathering method calls this pass creates: the parser evaluator has no
// knowledge of post-lowering nodes, and guarding a `?:` needs every arm typed.
const ast_type *Evaluate_HLSLMethodResultType(const TrUnit *tu, const ast_method_call *call) {
    static const char *const g_vec4_methods[] = {"Sample",         "SampleBias",    "SampleLevel",   "SampleGrad",
                                                 "Load",           "Gather",        "GatherRed",     "GatherGreen",
                                                 "GatherBlue",     "GatherAlpha",   "GatherCmp",     "GatherCmpRed",
                                                 "GatherCmpGreen", "GatherCmpBlue", "GatherCmpAlpha"};
    static const char *const g_scalar_methods[] = {"SampleCmp", "SampleCmpLevelZero"};

    bool scalar_result = false;
    bool known_method = false;
    for (const char *name : g_scalar_methods) {
        if (strcmp(call->name, name) == 0) {
            scalar_result = true;
            known_method = true;
        }
    }
    if (!known_method) {
        for (const char *name : g_vec4_methods) {
            if (strcmp(call->name, name) == 0) {
                known_method = true;
                break;
            }
        }
    }
    if (!known_method) {
        return nullptr;
    }

    const ast_variable *tex = Find_TextureImageVariable(call->object);
    if (!tex || !tex->base_type || !tex->base_type->builtin) {
        return nullptr; // ByteAddressBuffer Load: its leaf type is not recoverable from the call
    }

    const bool is_int = is_int_image(tex->base_type);
    const bool is_uint = !is_int && is_uint_image(tex->base_type);
    eKeyword result = eKeyword::K_vec4;
    if (scalar_result) {
        result = is_int ? eKeyword::K_int : (is_uint ? eKeyword::K_uint : eKeyword::K_float);
    } else if (is_int) {
        result = eKeyword::K_ivec4;
    } else if (is_uint) {
        result = eKeyword::K_uvec4;
    }
    // The builtin table is filled on demand, which the evaluator's const TrUnit cannot express.
    return const_cast<TrUnit *>(tu)->FindOrAddBuiltin(result);
}

bool is_scalar_type(const eKeyword type) {
    switch (type) {
    case eKeyword::K_int:
    case eKeyword::K_uint:
    case eKeyword::K_float:
    case eKeyword::K_double:
    case eKeyword::K_bool:
        return true;
    default:
        return false;
    }
}

bool is_scalar_type(const ast_type *type) {
    return type->builtin && is_scalar_type(static_cast<const ast_builtin *>(type)->type);
}

// All GLSL scalar numeric types, including the 16/64-bit ones; matrix ctors convert them to the
// matrix element type, unlike the vecN splat path which only covers the core scalars.
static bool is_struct_constructor(const ast_expression *expr) {
    return expr->type == eExprType::ConstructorCall && !static_cast<const ast_constructor_call *>(expr)->type->builtin;
}

static ast_array_specifier *Build_StructInit(TrUnit *tu, ast_constructor_call *call) {
    auto *init = tu->make<ast_array_specifier>(tu->alloc.allocator);
    for (ast_expression *p : call->parameters) {
        init->expressions.push_back(p);
    }
    return init;
}

// HLSL has no struct constructors (`(S)(a, b)` is a cast of a comma expression), so a declaration
// initializer becomes a brace list, recursing through nested struct ctors and array specifiers.
static ast_expression *Convert_StructInit(TrUnit *tu, ast_expression *expr) {
    if (!expr) {
        return expr;
    }
    if (is_struct_constructor(expr)) {
        auto *call = static_cast<ast_constructor_call *>(expr);
        for (ast_expression *&p : call->parameters) {
            p = Convert_StructInit(tu, p);
        }
        return Build_StructInit(tu, call);
    }
    if (expr->type == eExprType::ArraySpecifier) {
        for (ast_expression *&e : static_cast<ast_array_specifier *>(expr)->expressions) {
            e = Convert_StructInit(tu, e);
        }
    }
    return expr;
}

static bool is_scalar_numeric_type(const ast_type *type) {
    if (!type->builtin) {
        return false;
    }
    switch (static_cast<const ast_builtin *>(type)->type) {
    case eKeyword::K_int:
    case eKeyword::K_uint:
    case eKeyword::K_float:
    case eKeyword::K_double:
    case eKeyword::K_bool:
    case eKeyword::K_int16_t:
    case eKeyword::K_uint16_t:
    case eKeyword::K_float16_t:
    case eKeyword::K_int64_t:
    case eKeyword::K_uint64_t:
        return true;
    default:
        return false;
    }
}

ast_expression *Build_Swizzle(TrUnit *tu, ast_expression *operand, const char *name) {
    auto *field = tu->make<ast_field_or_swizzle>();
    field->operand = operand;
    field->name = tu->makestr(name);
    return field;
}

// True when an expression may appear twice in the output without changing behavior: it neither calls
// anything nor contains a sub-expression that lowering turns into statements. The texture-coordinate
// helpers below use this to decide whether a coordinate has to be materialized in a temporary.
bool is_repeatable_coord_expr(const ast_expression *expr) {
    switch (expr->type) {
    case eExprType::ShortConstant:
    case eExprType::UShortConstant:
    case eExprType::IntConstant:
    case eExprType::UIntConstant:
    case eExprType::LongConstant:
    case eExprType::ULongConstant:
    case eExprType::HalfConstant:
    case eExprType::FloatConstant:
    case eExprType::DoubleConstant:
    case eExprType::BoolConstant:
    case eExprType::VariableIdentifier:
        return true;
    case eExprType::FieldOrSwizzle:
        return is_repeatable_coord_expr(static_cast<const ast_field_or_swizzle *>(expr)->operand);
    case eExprType::ArraySubscript: {
        const auto *subscript = static_cast<const ast_array_subscript *>(expr);
        return is_repeatable_coord_expr(subscript->operand) && is_repeatable_coord_expr(subscript->index);
    }
    case eExprType::UnaryMinus:
    case eExprType::UnaryPlus:
        return is_repeatable_coord_expr(static_cast<const ast_unary_expression *>(expr)->operand);
    case eExprType::Operation: {
        const auto *operation = static_cast<const ast_operation_expression *>(expr);
        return is_repeatable_coord_expr(operation->operand1) && is_repeatable_coord_expr(operation->operand2);
    }
    case eExprType::ConstructorCall:
        return std::all_of(static_cast<const ast_constructor_call *>(expr)->parameters.begin(),
                           static_cast<const ast_constructor_call *>(expr)->parameters.end(), is_repeatable_coord_expr);
    default:
        return false;
    }
}

// Coordinate dimensions textureProj* divides by its last component -- only defined for plain (non-
// shadow, non-cube, non-array) 1D/2D/3D samplers; 0 means "unsupported, caller asserts".
int Proj_CoordDims(const eKeyword kw) {
    switch (kw) {
    case eKeyword::K_sampler1D:
    case eKeyword::K_isampler1D:
    case eKeyword::K_usampler1D:
        return 1;
    case eKeyword::K_sampler2D:
    case eKeyword::K_isampler2D:
    case eKeyword::K_usampler2D:
        return 2;
    case eKeyword::K_sampler3D:
    case eKeyword::K_isampler3D:
    case eKeyword::K_usampler3D:
        return 3;
    default:
        return 0;
    }
}

// HLSL has no projective sampling: emit P.xy.. / P.<last>. P is swizzled twice, so the caller hoists it
// into a temporary when it is not repeatable.
ast_expression *Build_ProjCoord(TrUnit *tu, ast_expression *P, const int coord_dims, const int p_vec_size) {
    static const char kSwizzle[] = "xyzw";
    char coord_name[4] = {};
    for (int i = 0; i < coord_dims; ++i) {
        coord_name[i] = kSwizzle[i];
    }
    const char div_name[2] = {kSwizzle[p_vec_size - 1], '\0'};

    auto *div = tu->make<ast_operation_expression>(eOperator::divide);
    div->operand1 = Build_Swizzle(tu, P, coord_name);
    div->operand2 = Build_Swizzle(tu, P, div_name);
    return div;
}

struct shadow_coord_t {
    ast_expression *coord = nullptr;
    ast_expression *cmp = nullptr;
};

// Shadow samplers bundle the compare value into P's last component, except CubeArrayShadow, where
// texture()/textureOffset() pass it as a genuinely separate argument (compare_arg). P is swizzled
// twice, so the caller hoists it into a temporary when it is not repeatable.
shadow_coord_t Split_ShadowCoord(TrUnit *tu, ast_expression *P, const eKeyword kw, ast_expression *compare_arg) {
    switch (kw) {
    case eKeyword::K_sampler1DShadow:
        return {Build_Swizzle(tu, P, "x"), Build_Swizzle(tu, P, "z")};
    case eKeyword::K_sampler1DArrayShadow:
    case eKeyword::K_sampler2DShadow:
        return {Build_Swizzle(tu, P, "xy"), Build_Swizzle(tu, P, "z")};
    case eKeyword::K_sampler2DArrayShadow:
    case eKeyword::K_samplerCubeShadow:
        return {Build_Swizzle(tu, P, "xyz"), Build_Swizzle(tu, P, "w")};
    case eKeyword::K_samplerCubeArrayShadow:
        return {P, compare_arg};
    default:
        return {};
    }
}

// Gather method suffix: "" or Red/Green/Blue/Alpha from `comp`. `force_channel` (4-offset form) makes
// the default "Red", since only the per-channel methods have a 4-offset overload.
const char *Gather_MethodSuffix(const ast_expression *comp_expr, const bool force_channel) {
    if (!comp_expr && !force_channel) {
        return "";
    }
    int32_t comp = 0;
    if (comp_expr) {
        if (comp_expr->type == eExprType::IntConstant) {
            comp = static_cast<const ast_int_constant *>(comp_expr)->value;
        } else if (comp_expr->type == eExprType::UIntConstant) {
            comp = int32_t(static_cast<const ast_uint_constant *>(comp_expr)->value);
        } else {
            assert(false && "textureGather comp must be a compile-time literal 0-3");
        }
    }
    switch (comp) {
    case 0:
        return "Red";
    case 1:
        return "Green";
    case 2:
        return "Blue";
    case 3:
        return "Alpha";
    default:
        assert(false && "textureGather comp out of range (must be 0-3)");
        return "Red";
    }
}

// rayQueryGetIntersectionXxxEXT(q, committed) builtins all reduce to a pair of zero-arg RayQuery
// methods -- q.Committed...() when `committed` is true, q.Candidate...() otherwise.
struct ray_query_accessor_t {
    const char *glsl_name;
    const char *committed_method;
    const char *candidate_method;
};

const ray_query_accessor_t g_ray_query_accessors[] = {
    {"rayQueryGetIntersectionTypeEXT", "CommittedStatus", "CandidateType"},
    {"rayQueryGetIntersectionInstanceCustomIndexEXT", "CommittedInstanceID", "CandidateInstanceID"},
    {"rayQueryGetIntersectionInstanceIdEXT", "CommittedInstanceIndex", "CandidateInstanceIndex"},
    {"rayQueryGetIntersectionPrimitiveIndexEXT", "CommittedPrimitiveIndex", "CandidatePrimitiveIndex"},
    {"rayQueryGetIntersectionFrontFaceEXT", "CommittedTriangleFrontFace", "CandidateTriangleFrontFace"},
    {"rayQueryGetIntersectionBarycentricsEXT", "CommittedTriangleBarycentrics", "CandidateTriangleBarycentrics"},
    {"rayQueryGetIntersectionTEXT", "CommittedRayT", "CandidateTriangleRayT"},
};

// The 5 gl_RayQuery*EXT constants are Hidden AST globals (added unconditionally in
// Parser::InitSpecialGlobals) that WriterHLSL's generic globals loop never prints -- lower a
// reference to one directly to its HLSL RAY_QUERY enum spelling instead.
struct ray_query_constant_t {
    const char *glsl_name;
    const char *hlsl_name;
};

const ray_query_constant_t g_ray_query_constants[] = {
    {"gl_RayQueryCommittedIntersectionNoneEXT", "COMMITTED_NOTHING"},
    {"gl_RayQueryCommittedIntersectionTriangleEXT", "COMMITTED_TRIANGLE_HIT"},
    {"gl_RayQueryCommittedIntersectionGeneratedEXT", "COMMITTED_PROCEDURAL_PRIMITIVE_HIT"},
    {"gl_RayQueryCandidateIntersectionTriangleEXT", "CANDIDATE_NON_OPAQUE_TRIANGLE"},
    {"gl_RayQueryCandidateIntersectionAABBEXT", "CANDIDATE_PROCEDURAL_PRIMITIVE"},
};

// Builds a `object.method_name()` call. The ray-query call-site rewrites below use this both for
// simple method renames and for the two branches of the paired accessors above.
ast_method_call *Build_RayQueryMethodCall(TrUnit *tu, ast_expression *object, const char *method_name) {
    auto *method_call = tu->make<ast_method_call>(tu->alloc.allocator);
    method_call->object = object;
    method_call->name = tu->makestr(method_name);
    return method_call;
}
} // namespace glslx

glslx::ast_expression *glslx::LowerHLSL::Rewrite_TextureImageCall(ast_function_call *call) {
    if (strcmp(call->name, "texelFetch") == 0) {
        const texture_arg_t targ = Resolve_TextureArg(call);
        eKeyword coord_kw;
        switch (targ.kw) {
        case eKeyword::K_sampler2D:
        case eKeyword::K_isampler2D:
        case eKeyword::K_usampler2D:
        case eKeyword::K_texture2D:
        case eKeyword::K_itexture2D:
        case eKeyword::K_utexture2D:
            coord_kw = eKeyword::K_ivec3;
            break;
        case eKeyword::K_sampler2DArray:
        case eKeyword::K_isampler2DArray:
        case eKeyword::K_usampler2DArray:
        case eKeyword::K_sampler3D:
        case eKeyword::K_isampler3D:
        case eKeyword::K_usampler3D:
        case eKeyword::K_texture2DArray:
        case eKeyword::K_itexture2DArray:
        case eKeyword::K_utexture2DArray:
        case eKeyword::K_texture3D:
        case eKeyword::K_itexture3D:
        case eKeyword::K_utexture3D:
            coord_kw = eKeyword::K_ivec4;
            break;
        default:
            return nullptr;
        }

        auto *coord = tu_->make<ast_constructor_call>(tu_->alloc.allocator);
        coord->type = tu_->FindOrAddBuiltin(coord_kw);
        coord->parameters.push_back(call->parameters[1]);
        coord->parameters.push_back(call->parameters[2]);

        auto *method_call = tu_->make<ast_method_call>(tu_->alloc.allocator);
        method_call->object = targ.object;
        method_call->name = tu_->makestr("Load");
        method_call->parameters.push_back(coord);
        return method_call;
    }

    if (strcmp(call->name, "textureLod") == 0 || strcmp(call->name, "textureLodOffset") == 0) {
        const bool is_offset = strcmp(call->name, "textureLodOffset") == 0;
        const texture_arg_t targ = Resolve_TextureArg(call);
        switch (targ.kw) {
        case eKeyword::K_sampler2D:
            break;
        case eKeyword::K_sampler3D:
        case eKeyword::K_sampler2DArray:
            if (is_offset) {
                return nullptr;
            }
            break;
        default:
            return nullptr;
        }

        ast_expression *sampler_ref = targ.sampler ? targ.sampler : Build_SamplerRef(tu_, targ.object);
        if (!sampler_ref) {
            return nullptr;
        }

        auto *method_call = tu_->make<ast_method_call>(tu_->alloc.allocator);
        method_call->object = targ.object;
        method_call->name = tu_->makestr("SampleLevel");
        method_call->parameters.push_back(sampler_ref);
        method_call->parameters.push_back(call->parameters[1]); // P
        method_call->parameters.push_back(call->parameters[2]); // lod
        if (is_offset) {
            method_call->parameters.push_back(call->parameters[3]); // offset
        }
        return method_call;
    }

    if (strcmp(call->name, "imageLoad") == 0 || strcmp(call->name, "imageStore") == 0) {
        if (!call->func || call->func->parameters.empty() || !call->func->parameters[0]->base_type->builtin) {
            return nullptr;
        }
        const eKeyword kw = static_cast<const ast_builtin *>(call->func->parameters[0]->base_type)->type;
        if (kw != eKeyword::K_image2D && kw != eKeyword::K_iimage2D && kw != eKeyword::K_uimage2D) {
            return nullptr;
        }
        const ast_variable *image_var = Find_TextureImageVariable(call->parameters[0]);
        if (!image_var) {
            return nullptr;
        }
        const int comp_count = Image_ComponentCount(image_var);
        if (comp_count == 3) {
            // r11f_g11f_b10f -- unsupported (the removed polyfill block had no overload for it either).
            return nullptr;
        }

        auto *subscript = tu_->make<ast_array_subscript>();
        subscript->operand = call->parameters[0];
        subscript->index = call->parameters[1];

        const bool is_store = strcmp(call->name, "imageStore") == 0;
        if (comp_count == 4) {
            if (!is_store) {
                return subscript;
            }
            auto *assign = tu_->make<ast_assignment_expression>(eOperator::assign);
            assign->operand1 = subscript;
            assign->operand2 = call->parameters[2];
            return assign;
        }

        if (is_store) {
            auto *field = tu_->make<ast_field_or_swizzle>();
            field->operand = call->parameters[2];
            field->name = tu_->makestr(comp_count == 2 ? "xy" : "x");

            auto *assign = tu_->make<ast_assignment_expression>(eOperator::assign);
            assign->operand1 = subscript;
            assign->operand2 = field;
            return assign;
        }

        const eKeyword vec4_kw = is_int_image(image_var->base_type)
                                     ? eKeyword::K_ivec4
                                     : (is_uint_image(image_var->base_type) ? eKeyword::K_uvec4 : eKeyword::K_vec4);
        auto *ctor = tu_->make<ast_constructor_call>(tu_->alloc.allocator);
        ctor->type = tu_->FindOrAddBuiltin(vec4_kw);
        ctor->parameters.push_back(subscript);
        const int zero_count = (comp_count == 2) ? 2 : 3;
        for (int i = 0; i < zero_count; ++i) {
            ctor->parameters.push_back(tu_->make<ast_int_constant>(0));
        }
        return ctor;
    }

    if (ast_expression *r = Rewrite_SampleCall(call)) {
        return r;
    }

    return nullptr;
}

// The proj divide and the shadow coord/compare split swizzle the same coordinate twice, so anything
// that is not a plain read goes through a temporary instead. The declaration itself is emitted by
// Lower_Atomics when it first visits the temporary's identifier, which keeps the coordinate in the
// sample call's own evaluation position (and inside the guard of a short-circuited operand).
glslx::ast_expression *glslx::LowerHLSL::Hoist_SharedCoord(ast_expression *coord) {
    if (is_repeatable_coord_expr(coord)) {
        return coord;
    }

    int array_dims = 0;
    const ast_type *coord_type = Evaluate_ExpressionResultType(tu_, coord, array_dims);
    assert(coord_type && array_dims == 0 && "coordinate used twice where it cannot be typed");
    if (!coord_type || array_dims != 0) {
        return coord;
    }

    const int index = temp_index_++;
    auto *coord_var = tu_->make<ast_function_variable>(tu_->alloc.allocator);
    coord_var->base_type = const_cast<ast_type *>(coord_type);
    coord_var->name = tu_->makestr(("__coord" + std::to_string(index)).c_str());
    shared_coords_.push_back({coord_var, coord});

    return tu_->make<ast_variable_identifier>(coord_var);
}

// Scalar-filled matrix ctors reference the scalar at every diagonal slot, so a runtime scalar goes
// through a temporary (evaluated once) declared by the same first-use flush as shared coordinates.
glslx::ast_expression *glslx::LowerHLSL::Hoist_MatrixScalar(ast_expression *scalar, const ast_type *scalar_type) {
    const int index = temp_index_++;
    auto *mat_var = tu_->make<ast_function_variable>(tu_->alloc.allocator);
    mat_var->base_type = const_cast<ast_type *>(scalar_type);
    mat_var->name = tu_->makestr(("__mat" + std::to_string(index)).c_str());
    shared_coords_.push_back({mat_var, scalar});

    return tu_->make<ast_variable_identifier>(mat_var);
}

// Emits the declaration for a hoisted shared coordinate at its first use.
void glslx::LowerHLSL::Flush_SharedCoord(const ast_variable *var, global_vector<ast_statement *> &pre) {
    for (auto it = shared_coords_.begin(); it != shared_coords_.end(); ++it) {
        if (static_cast<ast_variable *>(it->var) != var) {
            continue;
        }
        ast_function_variable *coord_var = it->var;
        ast_expression *coord = it->coord;
        // Erased before recursing: a nested coordinate flushes (and erases) from the same vector.
        shared_coords_.erase(it);

        // Lowers (and hoists) everything the coordinate needs before taking the temporary.
        coord_var->initial_value = Lower_Atomics(coord, pre);
        auto *coord_decl = tu_->make<ast_declaration_statement>(tu_->alloc.allocator);
        coord_decl->variables.push_back(coord_var);
        pre.push_back(coord_decl);
        return;
    }
}

glslx::ast_expression *glslx::LowerHLSL::Rewrite_SampleCall(ast_function_call *call) {
    if (!is_sample_call_name(call->name)) {
        return nullptr;
    }

    const texture_arg_t targ = Resolve_TextureArg(call);
    assert(is_combined_sampler_keyword(targ.kw) && "unsupported sampler kind reached Rewrite_SampleCall");
    ast_expression *sampler_ref = targ.sampler ? targ.sampler : Build_SamplerRef(tu_, targ.object);
    assert(sampler_ref);
    const bool is_shadow = is_shadow_sampler_kw(targ.kw);
    ast_expression *P = call->parameters[1];
    const int param_count = int(call->parameters.size());

    const auto make_call = [&](const char *name) -> ast_method_call * {
        auto *method_call = tu_->make<ast_method_call>(tu_->alloc.allocator);
        method_call->object = targ.object;
        method_call->name = tu_->makestr(name);
        method_call->parameters.push_back(sampler_ref);
        return method_call;
    };
    const auto gather_name = [&](const char *suffix) { return tu_->makestr((std::string("Gather") + suffix).c_str()); };

    if (strcmp(call->name, "texture") == 0) {
        if (is_shadow) {
            if (param_count == 2) {
                P = Hoist_SharedCoord(P);
                const shadow_coord_t sc = Split_ShadowCoord(tu_, P, targ.kw, nullptr);
                ast_method_call *mc = make_call("SampleCmp");
                mc->parameters.push_back(sc.coord);
                mc->parameters.push_back(sc.cmp);
                return mc;
            }
            if (param_count == 3 && targ.kw == eKeyword::K_samplerCubeArrayShadow) {
                ast_method_call *mc = make_call("SampleCmp");
                mc->parameters.push_back(P);
                mc->parameters.push_back(call->parameters[2]); // compare
                return mc;
            }
            assert(false && "shadow texture() with bias has no HLSL equivalent");
            return nullptr;
        }
        ast_method_call *mc = make_call(param_count == 3 ? "SampleBias" : "Sample");
        mc->parameters.push_back(P);
        if (param_count == 3) {
            mc->parameters.push_back(call->parameters[2]); // bias
        }
        return mc;
    }

    if (strcmp(call->name, "textureOffset") == 0) {
        ast_expression *offset = call->parameters[2];
        if (is_shadow) {
            assert(param_count == 3 && "shadow textureOffset() with bias has no HLSL equivalent");
            P = Hoist_SharedCoord(P);
            const shadow_coord_t sc = Split_ShadowCoord(tu_, P, targ.kw, nullptr);
            ast_method_call *mc = make_call("SampleCmp");
            mc->parameters.push_back(sc.coord);
            mc->parameters.push_back(sc.cmp);
            mc->parameters.push_back(offset);
            return mc;
        }
        ast_method_call *mc = make_call(param_count == 4 ? "SampleBias" : "Sample");
        mc->parameters.push_back(P);
        if (param_count == 4) {
            mc->parameters.push_back(call->parameters[3]); // bias
        }
        mc->parameters.push_back(offset);
        return mc;
    }

    if (strcmp(call->name, "textureGrad") == 0) {
        assert(!is_shadow && "shadow textureGrad() has no HLSL equivalent");
        ast_method_call *mc = make_call("SampleGrad");
        mc->parameters.push_back(P);
        mc->parameters.push_back(call->parameters[2]); // dPdx
        mc->parameters.push_back(call->parameters[3]); // dPdy
        return mc;
    }

    if (strcmp(call->name, "textureGradOffset") == 0) {
        assert(!is_shadow && "shadow textureGradOffset() has no HLSL equivalent");
        ast_method_call *mc = make_call("SampleGrad");
        mc->parameters.push_back(P);
        mc->parameters.push_back(call->parameters[2]); // dPdx
        mc->parameters.push_back(call->parameters[3]); // dPdy
        mc->parameters.push_back(call->parameters[4]); // offset
        return mc;
    }

    if (strcmp(call->name, "textureProj") == 0 || strcmp(call->name, "textureProjOffset") == 0 ||
        strcmp(call->name, "textureProjLod") == 0 || strcmp(call->name, "textureProjGrad") == 0 ||
        strcmp(call->name, "textureProjGradOffset") == 0) {
        assert(!is_shadow && "shadow textureProj*() has no HLSL equivalent");
        const int coord_dims = Proj_CoordDims(targ.kw);
        assert(coord_dims != 0 && "textureProj*() is only defined for plain 1D/2D/3D samplers");
        P = Hoist_SharedCoord(P);
        int array_dims = 0;
        const ast_type *p_type = Evaluate_ExpressionResultType(tu_, P, array_dims);
        assert(p_type);
        const int p_vec_size = get_vector_size(p_type);
        ast_expression *coord_div = Build_ProjCoord(tu_, P, coord_dims, p_vec_size);

        if (strcmp(call->name, "textureProj") == 0) {
            ast_method_call *mc = make_call(param_count == 3 ? "SampleBias" : "Sample");
            mc->parameters.push_back(coord_div);
            if (param_count == 3) {
                mc->parameters.push_back(call->parameters[2]); // bias
            }
            return mc;
        }
        if (strcmp(call->name, "textureProjOffset") == 0) {
            ast_expression *offset = call->parameters[2];
            ast_method_call *mc = make_call(param_count == 4 ? "SampleBias" : "Sample");
            mc->parameters.push_back(coord_div);
            if (param_count == 4) {
                mc->parameters.push_back(call->parameters[3]); // bias
            }
            mc->parameters.push_back(offset);
            return mc;
        }
        if (strcmp(call->name, "textureProjLod") == 0) {
            ast_method_call *mc = make_call("SampleLevel");
            mc->parameters.push_back(coord_div);
            mc->parameters.push_back(call->parameters[2]); // lod
            return mc;
        }
        if (strcmp(call->name, "textureProjGrad") == 0) {
            ast_method_call *mc = make_call("SampleGrad");
            mc->parameters.push_back(coord_div);
            mc->parameters.push_back(call->parameters[2]); // dPdx
            mc->parameters.push_back(call->parameters[3]); // dPdy
            return mc;
        }
        // textureProjGradOffset
        ast_method_call *mc = make_call("SampleGrad");
        mc->parameters.push_back(coord_div);
        mc->parameters.push_back(call->parameters[2]); // dPdx
        mc->parameters.push_back(call->parameters[3]); // dPdy
        mc->parameters.push_back(call->parameters[4]); // offset
        return mc;
    }

    if (strcmp(call->name, "textureGather") == 0) {
        if (is_shadow) {
            assert(param_count == 3 && "shadow textureGather() always takes refZ");
            ast_method_call *mc = make_call("GatherCmp");
            mc->parameters.push_back(P);
            mc->parameters.push_back(call->parameters[2]); // refZ
            return mc;
        }
        const char *suffix = Gather_MethodSuffix(param_count == 3 ? call->parameters[2] : nullptr, false);
        ast_method_call *mc = make_call(gather_name(suffix));
        mc->parameters.push_back(P);
        return mc;
    }

    if (strcmp(call->name, "textureGatherOffset") == 0) {
        if (is_shadow) {
            assert(param_count == 4 && "shadow textureGatherOffset() always takes refZ and offset");
            ast_method_call *mc = make_call("GatherCmp");
            mc->parameters.push_back(P);
            mc->parameters.push_back(call->parameters[2]); // refZ
            mc->parameters.push_back(call->parameters[3]); // offset
            return mc;
        }
        ast_expression *offset = call->parameters[2];
        const char *suffix = Gather_MethodSuffix(param_count == 4 ? call->parameters[3] : nullptr, false);
        ast_method_call *mc = make_call(gather_name(suffix));
        mc->parameters.push_back(P);
        mc->parameters.push_back(offset);
        return mc;
    }

    // textureGatherOffsets
    assert(!is_shadow && "shadow textureGatherOffsets() has no HLSL equivalent (no 4-offset GatherCmp overload)");
    ast_expression *offsets_array = call->parameters[2];
    const char *suffix = Gather_MethodSuffix(param_count == 4 ? call->parameters[3] : nullptr, /*force_channel=*/true);
    ast_method_call *mc = make_call(gather_name(suffix));
    mc->parameters.push_back(P);
    for (int i = 0; i < 4; ++i) {
        auto *subscript = tu_->make<ast_array_subscript>();
        subscript->operand = offsets_array;
        subscript->index = tu_->make<ast_int_constant>(i);
        mc->parameters.push_back(subscript);
    }
    return mc;
}

glslx::ast_expression *glslx::LowerHLSL::Rewrite_RayQueryCall(ast_function_call *call) {
    // Note: ray-query builtins are never declared (not in BuiltinPrototypes.inl), so call->func is
    // always null here -- dispatch is by call->name alone. rayQueryInitializeEXT is handled
    // separately (Try_Lower_RayQueryInit) since it expands to two statements, not an expression.
    if (strcmp(call->name, "rayQueryProceedEXT") == 0 && call->parameters.size() == 1) {
        return Build_RayQueryMethodCall(tu_, call->parameters[0], "Proceed");
    }
    if (strcmp(call->name, "rayQueryConfirmIntersectionEXT") == 0 && call->parameters.size() == 1) {
        return Build_RayQueryMethodCall(tu_, call->parameters[0], "CommitNonOpaqueTriangleHit");
    }
    if (call->parameters.size() == 2) {
        for (const ray_query_accessor_t &accessor : g_ray_query_accessors) {
            if (strcmp(call->name, accessor.glsl_name) != 0) {
                continue;
            }
            if (call->parameters[1]->type == eExprType::BoolConstant) {
                // The condition is a literal constant -- select the branch right away instead of
                // emitting a runtime ternary.
                const bool committed = static_cast<const ast_bool_constant *>(call->parameters[1])->value;
                return Build_RayQueryMethodCall(tu_, call->parameters[0],
                                                committed ? accessor.committed_method : accessor.candidate_method);
            }

            auto *ternary = tu_->make<ast_ternary_expression>();
            ternary->condition = call->parameters[1];
            ternary->on_true = Build_RayQueryMethodCall(tu_, call->parameters[0], accessor.committed_method);
            ternary->on_false = Build_RayQueryMethodCall(tu_, call->parameters[0], accessor.candidate_method);
            return ternary;
        }
    }
    return nullptr;
}

glslx::ast_expression *glslx::LowerHLSL::Hoist_TextureSize(ast_function_call *call,
                                                           global_vector<ast_statement *> &pre) {
    const texture_arg_t targ = Resolve_TextureArg(call);
    bool mip_level = true;
    int components = GetDimensions_Components(targ.kw, mip_level);
    if (components == 0) {
        const ast_variable *var = Find_TextureImageVariable(targ.object);
        if (var && var->base_type && var->base_type->builtin) {
            components = GetDimensions_Components(static_cast<const ast_builtin *>(var->base_type)->type, mip_level);
        }
    }
    assert(components != 0 && "textureSize on an unsupported sampler kind");
    if (components == 0) {
        return call;
    }

    const int index = temp_index_++;

    auto *size_var = tu_->make<ast_function_variable>(tu_->alloc.allocator);
    size_var->base_type = tu_->FindOrAddBuiltin(Dims_VectorKeyword(true, components));
    size_var->name = tu_->makestr(("__texsize" + std::to_string(index)).c_str());
    auto *size_decl = tu_->make<ast_declaration_statement>(tu_->alloc.allocator);
    size_decl->variables.push_back(size_var);
    pre.push_back(size_decl);

    auto *extra_var = tu_->make<ast_function_variable>(tu_->alloc.allocator);
    extra_var->base_type = tu_->FindOrAddBuiltin(eKeyword::K_uint);
    extra_var->name =
        tu_->makestr((std::string(mip_level ? "__numlevels" : "__numsamples") + std::to_string(index)).c_str());
    auto *extra_decl = tu_->make<ast_declaration_statement>(tu_->alloc.allocator);
    extra_decl->variables.push_back(extra_var);
    pre.push_back(extra_decl);

    auto *get_dims = tu_->make<ast_method_call>(tu_->alloc.allocator);
    get_dims->object = targ.object;
    get_dims->name = tu_->makestr("GetDimensions");
    if (mip_level) {
        get_dims->parameters.push_back(call->parameters.size() > 1 ? call->parameters[1]
                                                                   : tu_->make<ast_int_constant>(0));
    }
    static const char *g_swizzles[] = {"x", "y", "z"};
    if (components == 1) {
        get_dims->parameters.push_back(tu_->make<ast_variable_identifier>(size_var));
    } else {
        for (int i = 0; i < components; ++i) {
            auto *field = tu_->make<ast_field_or_swizzle>();
            field->operand = tu_->make<ast_variable_identifier>(size_var);
            field->name = tu_->makestr(g_swizzles[i]);
            get_dims->parameters.push_back(field);
        }
    }
    get_dims->parameters.push_back(tu_->make<ast_variable_identifier>(extra_var));
    pre.push_back(tu_->make<ast_expression_statement>(get_dims));

    auto *cast = tu_->make<ast_constructor_call>(tu_->alloc.allocator);
    cast->type = tu_->FindOrAddBuiltin(Dims_VectorKeyword(false, components));
    cast->parameters.push_back(tu_->make<ast_variable_identifier>(size_var));
    return cast;
}

// Compute specials without a prologue slot: gl_WorkGroupSize comes from the local_size_* qualifiers, the
// subgroup masks are per-word lane-bit expressions over WaveGetLaneIndex() (waves of up to 128 lanes).
glslx::ast_expression *glslx::LowerHLSL::Rewrite_GlSpecial(const char *var_name) {
    if (strcmp(var_name, "gl_WorkGroupSize") == 0) {
        if (!local_size_scanned_) {
            local_size_scanned_ = true;
            for (const ast_interface_block *block : tu_->interface_blocks) {
                for (const ast_layout_qualifier *qualifier : block->layout_qualifiers) {
                    int index = -1;
                    if (strcmp(qualifier->name, "local_size_x") == 0) {
                        index = 0;
                    } else if (strcmp(qualifier->name, "local_size_y") == 0) {
                        index = 1;
                    } else if (strcmp(qualifier->name, "local_size_z") == 0) {
                        index = 2;
                    }
                    if (index == -1 || !qualifier->initial_value) {
                        continue;
                    }
                    if (qualifier->initial_value->type == eExprType::IntConstant) {
                        local_size_[index] = static_cast<const ast_int_constant *>(qualifier->initial_value)->value;
                    } else if (qualifier->initial_value->type == eExprType::UIntConstant) {
                        local_size_[index] =
                            int(static_cast<const ast_uint_constant *>(qualifier->initial_value)->value);
                    }
                }
            }
        }
        auto *ctor = tu_->make<ast_constructor_call>(tu_->alloc.allocator);
        ctor->type = tu_->FindOrAddBuiltin(eKeyword::K_uvec3);
        for (int size : local_size_) {
            ctor->parameters.push_back(tu_->make<ast_uint_constant>(uint32_t(size)));
        }
        return ctor;
    }

    static const struct {
        const char *name;
        int kind; // 0 Eq, 1 Lt, 2 Le, 3 Ge, 4 Gt
    } g_subgroup_masks[] = {
        {"gl_SubgroupEqMask", 0}, {"gl_SubgroupLtMask", 1}, {"gl_SubgroupLeMask", 2},
        {"gl_SubgroupGeMask", 3}, {"gl_SubgroupGtMask", 4},
    };
    for (const auto &mask : g_subgroup_masks) {
        if (strcmp(var_name, mask.name) != 0) {
            continue;
        }
        auto make_uint = [this](uint32_t value) {
            return static_cast<ast_expression *>(tu_->make<ast_uint_constant>(value));
        };
        auto make_op = [this](eOperator oper, ast_expression *lhs, ast_expression *rhs) {
            auto *op = tu_->make<ast_operation_expression>(oper);
            op->operand1 = lhs;
            op->operand2 = rhs;
            return static_cast<ast_expression *>(op);
        };
        auto make_ternary = [this](ast_expression *cond, ast_expression *on_true, ast_expression *on_false) {
            auto *ternary = tu_->make<ast_ternary_expression>();
            ternary->condition = cond;
            ternary->on_true = on_true;
            ternary->on_false = on_false;
            return static_cast<ast_expression *>(ternary);
        };
        auto make_lane = [this]() {
            auto *call = tu_->make<ast_function_call>(tu_->alloc.allocator);
            call->name = tu_->makestr("WaveGetLaneIndex");
            return static_cast<ast_expression *>(call);
        };
        auto make_not = [this](ast_expression *operand) {
            return static_cast<ast_expression *>(tu_->make<ast_unary_bit_not_expression>(operand));
        };

        auto *ctor = tu_->make<ast_constructor_call>(tu_->alloc.allocator);
        ctor->type = tu_->FindOrAddBuiltin(eKeyword::K_uvec4);
        for (uint32_t word = 0; word < 4; ++word) {
            const auto lane_word = [&]() { return make_op(eOperator::shift_right, make_lane(), make_uint(5)); };
            const auto lane_bit = [&](uint32_t base) {
                return make_op(eOperator::shift_left, make_uint(base),
                               make_op(eOperator::bit_and, make_lane(), make_uint(31)));
            };
            const auto is_this_word = [&]() { return make_op(eOperator::equal, lane_word(), make_uint(word)); };

            ast_expression *value = nullptr;
            switch (mask.kind) {
            case 0:
                value = make_ternary(is_this_word(), lane_bit(1), make_uint(0));
                break;
            case 1: // lanes below: whole words under this one are full, this word holds (1 << bit) - 1
                value = make_ternary(
                    make_op(eOperator::greater, lane_word(), make_uint(word)), make_not(make_uint(0)),
                    make_ternary(is_this_word(), make_op(eOperator::minus, lane_bit(1), make_uint(1)), make_uint(0)));
                break;
            case 2:
                value = make_ternary(
                    make_op(eOperator::greater, lane_word(), make_uint(word)), make_not(make_uint(0)),
                    make_ternary(is_this_word(), make_op(eOperator::minus, lane_bit(2), make_uint(1)), make_uint(0)));
                break;
            case 3:
                value = make_ternary(is_this_word(), make_not(make_op(eOperator::minus, lane_bit(1), make_uint(1))),
                                     make_uint(0));
                if (word != 0) {
                    value = make_ternary(make_op(eOperator::less, lane_word(), make_uint(word)), make_not(make_uint(0)),
                                         value);
                }
                break;
            case 4:
                value = make_ternary(is_this_word(), make_not(make_op(eOperator::minus, lane_bit(2), make_uint(1))),
                                     make_uint(0));
                if (word != 0) {
                    value = make_ternary(make_op(eOperator::less, lane_word(), make_uint(word)), make_not(make_uint(0)),
                                         value);
                }
                break;
            }
            ctor->parameters.push_back(value);
        }
        return ctor;
    }

    if (strcmp(var_name, "gl_NumWorkGroups") == 0 || strcmp(var_name, "gl_NumSubgroups") == 0 ||
        strcmp(var_name, "gl_SubgroupID") == 0) {
        // No HLSL equivalent without extra payload plumbing; fail loudly instead of emitting an
        // undeclared identifier.
        assert(false && "gl_NumWorkGroups/gl_NumSubgroups/gl_SubgroupID cannot be lowered to HLSL");
    }
    return nullptr;
}

glslx::ast_expression *glslx::LowerHLSL::Rewrite_Expression(ast_expression *expr) {
    if (!expr) {
        return expr;
    }
    switch (expr->type) {
    case eExprType::ArraySubscript:
    case eExprType::FieldOrSwizzle: {
        global_vector<access_index_t> indices;
        const buf_access_t access = Find_BufferAccessExpression(expr, 0, indices);
        int array_dims = 0;
        const ast_type *leaf_type =
            access.buf_index != -1 ? Evaluate_ExpressionResultType(tu_, expr, array_dims) : nullptr;
        if (access.buf_index != -1 && Is_InlineableLeaf(leaf_type, array_dims)) {
            for (access_index_t &idx : indices) {
                idx.index = Rewrite_Expression(idx.index);
            }
            return Build_Load(access.var, Build_OffsetSum(indices, access.offset), leaf_type);
        }
        if (access.buf_index != -1 && Is_AggregateType(leaf_type, array_dims)) {
            assert(hoist_ && "aggregate buffer read where statements cannot be hoisted");
            if (hoist_) {
                return Hoist_BufferRead(access, indices, leaf_type);
            }
        }
        if (expr->type == eExprType::ArraySubscript) {
            auto *subscript = static_cast<ast_array_subscript *>(expr);
            subscript->operand = Rewrite_Expression(subscript->operand);
            subscript->index = Rewrite_Expression(subscript->index);
        } else {
            auto *field = static_cast<ast_field_or_swizzle *>(expr);
            field->operand = Rewrite_Expression(field->operand);
        }
        return expr;
    }
    case eExprType::FunctionCall: {
        auto *call = static_cast<ast_function_call *>(expr);
        const int start = g_atomic_functions.Find(call->name) ? 1 : 0;
        for (int i = start; i < int(call->parameters.size()); ++i) {
            call->parameters[i] = Rewrite_Expression(call->parameters[i]);
        }
        if (ast_expression *replacement = Rewrite_TextureImageCall(call)) {
            return replacement;
        }
        if (ast_expression *replacement = Rewrite_RayQueryCall(call)) {
            return replacement;
        }
        const bool user_function = call->func && !(call->func->attributes & eFunctionAttribute::Builtin);
        if (!user_function && !g_atomic_functions.Find(call->name) && g_hlsl_function_mapping.Find(call->name)) {
            pending_renames_.push_back(call);
        }
        if (user_function &&
            std::any_of(call->parameters.begin(), call->parameters.end(),
                        [](const ast_expression *p) { return is_combined_sampler_argument(p); }) &&
            std::find(pending_sampler_calls_.begin(), pending_sampler_calls_.end(), call) ==
                pending_sampler_calls_.end()) {
            pending_sampler_calls_.push_back(call);
        }
        return expr;
    }
    case eExprType::ConstructorCall: {
        auto *call = static_cast<ast_constructor_call *>(expr);
        // HLSL has no scalar-to-vector constructor: `vecN(s)` becomes `vecN(s.xxx..)`. The scalar check
        // must run before the parameter is rewritten, while its type can still be evaluated.
        int splat_size = 0;
        if (call->type->builtin && call->parameters.size() == 1) {
            int array_dims = 0;
            const ast_type *param_type = Evaluate_ExpressionResultType(tu_, call->parameters[0], array_dims);
            const int vec_size = get_vector_size(static_cast<const ast_builtin *>(call->type)->type);
            if (param_type && is_scalar_type(param_type) && vec_size > 1) {
                splat_size = vec_size;
            }
        }
        // GLSL fills a matrix from a scalar along the diagonal; HLSL fills in order and zero-pads,
        // so expand scalar args into the full diagonal layout (columns of `rows` args). Constant
        // scalars fold to typed literals; runtime ones go through a hoisted temporary.
        int diag_cols = 0, diag_rows = 0;
        bool diag_double = false;
        const ast_type *diag_runtime_arg_type = nullptr;
        if (!splat_size && call->type->builtin && call->parameters.size() == 1) {
            if (Get_MatrixShape(call->type, diag_cols, diag_rows, diag_double)) {
                const ast_expression *p = call->parameters[0];
                float f_val = 0.0;
                double d_val = 0.0;
                bool expandable = true;
                switch (p->type) {
                case eExprType::IntConstant:
                    f_val = static_cast<float>(static_cast<const ast_int_constant *>(p)->value);
                    d_val = static_cast<double>(static_cast<const ast_int_constant *>(p)->value);
                    break;
                case eExprType::UIntConstant:
                    f_val = static_cast<float>(static_cast<const ast_uint_constant *>(p)->value);
                    d_val = static_cast<double>(static_cast<const ast_uint_constant *>(p)->value);
                    break;
                case eExprType::FloatConstant:
                    f_val = static_cast<float>(static_cast<const ast_float_constant *>(p)->value);
                    d_val = static_cast<double>(f_val);
                    break;
                case eExprType::DoubleConstant:
                    d_val = static_cast<const ast_double_constant *>(p)->value;
                    f_val = static_cast<float>(d_val);
                    break;
                default: {
                    expandable = false;
                    int array_dims = 0;
                    const ast_type *arg_type = Evaluate_ExpressionResultType(tu_, p, array_dims);
                    if (arg_type && array_dims == 0 && arg_type->builtin) {
                        int arg_rows = 0, arg_cols = 0;
                        const auto *arg_kw = static_cast<const ast_builtin *>(arg_type);
                        if (is_scalar_numeric_type(arg_type) || get_vector_size(arg_kw->type) > 0 ||
                            Get_MatrixShape(arg_type, arg_cols, arg_rows)) {
                            diag_runtime_arg_type = arg_type;
                        }
                    }
                    break;
                }
                }
                if (expandable) {
                    for (int c = 0; c < diag_cols; ++c) {
                        for (int r = 0; r < diag_rows; ++r) {
                            ast_expression *elem = diag_double
                                                       ? static_cast<ast_expression *>(
                                                             tu_->make<ast_double_constant>(c == r ? d_val : 0.0))
                                                       : static_cast<ast_expression *>(
                                                             tu_->make<ast_float_constant>(c == r ? f_val : 0.0f));
                            const int idx = c * diag_rows + r;
                            if (idx == 0) {
                                call->parameters[0] = elem;
                            } else {
                                call->parameters.push_back(elem);
                            }
                        }
                    }
                }
            }
        }
        for (int i = 0; i < int(call->parameters.size()); ++i) {
            call->parameters[i] = Rewrite_Expression(call->parameters[i]);
        }
        if (splat_size) {
            call->parameters[0] = Build_Swizzle(tu_, call->parameters[0], std::string(splat_size, 'x').c_str());
        }
        if (diag_runtime_arg_type) {
            const eKeyword elem_kw = diag_double ? eKeyword::K_double : eKeyword::K_float;
            const auto *arg_builtin = static_cast<const ast_builtin *>(diag_runtime_arg_type);
            const bool arg_scalar = is_scalar_numeric_type(diag_runtime_arg_type);
            const int arg_vec = arg_scalar ? 0 : get_vector_size(arg_builtin->type);
            int arg_cols = 0, arg_rows = 0;
            const bool arg_mat = Get_MatrixShape(diag_runtime_arg_type, arg_cols, arg_rows);
            ast_expression *arg = call->parameters[0];
            const ast_type *arg_value_type = diag_runtime_arg_type;
            if (arg_scalar && arg_builtin->type != elem_kw) {
                auto *cast = tu_->make<ast_constructor_call>(tu_->alloc.allocator);
                cast->type = tu_->FindOrAddBuiltin(elem_kw);
                cast->parameters.push_back(arg);
                arg = cast;
                arg_value_type = tu_->FindOrAddBuiltin(elem_kw);
            }
            if (arg->type != eExprType::VariableIdentifier)
                arg = Hoist_MatrixScalar(arg, arg_value_type);
            auto *base_var = static_cast<ast_variable_identifier *>(arg)->variable;

            int diag_limit = std::min(diag_cols, diag_rows);
            if (arg_mat) {
                diag_limit = std::min(diag_limit, std::min(arg_cols, arg_rows));
            } else if (arg_vec > 0) {
                diag_limit = std::min(diag_limit, arg_vec);
            }

            call->parameters.clear();
            for (int c = 0; c < diag_cols; ++c) {
                for (int r = 0; r < diag_rows; ++r) {
                    ast_expression *elem = nullptr;
                    if (c == r && c < diag_limit) {
                        if (arg_mat) {
                            auto *column = tu_->make<ast_array_subscript>();
                            column->operand = tu_->make<ast_variable_identifier>(base_var);
                            column->index = tu_->make<ast_int_constant>(c);
                            auto *component = tu_->make<ast_array_subscript>();
                            component->operand = column;
                            component->index = tu_->make<ast_int_constant>(r);
                            elem = component;
                        } else if (arg_vec > 0) {
                            auto *component = tu_->make<ast_array_subscript>();
                            component->operand = tu_->make<ast_variable_identifier>(base_var);
                            component->index = tu_->make<ast_int_constant>(c);
                            elem = component;
                        } else {
                            elem = tu_->make<ast_variable_identifier>(base_var);
                        }
                    }
                    if (!elem) {
                        elem = diag_double ? static_cast<ast_expression *>(tu_->make<ast_double_constant>(0.0))
                                           : static_cast<ast_expression *>(tu_->make<ast_float_constant>(0.0f));
                    }
                    call->parameters.push_back(elem);
                }
            }
        }
        return expr;
    }
    case eExprType::MethodCall: {
        auto *call = static_cast<ast_method_call *>(expr);
        call->object = Rewrite_Expression(call->object);
        for (int i = 0; i < int(call->parameters.size()); ++i) {
            call->parameters[i] = Rewrite_Expression(call->parameters[i]);
        }
        return expr;
    }
    case eExprType::PostIncrement:
    case eExprType::PostDecrement:
    case eExprType::UnaryMinus:
    case eExprType::UnaryPlus:
    case eExprType::BitNot:
    case eExprType::LogicalNot:
    case eExprType::PrefixIncrement:
    case eExprType::PrefixDecrement: {
        auto *un = static_cast<ast_unary_expression *>(expr);
        un->operand = Rewrite_Expression(un->operand);
        return expr;
    }
    case eExprType::Sequence:
    case eExprType::Assign: {
        auto *bin = static_cast<ast_binary_expression *>(expr);
        bin->operand1 = Rewrite_Expression(bin->operand1);
        bin->operand2 = Rewrite_Expression(bin->operand2);
        return expr;
    }
    case eExprType::Operation: {
        auto *op = static_cast<ast_operation_expression *>(expr);
        if (op->oper == eOperator::multiply) {
            int array_dims1 = 0, array_dims2 = 0;
            const ast_type *op1_type = Evaluate_ExpressionResultType(tu_, op->operand1, array_dims1);
            const ast_type *op2_type = Evaluate_ExpressionResultType(tu_, op->operand2, array_dims2);
            if (op1_type && op2_type && is_matrix_type(op1_type) && get_vector_size(op2_type) > 1) {
                // HLSL's `mul` takes the vector/matrix operands in the opposite order from GLSL's `*`.
                auto *call = tu_->make<ast_function_call>(tu_->alloc.allocator);
                call->name = tu_->makestr("mul");
                call->parameters.push_back(Rewrite_Expression(op->operand2));
                call->parameters.push_back(Rewrite_Expression(op->operand1));
                return call;
            }
        }
        op->operand1 = Rewrite_Expression(op->operand1);
        op->operand2 = Rewrite_Expression(op->operand2);
        return expr;
    }
    case eExprType::Ternary: {
        auto *ternary = static_cast<ast_ternary_expression *>(expr);
        ternary->condition = Rewrite_Expression(ternary->condition);
        ternary->on_true = Rewrite_Expression(ternary->on_true);
        ternary->on_false = Rewrite_Expression(ternary->on_false);
        return expr;
    }
    case eExprType::ArraySpecifier: {
        auto *arr_specifier = static_cast<ast_array_specifier *>(expr);
        for (int i = 0; i < int(arr_specifier->expressions.size()); ++i) {
            arr_specifier->expressions[i] = Rewrite_Expression(arr_specifier->expressions[i]);
        }
        return expr;
    }
    case eExprType::VariableIdentifier: {
        const auto *ident = static_cast<const ast_variable_identifier *>(expr);
        for (const ray_query_constant_t &c : g_ray_query_constants) {
            if (strcmp(ident->variable->name, c.glsl_name) != 0) {
                continue;
            }
            // Tree-detached synthetic variable (same technique as Build_SamplerRef) -- only its
            // name is ever printed, so its base_type is irrelevant.
            auto *enum_var = tu_->make<ast_function_variable>(tu_->alloc.allocator);
            enum_var->base_type = tu_->FindOrAddBuiltin(eKeyword::K_uint);
            enum_var->name = tu_->makestr(c.hlsl_name);
            return tu_->make<ast_variable_identifier>(enum_var);
        }
        if (ast_expression *r = Rewrite_GlSpecial(ident->variable->name)) {
            return r;
        }
        return expr;
    }
    default:
        return expr;
    }
}

void glslx::LowerHLSL::Rewrite_SimpleStatement(ast_simple_statement *stmt, global_vector<ast_statement *> &pre) {
    if (!stmt) {
        return;
    }
    if (stmt->type == eStatement::Declaration) {
        auto *decl = static_cast<ast_declaration_statement *>(stmt);
        for (ast_function_variable *v : decl->variables) {
            if (v->initial_value) {
                v->initial_value = Rewrite_Hoisting(Convert_StructInit(tu_, v->initial_value), pre);
            }
        }
    } else if (stmt->type == eStatement::Expression) {
        auto *expr_stmt = static_cast<ast_expression_statement *>(stmt);
        expr_stmt->expression = Rewrite_Hoisting(expr_stmt->expression, pre);
    }
}

glslx::ast_expression *glslx::LowerHLSL::Hoist_Atomic(ast_function_call *call, global_vector<ast_statement *> &pre) {
    int array_dims = 0;
    const ast_type *result_type = Evaluate_ExpressionResultType(tu_, call, array_dims);
    if (!result_type) {
        assert(false && "cannot type an atomic call result");
        return call;
    }

    const int index = temp_index_++;

    auto *temp_var = tu_->make<ast_function_variable>(tu_->alloc.allocator);
    temp_var->base_type = const_cast<ast_type *>(result_type);
    temp_var->name = tu_->makestr(("__temp" + std::to_string(index)).c_str());

    auto *temp_decl = tu_->make<ast_declaration_statement>(tu_->alloc.allocator);
    temp_decl->variables.push_back(temp_var);
    pre.push_back(temp_decl);

    std::string func_name = call->name;
    const auto *p_find = g_hlsl_function_mapping.Find(call->name);
    if (p_find) {
        func_name = *p_find;
    }
    if (func_name == "InterlockedCompareExchange" && result_type->builtin &&
        static_cast<const ast_builtin *>(result_type)->type == eKeyword::K_uint64_t) {
        func_name = "InterlockedCompareExchange64";
    }

    ast_expression *atomic_call = nullptr;
    global_vector<access_index_t> indices;
    const buf_access_t access = Find_BufferAccessExpression(call->parameters[0], 0, indices);
    if (access.buf_index != -1) {
        int leaf_dims = 0;
        const ast_type *leaf_type = Evaluate_ExpressionResultType(tu_, call->parameters[0], leaf_dims);
        assert(leaf_type && leaf_dims == 0 && leaf_type->builtin && "atomic buffer target must be a scalar leaf");

        for (access_index_t &idx : indices) {
            idx.index = Rewrite_Expression(idx.index);
        }

        auto *method_call = tu_->make<ast_method_call>(tu_->alloc.allocator);
        method_call->object = tu_->make<ast_variable_identifier>(access.var);
        method_call->name = tu_->makestr(func_name.c_str());
        method_call->parameters.push_back(Build_OffsetSum(indices, access.offset));

        for (int j = 1; j < int(call->parameters.size()); ++j) {
            method_call->parameters.push_back(call->parameters[j]);
        }
        method_call->parameters.push_back(tu_->make<ast_variable_identifier>(temp_var));
        atomic_call = method_call;
    }
    if (!atomic_call) {
        auto *func_call = tu_->make<ast_function_call>(tu_->alloc.allocator);
        func_call->name = tu_->makestr(func_name.c_str());
        for (ast_expression *p : call->parameters) {
            func_call->parameters.push_back(p);
        }
        func_call->parameters.push_back(tu_->make<ast_variable_identifier>(temp_var));
        atomic_call = func_call;
    }

    pre.push_back(tu_->make<ast_expression_statement>(atomic_call));

    return tu_->make<ast_variable_identifier>(temp_var);
}

glslx::ast_expression *glslx::LowerHLSL::Lower_Atomics(ast_expression *expr, global_vector<ast_statement *> &pre) {
    if (!expr) {
        return expr;
    }
    switch (expr->type) {
    case eExprType::VariableIdentifier:
        Flush_SharedCoord(static_cast<const ast_variable_identifier *>(expr)->variable, pre);
        return expr;
    case eExprType::FunctionCall: {
        auto *call = static_cast<ast_function_call *>(expr);
        const bool atomic = g_atomic_functions.Find(call->name) != nullptr;
        for (int i = atomic ? 1 : 0; i < int(call->parameters.size()); ++i) {
            call->parameters[i] = Lower_Atomics(call->parameters[i], pre);
        }
        if (atomic) {
            return Hoist_Atomic(call, pre);
        }
        if (strcmp(call->name, "textureSize") == 0) {
            return Hoist_TextureSize(call, pre);
        }
        break;
    }
    case eExprType::ConstructorCall: {
        auto *call = static_cast<ast_constructor_call *>(expr);
        for (int i = 0; i < int(call->parameters.size()); ++i) {
            call->parameters[i] = Lower_Atomics(call->parameters[i], pre);
        }
        if (!call->type->builtin) {
            auto *struct_var = tu_->make<ast_function_variable>(tu_->alloc.allocator);
            struct_var->base_type = const_cast<ast_type *>(call->type);
            struct_var->name = tu_->makestr(("__struct" + std::to_string(temp_index_++)).c_str());
            struct_var->initial_value = Build_StructInit(tu_, call);

            auto *struct_decl = tu_->make<ast_declaration_statement>(tu_->alloc.allocator);
            struct_decl->variables.push_back(struct_var);
            pre.push_back(struct_decl);
            return tu_->make<ast_variable_identifier>(struct_var);
        }
        break;
    }
    case eExprType::MethodCall: {
        auto *call = static_cast<ast_method_call *>(expr);
        call->object = Lower_Atomics(call->object, pre);
        for (int i = 0; i < int(call->parameters.size()); ++i) {
            call->parameters[i] = Lower_Atomics(call->parameters[i], pre);
        }
        break;
    }
    case eExprType::ArraySubscript: {
        auto *subscript = static_cast<ast_array_subscript *>(expr);
        subscript->operand = Lower_Atomics(subscript->operand, pre);
        subscript->index = Lower_Atomics(subscript->index, pre);
        break;
    }
    case eExprType::FieldOrSwizzle: {
        auto *field = static_cast<ast_field_or_swizzle *>(expr);
        field->operand = Lower_Atomics(field->operand, pre);
        break;
    }
    case eExprType::PostIncrement:
    case eExprType::PostDecrement:
    case eExprType::UnaryMinus:
    case eExprType::UnaryPlus:
    case eExprType::BitNot:
    case eExprType::LogicalNot:
    case eExprType::PrefixIncrement:
    case eExprType::PrefixDecrement: {
        auto *un = static_cast<ast_unary_expression *>(expr);
        un->operand = Lower_Atomics(un->operand, pre);
        break;
    }
    case eExprType::Assign: {
        auto *assign = static_cast<ast_assignment_expression *>(expr);
        assign->operand1 = Lower_Atomics(assign->operand1, pre);
        assign->operand2 = Lower_Atomics(assign->operand2, pre);
        break;
    }
    case eExprType::Sequence: {
        auto *sequence = static_cast<ast_sequence_expression *>(expr);
        sequence->operand1 = Lower_Atomics(sequence->operand1, pre);
        sequence->operand2 = Lower_Atomics(sequence->operand2, pre);
        break;
    }
    case eExprType::Operation: {
        auto *operation = static_cast<ast_operation_expression *>(expr);
        if (operation->oper == eOperator::logical_and || operation->oper == eOperator::logical_or) {
            return Lower_LogicalOperation(operation, pre);
        }
        operation->operand1 = Lower_Atomics(operation->operand1, pre);
        operation->operand2 = Lower_Atomics(operation->operand2, pre);
        break;
    }
    case eExprType::Ternary: {
        auto *ternary = static_cast<ast_ternary_expression *>(expr);
        ternary->condition = Lower_Atomics(ternary->condition, pre);

        const uint32_t true_mark = pre.size();
        ternary->on_true = Lower_Atomics(ternary->on_true, pre);
        const uint32_t false_mark = pre.size();
        ternary->on_false = Lower_Atomics(ternary->on_false, pre);
        if (true_mark == false_mark && pre.size() == false_mark) {
            break;
        }
        return Lower_GuardedTernary(ternary, pre, true_mark, false_mark);
    }
    case eExprType::ArraySpecifier: {
        auto *arr_specifier = static_cast<ast_array_specifier *>(expr);
        for (int i = 0; i < int(arr_specifier->expressions.size()); ++i) {
            arr_specifier->expressions[i] = Lower_Atomics(arr_specifier->expressions[i], pre);
        }
        break;
    }
    default:
        break;
    }
    return expr;
}

// GLSL skips the right operand of `&&` / `||` once the left one decides the result, so statements
// hoisted out of that operand move behind a test of the left operand's value.
glslx::ast_expression *glslx::LowerHLSL::Lower_LogicalOperation(ast_operation_expression *operation,
                                                                global_vector<ast_statement *> &pre) {
    operation->operand1 = Lower_Atomics(operation->operand1, pre);

    const uint32_t mark = pre.size();
    operation->operand2 = Lower_Atomics(operation->operand2, pre);
    if (pre.size() == mark) {
        return operation;
    }

    auto *flag_var = tu_->make<ast_function_variable>(tu_->alloc.allocator);
    flag_var->base_type = tu_->FindOrAddBuiltin(eKeyword::K_bool);
    flag_var->name = tu_->makestr(("__sc" + std::to_string(temp_index_++)).c_str());
    flag_var->initial_value = operation->operand1;

    auto *flag_decl = tu_->make<ast_declaration_statement>(tu_->alloc.allocator);
    flag_decl->variables.push_back(flag_var);
    pre.insert(pre.begin() + mark, flag_decl);

    ast_expression *flag_ref = tu_->make<ast_variable_identifier>(flag_var);
    if (operation->oper == eOperator::logical_or) {
        flag_ref = tu_->make<ast_unary_logical_not_expression>(flag_ref);
    }
    auto *guard = tu_->make<ast_if_statement>(Bitmask<eCtrlFlowAttribute>{});
    guard->condition = flag_ref;
    guard->then_statement = Take_Block(pre, mark + 1, Build_Assignment(flag_var, operation->operand2));
    pre.push_back(guard);

    return tu_->make<ast_variable_identifier>(flag_var);
}

// One `?:` arm is skipped, so its hoisted statements move into the arm that runs while the arms'
// values are captured into a temporary that replaces the ternary.
glslx::ast_expression *glslx::LowerHLSL::Lower_GuardedTernary(ast_ternary_expression *ternary,
                                                              global_vector<ast_statement *> &pre,
                                                              const uint32_t true_mark, const uint32_t false_mark) {
    int array_dims = 0;
    const ast_type *result_type = Evaluate_ExpressionResultType(tu_, ternary, array_dims);
    if (!result_type || array_dims != 0) {
        assert(false && "cannot type the result of a guarded ternary");
        return ternary;
    }

    auto *result_var = tu_->make<ast_function_variable>(tu_->alloc.allocator);
    result_var->base_type = const_cast<ast_type *>(result_type);
    result_var->name = tu_->makestr(("__tern" + std::to_string(temp_index_++)).c_str());

    auto *result_decl = tu_->make<ast_declaration_statement>(tu_->alloc.allocator);
    result_decl->variables.push_back(result_var);
    pre.insert(pre.begin() + true_mark, result_decl);

    auto *if_stmt = tu_->make<ast_if_statement>(Bitmask<eCtrlFlowAttribute>{});
    if_stmt->condition = ternary->condition;
    if_stmt->else_statement = Take_Block(pre, false_mark + 1, Build_Assignment(result_var, ternary->on_false));
    if_stmt->then_statement = Take_Block(pre, true_mark + 1, Build_Assignment(result_var, ternary->on_true));
    pre.push_back(if_stmt);

    return tu_->make<ast_variable_identifier>(result_var);
}

glslx::ast_statement *glslx::LowerHLSL::Build_ExitCheck(ast_expression *cond) {
    auto *exit_check = tu_->make<ast_if_statement>(Bitmask<eCtrlFlowAttribute>{});
    exit_check->condition = tu_->make<ast_unary_logical_not_expression>(cond);
    exit_check->then_statement = tu_->make<ast_break_statement>();
    return exit_check;
}

// Detaches the sink statements [from, end) into a compound statement, with `tail` appended last.
glslx::ast_compound_statement *glslx::LowerHLSL::Take_Block(global_vector<ast_statement *> &pre, const uint32_t from,
                                                            ast_statement *tail) {
    auto *block = tu_->make<ast_compound_statement>(tu_->alloc.allocator);
    block->statements.insert(block->statements.begin(), pre.begin() + from, pre.end());
    pre.resize(from);
    if (tail) {
        block->statements.push_back(tail);
    }
    return block;
}

glslx::ast_expression_statement *glslx::LowerHLSL::Build_Assignment(ast_variable *target, ast_expression *value) {
    auto *assign = tu_->make<ast_assignment_expression>(eOperator::assign);
    assign->operand1 = tu_->make<ast_variable_identifier>(target);
    assign->operand2 = value;
    return tu_->make<ast_expression_statement>(assign);
}

// Appends `if (!first) { guarded } first = false;` to `out` and declares `first` in `decls`, so that
// `continue` (which jumps to the loop top) still runs the guarded statements.
void glslx::LowerHLSL::Build_SkipFirst(global_vector<ast_statement *> &decls, global_vector<ast_statement *> &guarded,
                                       global_vector<ast_statement *> &out) {
    auto *first_var = tu_->make<ast_function_variable>(tu_->alloc.allocator);
    first_var->base_type = tu_->FindOrAddBuiltin(eKeyword::K_bool);
    first_var->name = tu_->makestr(("__first" + std::to_string(temp_index_++)).c_str());
    first_var->initial_value = tu_->make<ast_bool_constant>(true);

    auto *first_decl = tu_->make<ast_declaration_statement>(tu_->alloc.allocator);
    first_decl->variables.push_back(first_var);
    decls.push_back(first_decl);

    auto *guard = tu_->make<ast_if_statement>(Bitmask<eCtrlFlowAttribute>{});
    guard->condition = tu_->make<ast_unary_logical_not_expression>(tu_->make<ast_variable_identifier>(first_var));
    auto *guarded_block = tu_->make<ast_compound_statement>(tu_->alloc.allocator);
    for (ast_statement *s : guarded) {
        guarded_block->statements.push_back(s);
    }
    guard->then_statement = guarded_block;
    out.push_back(guard);

    auto *clear = tu_->make<ast_assignment_expression>(eOperator::assign);
    clear->operand1 = tu_->make<ast_variable_identifier>(first_var);
    clear->operand2 = tu_->make<ast_bool_constant>(false);
    out.push_back(tu_->make<ast_expression_statement>(clear));
}

glslx::ast_statement *glslx::LowerHLSL::Wrap_LoopBody(global_vector<ast_statement *> &pre, ast_expression *cond,
                                                      ast_statement *body) {
    if (cond) {
        pre.push_back(Build_ExitCheck(cond));
    }
    if (body->type == eStatement::Compound) {
        const auto *inner = static_cast<const ast_compound_statement *>(body);
        for (ast_statement *s : inner->statements) {
            pre.push_back(s);
        }
    } else {
        pre.push_back(body);
    }

    auto *compound = tu_->make<ast_compound_statement>(tu_->alloc.allocator);
    compound->statements.insert(compound->statements.begin(), pre.begin(), pre.end());
    return compound;
}

void glslx::LowerHLSL::Lower_StatementReplacements(ast_statement *statement,
                                                   global_vector<ast_statement *> &out_statements) {
    switch (statement->type) {
    case eStatement::Compound: {
        auto *compound = static_cast<ast_compound_statement *>(statement);
        Lower_StatementList(compound->statements);
        break;
    }
    case eStatement::If: {
        auto *if_stmt = static_cast<ast_if_statement *>(statement);
        if_stmt->condition = Rewrite_Hoisting(if_stmt->condition, out_statements);
        Lower_Slot(if_stmt->then_statement);
        if (if_stmt->else_statement) {
            Lower_Slot(if_stmt->else_statement);
        }
        break;
    }
    case eStatement::Switch: {
        auto *switch_stmt = static_cast<ast_switch_statement *>(statement);
        switch_stmt->expression = Rewrite_Hoisting(switch_stmt->expression, out_statements);
        Lower_StatementList(switch_stmt->statements);
        break;
    }
    case eStatement::While: {
        auto *while_stmt = static_cast<ast_while_statement *>(statement);
        global_vector<ast_statement *> pre;
        ast_expression *cond = nullptr;
        if (while_stmt->condition && while_stmt->condition->type == eStatement::Expression) {
            auto *cond_stmt = static_cast<ast_expression_statement *>(while_stmt->condition);
            cond = cond_stmt->expression = Rewrite_Hoisting(cond_stmt->expression, pre);
        } else if (while_stmt->condition && while_stmt->condition->type == eStatement::Declaration) {
            auto *cond_decl = static_cast<ast_declaration_statement *>(while_stmt->condition);
            assert(cond_decl->variables.size() == 1 && cond_decl->variables[0]->initial_value);
            ast_function_variable *cond_var = cond_decl->variables[0];
            cond_var->initial_value = Rewrite_Hoisting(cond_var->initial_value, pre);
            pre.push_back(cond_decl);
            cond = tu_->make<ast_variable_identifier>(cond_var);
        } else {
            Rewrite_SimpleStatement(while_stmt->condition, pre);
        }
        Lower_Slot(while_stmt->body);
        if (!pre.empty()) {
            while_stmt->body = Wrap_LoopBody(pre, cond, while_stmt->body);
            while_stmt->condition = tu_->make<ast_expression_statement>(tu_->make<ast_bool_constant>(true));
        }
        break;
    }
    case eStatement::Do: {
        auto *do_stmt = static_cast<ast_do_statement *>(statement);
        global_vector<ast_statement *> guarded;
        do_stmt->condition = Rewrite_Hoisting(do_stmt->condition, guarded);
        Lower_Slot(do_stmt->body);
        if (!guarded.empty()) {
            guarded.push_back(Build_ExitCheck(do_stmt->condition));
            global_vector<ast_statement *> pre;
            Build_SkipFirst(out_statements, guarded, pre);
            do_stmt->body = Wrap_LoopBody(pre, nullptr, do_stmt->body);
            do_stmt->condition = tu_->make<ast_bool_constant>(true);
        }
        break;
    }
    case eStatement::For: {
        auto *for_stmt = static_cast<ast_for_statement *>(statement);
        Rewrite_SimpleStatement(for_stmt->init, out_statements);
        global_vector<ast_statement *> cond_pre, step_pre;
        for_stmt->condition = Rewrite_Hoisting(for_stmt->condition, cond_pre);
        if (for_stmt->loop) {
            for_stmt->loop = Rewrite_Hoisting(for_stmt->loop, step_pre);
        }
        Lower_Slot(for_stmt->body);
        if (!step_pre.empty()) {
            global_vector<ast_statement *> pre;
            step_pre.push_back(tu_->make<ast_expression_statement>(for_stmt->loop));
            Build_SkipFirst(out_statements, step_pre, pre);
            pre.insert(pre.end(), cond_pre.begin(), cond_pre.end());
            for_stmt->body = Wrap_LoopBody(pre, for_stmt->condition, for_stmt->body);
            for_stmt->condition = nullptr;
            for_stmt->loop = nullptr;
        } else if (!cond_pre.empty()) {
            for_stmt->body = Wrap_LoopBody(cond_pre, for_stmt->condition, for_stmt->body);
            for_stmt->condition = nullptr;
        }
        break;
    }
    case eStatement::Expression: {
        auto *expr_stmt = static_cast<ast_expression_statement *>(statement);
        hoist_ = &out_statements;
        const bool lowered =
            Try_Lower_Store(expr_stmt, out_statements) || Try_Lower_RayQueryInit(expr_stmt, out_statements);
        if (!lowered) {
            expr_stmt->expression = Rewrite_Expression(expr_stmt->expression);
        }
        hoist_ = nullptr;
        if (lowered) {
            return;
        }
        expr_stmt->expression = Lower_Atomics(expr_stmt->expression, out_statements);
        break;
    }
    case eStatement::Declaration: {
        auto *decl = static_cast<ast_declaration_statement *>(statement);
        for (ast_function_variable *v : decl->variables) {
            if (v->initial_value) {
                v->initial_value = Rewrite_Hoisting(Convert_StructInit(tu_, v->initial_value), out_statements);
            }
        }
        break;
    }
    case eStatement::Return: {
        auto *ret_stmt = static_cast<ast_return_statement *>(statement);
        if (ret_stmt->expression) {
            ret_stmt->expression = Rewrite_Hoisting(ret_stmt->expression, out_statements);
        }
        break;
    }
    default:
        break;
    }
    out_statements.push_back(statement);
}

void glslx::LowerHLSL::Lower_StatementList(local_vector<ast_statement *> &statements) {
    global_vector<ast_statement *> new_list;
    for (ast_statement *stmt : statements) {
        Lower_StatementReplacements(stmt, new_list);
    }
    statements.clear();
    statements.insert(statements.begin(), new_list.begin(), new_list.end());
}

void glslx::LowerHLSL::Lower_Slot(ast_statement *&slot) {
    if (!slot) {
        return;
    }
    global_vector<ast_statement *> replacements;
    Lower_StatementReplacements(slot, replacements);
    if (replacements.size() == 1) {
        slot = replacements[0];
    } else {
        auto *compound = tu_->make<ast_compound_statement>(tu_->alloc.allocator);
        compound->statements.insert(compound->statements.begin(), replacements.begin(), replacements.end());
        slot = compound;
    }
}

// Materializes the compute entry prologue (static gl_* globals, GLSLX_Input with SV semantics, copy-in).
// Runs after all expression lowering; the synthesized calls bypass renames.
void glslx::LowerHLSL::Lower_ComputePrologue(TrUnit *tu) {
    auto *p_find = tu->functions_by_name.Find("main");
    if (!p_find || p_find->empty()) {
        return;
    }
    ast_function *main_function = (*p_find)[0];

    struct prologue_var_t {
        eKeyword keyword;
        const char *name;
        const char *semantic;       // GLSLX_Input field semantics, null for the wave intrinsics
        const char *wave_intrinsic; // RHS when there is no input field
    };
    static const prologue_var_t vars[] = {
        {eKeyword::K_uvec3, "gl_WorkGroupID", "SV_GroupID", nullptr},
        {eKeyword::K_uvec3, "gl_LocalInvocationID", "SV_GroupThreadID", nullptr},
        {eKeyword::K_uvec3, "gl_GlobalInvocationID", "SV_DispatchThreadID", nullptr},
        {eKeyword::K_uint, "gl_LocalInvocationIndex", "SV_GroupIndex", nullptr},
        {eKeyword::K_uint, "gl_SubgroupSize", nullptr, "WaveGetLaneCount"},
        {eKeyword::K_uint, "gl_SubgroupInvocationID", nullptr, "WaveGetLaneIndex"},
    };

    ast_global_variable *globals[std::size(vars)] = {};
    ast_struct *input_struct = tu->make<ast_struct>(tu->alloc.allocator);
    input_struct->name = tu->makestr("GLSLX_Input");
    for (int i = 0; i < int(std::size(vars)); ++i) {
        const prologue_var_t &v = vars[i];
        ast_global_variable *global = nullptr;
        for (ast_global_variable *g : tu->globals) {
            if (strcmp(g->name, v.name) == 0) {
                global = g;
                break;
            }
        }
        if (!global) {
            // Prune may have dropped an unreferenced hidden global; synthesize whatever the prologue needs.
            global = tu->make<ast_global_variable>(tu->alloc.allocator);
            global->base_type = tu->FindOrAddBuiltin(v.keyword);
            global->name = tu->makestr(v.name);
            tu->globals.push_back(global);
        }
        global->storage = eStorage::None; // Write_GlobalVariable prints "static" for storage-less globals
        global->precision = ePrecision::None;
        global->flags = global->flags & ~Bitmask<eVariableFlags>(eVariableFlags::Hidden);
        globals[i] = global;

        if (!v.semantic) {
            continue;
        }
        auto *field = tu->make<ast_variable>(eVariableType::Field, tu->alloc.allocator);
        field->base_type = tu->FindOrAddBuiltin(v.keyword);
        field->name = tu->makestr(v.name);
        field->semantic = tu->makestr(v.semantic);
        input_struct->fields.push_back(field);
    }
    tu->structures.push_back(input_struct);
    tu->structures_by_name.Insert(input_struct->name, input_struct);

    auto *input_param = tu->make<ast_function_parameter>(tu->alloc.allocator);
    input_param->base_type = input_struct;
    input_param->name = tu->makestr("glslx_input");
    main_function->parameters.insert(main_function->parameters.begin(), input_param);

    for (int i = 0; i < int(std::size(vars)); ++i) {
        ast_expression *value = nullptr;
        if (vars[i].semantic) {
            auto *field = tu->make<ast_field_or_swizzle>();
            field->operand = tu->make<ast_variable_identifier>(input_param);
            field->name = tu->makestr(vars[i].name);
            value = field;
        } else {
            auto *call = tu->make<ast_function_call>(tu->alloc.allocator);
            call->name = tu->makestr(vars[i].wave_intrinsic);
            value = call;
        }
        main_function->statements.insert(main_function->statements.begin() + i, Build_Assignment(globals[i], value));
    }
}

void glslx::LowerHLSL::Apply(TrUnit *tu) {
    tu_ = tu;
    bufs_ = Collect_ByteaddressBuffers(tu);
    temp_index_ = 0;
    pending_renames_.clear();
    pending_sampler_calls_.clear();
    shared_coords_.clear();
    for (ast_function *f : tu->functions) {
        Lower_StatementList(f->statements);
    }
    assert(shared_coords_.empty() && "shared texture coordinate never reached the lowering pass");
    shared_coords_.clear();
    for (ast_global_variable *g : tu->globals) {
        g->initial_value = Convert_StructInit(tu, g->initial_value);
    }
    // Deferred like renames: adding parameters would change how calls resolve during type evaluation.
    for (ast_function_call *call : pending_sampler_calls_) {
        for (int i = 0; i < int(call->parameters.size()); ++i) {
            if (is_combined_sampler_argument(call->parameters[i])) {
                ast_expression *sampler = Build_SamplerRef(tu, call->parameters[i]);
                assert(sampler && "combined-sampler argument without a companion sampler");
                call->parameters.insert(call->parameters.begin() + (++i), sampler);
            }
        }
    }
    pending_sampler_calls_.clear();
    for (int i = 0; i < int(tu->globals.size()); ++i) {
        const ast_global_variable *tex = tu->globals[i];
        if (!is_combined_texturesampler(tex->base_type)) {
            continue;
        }
        auto *sampler = tu->make<ast_global_variable>(tu->alloc.allocator);
        sampler->base_type = tu->FindOrAddBuiltin(Companion_SamplerKeyword(tex->base_type));
        sampler->name = tu->makestr((std::string(tex->name) + "_sampler").c_str());
        sampler->flags = tex->flags;
        sampler->storage = tex->storage;
        sampler->aux_storage = tex->aux_storage;
        for (ast_constant_expression *size : tex->array_sizes) {
            sampler->array_sizes.push_back(size);
        }
        for (ast_layout_qualifier *qualifier : tex->layout_qualifiers) {
            sampler->layout_qualifiers.push_back(qualifier);
        }
        tu->globals.insert(tu->globals.begin() + (++i), sampler);
    }
    for (ast_function *f : tu->functions) {
        if (f->attributes & eFunctionAttribute::Builtin) {
            continue;
        }
        for (int i = 0; i < int(f->parameters.size()); ++i) {
            const ast_function_parameter *param = f->parameters[i];
            if (!is_combined_texturesampler(param->base_type)) {
                continue;
            }
            auto *sampler = tu->make<ast_function_parameter>(tu->alloc.allocator);
            sampler->base_type = tu->FindOrAddBuiltin(Companion_SamplerKeyword(param->base_type));
            sampler->flags = param->flags;
            sampler->qualifiers = param->qualifiers;
            for (ast_constant_expression *size : param->array_sizes) {
                sampler->array_sizes.push_back(size);
            }
            if (param->name) {
                sampler->name = tu->makestr((std::string(param->name) + "_sampler").c_str());
            }
            f->parameters.insert(f->parameters.begin() + (++i), sampler);
        }
    }
    // Renamed last: result-type evaluation looks calls up by their GLSL name.
    for (ast_function_call *call : pending_renames_) {
        if (const auto *mapped = g_hlsl_function_mapping.Find(call->name)) {
            call->name = tu->makestr(mapped->c_str());
        }
    }
    pending_renames_.clear();
    if (tu->type == eTrUnitType::Compute) {
        Lower_ComputePrologue(tu);
    }
}
