#include "AsyncFileReader.h"

#include <algorithm>

#if defined(_WIN32)
#include <Windows.h>
#undef min
#undef max
#elif defined(__linux__)
#include <fcntl.h>
#include <linux/aio_abi.h>
#include <sys/syscall.h>
#include <unistd.h>
#endif

namespace Sys {
#if defined(_WIN32)
class AsyncFileReaderImpl {
    static const int SimultaniousRequestsCount = 16;

    uint32_t chunk_size_;
    HANDLE ev_[SimultaniousRequestsCount];
    OVERLAPPED olp_[SimultaniousRequestsCount];
    void *buf_;
    void *req_bufs_[SimultaniousRequestsCount];

    void RequestChunk(HANDLE h_file, int chunk_num) {
        const int i = chunk_num % SimultaniousRequestsCount;

        LARGE_INTEGER ofs;
        ofs.QuadPart = LONGLONG(chunk_num) * chunk_size_;

        OVERLAPPED &ov = olp_[i];
        ov = {0};
        ov.Offset = ofs.LowPart;
        ov.OffsetHigh = ofs.HighPart;
        ov.hEvent = ev_[i];

        void *b = req_bufs_[i];

        ::ReadFile(h_file, b, chunk_size_, NULL, &ov);
    }

  public:
    AsyncFileReaderImpl() noexcept {
        SYSTEM_INFO os_info;
        ::GetSystemInfo(&os_info);

        const DWORD chunk_size = os_info.dwPageSize * 128;
        chunk_size_ = uint32_t(chunk_size);

        buf_ = ::VirtualAlloc(NULL, SIZE_T(chunk_size_) * SimultaniousRequestsCount,
                              MEM_COMMIT, PAGE_READWRITE);

        for (int i = 0; i < SimultaniousRequestsCount; i++) {
            req_bufs_[i] = (char *)buf_ + i * size_t(chunk_size_);
        }

        for (int i = 0; i < SimultaniousRequestsCount; i++) {
            ev_[i] = ::CreateEvent(NULL, TRUE, FALSE, NULL);
        }
    }

    ~AsyncFileReaderImpl() {
        ::VirtualFree(buf_, 0, MEM_RELEASE);
        for (int i = 0; i < SimultaniousRequestsCount; i++) {
            ::CloseHandle(ev_[i]);
        }
    }

    bool ReadFile(const char *file_path, size_t max_size, void *out_data,
                  size_t &out_size) {
        HANDLE h_file =
            ::CreateFile(file_path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                         FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN |
                             FILE_FLAG_NO_BUFFERING | FILE_FLAG_OVERLAPPED,
                         NULL);

        if (h_file == INVALID_HANDLE_VALUE) {
            out_size = 0;
            return false;
        }

        LARGE_INTEGER size;
        GetFileSizeEx(h_file, &size);

        out_size = (size_t)size.QuadPart;

        if (max_size < out_size) {
            ::CloseHandle(h_file);
            return false;
        }

        const int chunks_count = int(size.QuadPart + chunk_size_ - 1) / chunk_size_;

        for (int i = 0; i < std::min(chunks_count, SimultaniousRequestsCount - 1); i++) {
            RequestChunk(h_file, i);
        }

        for (int i = 0; i < chunks_count; i++) {
            const int n = i % SimultaniousRequestsCount;

            OVERLAPPED &ov = olp_[n];
            void *b = req_bufs_[n];

            DWORD cb;
            ::GetOverlappedResult(h_file, &ov, &cb, TRUE);

            const int next_request = i + SimultaniousRequestsCount - 1;
            if (next_request < chunks_count) {
                RequestChunk(h_file, next_request);
            }

            memcpy((char *)out_data + i * size_t(chunk_size_), b, cb);
        }

        ::CloseHandle(h_file);
        return true;
    }
};
#elif defined(__linux__)
class AsyncFileReaderImpl {
    static const int SimultaniousRequestsCount = 16;

    uint32_t chunk_size_;
    aio_context_t ctx_ = 0;
    struct iocb req_cbs_[SimultaniousRequestsCount] = {};
    struct iocb *p_req_cbs_[SimultaniousRequestsCount] = {};

    static long io_setup(unsigned nr, aio_context_t *ctxp) {
        return syscall(__NR_io_setup, nr, ctxp);
    }

    static long io_destroy(aio_context_t ctx) { return syscall(__NR_io_destroy, ctx); }

    static long io_submit(aio_context_t ctx, long nr, struct iocb **iocbpp) {
        return syscall(__NR_io_submit, ctx, nr, iocbpp);
    }

    static long io_getevents(aio_context_t ctx, long min_nr, long max_nr,
                             struct io_event *events, struct timespec *timeout) {
        return syscall(__NR_io_getevents, ctx, min_nr, max_nr, events, timeout);
    }

  public:
    AsyncFileReaderImpl() noexcept {
        long ret = io_setup(SimultaniousRequestsCount, &ctx_);
        assert(ret >= 0 && "io_setup failed!");

        chunk_size_ = getpagesize() * 16;

        for (int i = 0; i < SimultaniousRequestsCount; i++) {
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
        int chunks_requested = std::min(chunks_count, SimultaniousRequestsCount);

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

const int AsyncFileReaderImpl::SimultaniousRequestsCount;
#else
class AsyncFileReaderImpl {
  public:
    bool ReadFile(const char *file_path, const size_t max_size, void *out_data,
                  size_t &out_size) {
        return false;
    }
};
#endif
} // namespace Sys

Sys::AsyncFileReader::AsyncFileReader() noexcept : impl_(new AsyncFileReaderImpl) {}

Sys::AsyncFileReader::~AsyncFileReader() = default;

bool Sys::AsyncFileReader::ReadFile(const char *file_path, const size_t max_size,
                                    void *out_data, size_t &out_size) {
    return impl_->ReadFile(file_path, max_size, out_data, out_size);
}
