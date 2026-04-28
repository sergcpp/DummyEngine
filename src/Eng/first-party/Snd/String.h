#pragma once

#include <cassert>
#include <cstring>
#include <memory>
#include <string_view>

// Simple COW string class WITHOUT small-string optimization, NOT thread safe
// Pointer returned by c_str() is persistent and safe to use after std::move

namespace Snd {
template <typename Allocator = std::allocator<char>> class BasicString : public Allocator {
    char *str_;
    size_t len_;

    void Detach() {
        if (!str_) {
            return;
        }
        uint32_t *counter = (uint32_t *)(str_ - sizeof(uint32_t));
        if (*counter == 1) {
            // unique
            return;
        }

        auto *storage = (uint32_t *)this->allocate(sizeof(uint32_t) + len_ + 1);
        // set number of users to 1
        *storage = 1;
        char *new_str = (char *)(storage + 1);
        memcpy(new_str, str_, len_ + 1);

        --(*counter);
        str_ = new_str;
    }
  public:
    BasicString() : str_(nullptr), len_(0) {}
    explicit BasicString(const char *str) {
        assert(str);
        len_ = strlen(str);
        auto *storage = (uint32_t *)this->allocate(sizeof(uint32_t) + len_ + 1);
        // set number of users to 1
        *storage = 1;
        str_ = (char *)(storage + 1);
        memcpy(str_, str, len_ + 1);
    }

    explicit BasicString(const char *start, const char *end) {
        assert(start && end && start <= end);
        len_ = end - start;
        auto *storage = (uint32_t *)this->allocate(sizeof(uint32_t) + len_ + 1);
        // set number of users to 1
        *storage = 1;
        str_ = (char *)(storage + 1);
        memcpy(str_, start, len_);
        str_[len_] = '\0';
    }

    explicit BasicString(const std::string_view str) {
        len_ = str.length();
        auto *storage = (uint32_t *)this->allocate(sizeof(uint32_t) + len_ + 1);
        // set number of users to 1
        *storage = 1;
        str_ = (char *)(storage + 1);
        memcpy(str_, str.data(), len_);
        str_[len_] = '\0';
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

    BasicString(BasicString &&rhs) noexcept : Allocator(static_cast<Allocator &&>(rhs)) {
        len_ = rhs.len_;
        rhs.len_ = 0;
        str_ = rhs.str_;
        rhs.str_ = nullptr;
    }

    ~BasicString() { Release(); }

    BasicString &operator=(const BasicString &rhs) {
        if (this == &rhs) {
            return *this;
        }
        Release();

        str_ = rhs.str_;
        len_ = rhs.len_;
        if (str_) {
            // increase number of users
            uint32_t *counter = reinterpret_cast<uint32_t *>(str_ - sizeof(uint32_t));
            ++(*counter);
        }

        return *this;
    }

    BasicString &operator=(BasicString &&rhs) noexcept {
        if (this == &rhs) {
            return *this;
        }
        Allocator::operator=(static_cast<Allocator &&>(rhs));

        Release();

        len_ = rhs.len_;
        rhs.len_ = 0;
        str_ = rhs.str_;
        rhs.str_ = nullptr;

        return *this;
    }

    operator std::string_view() const { return {str_, len_}; }

    const char *c_str() const { return str_; }
    size_t length() const { return len_; }
    bool empty() const { return len_ == 0; }

    template <typename IntType> const char &operator[](IntType i) const { return str_[i]; }

    template <typename IntType> char &operator[](IntType i) {
        Detach();
        return str_[i];
    }

    void Release() {
        if (str_) {
            uint32_t *counter = (uint32_t *)(str_ - sizeof(uint32_t));
            --(*counter);

            if (!*counter) {
                this->deallocate((char *)counter, len_ + 1 + sizeof(uint32_t));
            }

            str_ = nullptr;
            len_ = 0;
        }
    }

    BasicString &operator+=(std::string_view rhs) {
        if (rhs.empty()) {
            return *this;
        }
        const size_t new_len = len_ + rhs.length();
        auto *storage = (uint32_t *)this->allocate(sizeof(uint32_t) + new_len + 1);
        *storage = 1;
        char *new_str = (char *)(storage + 1);
        if (str_) {
            memcpy(new_str, str_, len_);
        }
        memcpy(new_str + len_, rhs.data(), rhs.length());
        new_str[new_len] = '\0';
        Release();
        str_ = new_str;
        len_ = new_len;
        return *this;
    }

    BasicString &operator+=(const char *rhs) { return operator+=(std::string_view{rhs}); }
    BasicString &operator+=(char rhs) { return operator+=(std::string_view{&rhs, 1}); }
    BasicString &operator+=(const BasicString &rhs) { return operator+=(std::string_view{rhs}); }

    bool StartsWith(const char *str) const {
        const size_t len = strlen(str);
        if (len > len_) {
            return false;
        }
        return memcmp(str_, str, len) == 0;
    }

    bool StartsWith(std::string_view str) const {
        if (str.length() > len_) {
            return false;
        }
        return memcmp(str_, str.data(), str.length()) == 0;
    }

    bool EndsWith(const char *str) const {
        const size_t len = strlen(str);
        if (len > len_) {
            return false;
        }
        return memcmp(str_ + len_ - len, str, len) == 0;
    }

    bool EndsWith(std::string_view str) const {
        if (str.length() > len_) {
            return false;
        }
        return memcmp(str_ + len_ - str.length(), str.data(), str.length()) == 0;
    }

    friend BasicString operator+(BasicString lhs, std::string_view rhs) {
        lhs += rhs;
        return lhs;
    }

    friend BasicString operator+(BasicString lhs, const BasicString &rhs) {
        lhs += rhs;
        return lhs;
    }

    friend BasicString operator+(std::string_view lhs, const BasicString &rhs) {
        BasicString result(lhs);
        result += rhs;
        return result;
    }

    friend bool operator==(const BasicString &s1, const BasicString &s2) {
        return s1.str_ == s2.str_ || (s1.len_ == s2.len_ && memcmp(s1.str_, s2.str_, s1.len_) == 0);
    }

    friend bool operator!=(const BasicString &s1, const BasicString &s2) {
        return s1.str_ != s2.str_ && (s1.len_ != s2.len_ || memcmp(s1.str_, s2.str_, s1.len_) != 0);
    }

    friend bool operator==(const BasicString &s1, const char *s2) { return s1.str_ && strcmp(s1.str_, s2) == 0; }

    friend bool operator!=(const BasicString &s1, const char *s2) { return !operator==(s1, s2); }

    friend bool operator==(const BasicString &s1, const std::string_view s2) { return s1.str_ == s2; }

    friend bool operator!=(const BasicString &s1, const std::string_view s2) { return s1.str_ != s2; }

    friend bool operator==(const char *s1, const BasicString &s2) {
        return s2.str_ ? strcmp(s1, s2.str_) == 0 : (*s1 == '\0');
    }

    friend bool operator!=(const char *s1, const BasicString &s2) { return !operator==(s1, s2); }

    friend bool operator==(const std::string_view s1, const BasicString &s2) {
        return s2.str_ ? s1 == s2.str_ : s1.empty();
    }

    friend bool operator!=(const std::string_view s1, const BasicString &s2) { return !operator==(s1, s2); }
};
static_assert(sizeof(BasicString<>) == sizeof(void *) + sizeof(size_t));

using String = BasicString<>;
} // namespace Snd
