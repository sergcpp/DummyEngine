#pragma once

#include <Sys/InplaceFunction.h>

#include "GraphBuilder.h"
#include "Resource.h"

// class RpBuilder;

class RpExecutor {
  public:
    virtual ~RpExecutor() {}

    virtual void Execute(RpBuilder &builder) = 0;
};

class RpSubpass {
  private:
    friend class RpBuilder;

    int16_t index_ = -1, actual_pass_index_ = -1;
    Ren::String name_;
    RpBuilder &builder_;
    Ren::SmallVector<RpResource, 16> input_;
    Ren::SmallVector<RpResource, 16> output_;
    uint32_t ref_count_ = 0;

    std::unique_ptr<RpExecutor> executor_;
    RpExecutor *p_executor_ = nullptr;
    Sys::InplaceFunction<void(RpBuilder &builder), 24> execute_cb_;

    mutable Ren::SmallVector<int16_t, 16> depends_on_passes_;
    mutable bool visited_ = false;

  public:
    RpSubpass(const int index, const char *name, RpBuilder &builder) : index_(index), name_(name), builder_(builder) {}

    template <typename T, class... Args> T *AllocPassData(Args &&...args) {
        return builder_.AllocPassData<T>(std::forward<Args>(args)...);
    }

    template <typename F> void set_execute_cb(F &&f) { execute_cb_ = f; }

    void set_executor(std::unique_ptr<RpExecutor> &&exec) {
        executor_ = std::move(exec);
        p_executor_ = executor_.get();
    }

    template <class T, class... Args> void make_executor(Args &&...args) {
        executor_ = std::unique_ptr<T>(new T(std::forward<Args>(args)...));
        p_executor_ = executor_.get();
    }

    // Non-owning version
    void set_executor(RpExecutor *exec) { p_executor_ = exec; }

    RpResRef AddTransferInput(const Ren::WeakBufferRef &buf);
    RpResRef AddTransferOutput(const char *name, const RpBufDesc &desc);
    RpResRef AddTransferOutput(const Ren::WeakBufferRef &buf);

    RpResRef AddTransferImageInput(const Ren::WeakTex2DRef &tex);
    RpResRef AddTransferImageInput(RpResRef handle);
    RpResRef AddTransferImageOutput(const char *name, const Ren::Tex2DParams &params);
    RpResRef AddTransferImageOutput(const Ren::WeakTex2DRef &tex);
    RpResRef AddTransferImageOutput(RpResRef handle);

    RpResRef AddStorageReadonlyInput(RpResRef handle, Ren::eStageBits stages);
    RpResRef AddStorageReadonlyInput(const Ren::WeakBufferRef &buf, Ren::eStageBits stages);
    RpResRef AddStorageOutput(const char *name, const RpBufDesc &desc, Ren::eStageBits stages);
    RpResRef AddStorageOutput(RpResRef handle, Ren::eStageBits stages);
    RpResRef AddStorageOutput(const Ren::WeakBufferRef &buf, Ren::eStageBits stages);

    RpResRef AddStorageImageOutput(const char *name, const Ren::Tex2DParams &params, Ren::eStageBits stages);
    RpResRef AddStorageImageOutput(RpResRef handle, Ren::eStageBits stages);
    RpResRef AddStorageImageOutput(const Ren::WeakTex2DRef &tex, Ren::eStageBits stages);

    RpResRef AddColorOutput(const char *name, const Ren::Tex2DParams &params);
    RpResRef AddColorOutput(RpResRef handle);
    RpResRef AddColorOutput(const Ren::WeakTex2DRef &tex);
    RpResRef AddColorOutput(const char *name);
    // RpResRef AddColorOutput(const char *name, const Ren::Tex2DParams &params);
    RpResRef AddDepthOutput(const char *name, const Ren::Tex2DParams &params);
    RpResRef AddDepthOutput(const Ren::WeakTex2DRef &tex);

    // TODO: try to get rid of this
    RpResRef ReplaceColorOutput(int slot_index, const Ren::WeakTex2DRef &tex);

    RpResRef AddUniformBufferInput(RpResRef handle, Ren::eStageBits stages);

    RpResRef AddTextureInput(RpResRef handle, Ren::eStageBits stages);
    RpResRef AddTextureInput(const Ren::WeakTex2DRef &tex, Ren::eStageBits stages);
    RpResRef AddTextureInput(const char *name, Ren::eStageBits stages);

    RpResRef AddHistoryTextureInput(RpResRef handle, Ren::eStageBits stages);

    RpResRef AddCustomTextureInput(RpResRef handle, Ren::eResState desired_state, Ren::eStageBits stages);

    RpResRef AddVertexBufferInput(RpResRef handle);
    RpResRef AddVertexBufferInput(const Ren::WeakBufferRef &buf);
    RpResRef AddIndexBufferInput(RpResRef handle);
    RpResRef AddIndexBufferInput(const Ren::WeakBufferRef &buf);
    RpResRef AddIndirectBufferInput(RpResRef handle);
    RpResRef AddIndirectBufferInput(const Ren::WeakBufferRef &buf);

    RpResRef AddASBuildReadonlyInput(RpResRef handle);
    RpResRef AddASBuildOutput(const Ren::WeakBufferRef &buf);
    RpResRef AddASBuildOutput(const char *name, const RpBufDesc &desc);

    void Execute(RpBuilder &builder) {
        if (p_executor_) {
            p_executor_->Execute(builder);
        } else if (execute_cb_) {
            execute_cb_(builder);
        }
    }

    const char *name() const { return name_.c_str(); }
};