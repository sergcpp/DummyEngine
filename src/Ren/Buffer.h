#pragma once

#include <vector>

#include "Storage.h"
#include "String.h"

namespace Ren {
class ILog;

enum class eBufferType : uint8_t { Undefined, VertexAttribs, VertexIndices, Texture, Uniform, _Count };
enum class eBufferAccessType : uint8_t {
    Draw, Read, Copy
};
enum class eBufferAccessFreq : uint8_t {
    Stream, // modified once, used a few times
    Static, // modified once, used many times
    Dynamic // modified often, used many times
};

struct BufHandle {
#if defined(USE_GL_RENDER) || defined(USE_SW_RENDER)
    uint32_t id = 0;
#endif
    uint32_t generation = 0;
};
inline bool operator==(BufHandle lhs, BufHandle rhs) {
    return
#if defined(USE_GL_RENDER) || defined(USE_SW_RENDER)
        lhs.id == rhs.id &&
#endif
        lhs.generation == rhs.generation;
}

class Buffer : public RefCounter {
    struct Node {
        bool is_free = true;
        int parent = -1;
        int child[2] = { -1, -1 };
        uint32_t offset = 0, size = 0;
#ifndef NDEBUG
        char tag[32] = {};
#endif

        bool has_children() const {
            return child[0] != -1 || child[1] != -1;
        }
    };

    BufHandle           handle_;
    String              name_;
    eBufferType         type_ = eBufferType::Undefined;
    eBufferAccessType   access_;
    eBufferAccessFreq   freq_;
    uint32_t            size_ = 0;
    SparseArray<Node>   nodes_;
    
    int Alloc_Recursive(int i, uint32_t req_size, const char *tag);
    int Find_Recursive(int i, uint32_t offset) const;
    bool Free_Node(int i);

    void PrintNode(int i, std::string prefix, bool is_tail, ILog *log);

    static int g_GenCounter;
public:
    Buffer() = default;
    explicit Buffer(const char *name, eBufferType type, eBufferAccessType access,
                    eBufferAccessFreq freq, uint32_t initial_size);
    Buffer(const Buffer &rhs) = delete;
    Buffer(Buffer &&rhs) noexcept {
        (*this) = std::move(rhs);
    }
    ~Buffer();

    Buffer &operator=(const Buffer &rhs) = delete;
    Buffer &operator=(Buffer &&rhs) noexcept;

    const String &name() const { return name_; }
    eBufferType type() const { return type_; }
    eBufferAccessType access() const { return access_; }
    eBufferAccessFreq freq() const { return freq_; }
    uint32_t size() const { return size_; }

    BufHandle handle() const { return handle_; }
#if defined(USE_GL_RENDER) || defined(USE_SW_RENDER)
    uint32_t id() const { return handle_.id; }
#endif
    uint32_t generation() const { return handle_.generation; }

    uint32_t AllocRegion(uint32_t size, const char *tag, const void *init_data = nullptr);
    bool FreeRegion(uint32_t offset);

    void Resize(uint32_t new_size);

    void Print(ILog *log);
};

#if defined(USE_GL_RENDER)
void GLUnbindBufferUnits(int start, int count);
#endif

typedef StrongRef<Buffer> BufferRef;
typedef Storage<Buffer> BufferStorage;
}