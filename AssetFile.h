#pragma once

#include <cstddef>
#include <string>

#if defined(__ANDROID__)
class AAsset;
class AAssetManager;
#else
#include <iosfwd>
#endif

namespace Sys {
class CannotOpenFileException;

// TODO: replace this with stream ???
class AssetFile {
#ifdef __ANDROID__
    static AAssetManager* asset_manager_;
    AAsset* asset_file_ = nullptr;
#else
    std::fstream *file_stream_ = nullptr;
#endif
    int mode_ = -1;
    std::string name_;
    size_t size_ = 0, pos_override_ = 0;
public:
    explicit AssetFile(const char *file_name, int mode = FileIn);
    explicit AssetFile(const std::string &file_name, int mode = FileIn) : AssetFile(file_name.c_str(), mode) {}
    AssetFile(const AssetFile &) = delete;
    AssetFile &operator=(const AssetFile &) = delete;

    ~AssetFile();

    size_t size() {
        return size_;
    }

    inline int mode() {
        return mode_;
    }

    std::string name() {
        return name_;
    }

    size_t pos();

    bool Read(char *buf, size_t size);

    bool ReadFloat(float &f);

#ifndef __ANDROID__
    bool Write(const char *buf, size_t size);
#endif

    void Seek(size_t pos);

    operator bool();

    enum {
        FileIn, FileOut
    };

    static void AddPackage(const char *name);
    static void RemovePackage(const char *name);
#ifdef __ANDROID__
    static void InitAssetManager(class AAssetManager*);
    int32_t descriptor(off_t *start, off_t *len);
#endif
};
}

