#pragma once

#include <cstdlib>

#include <vector>

#include "../Bitmask.h"
#include "../HashMap32.h"
#include "../Span.h"
#include "Lexer.h"

namespace glslx {
template <typename T> static void ast_destroy(MultiPoolAllocator<char> &alloc, void *self, size_t size) {
    ((T *)self)->~T();
    alloc.deallocate((char *)self, size);
}

struct ast_node_base {
    int8_t gc = 0;
};

struct ast_memory {
    ast_node_base *data = nullptr;
    size_t size = 0;
    void (*dtor)(MultiPoolAllocator<char> &, void *, size_t) = nullptr;

    ast_memory() noexcept = default;
    template <typename T> ast_memory(T *_data, size_t _size) : data(_data), size(_size), dtor(&ast_destroy<T>) {}
    void destroy(MultiPoolAllocator<char> &alloc) const { dtor(alloc, data, size); }
};

struct ast_allocator {
    MultiPoolAllocator<char> allocator;
    global_vector<ast_memory> allocations;

    ast_allocator() : allocator(8, 128) {}
};

#define OPERATOR_NEW(Type)                                                                                             \
    void *operator new(const size_t size, ast_allocator *allocator) noexcept {                                         \
        char *data = allocator->allocator.allocate(size);                                                              \
        if (data) {                                                                                                    \
            allocator->allocations.push_back(ast_memory{(Type *)data, size});                                          \
        }                                                                                                              \
        return data;                                                                                                   \
    }

template <typename T> struct ast_node : public ast_node_base {
    OPERATOR_NEW(T)
    void operator delete(void *, ast_allocator *) {}

    void *operator new(size_t) = delete;
    void operator delete(void *) = delete;
};

// https://registry.khronos.org/OpenGL/specs/gl/GLSLangSpec.4.60.html#storage-qualifiers
enum class eStorage : int8_t {
    None = -1, // local read/write memory, or an input parameter to a function
    Const,     // a variable whose value cannot be changed
    In,        // linkage into a shader from a previous stage, variable is copied in
    Out,       // linkage out of a shader to a subsequent stage, variable is copied out
    Attribute, // compatibility profile only and vertex language only; same as in when in a vertex shader
    Uniform, // value does not change across the primitive being processed, uniforms form the linkage between a shader,
             // API, and the application
    Varying, // compatibility profile only and vertex and fragment languages only; same as out when in a vertex shader
             // and same as in when in a fragment shader
    Buffer,  // value is stored in a buffer object, and can be read or written both by shader invocations and the API
    Shared,  // compute shader only; variable storage is shared across all work items in a workgroup
    RayPayload, // Ray generation, closest-hit, and miss shader only. Storage that becomes associated with the incoming
                // ray payload when it's location layout qualifier is passed as an input to traceRayEXT()
    RayPayloadIn, // Closest-hit, any-hit and miss shader only. Storage associated with the incoming ray payload that
                  // can be read to and written in the stages invoked by traceRayEXT()
    HitAttribute, // Intersection, any-hit, and closest-hit shader only. Storage associated with attributes of geometry
                  // intersected by a ray
    CallableData, // Ray generation, closest-hit, miss and callable shader only. Storage that becomes associated with
                  // the incoming callable data when it's location layout qualifier is passed as an input to
                  // executeCallableEXT()
    CallableDataIn, // Callable shader only. Storage associated with the incoming callable data can be read to or
                    // written in the callable stage
};

enum class eAuxStorage : int8_t {
    None = -1,
    Centroid, // centroid-based interpolation
    Sample,   // per-sample interpolation
    Patch     // per-tessellation-patch attributes
};

// https://registry.khronos.org/OpenGL/specs/gl/GLSLangSpec.4.60.html#interpolation-qualifiers
enum class eInterpolation : int8_t {
    None = -1,
    Smooth,       // perspective correct interpolation
    Flat,         // no interpolation
    Noperspective // linear interpolation
};

// https://registry.khronos.org/OpenGL/specs/gl/GLSLangSpec.4.60.html#precision-and-precision-qualifiers
enum class ePrecision : int8_t {
    None = -1,
    Highp,   // 32-bit two�s complement for integers, 32-bit IEEE 754 floating-point for float
    Mediump, // SPIR-V RelaxedPrecision when targeting Vulkan, otherwise none.
    Lowp     // SPIR-V RelaxedPrecision when targeting Vulkan, otherwise none.
};

// https://registry.khronos.org/OpenGL/specs/gl/GLSLangSpec.4.60.html#memory-qualifiers
enum class eMemory : uint8_t {
    Coherent, // memory variable where reads and writes are coherent with reads and writes from other shader invocations
    Volatile, // memory variable whose underlying value may be changed at any point during shader execution by some
              // source other than the current shader invocation
    Restrict, // memory variable where use of that variable is the only way to read and write the underlying memory in
              // the relevant shader stage
    Readonly, // memory variable that can be used to read the underlying memory, but cannot be used to write the
              // underlying memory
    Writeonly // memory variable that can be used to write the underlying memory, but cannot be used to read the
              // underlying memory
};

enum class eParamQualifier : uint8_t {
    None,
    Const, // for function parameters that cannot be written to
    In,    // for function parameters passed into a function
    Out,   // for function parameters passed back out of a function, but not initialized for use when passed in
    Inout  // for function parameters passed both into and out of a function
};

enum class eCtrlFlowAttribute : uint16_t {
    // loop attributes
    Unroll,
    DontUnroll,
    Loop = DontUnroll,
    DependencyInfinite,
    DependencyLength,
    MinIterations,     // the loop executes at least a given number of iterations
    MaxIterations,     // the loop executes at most a given number of iterations
    IterationMultiple, // the loop executes a multiple of a given number of iterations
    PeelCount,         // the loop should be peeled by a given number of loop iterations
    PartialCount,      // the loop should be partially unrolled by a given number of loop iterations
    // selection attributes
    Flatten,
    DontFlatten,
    Branch = DontFlatten
};

enum class eFunctionAttribute : uint8_t { Builtin, Prototype };

const Bitmask<eCtrlFlowAttribute> LoopAttributesMask =
    Bitmask{eCtrlFlowAttribute::Unroll} | eCtrlFlowAttribute::DontUnroll | eCtrlFlowAttribute::DependencyInfinite |
    eCtrlFlowAttribute::DependencyLength;
const Bitmask<eCtrlFlowAttribute> SelectionAttributesMask =
    Bitmask{eCtrlFlowAttribute::Flatten} | eCtrlFlowAttribute::DontFlatten;

struct ast_function;
struct ast_global_variable;
struct ast_version_directive;
struct ast_extension_directive;
struct ast_expression;
using ast_constant_expression = ast_expression;
struct ast_layout_qualifier;
struct ast_statement;
struct ast_builtin;
struct ast_struct;
struct ast_interface_block;
struct ast_variable;
struct ast_default_precision;

enum class eTrUnitType : uint8_t {
    Unknown,
    Compute,
    Vertex,
    TessControl,
    TessEvaluation,
    Geometry,
    Fragment,
    RayGen,
    Intersect,
    ClosestHit,
    AnyHit,
    Miss,
    Callable
};

struct TrUnit {
    eTrUnitType type;

