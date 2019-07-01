#pragma once

#include <cassert>
#include <string>

#include "String.h"

namespace Ren {
inline uint32_t _lua_hash(void const *v, uint32_t l) {
    uint32_t i, step = (l >> 5) + 1;
    uint32_t h = l + (l >= 4 ? *(uint32_t*)v : 0);
    for (i = l; i >= step; i -= step) {
        h = h ^ ((h << 5) + (h >> 2) + ((unsigned char *)v)[i - 1]);
    }
    return h;
}

inline uint32_t _str_hash(const char *s) {
    const uint32_t A = 54059;
    const uint32_t B = 76963;
    const uint32_t C = 86969;
    const uint32_t FIRSTH = 37;

    uint32_t h = FIRSTH;
    while (*s) {
        h = (h * A) ^ (s[0] * B);
        s++;
    }
    return h;
}

template <typename K>
class Hash {
public:
    uint32_t operator()(const K &k) const {
        return _lua_hash(&k, sizeof(K));
    }
};

template <>
class Hash<std::string> {
public:
    uint32_t operator()(const std::string &s) const {
        return _str_hash(s.c_str());
    }
};

template <>
class Hash<String> {
public:
    uint32_t operator()(const String &s) const {
        return _str_hash(s.c_str());
    }

    uint32_t operator()(const char *s) const {
        return _str_hash(s);
    }
};

template <typename K>
class Equal {
    std::equal_to<K> eq_;
public:
    bool operator()(const K &k1, const K &k2) const {
        return eq_(k1, k2);
    }
};

template <>
class Equal<String> {
public:

    template <typename K2>
    bool operator()(const String &k1, const K2 &k2) const {
        return k1 == k2;
    }
};

template <typename K, typename V, typename HashFunc = Hash<K>, typename KeyEqual = Equal<K>>
class HashMap32 {
    static const uint8_t OccupiedBit = 0b10000000;
    static const uint8_t HashMask = 0b01111111;

    struct Node {
        uint32_t hash;
        K        key;
        V        val;
    };

    uint8_t     *ctrl_;
    Node        *nodes_;
    size_t      capacity_, size_;
    HashFunc    hash_func_;
    KeyEqual    key_equal_;
public:
    HashMap32(const HashFunc &hash_func = HashFunc(), const KeyEqual &key_equal = KeyEqual())
        : ctrl_(nullptr), nodes_(nullptr), capacity_(0), size_(0), hash_func_(hash_func), key_equal_(key_equal) {
    }

    HashMap32(size_t capacity, const HashFunc &hash_func = HashFunc(), const KeyEqual &key_equal = KeyEqual())
        : hash_func_(hash_func), key_equal_(key_equal) {
        // Check if power of 2
        assert((capacity & (capacity - 1)) == 0);

        size_t mem_size = capacity;
        mem_size += (mem_size % alignof(Node));

        size_t node_begin = mem_size;
        mem_size += sizeof(Node) * capacity;

        ctrl_ = new uint8_t[mem_size];
        nodes_ = (Node *)&ctrl_[node_begin];

        memset(ctrl_, 0, capacity);

        capacity_ = capacity;
        size_ = 0;
    }

    HashMap32(const HashMap32 &rhs) = delete;
    HashMap32 &operator=(const HashMap32 &rhs) = delete;

    ~HashMap32() {
        clear();
        delete[] ctrl_;
    }

    size_t size() const { return size_; }
    size_t capacity() const { return capacity_; }

    void clear() {
        for (size_t i = 0; i < capacity_ && size_; i++) {
            if (ctrl_[i] & OccupiedBit) {
                size_--;
                nodes_[i].key.~K();
                nodes_[i].val.~V();
            }
        }

        memset(ctrl_, 0, capacity_);
    }

    V &operator[](const K &key) {
        V *v = Find(key);
        if (!v) {
            v = InsertNoCheck(key);
        }
        return *v;
    }

    bool Insert(const K &key, const V &val) {
        const V *v = Find(key);
        if (v) return false;

        *InsertNoCheck(key) = val;
        return true;
    }

    bool Insert(K &&key, V &&val) {
        uint32_t hash = hash_func_(key);

        const V *v = Find(hash, key);
        if (v) return false;

        InsertInternal(hash, std::forward<K>(key), std::forward<V>(val));
        return true;
    }

