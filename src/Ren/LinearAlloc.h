#pragma once

#include <string>

#include "SparseArray.h"

namespace Ren {
class ILog;

class LinearAlloc {
  protected:
    struct Node {
        bool is_free = true;
        int parent = -1;
        int child[2] = {-1, -1};
        uint32_t offset = 0, size = 0;
#ifndef NDEBUG
        char tag[32] = {};
#endif

        bool has_children() const { return child[0] != -1 || child[1] != -1; }
    };

    SparseArray<Node> nodes_;

  public:
    LinearAlloc() = default;
    explicit LinearAlloc(uint32_t initial_size) {
        nodes_.emplace();
        nodes_[0].size = initial_size;
    }

    LinearAlloc(const LinearAlloc &rhs) = delete;
    LinearAlloc(LinearAlloc &&rhs) noexcept = default;

    LinearAlloc &operator=(const LinearAlloc &rhs) = delete;
    LinearAlloc &operator=(LinearAlloc &&rhs) noexcept = default;

    uint32_t size() const { return nodes_[0].size; }
    uint32_t node_off(int i) { return nodes_[i].offset; }

    int Alloc_r(int i, uint32_t req_size, const char *tag);
    int Find_r(int i, uint32_t offset) const;
    bool Free_Node(int i);

    void PrintNode(int i, std::string prefix, bool is_tail, ILog *log) const;

    void Clear();
};
} // namespace Ren