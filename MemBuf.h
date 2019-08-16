#pragma once

#include <cstring>

#include <istream>

namespace Sys {
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
        count = std::min(count, end_ - cur_);
        std::memcpy(out_ptr, cur_, count);
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

    std::streampos seekoff(std::streamoff off, std::ios_base::seekdir way,
                           std::ios_base::openmode which) override {
        if (way == std::ios_base::beg) {
            cur_ = beg_ + off;
        } else if (way == std::ios_base::cur) {
            cur_ += off;
        } else if (way == std::ios_base::end) {
            cur_ = end_;
        }

        if (cur_ < beg_ || cur_ > end_) {
            return -1;
        }

        return cur_ - beg_;
    }

    std::streampos seekpos(std::streampos sp, std::ios_base::openmode which) override {
        cur_ = beg_ + (int)sp;

        if (cur_ < beg_ || cur_ > end_) {
            return -1;
        }

        return cur_ - beg_;
    }

    const uint8_t *beg_;
    const uint8_t *end_;
    const uint8_t *cur_;
};
} // namespace Sys