#pragma once

#include <Eng/Log.h>
#include <Ray/Log.h>

class ILog : public Eng::LogStdout, public Ray::ILog {
  public:
    void Info(const char *fmt, ...) override {}
    void Warning(const char *fmt, ...) override {}
    void Error(const char *fmt, ...) override {}
};

class LogStdout : public ILog {
  public:
    void Info(const char *fmt, ...) override {
        va_list vl;
        va_start(vl, fmt);
        Eng::LogStdout::TimedOutput(stdout, fmt, vl);
        va_end(vl);
    }
    void Warning(const char *fmt, ...) override {
        va_list vl;
        va_start(vl, fmt);
        Eng::LogStdout::TimedOutput(stdout, fmt, vl);
        va_end(vl);
    }
    void Error(const char *fmt, ...) override {
        va_list vl;
        va_start(vl, fmt);
        Eng::LogStdout::TimedOutput(stderr, fmt, vl);
        va_end(vl);
    }
};