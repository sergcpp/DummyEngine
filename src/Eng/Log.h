#pragma once

#include <cstdio>

#include <Ren/Log.h>
#include <Snd/Log.h>

#if defined(_MSC_VER)
#include <sal.h>
#define CHECK_FORMAT_STRING(format_string_index_one_based, vargs_index_one_based)
#else
#define _Printf_format_string_
#define CHECK_FORMAT_STRING(format_string_index_one_based, vargs_index_one_based)                                      \
    __attribute__((format(printf, format_string_index_one_based, vargs_index_one_based)))
#endif

namespace Eng {
class ILog : public Ren::ILog, public Snd::ILog {
  public:
    void CHECK_FORMAT_STRING(2, 3) Info(_Printf_format_string_ const char *fmt, ...) override {}
    void CHECK_FORMAT_STRING(2, 3) Warning(_Printf_format_string_ const char *fmt, ...) override {}
    void CHECK_FORMAT_STRING(2, 3) Error(_Printf_format_string_ const char *fmt, ...) override {}
};

class LogStdout : public ILog {
  protected:
    void TimedOutput(FILE *dst, const char *fmt, va_list args);
  public:
    void CHECK_FORMAT_STRING(2, 3) Info(_Printf_format_string_ const char *fmt, ...) override;
    void CHECK_FORMAT_STRING(2, 3) Warning(_Printf_format_string_ const char *fmt, ...) override;
    void CHECK_FORMAT_STRING(2, 3) Error(_Printf_format_string_ const char *fmt, ...) override;
};

#ifdef __ANDROID__
class LogAndroid : public Ren::ILog, public Snd::ILog {
    char log_tag_[32];

  public:
    LogAndroid(const char *log_tag);

    void CHECK_FORMAT_STRING(2, 3) Info(_Printf_format_string_ const char *fmt, ...) override;
    void CHECK_FORMAT_STRING(2, 3) Warning(_Printf_format_string_ const char *fmt, ...) override;
    void CHECK_FORMAT_STRING(2, 3) Error(_Printf_format_string_ const char *fmt, ...) override;
};
#endif
} // namespace Eng