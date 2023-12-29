#include "Pack.h"

#include <cassert>

#include <memory>

#include "AssetFile.h"

void Sys::ReadPackage(const char *pack_name, onfile_func on_file) {
    AssetFile in_file(pack_name, eOpenMode::In);
    size_t file_size = in_file.size();
    uint32_t num_files;
    in_file.Read((char *)&num_files, sizeof(uint32_t));

    std::vector<FileDesc> file_list;

    // size_t file_pos = sizeof(uint32_t) + num_files * (120 + 2 * sizeof(uint32_t));
    for (unsigned i = 0; i < num_files; i++) {
        file_list.emplace_back();
        FileDesc &f = file_list.back();
        in_file.Read((char *)&f, sizeof(FileDesc));
        assert(f.off + f.size <= file_size);
    }

    for (auto &f : file_list) {
        std::unique_ptr<char[]> buf(new char[f.size]);
        in_file.SeekAbsolute(f.off);
        in_file.Read(buf.get(), f.size);

        on_file(f.name, buf.get(), f.size);
    }
}

#ifndef __ANDROID__
void Sys::WritePackage(const char *pack_name, std::vector<std::string> &file_list) {
    AssetFile out_file(pack_name, eOpenMode::Out);
    uint32_t num_files = (uint32_t)file_list.size();
    out_file.Write((char *)&num_files, sizeof(uint32_t));

    size_t file_pos = sizeof(uint32_t) + num_files * (120 + 2 * sizeof(uint32_t));
    for (auto &f : file_list) {
        assert(f.length() < 124);
        AssetFile in_file(f.c_str(), eOpenMode::In);
        char name[120]{};
        strcpy(name, f.c_str());
        const auto file_size = uint32_t(in_file.size());
        out_file.Write(name, sizeof(name));
        out_file.Write((char *)&file_pos, sizeof(uint32_t));
        out_file.Write((char *)&file_size, sizeof(uint32_t));
        file_pos += file_size;
    }

    for (auto &f : file_list) {
        AssetFile in_file(f.c_str(), eOpenMode::In);
        std::unique_ptr<char[]> buf(new char[in_file.size()]);
        in_file.Read(buf.get(), in_file.size());
        out_file.Write(buf.get(), in_file.size());
    }
}
#endif

std::vector<Sys::FileDesc> Sys::EnumFilesInPackage(const char *pack_name) {
    AssetFile in_file(pack_name, eOpenMode::In);
    size_t file_size = in_file.size();
    uint32_t num_files;
    in_file.Read((char *)&num_files, sizeof(uint32_t));

    std::vector<FileDesc> file_list;

    // size_t file_pos = sizeof(uint32_t) + num_files * (120 + 2 * sizeof(uint32_t));
    for (unsigned i = 0; i < num_files; i++) {
        file_list.emplace_back();
        FileDesc &f = file_list.back();
        in_file.Read((char *)&f, sizeof(FileDesc));
        assert(f.off + f.size <= file_size);
    }

    return file_list;
}

bool Sys::ReadFromPackage(const char *pack_name, const char *fname, const size_t pos,
                          char *buf, const size_t /*size*/) {
    AssetFile in_file(pack_name, eOpenMode::In);
    // size_t file_size = in_file.size();
    uint32_t num_files;
    in_file.Read((char *)&num_files, sizeof(uint32_t));

    // size_t file_pos = sizeof(uint32_t) + num_files * (120 + 2 * sizeof(uint32_t));
    for (unsigned i = 0; i < num_files; i++) {
        char name[120]{};
        in_file.Read(name, sizeof(name));
        uint32_t off, size;
        in_file.Read((char *)&off, sizeof(uint32_t));
        in_file.Read((char *)&size, sizeof(uint32_t));
        if (strcmp(name, fname) == 0) {
            in_file.SeekAbsolute(off + pos);
            in_file.Read(buf, size);
            return true;
        }
    }
    return false;
}
