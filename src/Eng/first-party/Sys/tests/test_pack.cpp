#include "test_common.h"

#include <algorithm>
#include <fstream>
#include <memory>

#include "../AssetFile.h"
#include "../Pack.h"

namespace {
std::vector<std::string> file_list = {"./constant.fs", "./src/CMakeLists.txt"};
}

void test_pack() {

    /*{
        // Save/Load package
        Sys::WritePackage("./my_pack.pack", file_list);

        auto OnFile = [](const char *name, void *data, int size) {
            auto it = std::find(file_list.begin(), file_list.end(), name);
            require(it != file_list.end());

            Sys::AssetFile in_file(name, Sys::eOpenMode::In);
            require(in_file.size() == size);

            std::unique_ptr<char[]> buf(new char[size]);
            in_file.Read(buf.get(), (size_t)size);
            const char *p1 = reinterpret_cast<char *>(data);
            const char *p2 = buf.get();
            require(memcmp(p1, p2, (size_t)size) == 0);
        };
        Sys::ReadPackage("./my_pack.pack", OnFile);

        std::vector<Sys::FileDesc> list = Sys::EnumFilesInPackage("./my_pack.pack");

        require(std::string(list[0].name) == "./constant.fs");
        require(std::string(list[1].name) == "./src/CMakeLists.txt");
    }

    {
        // Add package to AssetFile
        Sys::AssetFile::AddPackage("./my_pack.pack");

        Sys::AssetFile in_file1("./constant.fs", Sys::eOpenMode::In);
        std::ifstream in_file2("./constant.fs", std::ios::ate | std::ios::binary);
        size_t size = (size_t)in_file2.tellg();
        in_file2.seekg(0, std::ios::beg);
        require(in_file1.size() == size);

        std::unique_ptr<char[]> buf1(new char[size]), buf2(new char[size]);
        require(in_file1.Read(buf1.get(), size));
        in_file2.read(buf2.get(), size);
        require(memcmp(buf1.get(), buf2.get(), size) == 0);
    }*/
}
