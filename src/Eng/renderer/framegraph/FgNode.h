#pragma once

#include <memory>

#include <Sys/InplaceFunction.h>

#include "FgResource.h"

namespace Eng {
class FgBuilder;
class FgContext;
class FgExecutor {
  public:
    virtual ~FgExecutor() {}

    virtual void Execute(const FgContext &fg) = 0;
};

class FgNode {
  private:
    friend class FgBuilder;

    std::string name_;
    int16_t index_ = -1;
    eFgQueueType queue_;
    FgBuilder &builder_;
    Ren::SmallVector<FgResource, 16> input_;
    Ren::SmallVector<FgResource, 16> output_;

    std::unique_ptr<FgExecutor> executor_;
    FgExecutor *p_executor_ = nullptr;
    Sys::InplaceFunction<void(const FgContext &fg), 48> execute_cb_;

    mutable Ren::SmallVector<int16_t, 16> depends_on_nodes_;
    mutable bool visited_ = false;

  public:
    FgNode(std::string_view name, const int16_t index, eFgQueueType queue, FgBuilder &builder)
        : name_(name), index_(index), queue_(queue), builder_(builder) {}

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
    void set_executor(FgExecutor *exec) { executor_.reset(); p_executor_ = exec; }

    FgBufROHandle AddTransferInput(FgBufROHandle handle);
    FgBufRWHandle AddTransferOutput(std::string_view name, const FgBufDesc &desc);
    FgBufRWHandle AddTransferOutput(FgBufRWHandle handle);

    FgImgROHandle AddTransferImageInput(FgImgROHandle handle);
    FgImgRWHandle AddTransferImageOutput(std::string_view name, const FgImgDesc &desc);
    FgImgRWHandle AddTransferImageOutput(FgImgRWHandle handle);

    FgBufROHandle AddStorageReadonlyInput(FgBufROHandle handle, Ren::Bitmask<Ren::eStage> stages);
    FgBufRWHandle AddStorageOutput(std::string_view name, const FgBufDesc &desc, Ren::Bitmask<Ren::eStage> stages);
    FgBufRWHandle AddStorageOutput(FgBufRWHandle handle, Ren::Bitmask<Ren::eStage> stages);

    FgImgRWHandle AddStorageImageOutput(std::string_view name, const FgImgDesc &desc, Ren::Bitmask<Ren::eStage> stages);
    FgImgRWHandle AddStorageImageOutput(FgImgRWHandle handle, Ren::Bitmask<Ren::eStage> stages);

    FgImgRWHandle AddClearImageOutput(std::string_view name, const FgImgDesc &desc) {
        return AddTransferImageOutput(name, desc);
    }
    FgImgRWHandle AddClearImageOutput(FgImgRWHandle handle) { return AddTransferImageOutput(handle); }

    FgImgRWHandle AddColorOutput(std::string_view name, const FgImgDesc &desc);
    FgImgRWHandle AddColorOutput(FgImgRWHandle handle);
    FgImgRWHandle AddDepthOutput(std::string_view name, const FgImgDesc &desc);
    FgImgRWHandle AddDepthOutput(FgImgRWHandle handle);

    // TODO: try to get rid of this
    FgBufROHandle ReplaceTransferInput(int slot_index, FgBufROHandle handle);
    FgImgRWHandle ReplaceColorOutput(int slot_index, FgImgRWHandle handle);

    FgBufROHandle AddUniformBufferInput(FgBufROHandle handle, Ren::Bitmask<Ren::eStage> stages);

    FgImgROHandle AddTextureInput(FgImgROHandle handle, Ren::Bitmask<Ren::eStage> stages);

    FgImgROHandle AddHistoryTextureInput(FgImgROHandle handle, Ren::Bitmask<Ren::eStage> stages);
    FgImgROHandle AddHistoryTextureInput(std::string_view name, Ren::Bitmask<Ren::eStage> stages);

    FgImgROHandle AddCustomTextureInput(FgImgROHandle handle, Ren::eResState desired_state,
                                        Ren::Bitmask<Ren::eStage> stages);

    FgBufROHandle AddVertexBufferInput(FgBufROHandle handle);
    FgBufROHandle AddIndexBufferInput(FgBufROHandle handle);
    FgBufROHandle AddIndirectBufferInput(FgBufROHandle handle);

    FgBufROHandle AddASBuildReadonlyInput(FgBufROHandle handle);
    FgBufRWHandle AddASBuildOutput(FgBufRWHandle handle);
    FgBufRWHandle AddASBuildOutput(std::string_view name, const FgBufDesc &desc);

    void Execute(const FgContext &fg) {
        if (p_executor_) {
            p_executor_->Execute(fg);
        } else if (execute_cb_) {
            execute_cb_(fg);
        }
    }

    std::string_view name() const { return name_; }
};
} // namespace Eng