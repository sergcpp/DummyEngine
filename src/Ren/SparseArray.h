#pragma once

#include <cassert>
#include <iterator>

namespace Ren {
template <typename T>
class SparseArray {
protected:
    uint8_t     *ctrl_;
    T           *data_;
    uint32_t    capacity_, size_;
    uint32_t    first_free_;

    static_assert(sizeof(T) >= sizeof(uint32_t), "!");
public:
    SparseArray(uint32_t initial_capacity = 0)
        : ctrl_(nullptr), data_(nullptr), capacity_(0), size_(0), first_free_(0) {
        if (initial_capacity) {
            reserve(initial_capacity);
        }
    }

    ~SparseArray() {
        clear();
        delete[] ctrl_;
    }

    SparseArray(const SparseArray &rhs)
        : ctrl_(nullptr), data_(nullptr), capacity_(0), size_(0), first_free_(0) {
        reserve(rhs.capacity_);

        memcpy(ctrl_, rhs.ctrl_, (capacity_ + 7) / 8);
        
        for (uint32_t i = 0; i < capacity_; i++) {
            if (ctrl_[i / 8] & (1 << (i % 8))) {
                data_[i] = rhs.data_[i];
            }
        }
    }

    SparseArray &operator=(const SparseArray &rhs) {
        clear();
        delete[] ctrl_;

        ctrl_ = nullptr;
        data_ = nullptr;

        reserve(rhs.capacity_);

        memcpy(ctrl_, rhs.ctrl_, capacity_);

        for (uint32_t i = 0; i < capacity_; i++) {
            if (ctrl_[i / 8] & (1 << (i % 8))) {
                data_[i] = rhs.data_[i];
            }
        }
    }

    uint32_t size() const { return size_; }
    uint32_t capacity() const { return capacity_; }

    T *data() { return data_; }
    const T *data() const { return data_; }

    void clear() {
        for (uint32_t i = 0; i < capacity_ && size_; i++) {
            if (ctrl_[i / 8] & (1 << (i % 8))) {
                size_--;
                data_[i].~T();
            }
        }
    }

    void reserve(uint32_t new_capacity) {
        if (new_capacity <= capacity_) return;

        uint8_t *old_ctrl = ctrl_;
        T *old_data = data_;

        size_t __aaa = alignof(T);

        size_t mem_size = (new_capacity + 7) / 8;
        if (mem_size % alignof(T)) {
            mem_size += alignof(T)-(mem_size % alignof(T));
        }

        size_t data_start = mem_size;
        mem_size += sizeof(T) * new_capacity;

        ctrl_ = new uint8_t[mem_size];
        data_ = reinterpret_cast<T *>(ctrl_ + data_start);
        assert(uintptr_t(data_) % alignof(T) == 0);

        // copy old control bits
        if (old_ctrl) {
            memcpy(ctrl_, old_ctrl, (capacity_ + 7) / 8);
        }
        // fill rest with zeroes
        memset(ctrl_ + (capacity_ + 7) / 8, 0, new_capacity - (capacity_ + 7) / 8);

        // move old data
        for (uint32_t i = 0; i < capacity_; i++) {
            if (ctrl_[i / 8] & (1 << (i % 8))) {
                T *el = data_ + i;
                new(el) T(std::move(old_data[i]));
                old_data[i].~T();
            }
        }

        delete[] old_ctrl;

        for (uint32_t i = capacity_; i < new_capacity - 1; i++) {
            uint32_t next_free = i + 1;
            memcpy(data_ + i, &next_free, sizeof(uint32_t));
        }

        memcpy(data_ + new_capacity - 1, &first_free_, sizeof(uint32_t));
        first_free_ = capacity_;

        capacity_ = new_capacity;
        if (size_ > capacity_) size_ = capacity_;
    }

