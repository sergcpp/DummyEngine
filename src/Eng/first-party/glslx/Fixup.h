#pragma once

#include "parser/AST.h"

namespace glslx {
struct fixup_config_t {
    bool randomize_loop_counters = true;
    bool remove_const = false;
    bool remove_ctrl_flow_attributes = false;
    bool remove_inout_layout = false;
    bool flip_vertex_y = false;
};

class Fixup {
    fixup_config_t config_;
    TrUnit *tu_ = nullptr;
    int next_counter_ = 0;

    void Visit_Statement(ast_statement *statement);

    void Visit_FunctionParameter(ast_function_parameter *parameter);
    void Visit_Function(ast_function *func);

  public:
    Fixup(const fixup_config_t &config = {}) : config_(config) {}

    void Apply(TrUnit *tu);
};
} // namespace glslx