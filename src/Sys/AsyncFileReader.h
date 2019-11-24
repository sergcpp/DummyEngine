#pragma once

#include <memory>

namespace Sys {
    class AsyncFileReaderImpl;

    class AsyncFileReader {
        std::unique_ptr<AsyncFileReaderImpl> impl_;
    public:
        AsyncFileReader() noexcept;
        ~AsyncFileReader();

        bool ReadFile(const char *file_path, size_t max_size, void *out_data, size_t &out_size);
    };
}