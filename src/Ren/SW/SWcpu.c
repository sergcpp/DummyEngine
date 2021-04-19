#include "SWcpu.h"

#include <stdio.h>
#include <string.h>

#ifdef _WIN32
//  Windows
#include <intrin.h>
#ifdef __GNUC__
#include <cpuid.h>
inline void cpuid(int info[4], int InfoType) {
    __cpuid_count(InfoType, 0, info[0], info[1], info[2], info[3]);
}
#if defined(__GNUC__) && (__GNUC__ < 9)
inline unsigned long long _xgetbv(unsigned int index) {
    unsigned int eax, edx;
    __asm__ __volatile__(
        "xgetbv;"
        : "=a" (eax), "=d"(edx)
        : "c" (index)
        );
    return ((unsigned long long)edx << 32) | eax;
}
#endif
#else
#define cpuid(info, x)    __cpuidex(info, x, 0)
#endif

#else

#if !defined(__arm__) && !defined(__aarch64__) && !defined(__EMSCRIPTEN__) && !defined(__ANDROID__)
//  GCC Intrinsics
#include <cpuid.h>
#include <immintrin.h>
inline void cpuid(int info[4], int InfoType) {
    __cpuid_count(InfoType, 0, info[0], info[1], info[2], info[3]);
}
#if defined(__GNUC__) && (__GNUC__ < 9)
inline unsigned long long _xgetbv(unsigned int index) {
    unsigned int eax, edx;
    __asm__ __volatile__(
        "xgetbv;"
        : "=a" (eax), "=d"(edx)
        : "c" (index)
        );
    return ((unsigned long long)edx << 32) | eax;
}
#endif
#endif

#endif

#ifdef _WIN32

#include <intrin.h>
#include <Windows.h>

#ifdef _MSC_VER
#pragma warning(disable : 4996)
#endif
#endif

#if defined(__linux) && !defined(__ANDROID__)

#include <sys/sysinfo.h>
#include <sys/types.h>

ssize_t getline(char **lineptr, size_t *n, FILE *stream);
char *strdup(const char *s);

#endif

unsigned long long get_xcr_feature_mask();

