#pragma once

#if defined(_WIN32)
#ifndef _WINDEF_
struct HINSTANCE__; // Forward or never
typedef HINSTANCE__* HINSTANCE;
#endif
#endif

namespace Sys {
class DynLib {
#if defined(_WIN32)
    HINSTANCE handle_;
#elif defined(__unix__) || defined(__APPLE__)
    void *handle_;
#endif
public:
    DynLib();
    explicit DynLib(const char *name);

    DynLib(const DynLib &rhs) = delete;
    DynLib(DynLib &&rhs) noexcept;

    DynLib &operator=(const DynLib &rhs) = delete;
    DynLib &operator=(DynLib &&rhs) noexcept;

    ~DynLib();

    explicit operator bool() const;

    void *GetProcAddress(const char *name);
};
}

#if defined _WIN32 || defined __CYGWIN__
#ifdef __GNUC__
#define DLL_EXPORT __attribute__ ((dllexport))
#define DLL_IMPORT __attribute__ ((dllimport))
#else
#define DLL_EXPORT __declspec(dllexport)
#define DLL_IMPORT __declspec(dllimport)
#endif
#define DLL_LOCAL
#else
#if __GNUC__ >= 4
#define DLL_EXPORT __attribute__ ((visibility ("default")))
#define DLL_IMPORT __attribute__ ((visibility ("default")))
#define DLL_LOCAL  __attribute__ ((visibility ("hidden")))
#else
#define DLL_EXPORT
#define DLL_IMPORT
#define DLL_LOCAL
#endif
#endif

#ifdef BUILD_DLL
#define DLL_PUBLIC DLL_EXPORT
#else
#define DLL_PUBLIC DLL_IMPORT
#endif
