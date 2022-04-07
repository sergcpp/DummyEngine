#pragma once

#include <Sys/InplaceFunction.h>

#include "GraphBuilder.h"
#include "Resource.h"

//class RpBuilder;

class RenderPassExecutor {
  public:
    virtual ~RenderPassExecutor() {}

    virtual void Execute(RpBuilder &builder) = 0;
};

class RenderPass {
  private:
    friend class RpBuilder;

    Ren::String name_;
    RpBuilder &builder_;
    Ren::SmallVector<RpResource, 16> input_;
    Ren::SmallVector<RpResource, 16> output_;
    uint32_t ref_count_ = 0;

    std::unique_ptr<RenderPassExecutor> executor_;
    RenderPassExecutor *p_executor_ = nullptr;
    Sys::InplaceFunction<void(RpBuilder &builder), 24> execute_cb;

  public:
    RenderPass(const char *name, RpBuilder &builder) : name_(name), builder_(builder) {}

    template <typename T, class... Args> T *AllocPassData(Args &&...args) {
        return builder_.AllocPassData<T>(std::forward<Args>(args)...);
    }

    template <typename F> void set_execute_cb(F &&f) { execute_cb = f; }

    void set_executor(std::unique_ptr<RenderPassExecutor> &&exec) {
        executor_ = std::move(exec);
        p_executor_ = executor_.get();
    }

    template <class T, class... Args> void make_executor(Args &&...args) {
        executor_ = std::unique_ptr<T>(new T(std::forward<Args>(args)...));
        p_executor_ = executor_.get();
    }

    // Non-owning version
    void set_executor(RenderPassExecutor *exec) { p_executor_ = exec; }

    RpResource AddTransferInput(const Ren::WeakBufferRef &buf);
    RpResource AddTransferOutput(const char *name, const RpBufDesc &desc);
    RpResource AddTransferOutput(const Ren::WeakBufferRef &buf);
    
    RpResource AddTransferImageInput(const Ren::WeakTex2DRef &tex);
    RpResource AddTransferImageInput(RpResource handle);
    RpResource AddTransferImageOutput(const char *name, const Ren::Tex2DParams &params);
    RpResource AddTransferImageOutput(const Ren::WeakTex2DRef &tex);
    RpResource AddTransferImageOutput(RpResource handle);

    RpResource AddStorageReadonlyInput(RpResource handle, Ren::eStageBits stages);
    RpResource AddStorageReadonlyInput(const Ren::WeakBufferRef &buf, Ren::eStageBits stages);
    RpResource AddStorageOutput(const char *name, const RpBufDesc &desc, Ren::eStageBits stages);
    RpResource AddStorageOutput(RpResource handle, Ren::eStageBits stages);
    RpResource AddStorageOutput(const Ren::WeakBufferRef &buf, Ren::eStageBits stages);

    RpResource AddStorageImageOutput(const char *name, const Ren::Tex2DParams &params, Ren::eStageBits stages);
    RpResource AddStorageImageOutput(RpResource handle, Ren::eStageBits stages);
    RpResource AddStorageImageOutput(const Ren::WeakTex2DRef &tex, Ren::eStageBits stages);

    RpResource AddColorOutput(const char *name, const Ren::Tex2DParams &params);
    RpResource AddColorOutput(RpResource handle);
    RpResource AddColorOutput(const Ren::WeakTex2DRef &tex);
    RpResource AddColorOutput(const char *name);
    // RpResource AddColorOutput(const char *name, const Ren::Tex2DParams &params);
    RpResource AddDepthOutput(const char *name, const Ren::Tex2DParams &params);

    RpResource AddUniformBufferInput(RpResource handle, Ren::eStageBits stages);

    RpResource AddTextureInput(RpResource handle, Ren::eStageBits stages);
    RpResource AddTextureInput(const Ren::WeakTex2DRef &tex, Ren::eStageBits stages);
    RpResource AddTextureInput(const char *name, Ren::eStageBits stages);

    RpResource AddCustomTextureInput(RpResource handle, Ren::eResState desired_state, Ren::eStageBits stages);

    RpResource AddVertexBufferInput(RpResource handle);
    RpResource AddVertexBufferInput(const Ren::WeakBufferRef &buf);
    RpResource AddIndexBufferInput(RpResource handle);
    RpResource AddIndexBufferInput(const Ren::WeakBufferRef &buf);
    RpResource AddIndirectBufferInput(RpResource handle);
    RpResource AddIndirectBufferInput(const Ren::WeakBufferRef &buf);

    RpResource AddASBuildReadonlyInput(RpResource handle);
    RpResource AddASBuildOutput(const Ren::WeakBufferRef &buf);
    RpResource AddASBuildOutput(const char *name, const RpBufDesc &desc);

    void Execute(RpBuilder &builder) {
        if (p_executor_) {
            p_executor_->Execute(builder);
        } else if (execute_cb) {
            execute_cb(builder);
        }
    }

    const char *name() const { return name_.c_str(); }
};