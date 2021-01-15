#include "RpUpdateBuffers.h"

#include "../Renderer_Names.h"
#include "../Renderer_Structs.h"

void RpUpdateBuffers::Setup(RpBuilder &builder, const DrawList &list,
                            const ViewState *view_state, int orphan_index,
                            void **fences) {
    assert(list.instances.count < REN_MAX_INSTANCES_TOTAL);
    assert(list.skin_transforms.count < REN_MAX_SKIN_XFORMS_TOTAL);
    assert(list.skin_regions.count < REN_MAX_SKIN_REGIONS_TOTAL);
    assert(list.skin_vertices_count < REN_MAX_SKIN_VERTICES_TOTAL);
    assert(list.light_sources.count < REN_MAX_LIGHTS_TOTAL);
    assert(list.decals.count < REN_MAX_DECALS_TOTAL);
    assert(list.probes.count < REN_MAX_PROBES_TOTAL);
    assert(list.ellipsoids.count < REN_MAX_ELLIPSES_TOTAL);
    assert(list.items.count < REN_MAX_ITEMS_TOTAL);

    orphan_index_ = orphan_index;

    fences_ = fences;
    skin_transforms_ = list.skin_transforms;
    shape_keys_data_ = list.shape_keys_data;
    instances_ = list.instances;
    cells_ = list.cells;
    light_sources_ = list.light_sources;
    decals_ = list.decals;
    items_ = list.items;
    shadow_regions_ = list.shadow_regions;
    probes_ = list.probes;
    ellipsoids_ = list.ellipsoids;
    render_flags_ = list.render_flags;

    env_ = &list.env;

    draw_cam_ = &list.draw_cam;
    view_state_ = view_state;

    { // create skin transforms buffer
        RpBufDesc desc;
        desc.type = Ren::eBufferType::Texture;
        desc.size = FrameSyncWindow * SkinTransformsBufChunkSize;
        desc.access = Ren::eBufferAccessType::Draw;
        desc.freq = Ren::eBufferAccessFreq::Dynamic;

        input_[0] = builder.CreateBuffer(SKIN_TRANSFORMS_BUF, desc);
    }
    { // create shape keys buffer
        RpBufDesc desc;
        desc.type = Ren::eBufferType::Texture;
        desc.size = FrameSyncWindow * ShapeKeysBufChunkSize;
        desc.access = Ren::eBufferAccessType::Draw;
        desc.freq = Ren::eBufferAccessFreq::Dynamic;

        input_[1] = builder.CreateBuffer(SHAPE_KEYS_BUF, desc);
    }
    { // create instances buffer
        RpBufDesc desc;
        desc.type = Ren::eBufferType::Texture;
        desc.size = FrameSyncWindow * InstanceDataBufChunkSize;
        desc.access = Ren::eBufferAccessType::Draw;
        desc.freq = Ren::eBufferAccessFreq::Dynamic;

        input_[2] = builder.CreateBuffer(INSTANCES_BUF, desc);
    }
    { // create cells buffer
        RpBufDesc desc;
        desc.type = Ren::eBufferType::Texture;
        desc.size = FrameSyncWindow * CellsBufChunkSize;
        desc.access = Ren::eBufferAccessType::Draw;
        desc.freq = Ren::eBufferAccessFreq::Dynamic;

        input_[3] = builder.CreateBuffer(CELLS_BUF, desc);
    }
    { // create lights buffer
        RpBufDesc desc;
        desc.type = Ren::eBufferType::Texture;
        desc.size = FrameSyncWindow * LightsBufChunkSize;
        desc.access = Ren::eBufferAccessType::Draw;
        desc.freq = Ren::eBufferAccessFreq::Dynamic;

        input_[4] = builder.CreateBuffer(LIGHTS_BUF, desc);
    }
    { // create decals buffer
        RpBufDesc desc;
        desc.type = Ren::eBufferType::Texture;
        desc.size = FrameSyncWindow * DecalsBufChunkSize;
        desc.access = Ren::eBufferAccessType::Draw;
        desc.freq = Ren::eBufferAccessFreq::Dynamic;

        input_[5] = builder.CreateBuffer(DECALS_BUF, desc);
    }

    { // create items buffer
        RpBufDesc desc;
        desc.type = Ren::eBufferType::Texture;
        desc.size = FrameSyncWindow * ItemsBufChunkSize;
        desc.access = Ren::eBufferAccessType::Draw;
        desc.freq = Ren::eBufferAccessFreq::Dynamic;

        input_[6] = builder.CreateBuffer(ITEMS_BUF, desc);
    }
    { // create uniform buffer
        RpBufDesc desc;
        desc.type = Ren::eBufferType::Uniform;
        desc.size = FrameSyncWindow * SharedDataBlockSize;
        desc.access = Ren::eBufferAccessType::Draw;
        desc.freq = Ren::eBufferAccessFreq::Dynamic;

        input_[7] = builder.CreateBuffer(SHARED_DATA_BUF, desc);
    }
    input_count_ = 8;

    output_[0] = builder.WriteBuffer(input_[0], *this);
    output_[1] = builder.WriteBuffer(input_[1], *this);
    output_[2] = builder.WriteBuffer(input_[2], *this);
    output_[3] = builder.WriteBuffer(input_[3], *this);
    output_[4] = builder.WriteBuffer(input_[4], *this);
    output_[5] = builder.WriteBuffer(input_[5], *this);
    output_[6] = builder.WriteBuffer(input_[6], *this);
    output_[7] = builder.WriteBuffer(input_[7], *this);
    output_count_ = 8;
}
