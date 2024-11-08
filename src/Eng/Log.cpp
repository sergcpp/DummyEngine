#include "Log.h"

#include <cstdarg>
#include <cstdio>
#include <ctime>

#include <chrono>
#include <mutex>

#ifdef __ANDROID__
#include <android/log.h>
#endif

#pragma warning(disable : 4996) // localtime is not thread-safe, but called under mutex so we don't care

namespace EngInternal {
std::mutex g_log_mtx;
} // namespace EngInternal

void Eng::LogStdout::TimedOutput(FILE *dst, const char *fmt, va_list args) {
    auto tp = std::chrono::system_clock::now();
    time_t now = std::chrono::system_clock::to_time_t(tp); // time(nullptr);

    std::lock_guard<std::mutex> _(EngInternal::g_log_mtx);

    char buff[32];
    strftime(buff, sizeof(buff), "%Y-%m-%d %H:%M:%S", localtime(&now));

    fputs(buff, dst);
    fprintf(dst, ".%03d | ",
            int(std::chrono::duration_cast<std::chrono::milliseconds>(tp.time_since_epoch()).count() % 1000));
    vfprintf(dst, fmt, args);
    putc('\n', dst);
}

void Eng::LogStdout::Info(_Printf_format_string_ const char *fmt, ...) {
    va_list vl;
    va_start(vl, fmt);
    TimedOutput(stdout, fmt, vl);
    va_end(vl);
}

void Eng::LogStdout::Warning(_Printf_format_string_ const char *fmt, ...) {
    va_list vl;
    va_start(vl, fmt);
    TimedOutput(stdout, fmt, vl);
    va_end(vl);
}

void Eng::LogStdout::Error(_Printf_format_string_ const char *fmt, ...) {
    va_list vl;
    va_start(vl, fmt);
    TimedOutput(stderr, fmt, vl);
    va_end(vl);
}

#ifdef __ANDROID__

Eng::LogAndroid::LogAndroid(const char *log_tag) { strcpy(log_tag_, log_tag); }

void Eng::LogAndroid::Info(_Printf_format_string_ const char *fmt, ...) {
    va_list vl;
    va_start(vl, fmt);
    __android_log_vprint(ANDROID_LOG_INFO, log_tag_, fmt, vl);
    va_end(vl);
}

void Eng::LogAndroid::Warning(_Printf_format_string_ const char *fmt, ...) {
    va_list vl;
    va_start(vl, fmt);
    __android_log_vprint(ANDROID_LOG_INFO, log_tag_, fmt, vl);
    va_end(vl);
}

void Eng::LogAndroid::Error(_Printf_format_string_ const char *fmt, ...) {
    va_list vl;
    va_start(vl, fmt);
    __android_log_vprint(ANDROID_LOG_ERROR, log_tag_, fmt, vl);
    va_end(vl);
}

#endif