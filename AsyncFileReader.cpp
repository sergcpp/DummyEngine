#include "AsyncFileReader.h"

#include <algorithm>

#ifdef _WIN32
#include <Windows.h>
#undef min
#undef max
#endif

namespace Sys {
#ifdef _WIN32
    class AsyncFileReaderImpl {
        static const int RequestsCount = 16;

        uint32_t chunk_size_;
        HANDLE ev_[RequestsCount];
        OVERLAPPED olp_[RequestsCount];
        void *buf_;
        void *req_bufs_[RequestsCount];

        void RequestChunk(HANDLE h_file, int chunk_num) {
            int i = chunk_num % RequestsCount;

            OVERLAPPED &ov = olp_[i];
            ov = { 0 };
            void *b = req_bufs_[i];

            LARGE_INTEGER ofs;
            ofs.QuadPart = chunk_num * chunk_size_;
            ov.Offset = ofs.LowPart;
            ov.OffsetHigh = ofs.HighPart;
            ov.hEvent = ev_[i];

            ::ReadFile(h_file, b, chunk_size_, NULL, &ov);
        }
    public:
        AsyncFileReaderImpl() {
            SYSTEM_INFO os_info;
            ::GetSystemInfo(&os_info);

            DWORD chunk_size = os_info.dwPageSize * 128;
            chunk_size_ = (uint32_t)chunk_size;

            buf_ = ::VirtualAlloc(NULL, chunk_size_ * RequestsCount, MEM_COMMIT, PAGE_READWRITE);

            for (int i = 0; i < RequestsCount; i++) {
                req_bufs_[i] = (char *)buf_ + i * chunk_size_;
            }

            for (int i = 0; i < RequestsCount; i++) {
                ev_[i] = ::CreateEvent(NULL, TRUE, FALSE, NULL);
            }
        }

        ~AsyncFileReaderImpl() {
            ::VirtualFree(buf_, 0, MEM_RELEASE);
            for (int i = 0; i < RequestsCount; i++) {
                ::CloseHandle(ev_[i]);
            }
        }

        bool ReadFile(const char *file_path, size_t max_size, void *out_data, size_t &out_size) {
            HANDLE h_file = ::CreateFile(file_path, GENERIC_READ, FILE_SHARE_READ,
                                         NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL |
                                         FILE_FLAG_SEQUENTIAL_SCAN | FILE_FLAG_NO_BUFFERING |
                                         FILE_FLAG_OVERLAPPED, NULL);

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

            int chunks_count = (int)(size.QuadPart + chunk_size_ - 1) / chunk_size_;

            for (int i = 0; i < (int)std::min(chunks_count, RequestsCount - 1); i++) {
                RequestChunk(h_file, i);
            }

            for (int i = 0; i < chunks_count; i++) {
                int n = i % RequestsCount;

                OVERLAPPED &ov = olp_[n];
                void *b = req_bufs_[n];

                DWORD cb;
                ::GetOverlappedResult(h_file, &ov, &cb, TRUE);

                int next_request = i + RequestsCount - 1;
                if (next_request < chunks_count) {
                    RequestChunk(h_file, next_request);
                }

                memcpy((char *)out_data + (i * chunk_size_), b, cb);
            }

            ::CloseHandle(h_file);
            return true;
        }
    };
#else
    class AsyncFileReaderImpl {
    public:
        bool ReadFile(const char *file_path, size_t max_size, void *out_data, size_t &out_size) { return false; }
    };
#endif
}

Sys::AsyncFileReader::AsyncFileReader() : impl_(new AsyncFileReaderImpl) {

}

Sys::AsyncFileReader::~AsyncFileReader() {}

bool Sys::AsyncFileReader::ReadFile(const char *file_path, size_t max_size, void *out_data, size_t &out_size) {
    return impl_->ReadFile(file_path, max_size, out_data, out_size);
}