    ast_version_directive *version = nullptr;
    global_vector<ast_extension_directive *> extensions;
    global_vector<ast_default_precision *> default_precision;
    global_vector<ast_interface_block *> interface_blocks;
    global_vector<ast_builtin *> builtins; // always sorted by keyword
    global_vector<ast_struct *> structures;
    // NOTE: structure declarations must be in specific order
    HashMap32<const char *, ast_struct *> structures_by_name;
    global_vector<ast_global_variable *> globals;
    global_vector<ast_function *> functions;
    // NOTE: function declarations must be in specific order
    HashMap32<const char *, SmallVector<ast_function *, 1>> functions_by_name;

    mutable ast_allocator alloc;
    HashSet32<char *> str;

    explicit TrUnit(const eTrUnitType _type = eTrUnitType::Unknown) : type(_type) {}
    ~TrUnit();

    template <class T, class... Args> T *make(Args &&...args) { return new (&alloc) T(std::forward<Args>(args)...); }
    char *makestr(const char *s);

    ast_builtin *FindBuiltin(eKeyword type) const;
    int FindBuiltinIndex(eKeyword type) const;
    ast_builtin *FindOrAddBuiltin(eKeyword type);

    friend int Compare(const TrUnit *lhs, const TrUnit *rhs);
};

template <typename T> [[nodiscard]] int Compare_PointerSpans(Span<const T> lhs, Span<const T> rhs) {
    const ptrdiff_t min_size = std::min(lhs.size(), rhs.size());
    for (ptrdiff_t i = 0; i < min_size; ++i) {
        const int res = Compare(lhs[i], rhs[i]);
        if (res != 0) {
            return res;
        }
    }
    if (lhs.size() < rhs.size()) {
        return -1;
    }
    if (rhs.size() < lhs.size()) {
        return +1;
    }
    return 0;
}

struct ast_type : ast_node<ast_type> {
    bool builtin;

