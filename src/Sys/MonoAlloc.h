#pragma once

#include <stdexcept>

namespace Sys {
    template <typename T>
    class MonoAlloc {
    public:
        // The following will be the same for virtually all allocators.
        typedef T * pointer;
        typedef const T * const_pointer;
        typedef T& reference;
        typedef const T& const_reference;
        typedef T value_type;
        typedef std::size_t size_type;
        typedef ptrdiff_t difference_type;

        T *address(T &r) const {
            return &r;
        }

        const T *address(const T &s) const {
            return &s;
        }

        std::size_t max_size() const {
            // The following has been carefully written to be independent of
            // the definition of size_t and to avoid signed/unsigned warnings.
            return (static_cast<std::size_t>(0) - static_cast<std::size_t>(1)) / sizeof(T);
        }


        // The following must be the same for all allocators.
        template <typename U>
        struct rebind {
            typedef MonoAlloc<U> other;
        };

        bool operator!=(const MonoAlloc& other) const {
            return !(*this == other);
        }

        template<class U, class... Args>
        void construct(U* p, Args&&... args) {
            ::new((void *)p) U(std::forward<Args>(args)...);
        }

        template<class U>
        void destroy(U *p) {
            ((void)p);
            p->~U();
        }

        // Returns true if and only if storage allocated from *this
        // can be deallocated from other, and vice versa.
        bool operator==(const MonoAlloc& other) const {
            ((void)other);
            return false;
        }

        MonoAlloc(char *memory, size_t memory_size) : memory_(memory), memory_size_(memory_size) {
            memory_pos_ = 0;
            p_memory_pos_ = &memory_pos_;
        }
        MonoAlloc(const MonoAlloc &other)
            : memory_(other.memory_), memory_size_(other.memory_size_), memory_pos_(0xffffffff), p_memory_pos_(other.p_memory_pos_) {
        }

        MonoAlloc& operator=(const MonoAlloc &) = delete;

        template <typename U> MonoAlloc(const MonoAlloc<U> &other)
            : memory_(other.memory_), memory_size_(other.memory_size_), memory_pos_(0xffffffff), p_memory_pos_(other.p_memory_pos_) {
        }

        ~MonoAlloc() {}


        T *allocate(const std::size_t n) {
            if (n == 0) {
                return nullptr;
            }

            if (n > max_size()) {
                throw std::length_error("MonoAlloc<T>::allocate() - Integer overflow.");
            }

            if ((*p_memory_pos_) + n * sizeof(T) >= memory_size_) {
                throw std::runtime_error("MonoAlloc<T>::allocate() - Buffer overflow.");
            }

            T * const p = reinterpret_cast<T *>(memory_ + (*p_memory_pos_));
            (*p_memory_pos_) += n * sizeof(T);

            return p;
        }

        void deallocate(T * const p, const std::size_t n) const {
            ((void)p);
            ((void)n);
        }


        // The following will be the same for all allocators that ignore hints.
        template <typename U>
        T *allocate(const std::size_t n, const U * /* const hint */) const {
            return allocate(n);
        }

    private:
        char *memory_;
        mutable size_t memory_pos_;
        size_t memory_size_;
        size_t *p_memory_pos_;

        template<class U>
        friend class MonoAlloc;
    };
}