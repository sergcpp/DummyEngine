#pragma once

#include <cstdio>

#include <Ren/Log.h>
#include <Snd/Log.h>

namespace Eng {
class ILog : public Ren::ILog, public Snd::ILog {
  public:
    void Info(const char *fmt, ...) override {}
    void Warning(const char *fmt, ...) override {}
    void Error(const char *fmt, ...) override {}
};
class LogStdout : public ILog {
  protected:
    void TimedOutput(FILE *dst, const char *fmt, va_list args);
  public:
    void Info(const char *fmt, ...) override;
    void Warning(const char *fmt, ...) override;
    void Error(const char *fmt, ...) override;
};

#ifdef __ANDROID__
class LogAndroid : public Ren::ILog, public Snd::ILog {
    char log_tag_[32];

  public:
    LogAndroid(const char *log_tag);

    void Info(const char *fmt, ...) override;
    void Warning(const char *fmt, ...) override;
    void Error(const char *fmt, ...) override;
};
#endif
} // namespace Eng