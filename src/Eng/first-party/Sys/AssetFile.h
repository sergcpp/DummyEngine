#pragma once

#include <cstddef>
#include <string>
#include <string_view>

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
    explicit AssetFile(std::string_view file_name, eOpenMode mode = eOpenMode::In);
    AssetFile(const AssetFile &) = delete;
    AssetFile(AssetFile &&rhs) noexcept;
    AssetFile &operator=(const AssetFile &) = delete;
    AssetFile &operator=(AssetFile &&rhs) noexcept;

    ~AssetFile();

    [[nodiscard]] size_t size() const { return size_; }

    [[nodiscard]] eOpenMode mode() const { return mode_; }

    [[nodiscard]] std::string name() { return name_; }

    [[nodiscard]] size_t pos();

    bool Open(std::string_view file_name, eOpenMode mode = eOpenMode::In);
    void Close();

    size_t Read(char *buf, size_t size);

#ifndef __ANDROID__
    bool Write(const char *buf, size_t size);
#endif

    void SeekAbsolute(uint64_t pos);
    void SeekRelative(int64_t off);

    explicit operator bool();

    static void AddPackage(const char *name);
    static void RemovePackage(const char *name);
#ifdef __ANDROID__
    static void InitAssetManager(class AAssetManager *);
    int32_t descriptor(off_t *start, off_t *len);
#endif
};
} // namespace Sys
