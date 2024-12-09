#include "test_common.h"

#include <cstring>
#include <fstream>
#include <thread>

#include "../AssetFile.h"
#include "../AsyncFileReader.h"

void test_async_file() {
    using namespace Sys;

    printf("Test async_file         | ");

    const char *test_file_name = "test.bin";
    const size_t test_file_size = 64 * 1000 * 1000;

    uint8_t test_data[1000];
    for (uint8_t &j : test_data) {
        j = uint8_t(rand() % 256);
    }

    { // create test file
        std::ofstream out_file(test_file_name, std::ios::binary);

        for (size_t i = 0; i < test_file_size; i += 1000) {
            out_file.write((char *)test_data, sizeof(test_data));
        }
    }

    { // read file (blocking 1)
        AsyncFileReader reader;
        DefaultFileReadBuf buf;

        require(reader.ReadFileBlocking(test_file_name, 0 /* read_offset */, WholeFile, buf));
        require(buf.data_len() == test_file_size);

        for (size_t i = 0; i < buf.data_len(); i += 1000) {
            require(memcmp(&buf.data()[i], &test_data[0], 1000) == 0);
        }
    }

    { // read file (blocking 2)
        std::unique_ptr<char[]> file_data_buf;
        const size_t file_data_buf_size = test_file_size;
        file_data_buf = std::make_unique<char[]>(file_data_buf_size);

        AsyncFileReader reader;

        size_t file_size = file_data_buf_size;
        require(
            reader.ReadFileBlocking(test_file_name, 0 /* read_offset */, WholeFile, file_data_buf.get(), file_size));
        require(file_size == test_file_size);

        for (size_t i = 0; i < file_size; i += 1000) {
            require(memcmp(&file_data_buf[i], &test_data[0], 1000) == 0);
        }
    }

    { // read file (non-blocking 1)
        AsyncFileReader reader;
        DefaultFileReadBuf buf;
        FileReadEvent event;

        require(reader.ReadFileNonBlocking(test_file_name, 0 /* read_offset */, WholeFile, buf, event));

        size_t bytes_read;
        require(event.GetResult(true, &bytes_read) == eFileReadResult::Successful);

        require(bytes_read == test_file_size);
        require(buf.data_len() == test_file_size);

        for (size_t i = 0; i < buf.data_len(); i += 1000) {
            require(memcmp(&buf.data()[i], &test_data[0], 1000) == 0);
        }
    }

    { // read file (non-blocking 2)
        AsyncFileReader reader;
        DefaultFileReadBuf buf;
        FileReadEvent event;

        require(reader.ReadFileNonBlocking(test_file_name, 0 /* read_offset */, WholeFile, buf, event));

        size_t bytes_read;
        while (event.GetResult(false /* block */, &bytes_read) == eFileReadResult::Pending) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        require(bytes_read == test_file_size);
        require(buf.data_len() == test_file_size);

        for (size_t i = 0; i < buf.data_len(); i += 1000) {
            require(memcmp(&buf.data()[i], &test_data[0], 1000) == 0);
        }
    }

    // remove test file
    std::remove(test_file_name);

    printf("OK\n");
}
