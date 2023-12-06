#pragma once

#include <istream>

class membuf : public std::streambuf {
public:
    membuf(const uint8_t *beg, const size_t size) : beg_(beg), end_(beg + size), cur_(beg) {}

    membuf(const membuf &) = delete;
    membuf &operator= (const membuf &) = delete;

private:
    int_type underflow() {
        if (cur_ == end_) {
            return traits_type::eof();
        }
        return traits_type::to_int_type(*cur_);
    }

    int_type uflow() {
        if (cur_ == end_) {
            return traits_type::eof();
        }
        return traits_type::to_int_type(*cur_++);
    }

    int_type pbackfail(int_type ch) {
        if (cur_ == beg_ || (ch != traits_type::eof() && ch != cur_[-1])) {
            return traits_type::eof();
        }
        return traits_type::to_int_type(*--cur_);
    }

    std::streamsize showmanyc() {
        return end_ - cur_;
    }

    std::streampos seekoff(std::streamoff off, std::ios_base::seekdir way,
                           std::ios_base::openmode which = std::ios_base::in | std::ios_base::out) {
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

    std::streampos seekpos(std::streampos sp,
                           std::ios_base::openmode which = std::ios_base::in | std::ios_base::out) {
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