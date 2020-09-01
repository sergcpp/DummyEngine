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
enum class eOpenMode { None, In, Out };

// TODO: replace this with stream ???
class AssetFile {
#ifdef __ANDROID__
    static AAssetManager *asset_manager_;
    AAsset *asset_file_ = nullptr;
#else
    std::fstream *file_stream_ = nullptr;
#endif
    eOpenMode mode_ = eOpenMode::None;
    std::string name_;
    size_t size_ = 0, pos_override_ = 0;

  public:
    AssetFile() = default;
    explicit AssetFile(const char *file_name, eOpenMode mode = eOpenMode::In);
    explicit AssetFile(const std::string &file_name, eOpenMode mode = eOpenMode::In)
        : AssetFile(file_name.c_str(), mode) {}
    AssetFile(const AssetFile &) = delete;
    AssetFile(AssetFile &&rhs) noexcept;
    AssetFile &operator=(const AssetFile &) = delete;
    AssetFile &operator=(AssetFile &&rhs) noexcept;

    ~AssetFile();

    size_t size() const { return size_; }

    eOpenMode mode() const { return mode_; }

    std::string name() { return name_; }

    size_t pos();

    bool Open(const char* file_name, eOpenMode mode = eOpenMode::In);
    void Close();

    size_t Read(char *buf, size_t size);

#ifndef __ANDROID__
    bool Write(const char *buf, size_t size);
#endif

    void SeekAbsolute(size_t pos);
    void SeekRelative(int64_t off);

    operator bool();

    static void AddPackage(const char *name);
    static void RemovePackage(const char *name);
#ifdef __ANDROID__
    static void InitAssetManager(class AAssetManager *);
    int32_t descriptor(off_t *start, off_t *len);
#endif
};
} // namespace Sys
