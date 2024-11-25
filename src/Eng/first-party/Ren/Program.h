#pragma once

#if defined(REN_VK_BACKEND)
#include "ProgramVK.h"
#elif defined(REN_GL_BACKEND)
#include "ProgramGL.h"
#elif defined(REN_SW_BACKEND)
#include "ProgramSW.h"
#endif