    explicit ast_type(const bool _builtin) noexcept : builtin(_builtin) {}

    friend int Compare(const ast_type *lhs, const ast_type *rhs);
};

struct ast_builtin : ast_type {
    eKeyword type;

    explicit ast_builtin(const eKeyword _type) noexcept : ast_type(true), type(_type) {}

    friend int Compare(const ast_builtin *lhs, const ast_builtin *rhs);
};

struct ast_struct : ast_type {
    char *name = nullptr;
    local_vector<ast_variable *> fields;

    explicit ast_struct(MultiPoolAllocator<char> &_alloc) noexcept : ast_type(false), fields(_alloc) {}
    OPERATOR_NEW(ast_struct)

    friend int Compare(const ast_struct *lhs, const ast_struct *rhs);
};

struct ast_interface_block : ast_struct {
    eStorage storage = eStorage::None;
    Bitmask<eMemory> memory_flags;
    local_vector<ast_layout_qualifier *> layout_qualifiers;

    explicit ast_interface_block(MultiPoolAllocator<char> &_alloc) noexcept
        : ast_struct(_alloc), layout_qualifiers(_alloc) {}
    OPERATOR_NEW(ast_interface_block)

    friend int Compare(const ast_interface_block *lhs, const ast_interface_block *rhs);
};

struct ast_version_directive : ast_type {
    eVerType type = eVerType::Core;
    int32_t number = -1;

    ast_version_directive() noexcept : ast_type(false) {}

    friend int Compare(const ast_version_directive *lhs, const ast_version_directive *rhs);
};

struct ast_extension_directive : ast_type {
    eExtBehavior behavior = eExtBehavior::Invalid;
    char *name = nullptr;

    ast_extension_directive() noexcept : ast_type(false) {}

    friend int Compare(const ast_extension_directive *lhs, const ast_extension_directive *rhs);
};

struct ast_default_precision : ast_type {
    ePrecision precision = ePrecision::None;
    ast_builtin *type = nullptr;

    ast_default_precision() noexcept : ast_type(false) {}

    friend int Compare(const ast_default_precision *lhs, const ast_default_precision *rhs);
};

enum class eVariableType : uint8_t { Function, Parameter, Global, Field };
enum class eVariableFlags : uint8_t { Array, Precise, Const, Invariant, Hidden };

struct ast_variable : ast_node<ast_variable> {
    eVariableType type;
    Bitmask<eVariableFlags> flags;
    ePrecision precision = ePrecision::None;

    char *name = nullptr;
    ast_type *base_type = nullptr;
    local_vector<ast_constant_expression *> array_sizes;

    ast_variable(const eVariableType _type, MultiPoolAllocator<char> &_alloc) noexcept
        : type(_type), array_sizes(_alloc) {}
    OPERATOR_NEW(ast_variable)

    friend int Compare(const ast_variable *lhs, const ast_variable *rhs);
};

struct ast_function_variable : ast_variable {
    ast_expression *initial_value = nullptr;

