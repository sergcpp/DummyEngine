#pragma once

#include "../Graph/GraphBuilder.h"
#include "../Renderer_DrawList.h"

class RpUpdateBuffers : public RenderPassBase {
    // temp data (valid only between Setup and Execute calls)
    DynArrayConstRef<SkinTransform> skin_transforms_;
    Ren::WeakBufferRef skin_transforms_stage_buf_;
    DynArrayConstRef<ShapeKeyData> shape_keys_;
    Ren::WeakBufferRef shape_keys_stage_buf_;
    DynArrayConstRef<InstanceData> instances_;
    Ren::WeakBufferRef instances_stage_buf_;
    DynArrayConstRef<CellData> cells_;
    Ren::WeakBufferRef cells_stage_buf_;
    DynArrayConstRef<LightSourceItem> light_sources_;
    Ren::WeakBufferRef lights_stage_buf_;
    DynArrayConstRef<DecalItem> decals_;
    Ren::WeakBufferRef decals_stage_buf_;
    DynArrayConstRef<ItemData> items_;
    Ren::WeakBufferRef items_stage_buf_;
    DynArrayConstRef<ShadowMapRegion> shadow_regions_;
    DynArrayConstRef<ProbeItem> probes_;
    DynArrayConstRef<EllipsItem> ellipsoids_;
    uint32_t render_flags_ = 0;

    Ren::WeakBufferRef shared_data_stage_buf_;

    const EnvironmentWeak *env_ = nullptr;

    const Ren::Camera *draw_cam_ = nullptr;
    const ViewState *view_state_ = nullptr;

    RpResource skin_transforms_buf_;
    RpResource shape_keys_buf_;
    RpResource instances_buf_;
    RpResource cells_buf_;
    RpResource lights_buf_;
    RpResource decals_buf_;
    RpResource items_buf_;
    RpResource shared_data_buf_;
    RpResource atomic_cnt_buf_;

  public:
    void Setup(RpBuilder &builder, const DrawList &list, const ViewState *view_state, const char skin_transforms_buf[],
               const char shape_keys_buf[], const char instances_buf[], const char cells_buf[], const char lights_buf[],
               const char decals_buf[], const char items_buf[], const char shared_data_buf[], const char atomic_counter_buf[]);
    void Execute(RpBuilder &builder) override;

    const char *name() const override { return "UPDATE BUFFERS"; }
};