#pragma once

#include "../Graph/GraphBuilder.h"

class RpUpdateBuffers : public RenderPassBase {
    int orphan_index_ = 0;

    // temp data (valid only between Setup and Execute calls)
    void **fences_;
    DynArrayConstRef<SkinTransform> skin_transforms_;
    DynArrayConstRef<ShapeKeyData> shape_keys_data_;
    DynArrayConstRef<InstanceData> instances_;
    DynArrayConstRef<CellData> cells_;
    DynArrayConstRef<LightSourceItem> light_sources_;
    DynArrayConstRef<DecalItem> decals_;
    DynArrayConstRef<ItemData> items_;
    DynArrayConstRef<ShadowMapRegion> shadow_regions_;
    DynArrayConstRef<ProbeItem> probes_;
    DynArrayConstRef<EllipsItem> ellipsoids_;
    uint32_t render_flags_ = 0;

    const Environment *env_ = nullptr;

    const Ren::Camera *draw_cam_ = nullptr;
    const ViewState *view_state_ = nullptr;

  public:
    void Setup(RpBuilder &builder, const DrawList &list,
               const ViewState *view_state, int orphan_index, void **fences);
    void Execute(RpBuilder &builder) override;

    const char *name() const override { return "UPDATE BUFFERS"; }
};