    explicit ast_function_variable(MultiPoolAllocator<char> &_alloc) noexcept
        : ast_variable(eVariableType::Function, _alloc) {}
};

struct ast_function_parameter : ast_variable {
    Bitmask<eParamQualifier> qualifiers = eParamQualifier::None;

    explicit ast_function_parameter(MultiPoolAllocator<char> &_alloc) noexcept
        : ast_variable(eVariableType::Parameter, _alloc) {}
};

struct ast_global_variable : ast_variable {
    eStorage storage = eStorage::None;
    eAuxStorage aux_storage = eAuxStorage::None;
    Bitmask<eMemory> memory_flags;
    eInterpolation interpolation = eInterpolation::None;
    ast_constant_expression *initial_value = nullptr;
    local_vector<ast_layout_qualifier *> layout_qualifiers;

    explicit ast_global_variable(MultiPoolAllocator<char> &_alloc) noexcept
        : ast_variable(eVariableType::Global, _alloc), layout_qualifiers(_alloc) {}
    OPERATOR_NEW(ast_global_variable)
};

struct ast_layout_qualifier : ast_node<ast_layout_qualifier> {
    char *name = nullptr;
    ast_constant_expression *initial_value = nullptr;

    friend int Compare(const ast_layout_qualifier *lhs, const ast_layout_qualifier *rhs);
};

struct ast_function : ast_node<ast_function> {
    Bitmask<eFunctionAttribute> attributes;
    ast_type *return_type = nullptr;
    char *name = nullptr;
    local_vector<ast_function_parameter *> parameters;
    local_vector<ast_statement *> statements;
    ast_function *prototype = nullptr;

    explicit ast_function(MultiPoolAllocator<char> &_alloc) noexcept : parameters(_alloc), statements(_alloc) {}
    OPERATOR_NEW(ast_function)

    friend int Compare(const ast_function *lhs, const ast_function *rhs);
};

enum class eStatement : uint8_t {
    Invalid, // used only during serialization
    Compound,
    Empty,
    Declaration,
    Expression,
    If,
    Switch,
    CaseLabel,
    While,
    Do,
    For,
    Continue,
    Break,
    Return,
    Discard,
    ExtJump,
    _Count
};

struct ast_statement : ast_node<ast_statement> {
    eStatement type;

    explicit ast_statement(const eStatement _type) noexcept : type(_type) {}

    friend int Compare(const ast_statement *lhs, const ast_statement *rhs);
};

struct ast_simple_statement : ast_statement {
    explicit ast_simple_statement(const eStatement _type) noexcept : ast_statement(_type) {}
};

struct ast_compound_statement : ast_statement {
    local_vector<ast_statement *> statements;
    explicit ast_compound_statement(MultiPoolAllocator<char> &_alloc) noexcept
        : ast_statement(eStatement::Compound), statements(_alloc) {}
    OPERATOR_NEW(ast_compound_statement)
};

struct ast_empty_statement : ast_simple_statement {
    ast_empty_statement() noexcept : ast_simple_statement(eStatement::Empty) {}
};

struct ast_declaration_statement : ast_simple_statement {
    local_vector<ast_function_variable *> variables;

    explicit ast_declaration_statement(MultiPoolAllocator<char> &_alloc) noexcept
        : ast_simple_statement(eStatement::Declaration), variables(_alloc) {}
    OPERATOR_NEW(ast_declaration_statement)
};

struct ast_expression_statement : ast_simple_statement {
    ast_expression *expression = nullptr;

    explicit ast_expression_statement(ast_expression *_expression) noexcept
        : ast_simple_statement(eStatement::Expression), expression(_expression) {}
};

struct ast_if_statement : ast_simple_statement {
    Bitmask<eCtrlFlowAttribute> attributes;
    ast_expression *condition = nullptr;
    ast_statement *then_statement = nullptr;
    ast_statement *else_statement = nullptr;

    explicit ast_if_statement(const Bitmask<eCtrlFlowAttribute> _attributes) noexcept
        : ast_simple_statement(eStatement::If), attributes(_attributes) {}
};

struct ast_switch_statement : ast_simple_statement {
    Bitmask<eCtrlFlowAttribute> attributes;
    ast_expression *expression = nullptr;
    local_vector<ast_statement *> statements;

