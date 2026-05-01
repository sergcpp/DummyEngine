#include "ResizableBuffer.h"

#include "ApiContext.h"

Ren::ResizableBuffer::~ResizableBuffer() { Release(false); }

bool Ren::ResizableBuffer::Resize(const uint32_t new_size, ILog *log, const bool keep_content,
                                  const bool release_immediately) {
    if (handle_) {
        const auto &[buf_main, buf_cold] = storage_[handle_];
        if (buf_cold.size == new_size) {
            return true;
        }
    }

    const BufferHandle new_handle = storage_.Emplace();

    const auto &[buf_main, buf_cold] = storage_[new_handle];
    const bool res = Buffer_Init(api_, buf_main, buf_cold, name_, type_, new_size, log, size_alignment_);
    if (!res) {
        return false;
    }
    for (const eFormat f : views_) {
        Buffer_AddView(api_, buf_main, buf_cold, f);
    }

    if (handle_ && keep_content) {
        const auto &[old_buf_main, old_buf_cold] = storage_[handle_];

        CommandBuffer cmd_buf = api_.BegSingleTimeCommands();
        Buffer_UpdateSubRegion(api_, buf_main, buf_cold, 0, old_buf_cold.size, old_buf_main, 0, cmd_buf);
        api_.EndSingleTimeCommands(cmd_buf);
    }

    Release(release_immediately);

    handle_ = new_handle;

    return true;
}

void Ren::ResizableBuffer::Release(const bool immediately) {
    if (handle_) {
        const auto &[buf_main, buf_cold] = storage_[handle_];
        if (immediately) {
            Buffer_DestroyImmediately(api_, buf_main, buf_cold);
        } else {
            Buffer_Destroy(api_, buf_main, buf_cold);
        }
        storage_.Erase(handle_);
    }
    handle_ = {};
}

int Ren::ResizableBuffer::AddView(const eFormat format) {
    const int ret = int(views_.size());
    views_.push_back(format);
    if (handle_) {
        const auto &[buf_main, buf_cold] = storage_[handle_];
        const int view_index = Buffer_AddView(api_, buf_main, buf_cold, format);
        assert(view_index == ret);
    }
    return ret;
}

Ren::SubAllocation Ren::ResizableBuffer::AllocSubRegion(uint32_t req_size, uint32_t req_alignment, std::string_view tag,
                                                        ILog *log, const BufferMain *init_buf, CommandBuffer cmd_buf,
                                                        uint32_t init_off) {
    const auto &[buf_main, buf_cold] = storage_[handle_];

    SubAllocation alloc =
        Buffer_AllocSubRegion(api_, buf_main, buf_cold, req_size, req_alignment, tag, log, init_buf, cmd_buf, init_off);
    while (!alloc) {
        const uint64_t grown = uint64_t(buf_cold.size) * 5 / 4 + req_size;
        const uint32_t new_size = uint32_t(((grown + req_alignment - 1) / req_alignment) * req_alignment);
        if (!Buffer_Resize(api_, buf_main, buf_cold, new_size, log)) {
            return {};
        }
        alloc = Buffer_AllocSubRegion(api_, buf_main, buf_cold, req_size, req_alignment, tag, log, init_buf, cmd_buf,
                                      init_off);
    }

    return alloc;
}

void Ren::ResizableBuffer::FreeSubRegion(SubAllocation alloc) {
    const auto &[buf_main, buf_cold] = storage_[handle_];
    Buffer_FreeSubRegion(buf_cold, alloc);
}