#pragma once

#include <Sys/InplaceFunction.h>

#include "FgBuilder.h"
#include "FgResource.h"

namespace Eng {
class FgExecutor {
  public:
    virtual ~FgExecutor() {}

    virtual void Execute(FgBuilder &builder) = 0;
};

class FgNode {
  private:
    friend class FgBuilder;

    int16_t index_ = -1;
    std::string name_;
    FgBuilder &builder_;
    Ren::SmallVector<FgResource, 16> input_;
    Ren::SmallVector<FgResource, 16> output_;

    std::unique_ptr<FgExecutor> executor_;
    FgExecutor *p_executor_ = nullptr;
    Sys::InplaceFunction<void(FgBuilder &builder), 32> execute_cb_;

    mutable Ren::SmallVector<int16_t, 16> depends_on_nodes_;
    mutable bool visited_ = false;

  public:
    FgNode(const int16_t index, std::string_view name, FgBuilder &builder)
        : index_(index), name_(name), builder_(builder) {}

    template <typename T, class... Args> T *AllocNodeData(Args &&...args) {
        return builder_.AllocNodeData<T>(std::forward<Args>(args)...);
    }

    template <typename F> void set_execute_cb(F &&f) { execute_cb_ = f; }

    void set_executor(std::unique_ptr<FgExecutor> &&exec) {
        executor_ = std::move(exec);
        p_executor_ = executor_.get();
    }

    template <class T, class... Args> void make_executor(Args &&...args) {
        executor_ = std::unique_ptr<T>(new T(std::forward<Args>(args)...));
        p_executor_ = executor_.get();
    }

    Ren::Span<const FgResource> input() const { return input_; }
    Ren::Span<const FgResource> output() const { return output_; }

    FgResource *FindUsageOf(eFgResType type, uint16_t index);

    // Non-owning version
    void set_executor(FgExecutor *exec) { p_executor_ = exec; }

    FgResRef AddTransferInput(const Ren::WeakBufRef &buf);
    FgResRef AddTransferInput(FgResRef handle);
    FgResRef AddTransferOutput(std::string_view name, const FgBufDesc &desc);
    FgResRef AddTransferOutput(const Ren::WeakBufRef &buf);
    FgResRef AddTransferOutput(FgResRef handle);

    FgResRef AddTransferImageInput(const Ren::WeakTexRef &tex);
    FgResRef AddTransferImageInput(FgResRef handle);
    FgResRef AddTransferImageOutput(std::string_view name, const Ren::TexParams &params);
    FgResRef AddTransferImageOutput(const Ren::WeakTexRef &tex);
    FgResRef AddTransferImageOutput(FgResRef handle);

    FgResRef AddStorageReadonlyInput(FgResRef handle, Ren::eStageBits stages);
    FgResRef AddStorageReadonlyInput(const Ren::WeakBufRef &buf, Ren::eStageBits stages);
    FgResRef AddStorageReadonlyInput(const Ren::WeakBufRef &buf, const Ren::WeakTexBufRef &tbo, Ren::eStageBits stages);
    FgResRef AddStorageOutput(std::string_view name, const FgBufDesc &desc, Ren::eStageBits stages);
    FgResRef AddStorageOutput(FgResRef handle, Ren::eStageBits stages);
    FgResRef AddStorageOutput(const Ren::WeakBufRef &buf, Ren::eStageBits stages);

    FgResRef AddStorageImageOutput(std::string_view name, const Ren::TexParams &params, Ren::eStageBits stages);
    FgResRef AddStorageImageOutput(FgResRef handle, Ren::eStageBits stages);
    FgResRef AddStorageImageOutput(const Ren::WeakTexRef &tex, Ren::eStageBits stages);
    FgResRef AddStorageImageOutput(const Ren::Texture2DArray *tex, Ren::eStageBits stages);

    FgResRef AddColorOutput(std::string_view name, const Ren::TexParams &params);
    FgResRef AddColorOutput(FgResRef handle);
    FgResRef AddColorOutput(const Ren::WeakTexRef &tex);
    FgResRef AddColorOutput(std::string_view name);
    FgResRef AddDepthOutput(std::string_view name, const Ren::TexParams &params);
    FgResRef AddDepthOutput(FgResRef handle);
    FgResRef AddDepthOutput(const Ren::WeakTexRef &tex);

    // TODO: try to get rid of this
    FgResRef ReplaceTransferInput(int slot_index, const Ren::WeakBufRef &buf);
    FgResRef ReplaceColorOutput(int slot_index, const Ren::WeakTexRef &tex);

    FgResRef AddUniformBufferInput(FgResRef handle, Ren::eStageBits stages);

    FgResRef AddTextureInput(FgResRef handle, Ren::eStageBits stages);
    FgResRef AddTextureInput(const Ren::WeakTexRef &tex, Ren::eStageBits stages);
    FgResRef AddTextureInput(std::string_view name, Ren::eStageBits stages);
    FgResRef AddTextureInput(const Ren::Texture2DArray *tex, Ren::eStageBits stages);

    FgResRef AddHistoryTextureInput(FgResRef handle, Ren::eStageBits stages);
    FgResRef AddHistoryTextureInput(std::string_view name, Ren::eStageBits stages);

    FgResRef AddCustomTextureInput(FgResRef handle, Ren::eResState desired_state, Ren::eStageBits stages);

    FgResRef AddVertexBufferInput(FgResRef handle);
    FgResRef AddVertexBufferInput(const Ren::WeakBufRef &buf);
    FgResRef AddIndexBufferInput(FgResRef handle);
    FgResRef AddIndexBufferInput(const Ren::WeakBufRef &buf);
    FgResRef AddIndirectBufferInput(FgResRef handle);
    FgResRef AddIndirectBufferInput(const Ren::WeakBufRef &buf);

    FgResRef AddASBuildReadonlyInput(FgResRef handle);
    FgResRef AddASBuildOutput(const Ren::WeakBufRef &buf);
    FgResRef AddASBuildOutput(std::string_view name, const FgBufDesc &desc);

    void Execute(FgBuilder &builder) {
        if (p_executor_) {
            p_executor_->Execute(builder);
        } else if (execute_cb_) {
            execute_cb_(builder);
        }
    }

    std::string_view name() const { return name_; }
};
} // namespace Eng