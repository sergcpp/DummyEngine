#include "AsyncFileReader.h"

#include <cassert>

#include <algorithm>

#include <Windows.h>
#undef min
#undef max

namespace Sys {
static const int MaxVolumeSectorSize = 4096;
static const int SimultaniousFileRequests = 16;

uint32_t FileReadBufBase::GetOptimalChunkSize() {
    SYSTEM_INFO os_info;
    ::GetSystemInfo(&os_info);
    return uint32_t(os_info.dwPageSize * 128);
}

uint8_t *DefaultFileReadBuf::Alloc(const size_t new_size) {
    return (uint8_t *)::VirtualAlloc(NULL, SIZE_T(new_size), MEM_COMMIT, PAGE_READWRITE);
}

void DefaultFileReadBuf::Free() {
    if (mem_) {
        ::VirtualFree(mem_, 0, MEM_RELEASE);
        mem_ = nullptr;
    }
}

FileReadEvent::FileReadEvent() {
    static_assert(sizeof(OVERLAPPED) <= sizeof(ov_), "!");

    h_file_ = NULL;
    ev_ = ::CreateEvent(NULL /* attribs */, TRUE /* manual reset */, FALSE /* initial state */, NULL /* name */);
}

FileReadEvent::~FileReadEvent() {
    ::CloseHandle(h_file_);
    ::CloseHandle(ev_);
}

bool FileReadEvent::ReadFile(void *h_file, const size_t read_offset, const size_t read_size, uint8_t *out_buf) {
    assert(!h_file_);
    h_file_ = h_file;

    LARGE_INTEGER ofs;
    ofs.QuadPart = read_offset;

    auto &ov = reinterpret_cast<OVERLAPPED &>(ov_);

    ov = {0};
    ov.Offset = ofs.LowPart;
    ov.OffsetHigh = ofs.HighPart;
    ov.hEvent = ev_;

    assert(read_offset % MaxVolumeSectorSize == 0);
    assert(read_size % MaxVolumeSectorSize == 0);
    return ::ReadFile(h_file, out_buf, DWORD(read_size), NULL, &ov) == FALSE && ::GetLastError() == ERROR_IO_PENDING;
}

eFileReadResult FileReadEvent::GetResult(const bool block, size_t *bytes_read) {
    if (!h_file_) {
        return eFileReadResult::Failed;
    }

    DWORD cb;
    const bool success =
        ::GetOverlappedResult(h_file_, reinterpret_cast<OVERLAPPED *>(ov_), &cb, block ? TRUE : FALSE) != 0;
    eFileReadResult res;
    if (!block && !success && GetLastError() == ERROR_IO_INCOMPLETE) {
        res = eFileReadResult::Pending;
    } else {
        res = success ? eFileReadResult::Successful : eFileReadResult::Failed;
    }

    (*bytes_read) = size_t(cb);
    if (block) {
        h_file_ = NULL;
    } else if (res != eFileReadResult::Pending) {
        ::CloseHandle(h_file_);
        h_file_ = NULL;
    }

    return res;
}

class AsyncFileReaderImpl {
    DefaultFileReadBuf internal_buf_;
    FileReadEvent internal_ev_[SimultaniousFileRequests];

  public:
    AsyncFileReaderImpl() { internal_buf_.Realloc(size_t(internal_buf_.chunk_size()) * SimultaniousFileRequests); }

