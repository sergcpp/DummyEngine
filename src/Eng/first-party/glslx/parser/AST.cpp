#include "AST.h"

namespace glslx {
const char *g_statement_names[] = {
    "compound",    // Compound
    "empty",       // Empty
    "declaration", // Declaration
    "expression",  // Expression
    "if",          // If
    "switch",      // Switch
    "case label",  // CaseLabel
    "while",       // While
    "do",          // Do
    "for",         // For
    "continue",    // Continue
    "break",       // Break
    "return",      // Return
    "discard",     // Discard
    "ext jump"     // ExtJump
};
static_assert(sizeof(g_statement_names) / sizeof(g_statement_names[0]) == int(eStatement::_Count), "!");
} // namespace glslx


glslx::TrUnit::~TrUnit() {
    for (char *str : str) {
        free(str);
    }
    for (ast_memory &mem : mem) {
        mem.destroy();
    }
}