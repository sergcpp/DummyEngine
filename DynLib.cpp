#include "DynLib.h"

#if defined(WIN32)
#include "Windows.h"
#elif defined(__unix__) || defined(__APPLE__)
#include "dlfcn.h"
#endif

Sys::DynLib::DynLib() {
#if defined(WIN32) || defined(__unix__) || defined(__APPLE__)
    handle_ = nullptr;
#endif
}

Sys::DynLib::DynLib(const char *name) {
#if defined(WIN32)
    handle_ = LoadLibraryA(name);
#elif defined(__unix__) || defined(__APPLE__)
    handle_ = dlopen(name, RTLD_LAZY);
#endif
}

Sys::DynLib::DynLib(DynLib &&rhs) noexcept {
#if defined(WIN32) || defined(__unix__) || defined(__APPLE__)
    handle_ = rhs.handle_;
    rhs.handle_ = nullptr;
#endif
}

Sys::DynLib &Sys::DynLib::operator=(DynLib &&rhs) noexcept {
#if defined(WIN32)
    if (handle_) {
        FreeLibrary(handle_);
        handle_ = nullptr;
    }
    handle_ = rhs.handle_;
    rhs.handle_ = nullptr;
#elif defined(__unix__) || defined(__APPLE__)
    if (handle_) {
        dlclose(handle_);
        handle_ = nullptr;
    }
    handle_ = rhs.handle_;
    rhs.handle_ = nullptr;
#endif
    return *this;
}

Sys::DynLib::~DynLib() {
#if defined(WIN32)
    FreeLibrary(handle_);
    handle_ = nullptr;
#elif defined(__unix__) || defined(__APPLE__)
    if (handle_) {
        dlclose(handle_);
        handle_ = nullptr;
    }
#endif
}

Sys::DynLib::operator bool() const {
#if defined(WIN32) || defined(__unix__) || defined(__APPLE__)
    return handle_ != nullptr;
#endif
}

void *Sys::DynLib::GetProcAddress(const char *name) {
#if defined(WIN32)
    return (void *)::GetProcAddress(handle_, name);
#elif defined(__unix__) || defined(__APPLE__)
    return dlsym(handle_, name);
#endif
}
