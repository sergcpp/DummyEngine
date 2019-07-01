#pragma once

#include <memory>

// Simple COW string class WITHOUT small-string optimization, NOT thread safe
// Pointer returned by c_str() is persistent and safe to use after std::move

namespace Ren {
template <typename Alloc = std::allocator<char>>
class BasicString {
    char    *str_;
    size_t  len_;
    Alloc   alloc_;
public:
    BasicString() : str_(nullptr), len_(0) {}
    explicit BasicString(const char *str) {
        len_ = strlen(str);
        uint32_t *storage = (uint32_t *)alloc_.allocate(sizeof(uint32_t) + len_ + 1);
        // set number of users to 1
        *storage = 1;
        str_ = (char *)(storage + 1);
        memcpy(str_, str, len_ + 1);
    }

    BasicString(const BasicString &rhs) {
        str_ = rhs.str_;
        len_ = rhs.len_;
        if (str_) {
            // increase number of users
            uint32_t *counter = (uint32_t *)(str_ - sizeof(uint32_t));
            ++(*counter);
        }
    }

    BasicString(BasicString &&rhs) {
        len_ = rhs.len_;
        rhs.len_ = 0;
        str_ = rhs.str_;
        rhs.str_ = nullptr;
        alloc_ = std::move(rhs.alloc_);
    }

    ~BasicString() {
        Release();
    }

    BasicString &operator=(const BasicString &rhs) {
        Release();

        str_ = rhs.str_;
        len_ = rhs.len_;
        if (str_) {
            // increase number of users
            uint32_t *counter = str_ - sizeof(uint32_t);
            ++(*counter);
        }

        return *this;
    }

    BasicString &operator=(BasicString &&rhs) {
        Release();

        len_ = rhs.len_;
        rhs.len_ = 0;
        str_ = rhs.str_;
        rhs.str_ = nullptr;
        alloc_ = std::move(rhs.alloc_);

        return *this;
    }

    const char *c_str() const { return str_; }
    size_t length() const { return len_; }

    void Release() {
        if (str_) {
            uint32_t *counter = (uint32_t *)(str_ - sizeof(uint32_t));
            --(*counter);

            if (!*counter) {
                alloc_.deallocate((char *)counter, len_ + 1 + sizeof(uint32_t));
            }

            str_ = nullptr;
            len_ = 0;
        }
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
        return s1.str_ == s2.str_ || (s1.len_ == s2.len_ && memcmp(s1.str_, s2.str_, s1.len_) == 0);
    }

    friend bool operator!=(const BasicString &s1, const BasicString &s2) {
        return s1.str_ != s2.str_ && (s1.len_ != s2.len_ || memcmp(s1.str_, s2.str_, s1.len_) != 0);
    }

    friend bool operator==(const BasicString &s1, const char *s2) {
        return strcmp(s1.str_, s2) == 0;
    }

    friend bool operator==(const char *s1, const BasicString &s2) {
        return strcmp(s1, s2.str_) == 0;
    }

    friend bool operator!=(const BasicString &s1, const char *s2) {
        return strcmp(s1.str_, s2) != 0;
    }

    friend bool operator!=(const char *s1, const BasicString &s2) {
        return strcmp(s1, s2.str_) != 0;
    }
};

using String = BasicString<>;
}