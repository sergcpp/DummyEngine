#pragma once

#include <memory>

namespace Sys {
class AsyncFileReaderImpl;

const size_t WholeFile = std::numeric_limits<size_t>::max();

class FileReadBufBase {
  protected:
    uint8_t *mem_ = nullptr;

    size_t data_off_ = 0;
    size_t data_len_ = 0;

    uint32_t chunk_size_ = 0;
    uint32_t chunk_count_ = 0;

  public:
    explicit FileReadBufBase(const uint32_t chunk_size = 0xffffffff)
        : chunk_size_(chunk_size) {
        if (chunk_size_ == 0xffffffff) {
            chunk_size_ = GetOptimalChunkSize();
        }
    }
    virtual ~FileReadBufBase() = default;

    FileReadBufBase(const FileReadBufBase &rhs) = delete;
    FileReadBufBase(FileReadBufBase &&rhs) = delete;

    uint32_t chunk_size() const { return chunk_size_; }
    uint32_t chunk_count() const { return chunk_count_; }

    uint8_t *chunk(const int i) { return mem_ + i * ptrdiff_t(chunk_size_); }

    void set_data_off(const size_t off) { data_off_ = off; }
    void set_data_len(const size_t len) { data_len_ = len; }

    uint8_t *data() { return mem_ + data_off_; }
    size_t data_off() const { return data_off_; }
    size_t data_len() const { return data_len_; }

    void Realloc(size_t new_size) {
        if (new_size <= size_t(chunk_size_) * chunk_count_) {
            return;
        }

        Free();

        chunk_count_ = uint32_t((new_size + chunk_size_ - 1) / chunk_size_);
        mem_ = Alloc(size_t(chunk_size_) * chunk_count_);
    }

    virtual uint8_t *Alloc(size_t new_size) = 0;
    virtual void Free() = 0;

    static uint32_t GetOptimalChunkSize();
};

class DefaultFileReadBuf : public FileReadBufBase {
  public:
    DefaultFileReadBuf() = default;
    ~DefaultFileReadBuf() override { Free(); }

    uint8_t *Alloc(size_t new_size) override;
    void Free() override;
};

enum class eFileReadResult { Failed = -1, Pending = 0, Successful = 1 };

class FileReadEvent {
#if defined(_WIN32)
    void *h_file_ = nullptr;
    void *ev_ = nullptr;
    char ov_[32] = {};
#elif defined(__linux__) || defined(__APPLE__)
    unsigned long ctx_ = 0;
    int fd_ = 0;
    char cb_buf_[128] = {};
#endif

  public:
    FileReadEvent();
    ~FileReadEvent();

#if defined(_WIN32)
    bool ReadFile(void *h_file, size_t read_offset, size_t read_size, uint8_t *out_buf);
#else
    bool ReadFile(int fd, size_t read_offset, size_t read_size, uint8_t *out_buf);
#endif
    eFileReadResult GetResult(bool block, size_t *bytes_read);
};

class AsyncFileReader {
    std::unique_ptr<AsyncFileReaderImpl> impl_;

  public:
    AsyncFileReader() noexcept;
    ~AsyncFileReader();

    bool ReadFileBlocking(const char *file_path, size_t read_offset, size_t read_size,
                          FileReadBufBase &out_buf);

    bool ReadFileBlocking(const char *file_path, size_t read_offset, size_t read_size,
                          void *out_data, size_t &out_size);

    bool ReadFileNonBlocking(const char *file_path, size_t read_offset, size_t read_size,
                             FileReadBufBase &out_buf, FileReadEvent &out_event);
};
} // namespace Sys