    template<class... Args>
    uint32_t emplace(Args &&... args) {
        if (size_ + 1 > capacity_) {
            reserve(capacity_ ? (capacity_ * 2) : 8);
        }
        uint32_t index = first_free_;
        memcpy(&first_free_, data_ + index, sizeof(uint32_t));

        T *el = data_ + index;
        new(el) T(args...);

        ctrl_[index / 8] |= (1 << (index % 8));

        size_++;
        return index;
    }

    uint32_t push(const T &el) {
        if (size_ + 1 > capacity_) {
            reserve(capacity_ ? (capacity_ * 2) : 8);
        }
        uint32_t index = first_free_;
        memcpy(&first_free_, data_ + index, sizeof(uint32_t));

        data_[index] = el;
        ctrl_[index / 8] |= (1 << (index % 8));

        size_++;
        return index;
    }

    void erase(uint32_t index) {
        assert((ctrl_[index / 8] & (1 << (index % 8))) && "Invalid index!");

        data_[index].~T();
        ctrl_[index / 8] &= ~(1 << (index % 8));

        memcpy(data_ + index, &first_free_, sizeof(uint32_t));
        first_free_ = index;
        size_--;
    }

    T &at(uint32_t index) {
        assert((ctrl_[index / 8] & (1 << (index % 8))) && "Invalid index!");
        return data_[index];
    }

    const T &at(uint32_t index) const {
        assert((ctrl_[index / 8] & (1 << (index % 8))) && "Invalid index!");
        return data_[index];
    }

    T &operator[](uint32_t index) {
        assert((ctrl_[index / 8] & (1 << (index % 8))) && "Invalid index!");
        return data_[index];
    }

    const T &operator[](uint32_t index) const {
        assert((ctrl_[index / 8] & (1 << (index % 8))) && "Invalid index!");
        return data_[index];
    }

    class SparseArrayIterator : public std::iterator<std::bidirectional_iterator_tag, T> {
        friend class SparseArray<T>;

        SparseArray<T> *container_;
        uint32_t        index_;

        SparseArrayIterator(SparseArray<T> *container, uint32_t index) : container_(container), index_(index) {}
    public:
        T &operator*() {
            return container_->at(index_);
        }
        T &operator->() {
            return &container_->at(index_);
        }
        SparseArrayIterator &operator++() {
            index_ = container_->NextOccupied(index_);
            return *this;
        }
        SparseArrayIterator operator++(int) {
            SparseArrayIterator tmp(*this);
            ++(*this);
            return tmp;
        }

        uint32_t index() const {
            return index_;
        }

        bool operator< (const SparseArrayIterator &rhs) {
            return index_ < rhs.index_;
        }
        bool operator<=(const SparseArrayIterator &rhs) {
            return index_ <= rhs.index_;
        }
        bool operator> (const SparseArrayIterator &rhs) {
            return index_ > rhs.index_;
        }
        bool operator>=(const SparseArrayIterator &rhs) {
            return index_ >= rhs.index_;
        }
        bool operator==(const SparseArrayIterator &rhs) {
            return index_ == rhs.index_;
        }
        bool operator!=(const SparseArrayIterator &rhs) {
            return index_ != rhs.index_;
        }
    };

    using iterator = SparseArrayIterator;

    iterator begin() {
        for (uint32_t i = 0; i < capacity_; i++) {
            if (ctrl_[i / 8] & (1 << (i % 8))) {
                return iterator(this, i);
            }
        }
        return end();
    }

    iterator end() {
        return iterator(this, capacity_);
    }
private:
    uint32_t NextOccupied(uint32_t index) const {
        assert((ctrl_[index / 8] & (1 << (index % 8))) && "Invalid index!");
        for (uint32_t i = index + 1; i < capacity_; i++) {
            if (ctrl_[i / 8] & (1 << (i % 8))) {
                return i;
            }
        }
        for (uint32_t i = 0; i < index; i++) {
            if (ctrl_[i / 8] & (1 << (i % 8))) {
                return i;
            }
        }
        return capacity_;
    }
};
}
