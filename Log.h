#pragma once

#ifdef __ANDROID__
#include <android/log.h>
#define  LOG_TAG    "libjni"
#define  LOGI(...)  __android_log_print(ANDROID_LOG_INFO,LOG_TAG,__VA_ARGS__)
#define  LOGE(...)  __android_log_print(ANDROID_LOG_ERROR,LOG_TAG,__VA_ARGS__)
#else
#include <cstdio>
#include <ctime>
//#define  LOGI(...) fprintf(stdout, "\n" __VA_ARGS__)

//tm _tm;                                                                     \
//localtime_s(&_tm, &now);                                                    \

#define  LOGI(...) {                                                            \
    char buff[27];                                                              \
    time_t now = time(0);                                                       \
    strftime(buff, sizeof(buff), "%Y-%m-%d %H:%M:%S.000 | ", localtime(&now));  \
    fputs(buff, stdout); fprintf(stdout, __VA_ARGS__); fprintf(stdout, "\n"); }
#define  LOGE(...) {                                                            \
    char buff[27];                                                              \
    time_t now = time(0);                                                       \
    strftime(buff, sizeof(buff), "%Y-%m-%d %H:%M:%S.000 | ", localtime(&now));  \
    fputs(buff, stderr); fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n");}
#endif