    ast_switch_statement(MultiPoolAllocator<char> &_alloc, const Bitmask<eCtrlFlowAttribute> _attributes) noexcept
        : ast_simple_statement(eStatement::Switch), attributes(_attributes), statements(_alloc) {}
    OPERATOR_NEW(ast_switch_statement)
};

enum class eCaseLabelFlags : uint8_t { Default };

struct ast_case_label_statement : ast_simple_statement {
    Bitmask<eCaseLabelFlags> flags;
    ast_constant_expression *condition = nullptr;

    ast_case_label_statement() noexcept : ast_simple_statement(eStatement::CaseLabel) {}
};

struct ctrl_flow_params_t {
    Bitmask<eCtrlFlowAttribute> attributes;
    short dependency_length = 0;
    short min_iterations = 0, max_iterations = 0;
    short iteration_multiple = 0;
    short peel_count = 0, partial_count = 0;

    friend bool operator==(const ctrl_flow_params_t &lhs, const ctrl_flow_params_t &rhs) {
        return std::tie(lhs.attributes, lhs.dependency_length, lhs.min_iterations, lhs.max_iterations,
                        lhs.iteration_multiple, lhs.peel_count, lhs.partial_count) ==
               std::tie(rhs.attributes, rhs.dependency_length, rhs.min_iterations, rhs.max_iterations,
                        rhs.iteration_multiple, rhs.peel_count, rhs.partial_count);
    }
    friend bool operator!=(const ctrl_flow_params_t &lhs, const ctrl_flow_params_t &rhs) {
        return !operator==(lhs, rhs);
    }
    friend bool operator<(const ctrl_flow_params_t &lhs, const ctrl_flow_params_t &rhs) {
        return std::tie(lhs.attributes, lhs.dependency_length, lhs.min_iterations, lhs.max_iterations,
                        lhs.iteration_multiple, lhs.peel_count, lhs.partial_count) <
               std::tie(rhs.attributes, rhs.dependency_length, rhs.min_iterations, rhs.max_iterations,
                        rhs.iteration_multiple, rhs.peel_count, rhs.partial_count);
    }
};

struct ast_loop_statement : ast_simple_statement {
    ctrl_flow_params_t flow_params;

    ast_loop_statement(const eStatement _type, const ctrl_flow_params_t _flow_params) noexcept
        : ast_simple_statement(_type), flow_params(_flow_params) {}

    friend int Compare(const ast_loop_statement *lhs, const ast_loop_statement *rhs);
};

struct ast_while_statement : ast_loop_statement {
    ast_simple_statement *condition = nullptr;
    ast_statement *body = nullptr;

    explicit ast_while_statement(const ctrl_flow_params_t _flow_params) noexcept
        : ast_loop_statement(eStatement::While, _flow_params) {}
};

struct ast_do_statement : ast_loop_statement {
    ast_expression *condition = nullptr;
    ast_statement *body = nullptr;

    explicit ast_do_statement(const ctrl_flow_params_t _flow_params) noexcept
        : ast_loop_statement(eStatement::Do, _flow_params) {}
};

struct ast_for_statement : ast_loop_statement {
    ast_simple_statement *init = nullptr;
    ast_expression *condition = nullptr;
    ast_expression *loop = nullptr;
    ast_statement *body = nullptr;

    explicit ast_for_statement(const ctrl_flow_params_t _flow_params) noexcept
        : ast_loop_statement(eStatement::For, _flow_params) {}
};

struct ast_jump_statement : ast_statement {
    explicit ast_jump_statement(const eStatement _type) noexcept : ast_statement(_type) {}
};

struct ast_continue_statement : ast_jump_statement {
    ast_continue_statement() noexcept : ast_jump_statement(eStatement::Continue) {}
};

struct ast_break_statement : ast_jump_statement {
    ast_break_statement() noexcept : ast_jump_statement(eStatement::Break) {}
};

struct ast_return_statement : ast_jump_statement {
    ast_expression *expression = nullptr;

