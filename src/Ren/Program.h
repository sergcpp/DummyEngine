#pragma once

#if defined(USE_GL_RENDER)
#include "ProgramGL.h"
#elif defined(USE_SW_RENDER)
#include "ProgramSW.h"
#endif
