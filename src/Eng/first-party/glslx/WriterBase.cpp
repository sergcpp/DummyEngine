#include "WriterBase.h"

#include <cmath>
#include <iomanip>
#include <limits>
#include <ostream>

#include "parser/AST.h"

namespace glslx {
// TODO: this is stupid
bool will_use_scientific_notation(const float value, const int precision_threshold) {
    // Check if the magnitude of the value is beyond the precision threshold
    return std::abs(value) >= std::pow(10.0, precision_threshold);
}
} // namespace glslx

void glslx::WriterBase::Write_Tabs(std::ostream &out_stream) {
    for (int i = 0; i < nest_level_; ++i) {
        out_stream << config_.tab;
    }
}

void glslx::WriterBase::Write_Constant(const ast_short_constant *expression, std::ostream &out_stream) {
    out_stream << expression->value << "s";
}

void glslx::WriterBase::Write_Constant(const ast_ushort_constant *expression, std::ostream &out_stream) {
    out_stream << expression->value << "us";
}

void glslx::WriterBase::Write_Constant(const ast_int_constant *expression, std::ostream &out_stream) {
    out_stream << expression->value;
}

void glslx::WriterBase::Write_Constant(const ast_uint_constant *expression, std::ostream &out_stream) {
    out_stream << expression->value << "u";
}

void glslx::WriterBase::Write_Constant(const ast_long_constant *expression, std::ostream &out_stream) {
    out_stream << expression->value << "l";
}

void glslx::WriterBase::Write_Constant(const ast_ulong_constant *expression, std::ostream &out_stream) {
    out_stream << expression->value << "ul";
}

void glslx::WriterBase::Write_Constant(const ast_bool_constant *expression, std::ostream &out_stream) {
    out_stream << std::boolalpha << expression->value << std::noboolalpha;
}

void glslx::WriterBase::Write_Constant(const ast_half_constant *expression, std::ostream &out_stream) {
    out_stream << expression->value;
    if (std::round(expression->value) == expression->value) {
        out_stream << ".0";
    }
    if (!config_.drop_half_float_literals) {
        out_stream << "hf";
    }
}

void glslx::WriterBase::Write_Constant(const ast_float_constant *expression, std::ostream &out_stream) {
    out_stream << std::setprecision(std::numeric_limits<float>::max_digits10) << expression->value;
    // TODO: there must be a better way to do this
    if (!will_use_scientific_notation(expression->value, std::numeric_limits<float>::max_digits10) &&
        std::round(expression->value) == expression->value) {
        out_stream << ".0";
    }
}

void glslx::WriterBase::Write_Constant(const ast_double_constant *expression, std::ostream &out_stream) {
    out_stream << expression->value;
    if (expression->value != float(expression->value)) {
        out_stream << "lf";
    }
}