    ast_return_statement() noexcept : ast_jump_statement(eStatement::Return) {}
};

struct ast_discard_statement : ast_jump_statement {
    ast_discard_statement() noexcept : ast_jump_statement(eStatement::Discard) {}
};

struct ast_ext_jump_statement : ast_jump_statement {
    eKeyword keyword;

    explicit ast_ext_jump_statement(const eKeyword _keyword) noexcept
        : ast_jump_statement(eStatement::ExtJump), keyword(_keyword) {}
};

enum class eExprType : uint8_t {
    Undefined, // only used during serialization
    ShortConstant,
    UShortConstant,
    IntConstant,
    UIntConstant,
    LongConstant,
    ULongConstant,
    HalfConstant,
    FloatConstant,
    DoubleConstant,
    BoolConstant,
    VariableIdentifier,
    FieldOrSwizzle,
    ArraySubscript,
    FunctionCall,
    ConstructorCall,
    PostIncrement,
    PostDecrement,
    UnaryMinus,
    UnaryPlus,
    BitNot,
    LogicalNot,
    PrefixIncrement,
    PrefixDecrement,
    Sequence,
    Assign,
    Operation,
    Ternary,
    ArraySpecifier
};

struct ast_expression : ast_node<ast_expression> {
    eExprType type;

    explicit ast_expression(eExprType _type) noexcept : type(_type) {}

    friend int Compare(const ast_expression *lhs, const ast_expression *rhs);
};

struct ast_short_constant : ast_expression {
    int16_t value;

    explicit ast_short_constant(int16_t _value) noexcept : ast_expression(eExprType::ShortConstant), value(_value) {}
};

struct ast_ushort_constant : ast_expression {
    uint16_t value;

    explicit ast_ushort_constant(uint16_t _value) noexcept : ast_expression(eExprType::UShortConstant), value(_value) {}
};

struct ast_int_constant : ast_expression {
    int32_t value;

    explicit ast_int_constant(int32_t _value) noexcept : ast_expression(eExprType::IntConstant), value(_value) {}
};

struct ast_uint_constant : ast_expression {
    uint32_t value;

    explicit ast_uint_constant(uint32_t _value) noexcept : ast_expression(eExprType::UIntConstant), value(_value) {}
};

struct ast_long_constant : ast_expression {
    int64_t value;

    explicit ast_long_constant(int64_t _value) noexcept : ast_expression(eExprType::LongConstant), value(_value) {}
};

struct ast_ulong_constant : ast_expression {
    uint64_t value;

    explicit ast_ulong_constant(uint64_t _value) noexcept : ast_expression(eExprType::ULongConstant), value(_value) {}
};

struct ast_half_constant : ast_expression {
    float value;

    explicit ast_half_constant(float _value) noexcept : ast_expression(eExprType::HalfConstant), value(_value) {}
};

struct ast_float_constant : ast_expression {
    float value;

    explicit ast_float_constant(float _value) noexcept : ast_expression(eExprType::FloatConstant), value(_value) {}
};

struct ast_double_constant : ast_expression {
    double value;

    explicit ast_double_constant(double _value) noexcept : ast_expression(eExprType::DoubleConstant), value(_value) {}
};

struct ast_bool_constant : ast_expression {
    bool value;

    explicit ast_bool_constant(bool _value) noexcept : ast_expression(eExprType::BoolConstant), value(_value) {}
};

struct ast_array_specifier : ast_expression {
    local_vector<ast_expression *> expressions;

    explicit ast_array_specifier(MultiPoolAllocator<char> &_alloc) noexcept
        : ast_expression(eExprType::ArraySpecifier), expressions(_alloc) {}
    OPERATOR_NEW(ast_array_specifier)
};

struct ast_variable_identifier : ast_expression {
    ast_variable *variable;

    explicit ast_variable_identifier(ast_variable *_variable) noexcept
        : ast_expression(eExprType::VariableIdentifier), variable(_variable) {}
};

struct ast_field_or_swizzle : ast_expression {
    ast_expression *operand = nullptr;
    ast_variable_identifier *field = nullptr;
    char *name = nullptr;

