#include "CPUFeatures.h"

extern "C" {
#include "SW/SWcpu.h"
}

namespace Ren {
CpuFeatures GetCpuFeatures() noexcept {
    CpuFeatures ret = {};

    SWcpu_info info;
    swCPUInfoInit(&info);

    ret.sse2_supported = info.sse2_supported;
    ret.sse3_supported = info.sse3_supported;
    ret.ssse3_supported = info.ssse3_supported;
    ret.sse41_supported = info.sse41_supported;
    ret.avx_supported = info.avx_supported;
    ret.avx2_supported = info.avx2_supported;

    swCPUInfoDestroy(&info);

    return ret;
}

CpuFeatures g_CpuFeatures = GetCpuFeatures();
} // namespace Ren

#undef cpuid
