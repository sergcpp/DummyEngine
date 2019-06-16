#pragma once

#include "HashMap32.h"
#include "SparseArray.h"

namespace Ren {
template<typename T, template<typename val_t> class container = default_container>
class StorageRef;

template<typename T, template<typename val_t> class container = default_container>
class Storage : public SparseArray<T, container> {
    HashMap32<const char *, size_t> items_by_name_;
public:
    template<class... Args>
    StorageRef<T, container> Add(Args &&... args) {
        size_t index = SparseArray<T, container>::Add(args...);

        bool res = items_by_name_.Insert(Get(index)->name(), index);
        assert(res);

        return { this, index };
    }

    void Remove(size_t i) {
        const char *name = this->Get(i)->name();

        bool res = items_by_name_.Erase(name);
        assert(res);

        SparseArray<T, container>::Remove(i);
    }

    StorageRef<T, container> FindByName(const char *name) {
        size_t *p_index = items_by_name_.Find(name);
        if (p_index) {
            return { this, *p_index };
        } else {
            return { nullptr, 0 };
        }
    }
};

class RefCounter {
protected:
    template<class T, template<typename val_t> class container> friend class StorageRef;

    void add_ref() {
        ++counter_;
    }
    bool release() {
        return --counter_ == 0;
    }

    RefCounter() : counter_(0) {}
    RefCounter(const RefCounter&) : counter_(0) {}
    RefCounter &operator=(const RefCounter&) {
        return *this;
    }
    //~RefCounter() {}

    //RefCounter(const RefCounter &) = delete;
    RefCounter(RefCounter &&rhs) : counter_(rhs.counter_) {
        rhs.counter_ = 0;
    }
    //RefCounter &operator=(const RefCounter &) = delete;
    RefCounter &operator=(RefCounter &&rhs) {
        counter_ = rhs.counter_;
        rhs.counter_ = 0;
        return *this;
    }
private:
    mutable unsigned counter_;
};

template <class T, template<typename val_t> class container>
class StorageRef {
    Storage<T, container> *storage_;
    size_t index_;
public:
    StorageRef() : storage_(nullptr), index_(0) {}
    StorageRef(Storage<T, container> *storage, size_t index) : storage_(storage), index_(index) {
        if (storage_) {
            T *p = storage_->Get(index_);
            p->add_ref();
        }
    }
    ~StorageRef() {
        Release();
    }

    StorageRef(const StorageRef &rhs) {
        storage_ = rhs.storage_;
        index_ = rhs.index_;

        if (storage_) {
            T *p = storage_->Get(index_);
            p->add_ref();
        }
    }

    StorageRef(StorageRef &&rhs) {
        storage_ = rhs.storage_;
        rhs.storage_ = nullptr;
        index_ = rhs.index_;
        rhs.index_ = 0;
    }

    StorageRef &operator=(const StorageRef &rhs) {
        Release();

        storage_ = rhs.storage_;
        index_ = rhs.index_;

        if (storage_) {
            T *p = storage_->Get(index_);
            p->add_ref();
        }

        return *this;
    }

    StorageRef &operator=(StorageRef &&rhs) {
        Release();

        storage_ = rhs.storage_;
        rhs.storage_ = nullptr;
        index_ = rhs.index_;
        rhs.index_ = 0;

        return *this;
    }

    T *operator->() {
        assert(storage_);
        return storage_->Get(index_);
    }

    const T *operator->() const {
        assert(storage_);
        return storage_->Get(index_);
    }

    T &operator*() {
        assert(storage_);
        return *storage_->Get(index_);
    }

    const T &operator*() const {
        assert(storage_);
        return *storage_->Get(index_);
    }

    T *get() {
        assert(storage_);
        return storage_->Get(index_);
    }

    const T *get() const {
        assert(storage_);
        return storage_->Get(index_);
    }

    operator bool() const {
        return storage_ != nullptr;
    }

    size_t index() const {
        return index_;
    }

    void Release() {
        if (storage_) {
            T *p = storage_->Get(index_);
            if (p->release()) {
                storage_->Remove(index_);
            }
            storage_ = nullptr;
            index_ = 0;
        }
    }
};
}
