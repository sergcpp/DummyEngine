#include "AsyncFileReader.h"

#include <algorithm>

#include <fcntl.h>
#include <aio.h>
#include <sys/syscall.h>
#include <unistd.h>

namespace Sys {
static const int MaxVolumeSectorSize = 4096;
static const int SimultaniousFileRequests = 16;

uint32_t FileReadBufBase::GetOptimalChunkSize() {
    return uint32_t(getpagesize() * 128);
}

uint8_t *DefaultFileReadBuf::Alloc(const size_t new_size) {
    return (uint8_t *)::malloc(new_size);
}

void DefaultFileReadBuf::Free() {
    if (mem_) {
        ::free(mem_);
        mem_ = nullptr;
    }
}

FileReadEvent::FileReadEvent() {
    //const long ret = io_setup(1 /* requests count */, &ctx_);
    //assert(ret >= 0 && "io_setup failed!");
}

FileReadEvent::~FileReadEvent() {
    //io_destroy(ctx_);
}

bool FileReadEvent::ReadFile(int fd, size_t read_offset, size_t read_size, uint8_t *out_buf) {
    assert(!fd_);
    fd_ = fd;

    static_assert(sizeof(cb_buf_) >= sizeof(struct aiocb), "!");
    auto *cb = reinterpret_cast<struct aiocb *>(cb_buf_);

    cb->aio_fildes = fd;
    cb->aio_lio_opcode = LIO_READ;

    cb->aio_offset = int64_t(read_offset);
    cb->aio_nbytes = read_size;
    cb->aio_buf = out_buf;

    const long ret = aio_read(cb);
    return ret == 0;
}

eFileReadResult FileReadEvent::GetResult(const bool block, size_t *bytes_read) {
    if (!fd_) {
        return eFileReadResult::Failed;
    }

    eFileReadResult res;
    
    auto *cb = reinterpret_cast<struct aiocb *>(cb_buf_);
    if (block) {
        const int ret = aio_suspend(&cb, 1, nullptr);
        if (ret == -1) {
            res = eFileReadResult::Failed;
        }
    }
    
    const int ret = aio_error(cb);
    
    if (!block && ret == EINPROGRESS) {
        res = eFileReadResult::Pending;
    } else {
        res = ret == 0 ? eFileReadResult::Successful : eFileReadResult::Failed;
    }

    (*bytes_read) = size_t(aio_return(cb));
    if (block) {
        fd_ = 0;
    } else if (res != eFileReadResult::Pending) {
        close(fd_);
        fd_ = 0;
    }

    return res;
}

class AsyncFileReaderImpl {
    DefaultFileReadBuf internal_buf_;
    FileReadEvent internal_ev_[SimultaniousFileRequests];

  public:
    AsyncFileReaderImpl() {
        internal_buf_.Realloc(size_t(internal_buf_.chunk_size()) *
                              SimultaniousFileRequests);
    }

    bool ReadFileBlocking(const char *file_path, const size_t read_offset,
                          size_t read_size, void *out_data, size_t &out_size) {
        const int fd = open(file_path, O_RDONLY);
        if (!fd) {
            out_size = 0;
            return false;
        }

        const off_t file_size = lseek(fd, 0, SEEK_END);
        if (file_size == off_t(-1)) {
            close(fd);
            out_size = 0;
            return false;
        }

        read_size = std::min(read_size, size_t(file_size) - read_offset);
        const size_t out_buf_size = out_size;

        if (out_buf_size < read_size) {
            close(fd);
            return false;
        }

        out_size = read_size;

        // read offset must be aligned to volume sector size
        const size_t aligned_read_offset =
            read_offset - (read_offset % MaxVolumeSectorSize);

        const int chunks_count = int(read_size + (read_offset - aligned_read_offset) +
                                     internal_buf_.chunk_size() - 1) /
                                 internal_buf_.chunk_size();
        const size_t aligned_read_size =
            chunks_count * size_t(internal_buf_.chunk_size());

        auto *p_out_data = reinterpret_cast<uint8_t *>(out_data);
        size_t left_to_read = read_size;
        size_t left_to_request = aligned_read_size;

        for (int i = 0; i < std::min(chunks_count, SimultaniousFileRequests - 1); i++) {
            const size_t req_size =
                std::min(size_t(internal_buf_.chunk_size()), left_to_request);
            if (!internal_ev_[i].ReadFile(
                fd, aligned_read_offset + size_t(i) * internal_buf_.chunk_size(),
                req_size, internal_buf_.chunk(i % SimultaniousFileRequests))) {
                ::close(fd);
                return false;
            }
            left_to_request -= req_size;
        }

        for (int i = 0; i < chunks_count; i++) {
            const int n = i % SimultaniousFileRequests;

            size_t bytes_read;
            internal_ev_[n].GetResult(true, &bytes_read);

            const uint8_t *b = internal_buf_.chunk(n);
            if (i == 0) {
                b += (read_offset - aligned_read_offset);
                bytes_read -= read_offset - aligned_read_offset;
            }
            if (i == chunks_count - 1) {
                bytes_read -=
                    std::min(aligned_read_size, size_t(file_size) - read_offset) -
                    read_size;
                assert(left_to_read <= bytes_read);
            }

            const int next_request = i + SimultaniousFileRequests - 1;
            if (next_request < chunks_count) {
                const size_t req_size =
                    std::min(size_t(internal_buf_.chunk_size()), left_to_request);
                const int next_i = next_request % SimultaniousFileRequests;
                if (!internal_ev_[next_i].ReadFile(
                    fd,
                    aligned_read_offset +
                    size_t(next_request) * internal_buf_.chunk_size(),
                    req_size, internal_buf_.chunk(next_i))) {
                    close(fd);
                    return false;
                }
                left_to_request -= req_size;
            }

            const size_t copy_size = std::min(bytes_read, left_to_read);
            memcpy(p_out_data, b, copy_size);
            p_out_data += copy_size;
            left_to_read -= copy_size;
        }

        assert(left_to_request == 0);
        assert(left_to_read == 0);

        close(fd);
        return true;
    }

