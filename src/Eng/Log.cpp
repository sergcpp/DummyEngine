#include "Log.h"

#include <cstdarg>
#include <cstdio>
#include <ctime>

#ifdef __ANDROID__
#include <android/log.h>
#endif

namespace EngInternal {
    void TimedOutput(FILE *dst, const char *fmt, va_list args) {
        time_t now = time(nullptr);

        char buff[27];
        strftime(buff, sizeof(buff), "%Y-%m-%d %H:%M:%S.000 | ", localtime(&now));

        fputs(buff, dst);
        vfprintf(dst, fmt, args);
    }
}

void LogStdout::Info(const char *fmt, ...) {
    va_list vl;
    va_start(vl, fmt);
    EngInternal::TimedOutput(stdout, fmt, vl);
    va_end(vl);
}

void LogStdout::Error(const char *fmt, ...) {
    va_list vl;
    va_start(vl, fmt);
    EngInternal::TimedOutput(stderr, fmt, vl);
    va_end(vl);
}

#ifdef __ANDROID__

LogAndroid::LogAndroid(const char *log_tag) {
    strcpy(log_tag_, log_tag);
}

void LogAndroid::Info(const char *fmt, ...) {
    va_list vl;
    va_start(vl, fmt);
    __android_log_vprint(ANDROID_LOG_INFO, log_tag_, fmt, vl);
    va_end(vl);
}

void LogAndroid::Error(const char *fmt, ...) {
    va_list vl;
    va_start(vl, fmt);
    __android_log_vprint(ANDROID_LOG_ERROR, log_tag_, fmt, vl);
    va_end(vl);
}

#endif