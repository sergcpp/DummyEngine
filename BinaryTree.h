#pragma once

#include <functional>
#include <memory>

namespace Sys {
    template <typename T, typename Comp = std::less<typename T>, typename Alloc = std::allocator>
    class BinaryTree {
        struct Node {
            T val;
            Node *left, *right;

            Node(const T &_val) : val(_val), left(nullptr), right(nullptr) {}
            Node(T &&_val) : val(std::move(_val)), left(nullptr), right(nullptr) {}
        };

        Node *root_;
        size_t size_;
        Comp cmp_;
        typename Alloc::template rebind<Node>::other alloc_;
    public:
        BinaryTree(const Comp &cmp = Comp(), const Alloc &alloc = Alloc())
            : root_(nullptr), size_(0), cmp_(cmp), alloc_(alloc) {}
        ~BinaryTree() {
            if (root_) {
                Node *stack[128];
                size_t stack_size = 0;
                stack[stack_size++] = root_;
                while (stack_size) {
                    Node *cur = stack[--stack_size];
                    if (cur->left) {
                        stack[stack_size++] = cur->left;
                    }
                    if (cur->right) {
                        stack[stack_size++] = cur->right;
                    }

                    alloc_.destroy(cur);
                    alloc_.deallocate(cur, 1);
                }
            }
        }

        void push(const T &val) {
            if (!root_) {
                root_ = alloc_.allocate(1);
                alloc_.construct(root_, val);
            } else {
                Node *cur = root_;
                while (true) {
                    if (cmp_(val, cur->val)) {
                        if (cur->left) {
                            cur = cur->left;
                        } else {
                            cur->left = alloc_.allocate(1);
                            alloc_.construct(cur->left, val);
                            break;
                        }
                    } else {
                        if (cur->right) {
                            cur = cur->right;
                        } else {
                            cur->right = alloc_.allocate(1);
                            alloc_.construct(cur->right, val);
                            break;
                        }
                    }
                }
            }

            size_++;
        }

        template<class... Args>
        void emplace(Args&&... args) {
            Node *new_node = alloc_.allocate(1);
            alloc_.construct(new_node, std::forward<Args>(args)...);

            if (!root_) {
                root_ = new_node;
            } else {
                Node *cur = root_;
                while (true) {
                    if (cmp_(new_node->val, cur->val)) {
                        if (cur->left) {
                            cur = cur->left;
                        } else {
                            cur->left = new_node;
                            break;
                        }
                    } else {
                        if (cur->right) {
                            cur = cur->right;
                        } else {
                            cur->right = new_node;
                            break;
                        }
                    }
                }
            }

            size_++;
        }

        bool empty() {
            return root_ == nullptr;
        }

        size_t size() const {
            return size_;
        }

        void extract_top(T &out) {
            if (empty()) return;

            Node *cur = root_, *par = nullptr;
            while (cur->right) {
                par = cur;
                cur = cur->right;
            }

            if (par) {
                par->right = cur->left;
            } else {
                root_ = cur->left;
            }

            out = cur->val;
            alloc_.destroy(cur);
            alloc_.deallocate(cur, 1);

            size_--;
        }
    };
}