    bool ReadFileBlocking(const char *file_path, const size_t read_offset,
                          const size_t read_size, FileReadBufBase &out_buf) {
        return ReadFileBlocking(file_path, read_offset, read_size, out_buf, internal_ev_,
                                SimultaniousFileRequests);
    }

    bool ReadFileBlocking(const char *file_path, const size_t read_offset,
                          size_t read_size, FileReadBufBase &out_buf,
                          FileReadEvent *events, const int events_count) {
        const int fd = open(file_path, O_RDONLY);
        if (!fd) {
            out_buf.set_data_off(0);
            out_buf.set_data_len(0);
            return false;
        }

        const off_t file_size = lseek(fd, 0, SEEK_END);
        if (file_size == off_t(-1)) {
            close(fd);
            out_buf.set_data_off(0);
            out_buf.set_data_len(0);
            return false;
        }

        read_size = std::min(read_size, size_t(file_size) - read_offset);

        // read offset must be aligned to volume sector size
        const size_t aligned_read_offset =
            read_offset - (read_offset % MaxVolumeSectorSize);

        const int chunks_count = int(read_size + (read_offset - aligned_read_offset) +
                                     out_buf.chunk_size() - 1) /
                                 out_buf.chunk_size();
        const size_t aligned_read_size = chunks_count * size_t(out_buf.chunk_size());

        out_buf.Realloc(aligned_read_size);
        out_buf.set_data_off(read_offset - aligned_read_offset);
        out_buf.set_data_len(read_size);

        size_t left_to_request = aligned_read_size;

        for (int i = 0; i < std::min(chunks_count, events_count); i++) {
            const size_t req_size =
                std::min(size_t(out_buf.chunk_size()), left_to_request);
            if (!events[i].ReadFile(
                fd, aligned_read_offset + size_t(i) * out_buf.chunk_size(),
                req_size, out_buf.chunk(i))) {
                out_buf.set_data_off(0);
                out_buf.set_data_len(0);
                close(fd);
                return false;
            }
            left_to_request -= req_size;
        }

        for (int i = 0; i < chunks_count; i++) {
            size_t bytes_read;
            events[i % events_count].GetResult(true /* block */, &bytes_read);

            const int next_request = i + SimultaniousFileRequests;
            if (next_request < chunks_count) {
                const size_t req_size =
                    std::min(size_t(out_buf.chunk_size()), left_to_request);
                if (!events[next_request % events_count].ReadFile(
                    fd,
                    aligned_read_offset + size_t(next_request) * out_buf.chunk_size(),
                    req_size, out_buf.chunk(next_request % out_buf.chunk_count()))) {
                    out_buf.set_data_off(0);
                    out_buf.set_data_len(0);
                    close(fd);
                    return false;
                }
                left_to_request -= req_size;
            }
        }

        assert(left_to_request == 0);

        close(fd);
        return true;
    }

