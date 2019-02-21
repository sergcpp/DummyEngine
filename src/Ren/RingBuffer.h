#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#include <atomic>

template <class T>
class RingBuffer {
    T               *buf_;
    std::atomic_int head_, tail_;
    int             size_;

    int next(int cur) {
        return (cur + 1) % size_;
    }
public:
    explicit RingBuffer(int size) : size_(size) {
        head_ = 0;
        tail_ = 0;
        buf_ = new T[size];
    }
    ~RingBuffer() {
        delete[] buf_;
    }
    RingBuffer(const RingBuffer &) = delete;
    RingBuffer &operator=(const RingBuffer &) = delete;

    bool Push(const T &item) {
        int head = head_.load(std::memory_order_relaxed);
        int next_head = next(head);
        if (next_head == tail_.load(std::memory_order_acquire)) {
            return false;
        }
        buf_[head] = item;
        head_.store(next_head, std::memory_order_release);

        return true;
    }

    bool Push(const T &&item) {
        int head = head_.load(std::memory_order_relaxed);
        int next_head = next(head);
        if (next_head == tail_.load(std::memory_order_acquire)) {
            return false;
        }
        buf_[head] = std::move(item);
        head_.store(next_head, std::memory_order_release);

        return true;
    }

    bool Pop(T &item) {
        int tail = tail_.load(std::memory_order_relaxed);
        if (tail == head_.load(std::memory_order_acquire)) {
            return false;
        }
        item = std::move(buf_[tail]);
        tail_.store(next(tail), std::memory_order_release);

        return true;
    }

};

#endif // RING_BUFFER_H