#pragma once

#include <new>
#include <stdexcept>

namespace Sys {
template <typename T>
class Optional {
    T *p_obj_;
    alignas(T) char data_[sizeof(T)];

public:
    Optional() : p_obj_(nullptr) {}
    Optional(const Optional &rhs) : p_obj_(nullptr) {
        if (rhs.initialized()) {
            p_obj_ = new(&data_[0]) T(*rhs.p_obj_);
        }
    }
    Optional(Optional &&rhs) noexcept : p_obj_(nullptr) {
        if (rhs.initialized()) {
            p_obj_ = new(&data_[0]) T(std::move(*rhs.p_obj_));
            rhs.p_obj_ = nullptr;
        }
    }
    explicit Optional(const T &rhs) {
        p_obj_ = new (&data_[0]) T(rhs);
    }
    explicit Optional(T &&rhs) {
        p_obj_ = new (&data_[0]) T(std::move(rhs));
    }
    ~Optional() {
        destroy();
    }

    Optional &operator=(const Optional &rhs) {
        if (this != &rhs) {
            destroy();
            if (rhs.initialized()) {
                p_obj_ = new(&data_[0]) T(*rhs.p_obj_);
            }
        }
        return *this;
    }

    Optional &operator=(Optional &&rhs) noexcept {
        if (this != &rhs) {
            destroy();
            if (rhs.initialized()) {
                p_obj_ = new(&data_[0]) T(std::move(*rhs.p_obj_));
                rhs.p_obj_ = nullptr;
            }
        }
        return *this;
    }

    Optional &operator=(const T &rhs) {
        if (p_obj_ != &rhs) {
            destroy();
            p_obj_ = new(&data_[0]) T(rhs);
        }
        return *this;
    }

    Optional &operator=(T &&rhs) {
        if (p_obj_ != &rhs) {
            destroy();
            p_obj_ = new(&data_[0]) T(std::move(rhs));
        }
        return *this;
    }

    bool initialized() const {
        return p_obj_ != nullptr;
    }

    void destroy() {
        if (p_obj_) {
            p_obj_->~T();
            p_obj_ = nullptr;
        }
    }

    const T &GetValueOr(const T &default_val) {
        if (p_obj_) {
            return *p_obj_;
        } else {
            return default_val;
        }
    }

    const T &GetValue() {
        if (p_obj_) {
            return *p_obj_;
        } else {
            throw std::runtime_error("not initialized");
        }
    }
};
}