void swCPUInfoInit(SWcpu_info *info) {
    memset(info, 0, sizeof(SWcpu_info));

    info->vendor = strdup("Unknown");
    //info->model = "Unknown";
    info->num_cpus = 0;
    info->physical_memory = 0;

#if defined(_WIN32)
    int CPUInfo[4] = { -1 };
    unsigned nExIds, i = 0;
    char CPUBrandString[0x40];
    // Get the information associated with each extended ID.
    __cpuid(CPUInfo, 0x80000000);
    nExIds = CPUInfo[0];
    for (i = 0x80000000; i <= nExIds; ++i) {
        __cpuid(CPUInfo, i);
        // Interpret CPU brand string
        if (i == 0x80000002)
            memcpy(CPUBrandString, CPUInfo, sizeof(CPUInfo));
        else if (i == 0x80000003)
            memcpy(CPUBrandString + 16, CPUInfo, sizeof(CPUInfo));
        else if (i == 0x80000004)
            memcpy(CPUBrandString + 32, CPUInfo, sizeof(CPUInfo));
    }

    info->model = strdup(CPUBrandString);

    SYSTEM_INFO sys_info;
    GetSystemInfo(&sys_info);

    info->num_cpus = sys_info.dwNumberOfProcessors;

    MEMORYSTATUSEX statex;
    statex.dwLength = sizeof(statex);
    GlobalMemoryStatusEx(&statex);

    info->physical_memory = ((statex.ullTotalPhys / 1024.0f) / 1024) / 1024;

    char vendor[13];
    __cpuid(CPUInfo, 0);
    memcpy(vendor, &CPUInfo[1], 4);   // copy EBX
    memcpy(vendor + 4, &CPUInfo[2], 4); // copy ECX
    memcpy(vendor + 8, &CPUInfo[3], 4); // copy EDX
    vendor[12] = '\0';
#elif !defined(__ANDROID__)
    struct sysinfo mem_info;
    sysinfo(&mem_info);
    long long total_virtual_mem = (long long)mem_info.totalram;
    total_virtual_mem *= mem_info.mem_unit;

    info->physical_memory = (SWfloat)(((total_virtual_mem / 1024.0) / 1024) / 1024);

    FILE *cpuinfo = fopen("/proc/cpuinfo", "rb");
    char *arg = 0;
    size_t size = 0;
    while (getline(&arg, &size, cpuinfo) != -1) {
        char *tok = arg;
        if ((tok = strtok(tok, " \t:\n")) != NULL) {
            if (strcmp(tok, "vendor_id") == 0) {
                if ((tok = strtok(NULL, "\t:\n")) != NULL) {
                    while (*tok && *tok == ' ') tok++;
                    info->vendor = strdup(tok);
                }
            } else if (strcmp(tok, "model") == 0) {
                if ((tok = strtok(NULL, " \t:\n")) != NULL) {
                    if (strcmp(tok, "name") == 0) {
                        if ((tok = strtok(NULL, "\t:\n")) != NULL) {
                            while (*tok && *tok == ' ') tok++;
                            info->model = strdup(tok);
                        }
                    }
                }
            } else if (strcmp(tok, "processor") == 0) {
                info->num_cpus++;
            }
        }
    }
    free(arg);
    fclose(cpuinfo);
#endif

#if !defined(__ANDROID__)
    int cpu_info[4];
    cpuid(cpu_info, 0);
    int ids_count = cpu_info[0];

    cpuid(cpu_info, 0x80000000u);
    //unsigned ex_ids_count = cpu_info[0];

    //  Detect Features
    if (ids_count >= 0x00000001) {
        cpuid(cpu_info, 0x00000001);
        info->sse2_supported = (cpu_info[3] & ((int)1 << 26)) != 0;
        info->sse3_supported = (cpu_info[2] & ((int)1 << 0)) != 0;
        info->ssse3_supported = (cpu_info[2] & ((int)1 << 9)) != 0;
        info->sse41_supported = (cpu_info[2] & ((int)1 << 19)) != 0;

        int os_uses_XSAVE_XRSTORE = (cpu_info[2] & (1 << 27)) != 0;
        int os_saves_YMM = 0;
        if (os_uses_XSAVE_XRSTORE) {
            // Check if the OS will save the YMM registers
            // _XCR_XFEATURE_ENABLED_MASK = 0
            unsigned long long xcr_feature_mask = get_xcr_feature_mask();
            os_saves_YMM = (xcr_feature_mask & 0x6) != 0;
        }

        int cpu_FMA_support = (cpu_info[3] & ((int)1 << 12)) != 0;

        int cpu_AVX_support = (cpu_info[2] & (1 << 28)) != 0;
        info->avx_supported = os_saves_YMM && cpu_AVX_support;

        if (ids_count >= 0x00000007) {
            cpuid(cpu_info, 0x00000007);

            int cpu_AVX2_support = (cpu_info[1] & (1 << 5)) != 0;
            // use fma in conjunction with avx2 support (like microsoft compiler does)
            info->avx2_supported = os_saves_YMM && cpu_AVX2_support && cpu_FMA_support;

            info->avx512_supported = (cpu_info[1] & (1 << 16)) != 0;    // HW_AVX512F
            //info->avx512_supported &= (cpu_info[1] & (1 << 28)) != 0;   // HW_AVX512CD
            //info->avx512_supported &= (cpu_info[1] & (1 << 26)) != 0;   // HW_AVX512PF
            //info->avx512_supported &= (cpu_info[1] & (1 << 27)) != 0;   // HW_AVX512ER
            //info->avx512_supported &= (cpu_info[1] & (1 << 31)) != 0;   // HW_AVX512VL
            info->avx512_supported &= (cpu_info[1] & (1 << 30)) != 0;   // HW_AVX512BW
            info->avx512_supported &= (cpu_info[1] & (1 << 17)) != 0;   // HW_AVX512DQ
            //info->avx512_supported &= (cpu_info[1] & (1 << 21)) != 0;   // HW_AVX512IFMA
            //info->avx512_supported &= (cpu_info[2] & (1 << 1)) != 0;    // HW_AVX512VBMI
        }
    }
#elif defined(__i386__) || defined(__x86_64__)
    info->sse2_supported = true;
#endif
}

#if !defined(_WIN32) && !defined(__linux) || defined(__ANDROID__)

void swCInfoInit(SWcpu_info *info) {
    memset(info, 0, sizeof(SWcpu_info));
}

#endif

void swCPUInfoDestroy(SWcpu_info *info) {
    free(info->vendor);
    free(info->model);

    memset(info, 0, sizeof(SWcpu_info));
}
