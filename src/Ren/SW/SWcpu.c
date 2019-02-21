#include "SWcpu.h"

#include <stdio.h>
#include <string.h>

#ifdef _WIN32

#include <intrin.h>
#include <Windows.h>

#ifdef _MSC_VER
#pragma warning(disable : 4996)
#endif

void swCInfoInit(SWcpu_info *info) {
    memset(info, 0, sizeof(SWcpu_info));

    info->vendor = strdup("Unknown");
    //info->model = "Unknown";
    info->num_cpus = 0;
    info->physical_memory = 0;

#if !defined(__TINYC__)
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
    printf("My CPU is a %s\n", vendor);
#endif
}

#endif

#if defined(__linux) && !defined(__ANDROID__)

#include <sys/sysinfo.h>
#include <sys/types.h>

ssize_t getline(char **lineptr, size_t *n, FILE *stream);
char *strdup(const char *s);

void swCInfoInit(SWcpu_info *info) {
    memset(info, 0, sizeof(SWcpu_info));

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
}

#endif

#if !defined(_WIN32) && !defined(__linux) || defined(__ANDROID__)

void swCInfoInit(SWcpu_info *info) {
    memset(info, 0, sizeof(SWcpu_info));
}

#endif

void swCInfoDestroy(SWcpu_info *info) {
    free(info->vendor);
    free(info->model);

    memset(info, 0, sizeof(SWcpu_info));
}
