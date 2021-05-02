#pragma once

#include <atomic>

namespace Ren {
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

template <typename T, int AlignmentOfT = alignof(T)> class RingBuffer {
    T *buf_;
    size_t head_, tail_;
    size_t capacity_;

    size_t next(const size_t i) const { return (i + 1) % capacity_; }
    size_t prev(const size_t i) const { return (capacity_ + i - 1) % capacity_; }

    bool is_valid(const size_t i) {
        if (head_ >= tail_) {
            return (tail_ <= i) && (i <= head_);
        }
        return (i >= 0 && i <= head_) || (i >= tail_ && i < capacity_);
    }

    T &at(const size_t i) {
        assert(is_valid(i));
        return buf_[i];
    }

  public:
    RingBuffer() : buf_(nullptr), head_(0), tail_(0), capacity_(0) {}
    explicit RingBuffer(size_t size) : RingBuffer() { Reserve(size); }
    ~RingBuffer() {
        for (size_t i = prev(head_); i != prev(tail_); i = prev(i)) {
            buf_[i].~T();
        }
        aligned_free(buf_);
    }
    RingBuffer(const RingBuffer &) = delete;
    RingBuffer &operator=(const RingBuffer &) = delete;

    void Reserve(size_t req_capacity) {
        if (++req_capacity <= capacity_) {
            return;
        }

        size_t new_capacity = capacity_ == 0 ? 1 : capacity_;
        while (new_capacity < req_capacity) {
            new_capacity *= 2;
        }

        T *new_buf = (T *)aligned_malloc(new_capacity * sizeof(T), AlignmentOfT);
        size_t new_head = Size();

        if (capacity_) {
            size_t src = prev(head_);
            size_t dst = prev(new_head);
            do {
                new (new_buf + dst) T(std::move(buf_[src]));
                buf_[src].~T();
                src = prev(src);
                dst = prev(dst);
            } while (src != prev(tail_));
        }

        aligned_free(buf_);

        buf_ = new_buf;
        head_ = new_head;
        tail_ = 0;
        capacity_ = new_capacity;
    }

    void Clear() {
        for (size_t i = prev(head_); i != prev(tail_); i = prev(i)) {
            buf_[i].~T();
        }
        head_ = tail_ = 0;
    }

    bool Empty() const { return (tail_ == head_); }

    bool Full() const { return next(head_) == tail_; }

    size_t Capacity() const { return capacity_; }
    size_t Size() const {
        if (head_ >= tail_) {
            return head_ - tail_;
        }
        return capacity_ + head_ - tail_;
    }

    void Push(const T &item) {
        Reserve(Size() + 1);

        new (buf_ + head_) T(item);
        head_ = next(head_);
    }

    void Push(T &&item) {
        Reserve(Size() + 1);

        new (buf_ + head_) T(std::move(item));
        head_ = next(head_);
    }

    bool Pop(T &item) {
        if (tail_ == head_) {
            return false;
        }
        item = std::move(buf_[tail_]);
        tail_ = next(tail_);

        return true;
    }

    class RingBufferIterator : public std::iterator<std::bidirectional_iterator_tag, T> {
        friend class RingBuffer<T>;

        RingBuffer<T> *container_;
        size_t index_;

        RingBufferIterator(RingBuffer<T> *container, const size_t index)
            : container_(container), index_(index) {}

      public:
        T &operator*() { return container_->at(index_); }
        T *operator->() { return &container_->at(index_); }
        RingBufferIterator &operator++() {
            index_ = container_->next(index_);
            assert(container_->is_valid(index_));
            return *this;
        }
        RingBufferIterator operator++(int) {
            RingBufferIterator tmp(*this);
            ++(*this);
            return tmp;
        }
        RingBufferIterator &operator--() {
            index_ = container_->prev(index_);
            assert(container_->is_valid(index_));
            return *this;
        }

        uint32_t index() const { return index_; }

        bool operator==(const RingBufferIterator &rhs) {
            assert(container_ == rhs.container_);
            return index_ == rhs.index_;
        }
        bool operator!=(const RingBufferIterator &rhs) {
            assert(container_ == rhs.container_);
            return index_ != rhs.index_;
        }
    };

    using iterator = RingBufferIterator;

    iterator begin() { return {this, tail_}; }
    iterator end() { return {this, head_}; }
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
