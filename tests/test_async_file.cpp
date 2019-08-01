#include "test_common.h"

#include <cstring>
#include <fstream>

#include "../AssetFile.h"
#include "../AsyncFileReader.h"
#include "../Log.h"

void test_async_file() {
    const char *test_file_name = "test.bin";
    size_t test_file_size = 64 * 1024 * 1024;

    char test_data[1024];
    for (int j = 0; j < 1024; j++) {
        test_data[j] = (char)(rand() % 255);
    }

    {   // create test file
        std::ofstream out_file(test_file_name, std::ios::binary);

        for (size_t i = 0; i < test_file_size; i += 1024) {
            out_file.write(test_data, sizeof(test_data));
        }
    }

    std::unique_ptr<char[]> file_data_buf;
    size_t file_data_buf_size = test_file_size;
    file_data_buf.reset(new char[file_data_buf_size]);

    {   // read file
        Sys::AsyncFileReader reader;
        size_t file_size = 0;

        require(reader.ReadFile(test_file_name, file_data_buf_size, file_data_buf.get(), file_size));
        require(file_size == test_file_size);

        for (size_t i = 0; i < file_size; i += 1024) {
            require(memcmp(&file_data_buf[i], &test_data[0], 1024) == 0);
        }
    }

    {   // remove test file
        remove(test_file_name);
    }

    volatile int ii = 0;
}