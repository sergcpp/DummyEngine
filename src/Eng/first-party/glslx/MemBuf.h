#pragma once

#include <cstring>

#include <istream>

namespace glslx {
class MemBuf : public std::streambuf {
  public:
    MemBuf(const uint8_t *beg, const size_t size)
        : beg_(beg), end_(beg + size), cur_(beg) {}

    MemBuf(const MemBuf &) = delete;
    MemBuf &operator=(const MemBuf &) = delete;

  private:
    int_type underflow() override {
        if (cur_ == end_) {
            return traits_type::eof();
        }
        return traits_type::to_int_type(*cur_);
    }

    int_type uflow() override {
        if (cur_ == end_) {
            return traits_type::eof();
        }
        return traits_type::to_int_type(*cur_++);
    }

    std::streamsize xsgetn(char *out_ptr, std::streamsize count) override {
        if (count <= 0) {
            return 0;
        }
        count = std::min(count, std::streamsize(end_ - cur_));
        memcpy(out_ptr, cur_, size_t(count));
        cur_ += count;
        return count;
    }

    int_type pbackfail(int_type ch) override {
        if (cur_ == beg_ || (ch != traits_type::eof() && ch != cur_[-1])) {
            return traits_type::eof();
        }
        return traits_type::to_int_type(*--cur_);
    }

    std::streamsize showmanyc() override { return end_ - cur_; }

    std::streampos seekoff(std::streamoff off, std::ios_base::seekdir way, std::ios_base::openmode which) override {
        (void)which;
        const uint8_t *new_cur = cur_;
        if (way == std::ios_base::beg) {
            new_cur = beg_ + off;
        } else if (way == std::ios_base::cur) {
            new_cur = cur_ + off;
        } else if (way == std::ios_base::end) {
            new_cur = end_ + off;
        }

        // Only commit the new position when it stays inside the buffer.
        if (new_cur < beg_ || new_cur > end_) {
            return std::streampos(-1);
        }

        cur_ = new_cur;
        return cur_ - beg_;
    }

    std::streampos seekpos(std::streampos sp, std::ios_base::openmode which) override {
        (void)which;
        const uint8_t *new_cur = beg_ + int(sp);

        if (new_cur < beg_ || new_cur > end_) {
            return std::streampos(-1);
        }

        cur_ = new_cur;
        return cur_ - beg_;
    }

    const uint8_t *beg_;
    const uint8_t *end_;
    const uint8_t *cur_;
};
} // namespace Sys