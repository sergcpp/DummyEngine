#pragma once

#include "Fwd.h"

namespace Ren {
struct BindlessDescriptors {
#if defined(REN_VK_BACKEND)
    VkDescriptorSet descr_set;
#elif defined(REN_GL_BACKEND)
    Ren::WeakBufRef buf;
#endif
};
}