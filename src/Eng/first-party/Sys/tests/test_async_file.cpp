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

    { // error: non-existent file (blocking buf variant)
        AsyncFileReader reader;
        DefaultFileReadBuf buf;
        require(!reader.ReadFileBlocking("__no_such_file__.bin", 0, WholeFile, buf));
    }

    { // error: non-existent file (blocking void* variant)
        AsyncFileReader reader;
        char tmp[64];
        size_t sz = sizeof(tmp);
        require(!reader.ReadFileBlocking("__no_such_file__.bin", 0, WholeFile, tmp, sz));
    }

    { // error: read_offset past end of file
        AsyncFileReader reader;
        DefaultFileReadBuf buf;
        require(!reader.ReadFileBlocking(test_file_name, test_file_size + 1, WholeFile, buf));
    }

    { // error: read_offset past end of file (non-blocking)
        AsyncFileReader reader;
        DefaultFileReadBuf buf;
        FileReadEvent event;
        require(!reader.ReadFileNonBlocking(test_file_name, test_file_size + 1, WholeFile, buf, event));
    }

    { // partial read with sector-aligned offset (blocking buf variant)
        const size_t read_off = 4096 * 1000; // multiple of sector size and of pattern period
        const size_t read_len = 1000 * 1000;

        AsyncFileReader reader;
        DefaultFileReadBuf buf;
        require(reader.ReadFileBlocking(test_file_name, read_off, read_len, buf));
        require(buf.data_len() == read_len);

        for (size_t i = 0; i < buf.data_len(); i += 1000) {
            require(memcmp(&buf.data()[i], &test_data[0], 1000) == 0);
        }
    }

    { // partial read with sector-aligned offset (blocking void* variant)
        const size_t read_off = 4096 * 1000;
        const size_t read_len = 1000 * 1000;

        AsyncFileReader reader;
        std::unique_ptr<char[]> tmp(new char[read_len]);
        size_t sz = read_len;
        require(reader.ReadFileBlocking(test_file_name, read_off, read_len, tmp.get(), sz));
        require(sz == read_len);

        for (size_t i = 0; i < sz; i += 1000) {
            require(memcmp(&tmp[i], &test_data[0], 1000) == 0);
        }
    }

    { // partial read with unaligned offset (tests internal data_off_ alignment)
        const size_t read_off = 4096 + 512; // not sector-aligned
        const size_t read_len = 1000 * 1000;

        AsyncFileReader reader;
        DefaultFileReadBuf buf;
        require(reader.ReadFileBlocking(test_file_name, read_off, read_len, buf));
        require(buf.data_len() == read_len);

        // verify data at the given offset: pattern repeats every 1000 bytes
        const size_t pattern_off = read_off % 1000;
        for (size_t i = 0; i < read_len; ++i) {
            require(buf.data()[i] == test_data[(pattern_off + i) % 1000]);
        }
    }

    { // partial non-blocking read with offset
        const size_t read_off = 4096 * 2000;
        const size_t read_len = 1000 * 1000;

        AsyncFileReader reader;
        DefaultFileReadBuf buf;
        FileReadEvent event;

        require(reader.ReadFileNonBlocking(test_file_name, read_off, read_len, buf, event));

        size_t bytes_read;
        require(event.GetResult(true, &bytes_read) == eFileReadResult::Successful);
        require(buf.data_len() == read_len);

        for (size_t i = 0; i < buf.data_len(); i += 1000) {
            require(memcmp(&buf.data()[i], &test_data[0], 1000) == 0);
        }
    }

    { // FileReadEvent move assignment — result survives move
        AsyncFileReader reader;
        DefaultFileReadBuf buf;
        FileReadEvent event1;

        require(reader.ReadFileNonBlocking(test_file_name, 0, WholeFile, buf, event1));

        FileReadEvent event2;
        event2 = std::move(event1); // move — event1 is now empty

        size_t bytes_read;
        require(event2.GetResult(true, &bytes_read) == eFileReadResult::Successful);
        require(buf.data_len() == test_file_size);
    }

    { // FileReadEvent destroyed while a non-blocking read is in-flight (no crash/leak)
        AsyncFileReader reader;
        DefaultFileReadBuf buf;
        {
            FileReadEvent event;
            require(reader.ReadFileNonBlocking(test_file_name, 0, WholeFile, buf, event));
            // event goes out of scope here — destructor must cancel cleanly
        }
    }

    // remove test file
    std::remove(test_file_name);

    printf("OK\n");
}