    void Set(const K &key, const V &val) {
        V *v = Find(key);
        if (!v) {
            v = InsertNoCheck(key);
        }
        *v = val;
        return v;
    }

    V *InsertNoCheck(const K &key) {
        uint32_t hash = hash_func_(key);
        return InsertInternal(hash, key);
    }

    bool Erase(const K &key) {
        uint32_t hash = hash_func_(key);
        uint8_t ctrl = OccupiedBit | (hash & HashMask);

        uint32_t i = hash & (capacity_ - 1);
        uint32_t end = i;
        while (ctrl_[i]) {
            if (ctrl_[i] == ctrl && nodes_[i].hash == hash && key_equal_(nodes_[i].key, key)) {

                size_--;
                ctrl_[i] = HashMask;
                nodes_[i].key.~K();
                nodes_[i].val.~V();

                return true;
            }

            i = (i + 1) & (capacity_ - 1);
            if (i == end) break;
        }

        return false;
    }

    template <typename K2>
    V *Find(const K2 &key) {
        uint32_t hash = hash_func_(key);
        return Find(hash, key);
    }

    template <typename K2>
    V *Find(uint32_t hash, const K2 &key) {
        if (!capacity_) return nullptr;

        uint8_t ctrl = OccupiedBit | (hash & HashMask);

        uint32_t i = hash & (capacity_ - 1);
        uint32_t end = i;
        while (ctrl_[i]) {
            if (ctrl_[i] == ctrl && nodes_[i].hash == hash && key_equal_(nodes_[i].key, key)) {
                return &nodes_[i].val;
            }

            i = (i + 1) & (capacity_ - 1);
            if (i == end) break;
        }

        return nullptr;
    }

private:
    void CheckRealloc() {
        if ((size_ + 1) > uint32_t(0.8 * capacity_)) {
            uint8_t *old_ctrl = ctrl_;
            Node *old_nodes = nodes_;
            size_t old_capacity = capacity_;

            if (capacity_) {
                capacity_ *= 2;
            } else {
                capacity_ = 8;
            }
            size_ = 0;

            size_t mem_size = capacity_;
            mem_size += (mem_size % alignof(Node));

            size_t node_begin = mem_size;
            mem_size += sizeof(Node) * capacity_;

            ctrl_ = new uint8_t[mem_size];
            nodes_ = (Node *)&ctrl_[node_begin];

            memset(ctrl_, 0, capacity_);

            for (size_t i = 0; i < old_capacity; i++) {
                if (old_ctrl[i] & OccupiedBit) {
                    InsertInternal(old_nodes[i].hash, std::move(old_nodes[i].key), std::move(old_nodes[i].val));
                }
            }

            delete[] old_ctrl;
        }
    }

    V *InsertInternal(uint32_t hash, const K &key) {
        CheckRealloc();

        uint32_t i = hash & (capacity_ - 1);
        while (ctrl_[i] & OccupiedBit) {
            i = (i + 1) & (capacity_ - 1);
        }

        size_++;
        ctrl_[i] = OccupiedBit | (hash & HashMask);
        nodes_[i].hash = hash;
        new (&nodes_[i].key) K(key);
        new (&nodes_[i].val) V;

        return &nodes_[i].val;
    }

    void InsertInternal(uint32_t hash, const K &key, const V &val) {
        CheckRealloc();

        uint32_t i = hash & (capacity_ - 1);
        while (ctrl_[i] & OccupiedBit) {
            i = (i + 1) & (capacity_ - 1);
        }

        size_++;
        ctrl_[i] = OccupiedBit | (hash & HashMask);
        nodes_[i].hash = hash;
        new (&nodes_[i].key) K(key);
        new (&nodes_[i].val) V(val);

        return &nodes_[i].val;
    }

    void InsertInternal(uint32_t hash, K &&key, V &&val) {
        CheckRealloc();

        uint32_t i = hash & (capacity_ - 1);
        while (ctrl_[i] & OccupiedBit) {
            i = (i + 1) & (capacity_ - 1);
        }

        size_++;
        ctrl_[i] = OccupiedBit | (hash & HashMask);
        nodes_[i].hash = hash;
        new (&nodes_[i].key) K(std::forward<K>(key));
        new (&nodes_[i].val) V(std::forward<V>(val));
    }
};
}