#include "DynLib.h"

#include <string>

#if defined(_WIN32)
#include "Windows.h"
#elif defined(__unix__) || defined(__APPLE__)
#include "dlfcn.h"
#endif

Sys::DynLib::DynLib() {
#if defined(_WIN32) || defined(__unix__) || defined(__APPLE__)
    handle_ = nullptr;
#endif
}

Sys::DynLib::DynLib(const char *name) {
    std::string name_with_ext;
    const char *ext = strrchr(name, '.');
    if (!ext) {
        name_with_ext = name;
        // Attach platform-preferred extension
#if defined(_WIN32)
        name_with_ext += ".dll";
#elif defined(__unix__)
        name_with_ext += ".so";
#elif defined(__APPLE__)
        name_with_ext += ".dylib";
#endif
        name = name_with_ext.c_str();
    }
#if defined(_WIN32)
    handle_ = LoadLibraryA(name);
#elif defined(__unix__) || defined(__APPLE__)
    handle_ = dlopen(name, RTLD_LAZY);
#endif
}

Sys::DynLib::DynLib(DynLib &&rhs) noexcept {
#if defined(_WIN32) || defined(__unix__) || defined(__APPLE__)
    handle_ = rhs.handle_;
    rhs.handle_ = nullptr;
#endif
}

Sys::DynLib &Sys::DynLib::operator=(DynLib &&rhs) noexcept {
#if defined(_WIN32)
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
#if defined(_WIN32)
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
#if defined(_WIN32) || defined(__unix__) || defined(__APPLE__)
    return handle_ != nullptr;
#endif
}

void *Sys::DynLib::GetProcAddress(const char *name) {
#if defined(_WIN32)
    return (void *)::GetProcAddress(handle_, name);
#elif defined(__unix__) || defined(__APPLE__)
    return dlsym(handle_, name);
#endif
}
