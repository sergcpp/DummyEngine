#pragma once

#include <memory>

// Simple String class WITHOUT small-string optimization
// Pointer returned by c_str() is persistent and safe to use after std::move

namespace Ren {
template <typename Alloc = std::allocator<char>>
class BasicString {
    char    *str_;
    size_t  len_;
    Alloc   alloc_;
public:
    BasicString() : str_(nullptr), len_(0) {}
    BasicString(const char *str) {
        len_ = strlen(str);
        str_ = alloc_.allocate(len_ + 1);
        memcpy(str_, str, len_ + 1);
    }

    BasicString(const BasicString &rhs) {
        len_ = rhs.len_;
        str_ = alloc_.allocate(len_ + 1);
        memcpy(str_, rhs.str_, len_ + 1);
    }

    BasicString(BasicString &&rhs) {
        len_ = rhs.len_;
        rhs.len_ = 0;
        str_ = rhs.str_;
        rhs.str_ = nullptr;
    }

    ~BasicString() {
        Release();
    }

    BasicString &operator=(const BasicString &rhs) {
        Release();

        len_ = rhs.len_;
        str_ = alloc_.allocate(len_ + 1);
        memcpy(str_, rhs.str_, len_ + 1);

        return *this;
    }

    BasicString &operator=(BasicString &&rhs) {
        Release();

        len_ = rhs.len_;
        rhs.len_ = 0;
        str_ = rhs.str_;
        rhs.str_ = nullptr;

        return *this;
    }

    const char *c_str() const { return str_; }
    size_t length() const { return len_; }

    void Release() {
        alloc_.deallocate(str_, len_ + 1);
        str_ = nullptr;
        len_ = 0;
    }

    bool EndsWith(const char *str) {
        size_t len = strlen(str);
        for (size_t i = 0; i < len; i++) {
            if (str_[len_ - i] != str[len - i]) {
                return false;
            }
        }
        return true;
    }

    friend bool operator==(const BasicString &s1, const BasicString &s2) {
        return s1.len_ == s2.len_ && memcmp(s1.str_, s2.str_, s1.len_) == 0;
    }

    friend bool operator!=(const BasicString &s1, const BasicString &s2) {
        return s1.len_ != s2.len_ || memcmp(s1.str_, s2.str_, s1.len_) != 0;
    }
};

using String = BasicString<>;
}