    bool ReadFileNonBlocking(const char *file_path, const size_t read_offset,
                             size_t read_size, FileReadBufBase &out_buf,
                             FileReadEvent &out_event) {
        const int fd = open(file_path, O_RDONLY);
        if (!fd) {
            out_buf.set_data_off(0);
            out_buf.set_data_len(0);
            return false;
        }

        const off_t file_size = lseek(fd, 0, SEEK_END);
        if (file_size == off_t(-1)) {
            close(fd);
            out_buf.set_data_off(0);
            out_buf.set_data_len(0);
            return false;
        }

        read_size = std::min(read_size, size_t(file_size) - read_offset);

        // read offset must be aligned to volume sector size
        const size_t aligned_read_offset =
            read_offset - (read_offset % MaxVolumeSectorSize);

        const int chunks_count = int(read_size + (read_offset - aligned_read_offset) +
                                     out_buf.chunk_size() - 1) /
                                 out_buf.chunk_size();
        const size_t aligned_read_size = chunks_count * size_t(out_buf.chunk_size());

        out_buf.Realloc(aligned_read_size);
        out_buf.set_data_off(read_offset - aligned_read_offset);
        out_buf.set_data_len(read_size);

        if (!out_event.ReadFile(fd, aligned_read_offset, aligned_read_size,
                                out_buf.chunk(0))) {
            out_buf.set_data_off(0);
            out_buf.set_data_len(0);
            close(fd);
            return false;
        }

        return true;
    }
};

#if 0
void FileReadBuf::Realloc(const size_t new_size) {
    if (new_size < size_t(chunk_size_) * chunk_count_) {
        return;
    }

    Free();

    chunk_count_ = uint32_t((new_size + chunk_size_ - 1) / chunk_size_);
    mem_ = (uint8_t *)::malloc(chunk_size_ * chunk_count_);
    assert(mem_);
}

void FileReadBuf::Free() {
    if (mem_) {
        ::free(mem_);
        mem_ = nullptr;
    }
}

uint32_t FileReadBuf::GetOptimalChunkSize() {
    return getpagesize() * 128;
}

class AsyncFileReaderImpl {
    uint32_t chunk_size_;
    aio_context_t ctx_ = 0;
    struct iocb req_cbs_[SimultaniousFileRequestsCount] = {};
    struct iocb *p_req_cbs_[SimultaniousFileRequestsCount] = {};



  public:
    AsyncFileReaderImpl() noexcept {
        long ret = io_setup(SimultaniousFileRequestsCount, &ctx_);
        assert(ret >= 0 && "io_setup failed!");

        chunk_size_ = getpagesize() * 16;

        for (int i = 0; i < SimultaniousFileRequestsCount; i++) {
            req_cbs_[i] = {};
            req_cbs_[i].aio_nbytes = chunk_size_;

            p_req_cbs_[i] = &req_cbs_[i];
        }
    }

    ~AsyncFileReaderImpl() { io_destroy(ctx_); }

    bool ReadFile(const char *file_path, const size_t max_size, void *out_data,
                  size_t &out_size) {
        const int fd = open(file_path, O_RDONLY);
        if (fd < 0) {
            out_size = 0;
            return false;
        }

        off_t file_size = lseek(fd, 0, SEEK_END);
        if (file_size == (off_t)-1) {
            close(fd);
            out_size = 0;
            return false;
        }

        out_size = (size_t)file_size;
        if (out_size > max_size) {
            close(fd);
            return false;
        }

        const int chunks_count = (int)((out_size + chunk_size_ - 1) / chunk_size_);
        int chunks_requested = std::min(chunks_count, SimultaniousFileRequestsCount);

        for (int i = 0; i < chunks_requested; i++) {
            struct iocb &cb = req_cbs_[i];

            cb.aio_fildes = fd;
            cb.aio_lio_opcode = IOCB_CMD_PREAD;

            cb.aio_offset = i * chunk_size_;
            cb.aio_buf = reinterpret_cast<const uint64_t &>(out_data) + cb.aio_offset;

            long ret = io_submit(ctx_, 1, &p_req_cbs_[i]);
            assert(ret == 1 && "io_submit failed!");
        }

        int chunks_done = 0;
        while (chunks_done < chunks_count) {
            io_event ev = {0};

            long ret = io_getevents(ctx_, 1, 1, &ev, nullptr);
            assert(ret == 1 && "io_getevents failed!");

            ++chunks_done;

            auto *cb = reinterpret_cast<struct iocb *>(ev.obj);
            const int i = std::distance(req_cbs_, cb);

            int next_request = chunks_requested;
            if (next_request < chunks_count) {
                cb->aio_offset = next_request * chunk_size_;
                cb->aio_buf =
                    reinterpret_cast<const uint64_t &>(out_data) + cb->aio_offset;

                ret = io_submit(ctx_, 1, &p_req_cbs_[i]);
                assert(ret == 1 && "io_submit failed!");

                ++chunks_requested;
                assert(chunks_requested <= chunks_count);
            }
        }

        return true;
    }
};

const int AsyncFileReaderImpl::SimultaniousFileRequestsCount;
#endif
} // namespace Sys

Sys::AsyncFileReader::AsyncFileReader() noexcept : impl_(new AsyncFileReaderImpl) {}

Sys::AsyncFileReader::~AsyncFileReader() = default;

bool Sys::AsyncFileReader ::ReadFileBlocking(const char *file_path,
                                             const size_t read_offset,
                                             const size_t read_size,
                                             FileReadBufBase &out_buf) {
    return impl_->ReadFileBlocking(file_path, read_offset, read_size, out_buf);
}

bool Sys::AsyncFileReader::ReadFileBlocking(const char *file_path,
                                            const size_t read_offset,
                                            const size_t read_size, void *out_data,
                                            size_t &out_size) {
    return impl_->ReadFileBlocking(file_path, read_offset, read_size, out_data, out_size);
}

bool Sys::AsyncFileReader::ReadFileNonBlocking(const char *file_path, size_t read_offset,
                                               size_t read_size, FileReadBufBase &out_buf,
                                               FileReadEvent &out_event) {
    return impl_->ReadFileNonBlocking(file_path, read_offset, read_size, out_buf,
                                      out_event);
}
