#pragma once

#include <cstdarg>

namespace glslx {
// An implementation of vasprintf
int allocvfmt(char **str, const char *fmt, va_list vp);

// An implementation of vsprintf
int allocfmt(char **str, const char *fmt, ...);
} // namespace glslx
