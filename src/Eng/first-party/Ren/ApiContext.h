#pragma once

#if defined(USE_VK_RENDER)
#include "VKCtx.h"
#elif defined(USE_GL_RENDER)
#include "GLCtx.h"
#endif