    bool ReadFileBlocking(const char *file_path, const size_t read_offset, size_t read_size, void *out_data,
                          size_t &out_size) {
        HANDLE h_file = ::CreateFile(
            file_path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN | FILE_FLAG_NO_BUFFERING | FILE_FLAG_OVERLAPPED, NULL);

        if (h_file == INVALID_HANDLE_VALUE) {
            out_size = 0;
            return false;
        }

        LARGE_INTEGER size;
        GetFileSizeEx(h_file, &size);

        read_size = std::min(read_size, size_t(size.QuadPart) - read_offset);
        const size_t out_buf_size = out_size;

        if (out_buf_size < read_size) {
            ::CloseHandle(h_file);
            return false;
        }

        out_size = read_size;

        // read offset must be aligned to volume sector size
        const size_t aligned_read_offset = read_offset - (read_offset % MaxVolumeSectorSize);

        const int chunks_count = int(read_size + (read_offset - aligned_read_offset) + internal_buf_.chunk_size() - 1) /
                                 internal_buf_.chunk_size();
        const size_t aligned_read_size = chunks_count * size_t(internal_buf_.chunk_size());

        uint8_t *p_out_data = reinterpret_cast<uint8_t *>(out_data);
        size_t left_to_read = read_size;
        size_t left_to_request = aligned_read_size;

        for (int i = 0; i < std::min(chunks_count, SimultaniousFileRequests - 1); i++) {
            const size_t req_size = std::min(size_t(internal_buf_.chunk_size()), left_to_request);
            if (!internal_ev_[i].ReadFile(h_file, aligned_read_offset + size_t(i) * internal_buf_.chunk_size(),
                                          req_size, internal_buf_.chunk(i % SimultaniousFileRequests))) {
                ::CloseHandle(h_file);
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
                bytes_read -= std::min(aligned_read_size, size_t(size.QuadPart) - read_offset) - read_size;
                assert(left_to_read <= bytes_read);
            }

            const int next_request = i + SimultaniousFileRequests - 1;
            if (next_request < chunks_count) {
                const size_t req_size = std::min(size_t(internal_buf_.chunk_size()), left_to_request);
                const int next_i = next_request % SimultaniousFileRequests;
                if (!internal_ev_[next_i].ReadFile(
                        h_file, aligned_read_offset + size_t(next_request) * internal_buf_.chunk_size(), req_size,
                        internal_buf_.chunk(next_i))) {
                    ::CloseHandle(h_file);
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

        ::CloseHandle(h_file);
        return true;
    }

    bool ReadFileBlocking(const char *file_path, const size_t read_offset, const size_t read_size,
                          FileReadBufBase &out_buf) {
        return ReadFileBlocking(file_path, read_offset, read_size, out_buf, internal_ev_, SimultaniousFileRequests);
    }

    bool ReadFileBlocking(const char *file_path, const size_t read_offset, size_t read_size, FileReadBufBase &out_buf,
                          FileReadEvent *events, const int events_count) {
        HANDLE h_file = ::CreateFile(
            file_path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN | FILE_FLAG_NO_BUFFERING | FILE_FLAG_OVERLAPPED, NULL);

        if (h_file == INVALID_HANDLE_VALUE) {
            out_buf.set_data_off(0);
            out_buf.set_data_len(0);
            return false;
        }

        LARGE_INTEGER size;
        GetFileSizeEx(h_file, &size);

        read_size = std::min(read_size, size_t(size.QuadPart) - read_offset);

        // read offset must be aligned to volume sector size
        const size_t aligned_read_offset = read_offset - (read_offset % MaxVolumeSectorSize);

        const int chunks_count =
            int(read_size + (read_offset - aligned_read_offset) + out_buf.chunk_size() - 1) / out_buf.chunk_size();
        const size_t aligned_read_size = chunks_count * size_t(out_buf.chunk_size());

        out_buf.Realloc(aligned_read_size);
        out_buf.set_data_off(read_offset - aligned_read_offset);
        out_buf.set_data_len(read_size);

        size_t left_to_request = aligned_read_size;

        for (int i = 0; i < std::min(chunks_count, events_count); i++) {
            const size_t req_size = std::min(size_t(out_buf.chunk_size()), left_to_request);
            if (!events[i].ReadFile(h_file, aligned_read_offset + size_t(i) * out_buf.chunk_size(), req_size,
                                    out_buf.chunk(i))) {
                out_buf.set_data_off(0);
                out_buf.set_data_len(0);
                ::CloseHandle(h_file);
                return false;
            }
            left_to_request -= req_size;
        }

        for (int i = 0; i < chunks_count; i++) {
            size_t bytes_read;
            events[i % events_count].GetResult(true /* block */, &bytes_read);

            const int next_request = i + SimultaniousFileRequests;
            if (next_request < chunks_count) {
                const size_t req_size = std::min(size_t(out_buf.chunk_size()), left_to_request);
                if (!events[next_request % events_count].ReadFile(
                        h_file, aligned_read_offset + size_t(next_request) * out_buf.chunk_size(), req_size,
                        out_buf.chunk(next_request % out_buf.chunk_count()))) {
                    out_buf.set_data_off(0);
                    out_buf.set_data_len(0);
                    ::CloseHandle(h_file);
                    return false;
                }
                left_to_request -= req_size;
            }
        }

        assert(left_to_request == 0);

        ::CloseHandle(h_file);
        return true;
    }

    bool ReadFileNonBlocking(const char *file_path, const size_t read_offset, size_t read_size,
                             FileReadBufBase &out_buf, FileReadEvent &out_event) {
        HANDLE h_file = ::CreateFile(
            file_path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN | FILE_FLAG_NO_BUFFERING | FILE_FLAG_OVERLAPPED, NULL);

        if (h_file == INVALID_HANDLE_VALUE) {
            out_buf.set_data_off(0);
            out_buf.set_data_len(0);
            return false;
        }

        LARGE_INTEGER size;
        GetFileSizeEx(h_file, &size);

        read_size = std::min(read_size, size_t(size.QuadPart) - read_offset);

        // read offset must be aligned to volume sector size
        const size_t aligned_read_offset = read_offset - (read_offset % MaxVolumeSectorSize);

        const int chunks_count =
            int(read_size + (read_offset - aligned_read_offset) + out_buf.chunk_size() - 1) / out_buf.chunk_size();
        const size_t aligned_read_size = chunks_count * size_t(out_buf.chunk_size());

        out_buf.Realloc(aligned_read_size);
        out_buf.set_data_off(read_offset - aligned_read_offset);
        out_buf.set_data_len(read_size);

        if (!out_event.ReadFile(h_file, aligned_read_offset, aligned_read_size, out_buf.chunk(0))) {
            out_buf.set_data_off(0);
            out_buf.set_data_len(0);
            ::CloseHandle(h_file);
            return false;
        }

        return true;
    }
};
} // namespace Sys

Sys::AsyncFileReader::AsyncFileReader() noexcept : impl_(new AsyncFileReaderImpl) {}

Sys::AsyncFileReader::~AsyncFileReader() = default;

bool Sys::AsyncFileReader ::ReadFileBlocking(const char *file_path, const size_t read_offset, const size_t read_size,
                                             FileReadBufBase &out_buf) {
    return impl_->ReadFileBlocking(file_path, read_offset, read_size, out_buf);
}

bool Sys::AsyncFileReader::ReadFileBlocking(const char *file_path, const size_t read_offset, const size_t read_size,
                                            void *out_data, size_t &out_size) {
    return impl_->ReadFileBlocking(file_path, read_offset, read_size, out_data, out_size);
}

bool Sys::AsyncFileReader::ReadFileNonBlocking(const char *file_path, size_t read_offset, size_t read_size,
                                               FileReadBufBase &out_buf, FileReadEvent &out_event) {
    return impl_->ReadFileNonBlocking(file_path, read_offset, read_size, out_buf, out_event);
}