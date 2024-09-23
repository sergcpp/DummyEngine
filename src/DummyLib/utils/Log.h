#pragma once

#include <Eng/Log.h>
#include <Ray/Log.h>

#if defined(_MSC_VER)
#include <sal.h>
#define CHECK_FORMAT_STRING(format_string_index_one_based, vargs_index_one_based)
#else
#define _Printf_format_string_
#define CHECK_FORMAT_STRING(format_string_index_one_based, vargs_index_one_based)                                      \
    __attribute__((format(printf, format_string_index_one_based, vargs_index_one_based)))
#endif

class ILog : public Eng::LogStdout, public Ray::ILog {
  public:
    void CHECK_FORMAT_STRING(2, 3) Info(_Printf_format_string_ const char *fmt, ...) override {}
    void CHECK_FORMAT_STRING(2, 3) Warning(_Printf_format_string_ const char *fmt, ...) override {}
    void CHECK_FORMAT_STRING(2, 3) Error(_Printf_format_string_ const char *fmt, ...) override {}
};

class LogStdout : public ILog {
  public:
    void CHECK_FORMAT_STRING(2, 3) Info(_Printf_format_string_ const char *fmt, ...) override {
        va_list vl;
        va_start(vl, fmt);
        Eng::LogStdout::TimedOutput(stdout, fmt, vl);
        va_end(vl);
    }
    void CHECK_FORMAT_STRING(2, 3) Warning(_Printf_format_string_ const char *fmt, ...) override {
        va_list vl;
        va_start(vl, fmt);
        Eng::LogStdout::TimedOutput(stdout, fmt, vl);
        va_end(vl);
    }
    void CHECK_FORMAT_STRING(2, 3) Error(_Printf_format_string_ const char *fmt, ...) override {
        va_list vl;
        va_start(vl, fmt);
        Eng::LogStdout::TimedOutput(stderr, fmt, vl);
        va_end(vl);
    }
};

#undef CHECK_FORMAT_STRING
#if !defined(_MSC_VER)
#undef _Printf_format_string_
#endif