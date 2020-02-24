#include "AssetFile.h"

#include <cassert>
#include <cstring>
#include <stdexcept>

#ifdef __ANDROID__
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#else
#include <fstream>
#include <iostream>
#endif

#include "Pack.h"

namespace Sys {
struct Package {
    std::string name;
    std::vector<Sys::FileDesc> file_list;
};

std::vector<Package> added_packages;
} // namespace Sys

#ifdef __ANDROID__
AAssetManager *Sys::AssetFile::asset_manager_ = nullptr;
#endif

Sys::AssetFile::AssetFile(const char *file_name, const eOpenMode mode) {
    Open(file_name, mode);
}

Sys::AssetFile::~AssetFile() {
    Close();
}

Sys::AssetFile::AssetFile(AssetFile &&rhs) noexcept {
    (*this) = std::move(rhs);
}

Sys::AssetFile& Sys::AssetFile::operator=(AssetFile&& rhs) noexcept {
    Close();

#ifdef __ANDROID__
    asset_file_ = rhs.asset_file_;
    rhs.asset_file_ = nullptr;
#else
    file_stream_ = rhs.file_stream_;
    rhs.file_stream_ = nullptr;
#endif
    mode_ = rhs.mode_;
    rhs.mode_ = eOpenMode::None;
    name_ = std::move(rhs.name_);
    size_ = rhs.size_;
    rhs.size_ = 0;
    pos_override_ = rhs.pos_override_;
    rhs.pos_override_ = 0;

    return (*this);
}

bool Sys::AssetFile::Open(const char* file_name, const eOpenMode mode) {
    using namespace std;

    Close();

    if (mode == eOpenMode::In) {
#ifdef __ANDROID__
        if (file_name[0] == '.' && file_name[1] == '/') {
            file_name += 2;
        }

        if (strstr(file_name, "assets/") == file_name) {
            file_name += 7;
        }

        string full_path;
        if (strstr(file_name, "..")) {
            char path[100];
            char* toks[16];
            int num_toks = 0;
            strcpy(path, file_name);
            char* p = strtok(path, "/");
            while (p) {
                if (strstr(p, "..")) {
                    num_toks -= 1;
                    assert(num_toks >= 0);
                } else {
                    toks[num_toks++] = p;
                }
                p = strtok(NULL, "/");
            }
            for (int i = 0; i < num_toks; i++) {
                full_path.append(toks[i]);
                if (i != num_toks - 1) {
                    full_path.append("/");
                }
            }
            file_name = full_path.c_str();
        }

        asset_file_ =
            AAssetManager_open(asset_manager_, file_name, AASSET_MODE_STREAMING);
        if (asset_file_) {
            size_ = AAsset_getLength(asset_file_);
        } else {
            size_ = 0;
        }
#else
        file_stream_ = new std::fstream();

        bool found_in_package = false;

        string fname = file_name;
        for (Package& p : added_packages) {
            for (FileDesc& f : p.file_list) {
                if (fname == f.name) {
                    file_stream_->open(p.name, std::ios::in | std::ios::binary);
                    if (file_stream_->good()) {
                        file_stream_->seekg(f.off, ios::beg);
                        pos_override_ = f.off;
                        size_ = f.size;
                        found_in_package = true;
                        break;
                }
            }
        }
            if (found_in_package)
                break;
        }

        if (!found_in_package) {
            file_stream_->open(file_name, std::ios::in | std::ios::binary);
            file_stream_->seekg(0, std::ios::end);
            size_ = (size_t)file_stream_->tellg();
            file_stream_->seekg(0, std::ios::beg);
        }
#endif
    } else if (mode == eOpenMode::Out) {
#ifdef __ANDROID__
        throw std::runtime_error("Cannot write to assets folder on Android!");
#else
        file_stream_ = new std::fstream();
        file_stream_->open(file_name, std::ios::out | std::ios::binary);
#endif
    }

    mode_ = mode;
    name_ = file_name;

    return size_ != 0;
}

void Sys::AssetFile::Close() {
#ifdef __ANDROID__
    if (asset_file_) {
        AAsset_close(asset_file_);
    }
#else
    delete file_stream_;
#endif
    mode_ = eOpenMode::None;
    size_ = 0;
}

size_t Sys::AssetFile::Read(char *buf, const size_t size) {
#ifdef __ANDROID__
    return size_t(AAsset_read(asset_file_, buf, size));
#else
    if (!file_stream_) {
        return 0;
    }
    file_stream_->read(buf, size);
    return size_t(file_stream_->gcount());
#endif
}

void Sys::AssetFile::SeekAbsolute(const uint64_t pos) {
#ifdef __ANDROID__
    AAsset_seek(asset_file_, pos, SEEK_SET);
#else
    file_stream_->seekg(pos_override_ + pos);
#endif
}

void Sys::AssetFile::SeekRelative(const int64_t off) {
#ifdef __ANDROID__
    AAsset_seek(asset_file_, off, SEEK_CUR);
#else
    file_stream_->seekg(off, std::ios::cur);
#endif
}

Sys::AssetFile::operator bool() {
#ifdef __ANDROID__
    return asset_file_ && bool(AAsset_getLength(asset_file_));
#else
    return file_stream_ && file_stream_->good();
#endif
}

#ifndef __ANDROID__
bool Sys::AssetFile::Write(const char *buf, const size_t size) {
    if (!file_stream_) {
        return false;
    }
    file_stream_->write(buf, size);
    return file_stream_->good();
}
#endif

size_t Sys::AssetFile::pos() {
#ifdef __ANDROID__
    return AAsset_seek(asset_file_, 0, SEEK_CUR);
#else
    return size_t(file_stream_->tellg()) - pos_override_;
#endif
}

#ifdef __ANDROID__
void Sys::AssetFile::InitAssetManager(class AAssetManager *am) { asset_manager_ = am; }
int32_t Sys::AssetFile::descriptor(off_t *start, off_t *len) {
    return AAsset_openFileDescriptor(asset_file_, start, len);
}
#endif

void Sys::AssetFile::AddPackage(const char *name) {
    size_t ln = strlen(name);
    if (ln < 6 || name[ln - 5] != '.' || name[ln - 4] != 'p' || name[ln - 3] != 'a' ||
        name[ln - 2] != 'c' || name[ln - 1] != 'k') {
        throw std::runtime_error("Invalid package file!");
    }
    added_packages.emplace_back();
    Package &p = added_packages.back();
    p.name = name;
    p.file_list = Sys::EnumFilesInPackage(name);
}

void Sys::AssetFile::RemovePackage(const char *name) {
    for (auto it = added_packages.begin(); it != added_packages.end(); ++it) {
        if (it->name == name) {
            added_packages.erase(it);
            return;
        }
    }
}