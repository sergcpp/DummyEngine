#pragma once

#include <cstdint>

#include <stdexcept>

template<typename T>
class ObjectPool {
    struct Node {
        T *memory;
        size_t capacity;
        Node *next;

        explicit Node(size_t cap) : capacity(cap), next(nullptr) {
            memory = (T *)::operator new(capacity * sizeof(T));
        }
        ~Node() {
            ::operator delete(memory);
        }
    };

    T *memory_;
    T *first_deleted_;
    size_t node_size_;
    size_t node_capacity_;
    Node first_node_;
    Node *last_node_;

    void AllocateNewNode() {
        size_t size = node_size_;
        size *= 2;

        Node *new_node = new Node(size);
        last_node_->next = new_node;
        last_node_ = new_node;
        memory_ = new_node->memory;
        node_size_ = 0;
        node_capacity_ = size;
    }

public:
    static_assert(sizeof(T) > sizeof(void *), "Wrong object size!");

    explicit ObjectPool(size_t initial_capacity)
        : first_deleted_(nullptr),
          first_node_(initial_capacity),
          node_size_(0),
          node_capacity_(initial_capacity) {
        memory_ = first_node_.memory;
        last_node_ = &first_node_;
    }
    ~ObjectPool() {
        Node *node = first_node_.next;
        while (node) {
            Node *next_node = node->next;
            delete node;
            node = next_node;
        }
    }

    template<class... Args>
    T *New(Args &&... args) {
        if (first_deleted_) {
            T *result = first_deleted_;
            first_deleted_ = *((T **)first_deleted_);
            new(result) T(args...);
            return result;
        }

        if (node_size_ >= node_capacity_) {
            AllocateNewNode();
        }

        T *result = &memory_[node_size_];
        new(result) T(args...);
        node_size_++;
        return result;
    }

    void Delete(T *content) {
        content->~T();
        Free(content);
    }

    ////////

    T *Alloc() {
        if (first_deleted_) {
            T *result = first_deleted_;
            first_deleted_ = *((T **)first_deleted_);
            return result;
        }

        if (node_size_ >= node_capacity_) {
            AllocateNewNode();
        }

        T *result = &memory_[node_size_];
        node_size_++;
        return result;
    }

    void Free(T *content) {
        *((T **)content) = first_deleted_;
        first_deleted_ = content;
    }
};

