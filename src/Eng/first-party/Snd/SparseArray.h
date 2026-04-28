#pragma once

#include <cassert>
#include <cstring>
#include <iterator>
#include <utility>

#include "AlignedAlloc.h"

namespace Snd {
inline int CountTrailingZeroes(const uint64_t mask) {
#ifdef _MSC_VER
    return int(_tzcnt_u64(mask));
#else
    return __builtin_ctzll(mask);
#endif
}

template <typename T, typename Allocator = aligned_allocator<uint64_t, alignof(T)>> class SparseArray : Allocator {
  protected:
    uint64_t *ctrl_;
    T *data_;
    uint32_t capacity_, size_;
    uint32_t first_free_;

    static_assert(sizeof(T) >= sizeof(uint32_t));

    static size_t ctrl_size(const uint32_t cap) {
        const uint32_t word_count = (cap + 63) / 64;
        return word_count * sizeof(uint64_t);
    }

    static size_t mem_size(const uint32_t cap) {
        size_t mem_size = ctrl_size(cap);
        if (mem_size % alignof(T)) {
            mem_size += alignof(T) - (mem_size % alignof(T));
        }
        mem_size += sizeof(T) * cap;
        return mem_size;
    }

  public:
    SparseArray() : SparseArray(0) {}
    explicit SparseArray(const uint32_t initial_capacity)
        : ctrl_(nullptr), data_(nullptr), capacity_(0), size_(0), first_free_(0) {
        if (initial_capacity) {
            Reserve(initial_capacity);
        }
    }

    ~SparseArray() {
        Clear();
        this->deallocate(ctrl_, mem_size(capacity_));
    }

    SparseArray(const SparseArray &rhs) : ctrl_(nullptr), data_(nullptr), capacity_(0), size_(0), first_free_(0) {
        Reserve(rhs.capacity_);

        if (ctrl_) {
            memcpy(ctrl_, rhs.ctrl_, ctrl_size(rhs.capacity_));

            for (uint32_t i = 0; i < rhs.capacity_; ++i) {
                if (ctrl_[i / 64] & (1ull << (i % 64))) {
                    new (&data_[i]) T(rhs.data_[i]);
                } else {
                    // copy next free index
                    memcpy(data_ + i, rhs.data_ + i, sizeof(uint32_t));
                }
            }
        }

        first_free_ = rhs.first_free_;
        size_ = rhs.size_;
    }

    SparseArray(SparseArray &&rhs) noexcept
        : Allocator(static_cast<Allocator &&>(rhs)), ctrl_(nullptr), data_(nullptr), capacity_(0), size_(0),
          first_free_(0) {
        ctrl_ = std::exchange(rhs.ctrl_, nullptr);
        data_ = std::exchange(rhs.data_, nullptr);

        capacity_ = std::exchange(rhs.capacity_, 0);
        size_ = std::exchange(rhs.size_, 0);
        first_free_ = std::exchange(rhs.first_free_, 0);
    }

    SparseArray &operator=(const SparseArray &rhs) {
        if (this == &rhs) {
            return *this;
        }
        if constexpr (std::allocator_traits<Allocator>::propagate_on_container_copy_assignment::value) {
            Allocator::operator=(static_cast<const Allocator &>(rhs));
        }

        Clear();
        this->deallocate(ctrl_, mem_size(capacity_));

        ctrl_ = nullptr;
        data_ = nullptr;
        capacity_ = 0;

        Reserve(rhs.capacity_);

        if (rhs.ctrl_) {
            memcpy(ctrl_, rhs.ctrl_, ctrl_size(capacity_));
        }

        for (uint32_t i = 0; i < rhs.capacity_; ++i) {
            if (ctrl_[i / 64] & (1ull << (i % 64))) {
                new (&data_[i]) T(rhs.data_[i]);
            } else {
                // copy next free index
                memcpy(data_ + i, rhs.data_ + i, sizeof(uint32_t));
            }
        }

        first_free_ = rhs.first_free_;
        size_ = rhs.size_;

        return (*this);
    }

    SparseArray &operator=(SparseArray &&rhs) noexcept {
        if (this == &rhs) {
            return *this;
        }
        if constexpr (std::allocator_traits<Allocator>::propagate_on_container_move_assignment::value) {
            Allocator::operator=(static_cast<Allocator &&>(rhs));
        }

        Clear();
        this->deallocate(ctrl_, mem_size(capacity_));

        ctrl_ = std::exchange(rhs.ctrl_, nullptr);
        data_ = std::exchange(rhs.data_, nullptr);

        capacity_ = std::exchange(rhs.capacity_, 0);
        size_ = std::exchange(rhs.size_, 0);
        first_free_ = std::exchange(rhs.first_free_, 0);

        return (*this);
    }

    uint32_t size() const { return size_; }
    uint32_t capacity() const { return capacity_; }

    bool empty() const { return size_ == 0; }

    T *data() { return data_; }
    const T *data() const { return data_; }

    void Clear() {
        const uint32_t word_count = (capacity_ + 63) / 64;
        for (uint32_t i = 0; i < word_count && size_; ++i) {
            uint64_t mask = ctrl_[i];
            while (mask) {
                Erase(64 * i + CountTrailingZeroes(mask));
                mask &= mask - 1;
            }
        }
    }

    void Reserve(uint32_t new_capacity) {
        if (new_capacity <= capacity_) {
            return;
        }

        uint64_t *old_ctrl = ctrl_;
        T *old_data = data_;
        const uint32_t old_word_count = (capacity_ + 63) / 64;
        const uint32_t new_word_count = (new_capacity + 63) / 64;

        size_t total_mem = ctrl_size(new_capacity);
        if (total_mem % alignof(T)) {
            total_mem += alignof(T) - (total_mem % alignof(T));
        }
        const size_t data_start = total_mem;
        total_mem += sizeof(T) * new_capacity;
        assert(total_mem == mem_size(new_capacity));

        ctrl_ = this->allocate(total_mem);
        data_ = reinterpret_cast<T *>(uintptr_t(ctrl_) + data_start);
        assert(uintptr_t(data_) % alignof(T) == 0);

        // copy old control bits
        if (old_ctrl) {
            memcpy(ctrl_, old_ctrl, old_word_count * sizeof(uint64_t));
        }
        // fill rest with zeroes
        memset(ctrl_ + old_word_count, 0, (new_word_count - old_word_count) * sizeof(uint64_t));

        // move old data
        for (uint32_t i = 0; i < capacity_; ++i) {
            if (ctrl_[i / 64] & (1ull << (i % 64))) {
                new (&data_[i]) T(std::move(old_data[i]));
                old_data[i].~T();
            } else {
                // copy next free index
                memcpy(data_ + i, old_data + i, sizeof(uint32_t));
            }
        }

        this->deallocate(old_ctrl, mem_size(capacity_));

        for (uint32_t i = capacity_; i < new_capacity - 1; i++) {
            const uint32_t next_free = i + 1;
            memcpy(data_ + i, &next_free, sizeof(uint32_t));
        }

        memcpy(data_ + new_capacity - 1, &first_free_, sizeof(uint32_t));
        first_free_ = capacity_;

        capacity_ = new_capacity;
    }

    template <class... Args> uint32_t Emplace(Args &&...args) {
        if (size_ + 1 > capacity_) {
            Reserve(capacity_ ? (capacity_ * 2) : 64);
        }
        const uint32_t index = first_free_;
        assert((ctrl_[index / 64] & (1ull << (index % 64))) == 0);
        memcpy(&first_free_, data_ + index, sizeof(uint32_t));

        T *el = data_ + index;
        new (el) T(std::forward<Args>(args)...);

        ctrl_[index / 64] |= (1ull << (index % 64));

        ++size_;
        return index;
    }

    uint32_t Push(const T &_el) {
        if (size_ + 1 > capacity_) {
            Reserve(capacity_ ? (capacity_ * 2) : 64);
        }
        const uint32_t index = first_free_;
        assert((ctrl_[index / 64] & (1ull << (index % 64))) == 0);
        memcpy(&first_free_, data_ + index, sizeof(uint32_t));

        T *el = data_ + index;
        new (el) T(_el);
        ctrl_[index / 64] |= (1ull << (index % 64));

        ++size_;
        return index;
    }

    void Erase(const uint32_t index) {
        assert((ctrl_[index / 64] & (1ull << (index % 64))) && "Invalid index!");

        data_[index].~T();
        ctrl_[index / 64] &= ~(1ull << (index % 64));

        memcpy(data_ + index, &first_free_, sizeof(uint32_t));
        first_free_ = index;
        --size_;
    }

    T &at(const uint32_t index) {
        assert((ctrl_[index / 64] & (1ull << (index % 64))) && "Invalid index!");
        return data_[index];
    }

    const T &at(const uint32_t index) const {
        assert((ctrl_[index / 64] & (1ull << (index % 64))) && "Invalid index!");
        return data_[index];
    }

    T &operator[](const uint32_t index) {
        assert((ctrl_[index / 64] & (1ull << (index % 64))) && "Invalid index!");
        return data_[index];
    }

    const T &operator[](const uint32_t index) const {
        assert((ctrl_[index / 64] & (1ull << (index % 64))) && "Invalid index!");
        return data_[index];
    }

    T *GetOrNull(const uint32_t index) {
        if (index < capacity_ && (ctrl_[index / 64] & (1ull << (index % 64)))) {
            return &data_[index];
        } else {
            return nullptr;
        }
    }

    const T *GetOrNull(const uint32_t index) const {
        if (index < capacity_ && (ctrl_[index / 64] & (1ull << (index % 64)))) {
            return &data_[index];
        } else {
            return nullptr;
        }
    }

    class SparseArrayIterator {
        friend class SparseArray<T>;

        SparseArray<T> *container_;
        uint32_t index_;

        SparseArrayIterator(SparseArray<T> *container, uint32_t index) : container_(container), index_(index) {}

      public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = T;
        using difference_type = std::ptrdiff_t;
        using pointer = T *;
        using reference = T &;

        T &operator*() { return container_->at(index_); }
        T *operator->() { return &container_->at(index_); }
        SparseArrayIterator &operator++() {
            index_ = container_->NextOccupied(index_);
            return *this;
        }
        SparseArrayIterator operator++(int) {
            SparseArrayIterator tmp(*this);
            ++(*this);
            return tmp;
        }

        uint32_t index() const { return index_; }

        bool operator<(const SparseArrayIterator &rhs) const { return index_ < rhs.index_; }
        bool operator<=(const SparseArrayIterator &rhs) const { return index_ <= rhs.index_; }
        bool operator>(const SparseArrayIterator &rhs) const { return index_ > rhs.index_; }
        bool operator>=(const SparseArrayIterator &rhs) const { return index_ >= rhs.index_; }
        bool operator==(const SparseArrayIterator &rhs) const { return index_ == rhs.index_; }
        bool operator!=(const SparseArrayIterator &rhs) const { return index_ != rhs.index_; }
    };

    class SparseArrayConstIterator {
        friend class SparseArray<T>;

        const SparseArray<T> *container_;
        uint32_t index_;

        SparseArrayConstIterator(const SparseArray<T> *container, uint32_t index)
            : container_(container), index_(index) {}

      public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = T;
        using difference_type = std::ptrdiff_t;
        using pointer = const T *;
        using reference = const T &;

        const T &operator*() const { return container_->at(index_); }
        const T *operator->() const { return &container_->at(index_); }
        SparseArrayConstIterator &operator++() {
            index_ = container_->NextOccupied(index_);
            return *this;
        }

        SparseArrayConstIterator operator++(int) {
            SparseArrayConstIterator tmp(*this);
            ++(*this);
            return tmp;
        }

        uint32_t index() const { return index_; }

        bool operator<(const SparseArrayConstIterator &rhs) const { return index_ < rhs.index_; }
        bool operator<=(const SparseArrayConstIterator &rhs) const { return index_ <= rhs.index_; }
        bool operator>(const SparseArrayConstIterator &rhs) const { return index_ > rhs.index_; }
        bool operator>=(const SparseArrayConstIterator &rhs) const { return index_ >= rhs.index_; }
        bool operator==(const SparseArrayConstIterator &rhs) const { return index_ == rhs.index_; }
        bool operator!=(const SparseArrayConstIterator &rhs) const { return index_ != rhs.index_; }
    };

    using iterator = SparseArrayIterator;
    using const_iterator = SparseArrayConstIterator;

    iterator begin() {
        if (!empty()) {
            const uint32_t word_count = (capacity_ + 63) / 64;
            for (uint32_t i = 0; i < word_count; ++i) {
                const uint64_t mask = ctrl_[i];
                if (mask) {
                    return iterator(this, 64 * i + CountTrailingZeroes(mask));
                }
            }
        }
        return end();
    }
    const_iterator begin() const { return cbegin(); }

    const_iterator cbegin() const {
        if (!empty()) {
            const uint32_t word_count = (capacity_ + 63) / 64;
            for (uint32_t i = 0; i < word_count; ++i) {
                const uint64_t mask = ctrl_[i];
                if (mask) {
                    return const_iterator(this, 64 * i + CountTrailingZeroes(mask));
                }
            }
        }
        return cend();
    }

    iterator end() { return iterator(this, capacity_); }
    const_iterator end() const { return cend(); }
    const_iterator cend() const { return const_iterator(this, capacity_); }

    iterator iter_at(uint32_t i) { return iterator(this, i); }

    const_iterator citer_at(uint32_t i) const { return const_iterator(this, i); }

    iterator Erase(iterator it) {
        const uint32_t next_index = NextOccupied(it.index());
        Erase(it.index());
        return iterator(this, next_index);
    }

    uint32_t FindOccupiedInRange(uint32_t start, uint32_t end) const {
        if (start >= end) {
            return end;
        }

        const uint32_t start_word = start / 64;
        const uint32_t last_word = (end - 1) / 64; // word containing last valid index
        const uint64_t start_mask = ~0ull << (start % 64);
        const uint64_t end_mask = (end % 64) ? ((1ull << (end % 64)) - 1) : ~0ull;

        if (start_word == last_word) {
            const uint64_t mask = ctrl_[start_word] & start_mask & end_mask;
            if (mask) {
                return 64 * start_word + CountTrailingZeroes(mask);
            }
            return end;
        }

        {
            const uint64_t mask = ctrl_[start_word] & start_mask;
            if (mask) {
                return 64 * start_word + CountTrailingZeroes(mask);
            }
        }

        for (uint32_t i = start_word + 1; i < last_word; ++i) {
            const uint64_t mask = ctrl_[i];
            if (mask) {
                return 64 * i + CountTrailingZeroes(mask);
            }
        }

        {
            const uint64_t mask = ctrl_[last_word] & end_mask;
            if (mask) {
                return 64 * last_word + CountTrailingZeroes(mask);
            }
        }

        return end;
    }

  private:
    uint32_t NextOccupied(uint32_t index) const {
        assert((ctrl_[index / 64] & (1ull << (index % 64))) && "Invalid index!");
        if (++index >= capacity_) {
            return capacity_;
        }

        const uint32_t start_word = index / 64;
        const uint32_t word_count = (capacity_ + 63) / 64;

        { // Check first word
            uint64_t mask = ctrl_[start_word] & (~0ull << (index % 64));
            if (mask) {
                return 64 * start_word + CountTrailingZeroes(mask);
            }
        }

        for (uint32_t i = start_word + 1; i < word_count; ++i) {
            const uint64_t mask = ctrl_[i];
            if (mask) {
                return 64 * i + CountTrailingZeroes(mask);
            }
        }

        return capacity_;
    }
};
} // namespace Snd