    ast_field_or_swizzle() noexcept : ast_expression(eExprType::FieldOrSwizzle) {}
};

struct ast_array_subscript : ast_expression {
    ast_expression *operand = nullptr, *index = nullptr;

    ast_array_subscript() noexcept : ast_expression(eExprType::ArraySubscript) {}
};

struct ast_function_call : ast_expression {
    char *name = nullptr;
    ast_function *func = nullptr;
    local_vector<ast_expression *> parameters;

    explicit ast_function_call(MultiPoolAllocator<char> &_alloc) noexcept
        : ast_expression(eExprType::FunctionCall), parameters(_alloc) {}
    OPERATOR_NEW(ast_function_call)
};

struct ast_constructor_call : ast_expression {
    ast_type *type = nullptr;
    local_vector<ast_expression *> parameters;

    explicit ast_constructor_call(MultiPoolAllocator<char> &_alloc) noexcept
        : ast_expression(eExprType::ConstructorCall), parameters(_alloc) {}
    OPERATOR_NEW(ast_constructor_call)
};

struct ast_unary_expression : ast_expression {
    ast_expression *operand;

    ast_unary_expression(eExprType _type, ast_expression *_operand) noexcept
        : ast_expression(_type), operand(_operand) {}
};

struct ast_binary_expression : ast_expression {
    ast_expression *operand1 = nullptr, *operand2 = nullptr;

    explicit ast_binary_expression(eExprType _type) noexcept : ast_expression(_type) {}
};

struct ast_post_increment_expression : ast_unary_expression {
    explicit ast_post_increment_expression(ast_expression *operand) noexcept
        : ast_unary_expression(eExprType::PostIncrement, operand) {}
};

struct ast_post_decrement_expression : ast_unary_expression {
    explicit ast_post_decrement_expression(ast_expression *operand) noexcept
        : ast_unary_expression(eExprType::PostDecrement, operand) {}
};

struct ast_unary_plus_expression : ast_unary_expression {
    explicit ast_unary_plus_expression(ast_expression *operand) noexcept
        : ast_unary_expression(eExprType::UnaryPlus, operand) {}
};

struct ast_unary_minus_expression : ast_unary_expression {
    explicit ast_unary_minus_expression(ast_expression *operand) noexcept
        : ast_unary_expression(eExprType::UnaryMinus, operand) {}
};

struct ast_unary_bit_not_expression : ast_unary_expression {
    explicit ast_unary_bit_not_expression(ast_expression *operand) noexcept
        : ast_unary_expression(eExprType::BitNot, operand) {}
};

struct ast_unary_logical_not_expression : ast_unary_expression {
    explicit ast_unary_logical_not_expression(ast_expression *operand) noexcept
        : ast_unary_expression(eExprType::LogicalNot, operand) {}
};

struct ast_prefix_increment_expression : ast_unary_expression {
    explicit ast_prefix_increment_expression(ast_expression *operand) noexcept
        : ast_unary_expression(eExprType::PrefixIncrement, operand) {}
};

struct ast_prefix_decrement_expression : ast_unary_expression {
    explicit ast_prefix_decrement_expression(ast_expression *operand) noexcept
        : ast_unary_expression(eExprType::PrefixDecrement, operand) {}
};

struct ast_sequence_expression : ast_binary_expression {
    ast_sequence_expression() noexcept : ast_binary_expression(eExprType::Sequence) {}
};

struct ast_assignment_expression : ast_binary_expression {
    eOperator oper;

    explicit ast_assignment_expression(eOperator _oper) noexcept
        : ast_binary_expression(eExprType::Assign), oper(_oper) {}
};

struct ast_operation_expression : ast_binary_expression {
    eOperator oper;

    explicit ast_operation_expression(eOperator _oper) noexcept
        : ast_binary_expression(eExprType::Operation), oper(_oper) {}
};

struct ast_ternary_expression : ast_expression {
    ast_expression *condition = nullptr, *on_true = nullptr, *on_false = nullptr;

    ast_ternary_expression() noexcept : ast_expression(eExprType::Ternary) {}
};

bool IsConstantValue(const ast_expression *expression);
bool IsConstant(const ast_expression *expression);
} // namespace glslx