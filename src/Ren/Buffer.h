#pragma once

#include <vector>

#include "Storage.h"
#include "String.h"

namespace Ren {
enum eBufferType { UndefinedBuffer, VertexBuffer, IndexBuffer };

class Buffer : public RefCounter {
    struct Node {
        bool is_free = true;
        int parent = -1;
        int child[2] = { -1, -1 };
        uint32_t offset = 0, size = 0;

        bool has_children() const {
            return child[0] != 0 || child[1] != 0;
        }
    };

    String              name_;
    uint32_t            size_ = 0;
    std::vector<Node>   nodes_;

#if defined(USE_GL_RENDER) || defined(USE_SW_RENDER)
    uint32_t buf_id_ = 0xffffffff;
#endif

    int Alloc_Recursive(int i, uint32_t req_size);
    int Find_Recursive(int i, uint32_t offset) const;
    void SafeErase(int i, int *indices, int num);
    bool Free_Node(int i);
public:
    Buffer() = default;
    explicit Buffer(const char *name, uint32_t initial_size);
    Buffer(const Buffer &rhs) = delete;
    Buffer(Buffer &&rhs) noexcept {
        *this = std::move(rhs);
    }
    ~Buffer();

    Buffer &operator=(const Buffer &rhs) = delete;
    Buffer &operator=(Buffer &&rhs) noexcept;

    const String &name() const { return name_; }
    uint32_t size() const { return size_; }

#if defined(USE_GL_RENDER) || defined(USE_SW_RENDER)
    uint32_t buf_id() const {
        return buf_id_;
    }
#endif

    uint32_t Alloc(uint32_t size, const void *init_data = nullptr);
    bool Free(uint32_t offset);

    void Resize(uint32_t new_size);
};

typedef StorageRef<Buffer> BufferRef;
typedef Storage<Buffer> BufferStorage;
}