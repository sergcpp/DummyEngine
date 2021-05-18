#pragma once

#include <atomic>

namespace Ren {
#ifndef REN_ALIGNED_MALLOC_DEFINED
inline void *aligned_malloc(size_t size, size_t alignment) {
#if defined(_MSC_VER) || defined(__MINGW32__)
    return _mm_malloc(size, alignment);
#elif __STDC_VERSION__ >= 201112L
    return aligned_alloc(alignment, size);
#else
#ifdef __APPLE__
    void *p = nullptr;
    size_t mod = alignment % sizeof(void *);
    if (mod) {
        alignment += sizeof(void *) - mod;
    }
    posix_memalign(&p, alignment, size);
    return p;
#else
    return memalign(alignment, size);
#endif
#endif
}

inline void aligned_free(void *p) {
#if defined(_MSC_VER) || defined(__MINGW32__)
    _mm_free(p);
#elif __STDC_VERSION__ >= 201112L
    free(p);
#else
    free(p);
#endif
}
#define REN_ALIGNED_MALLOC_DEFINED
#endif

template <typename T, int AlignmentOfT = alignof(T)> class RingBuffer {
    T *buf_;
    size_t head_, tail_; // stored unbounded (should be masked later)
    size_t capacity_;    // required to be power of two!

    size_t mask(const size_t i) const { return i & (capacity_ - 1); }

    T &at(const size_t i) {
        assert(is_valid(i));
        return buf_[mask(i)];
    }

    bool is_valid(const size_t i) const { return i - tail_ <= head_ - tail_; }

  public:
    RingBuffer() : buf_(nullptr), head_(0), tail_(0), capacity_(0) {}
    explicit RingBuffer(size_t size) : RingBuffer() { reserve(size); }
    ~RingBuffer() {
        if (!empty()) {
            for (size_t i = head_ - 1; i != tail_ - 1; --i) {
                buf_[mask(i)].~T();
            }
        }
        aligned_free(buf_);
    }
    RingBuffer(const RingBuffer &) = delete;
    RingBuffer &operator=(const RingBuffer &) = delete;

    void reserve(size_t req_capacity) {
        if (++req_capacity <= capacity_) {
            return;
        }

        size_t new_capacity = capacity_ == 0 ? 1 : capacity_;
        while (new_capacity < req_capacity) {
            new_capacity *= 2;
        }

        T *new_buf = (T *)aligned_malloc(new_capacity * sizeof(T), AlignmentOfT);
        size_t new_head = size();

        if (new_head) {
            size_t src = head_ - 1;
            size_t dst = new_head - 1;
            do {
                new (&new_buf[dst--]) T(std::move(buf_[mask(src)]));
                buf_[mask(src--)].~T();
            } while (src != tail_ - 1);
        }

        aligned_free(buf_);

        buf_ = new_buf;
        head_ = new_head;
        tail_ = 0;
        capacity_ = new_capacity;
    }

    void clear() {
        for (size_t i = head_ - 1; i != tail_ - 1; --i) {
            buf_[mask(i)].~T();
        }
        head_ = tail_ = 0;
    }

    bool empty() const { return (tail_ == head_); }

    size_t capacity() const { return capacity_; }
    size_t size() const { return head_ - tail_; }

    T &front() {
        assert(tail_ != head_);
        return buf_[mask(tail_)];
    }

    const T &front() const {
        assert(tail_ != head_);
        return buf_[mask(tail_)];
    }

    T &back() {
        assert(tail_ != head_);
        return buf_[mask(head_ - 1)];
    }

    const T &back() const {
        assert(tail_ != head_);
        return buf_[mask(head_ - 1)];
    }

    void push_back(const T &item) {
        reserve(size() + 1);
        new (buf_ + mask(head_++)) T(item);
    }

    void push_back(T &&item) {
        reserve(size() + 1);
        new (buf_ + mask(head_++)) T(std::move(item));
    }

    void pop_back() {
        assert(tail_ != head_);
        buf_[mask(--head_)].~T();
    }

    void push_front(const T &item) {
        reserve(size() + 1);
        new (buf_ + mask(--tail_)) T(item);
    }

    void push_front(T &&item) {
        reserve(size() + 1);
        new (buf_ + mask(--tail_)) T(std::move(item));
    }

    void pop_front() {
        assert(tail_ != head_);
        buf_[mask(tail_++)].~T();
    }

    class iterator : public std::iterator<std::random_access_iterator_tag, T> {
        friend class RingBuffer<T>;

        RingBuffer<T> *container_;
        size_t index_;

        iterator(RingBuffer<T> *container, const size_t index)
            : container_(container), index_(index) {}

      public:
        T &operator*() const { return container_->at(index_); }
        T *operator->() const { return &container_->at(index_); }
        iterator &operator++() {
            ++index_;
            return *this;
        }
        iterator operator++(int) {
            Iterator tmp(*this);
            ++(*this);
            return tmp;
        }
        iterator &operator--() {
            --index_;
            return *this;
        }

        uint32_t index() const { return index_; }

        bool operator==(const iterator &rhs) const {
            assert(container_ == rhs.container_);
            return index_ == rhs.index_;
        }

        bool operator!=(const iterator &rhs) const {
            assert(container_ == rhs.container_);
            return index_ != rhs.index_;
        }

        bool operator<(const iterator &rhs) const {
            assert(container_ == rhs.container_);
            return index_ < rhs.index_;
        }

        bool operator<=(const iterator &rhs) const {
            assert(container_ == rhs.container_);
            return index_ < rhs.index_;
        }

        bool operator>(const iterator &rhs) const {
            assert(container_ == rhs.container_);
            return index_ > rhs.index_;
        }

        bool operator>=(const iterator &rhs) const {
            assert(container_ == rhs.container_);
            return index_ > rhs.index_;
        }

        template <typename IntType> iterator &operator[](IntType i) {
            return (*this) + i;
        }

        template <typename IntType> iterator &operator+=(IntType n) {
            index_ += n;
            return (*this);
        }

        template <typename IntType> iterator &operator-=(IntType n) {
            index_ -= n;
            return (*this);
        }

        template <typename IntType>
        friend iterator operator+(const iterator &lhs, IntType n) {
            iterator it = lhs;
            return it += n;
        }

        template <typename IntType>
        friend iterator operator+(IntType n, const iterator &rhs) {
            iterator it = rhs;
            return it += n;
        }

        template <typename IntType>
        friend iterator operator-(const iterator &lhs, IntType n) {
            iterator it = lhs;
            return it -= n;
        }

        friend difference_type operator-(const iterator &lhs, const iterator &rhs) {
            return difference_type(lhs.index_) - rhs.index_;
        }

        using difference_type = std::ptrdiff_t;
        using value_type = T;
        using pointer = T *;
        using reference = T &;
        using iterator_category = std::random_access_iterator_tag;
    };

    iterator begin() { return {this, tail_}; }
    iterator end() { return {this, head_}; }

    void insert(iterator it, const T &item) {
        assert(it.container_ == this);
        assert(is_valid(it.index_));

        reserve(size() + 1);

        if (it != end()) {
            size_t src = head_ - 1;
            do {
                new (&buf_[mask(src + 1)]) T(std::move(buf_[mask(src)]));
                buf_[mask(src)].~T();
            } while (src-- != it.index_);
        }

        ++head_;
        new (buf_ + mask(it.index_)) T(item);
    }

    iterator erase(iterator it) {
        assert(it.container_ == this);
        assert(is_valid(it.index_));

        buf_[mask(it.index_)].~T();

        if (it != end() - 1) {
            size_t dst = it.index_;
            do {
                new (&buf_[mask(dst)]) T(std::move(buf_[mask(dst + 1)]));
                buf_[mask(dst + 1)].~T();
            } while (++dst != head_);
        }

        --head_;
        return {this, it.index_};
    }
};

template <class T> class AtomicRingBuffer {
    T *buf_;
    std::atomic_int head_, tail_;
    int size_;

    int next(int cur) const { return (cur + 1) % size_; }

  public:
    explicit AtomicRingBuffer(int size) : size_(size) {
        head_ = 0;
        tail_ = 0;
        buf_ = new T[size];
    }
    ~AtomicRingBuffer() { delete[] buf_; }
    AtomicRingBuffer(const RingBuffer &) = delete;
    AtomicRingBuffer &operator=(const RingBuffer &) = delete;

    bool Empty() const {
        const int tail = tail_.load(std::memory_order_relaxed);
        return (tail == head_.load(std::memory_order_acquire));
    }

    bool Full() const {
        const int head = head_.load(std::memory_order_relaxed);
        const int next_head = next(head);
        return next_head == tail_.load(std::memory_order_acquire);
    }

    int Capacity() const { return size_; }

    bool Push(const T &item) {
        const int head = head_.load(std::memory_order_relaxed);
        const int next_head = next(head);
        if (next_head == tail_.load(std::memory_order_acquire)) {
            return false;
        }
        buf_[head] = item;
        head_.store(next_head, std::memory_order_release);

        return true;
    }

    bool Push(T &&item) {
        const int head = head_.load(std::memory_order_relaxed);
        const int next_head = next(head);
        if (next_head == tail_.load(std::memory_order_acquire)) {
            return false;
        }
        buf_[head] = std::move(item);
        head_.store(next_head, std::memory_order_release);

        return true;
    }

    bool Pop(T &item) {
        const int tail = tail_.load(std::memory_order_relaxed);
        if (tail == head_.load(std::memory_order_acquire)) {
            return false;
        }
        item = std::move(buf_[tail]);
        tail_.store(next(tail), std::memory_order_release);

        return true;
    }
};
} // namespace Ren
