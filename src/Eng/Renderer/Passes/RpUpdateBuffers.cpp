#include "RpUpdateBuffers.h"

#include "../Renderer_Structs.h"

void RpUpdateBuffers::Setup(RpBuilder &builder, const DrawList &list,
                            const ViewState *view_state, int orphan_index, void **fences,
                            const char skin_transforms_buf[], const char shape_keys_buf[],
                            const char instances_buf[], const char cells_buf[],
                            const char lights_buf[], const char decals_buf[],
                            const char items_buf[], const char shared_data_buf[]) {
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
    shape_keys_ = list.shape_keys_data;
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

        skin_transforms_buf_ = builder.WriteBuffer(skin_transforms_buf, desc, *this);
    }
    { // create shape keys buffer
        RpBufDesc desc;
        desc.type = Ren::eBufferType::Texture;
        desc.size = FrameSyncWindow * ShapeKeysBufChunkSize;
        desc.access = Ren::eBufferAccessType::Draw;
        desc.freq = Ren::eBufferAccessFreq::Dynamic;

        shape_keys_buf_ = builder.WriteBuffer(shape_keys_buf, desc, *this);
    }
    { // create instances buffer
        RpBufDesc desc;
        desc.type = Ren::eBufferType::Texture;
        desc.size = FrameSyncWindow * InstanceDataBufChunkSize;
        desc.access = Ren::eBufferAccessType::Draw;
        desc.freq = Ren::eBufferAccessFreq::Dynamic;

        instances_buf_ = builder.WriteBuffer(instances_buf, desc, *this);
    }
    { // create cells buffer
        RpBufDesc desc;
        desc.type = Ren::eBufferType::Texture;
        desc.size = FrameSyncWindow * CellsBufChunkSize;
        desc.access = Ren::eBufferAccessType::Draw;
        desc.freq = Ren::eBufferAccessFreq::Dynamic;

        cells_buf_ = builder.WriteBuffer(cells_buf, desc, *this);
    }
    { // create lights buffer
        RpBufDesc desc;
        desc.type = Ren::eBufferType::Texture;
        desc.size = FrameSyncWindow * LightsBufChunkSize;
        desc.access = Ren::eBufferAccessType::Draw;
        desc.freq = Ren::eBufferAccessFreq::Dynamic;

        lights_buf_ = builder.WriteBuffer(lights_buf, desc, *this);
    }
    { // create decals buffer
        RpBufDesc desc;
        desc.type = Ren::eBufferType::Texture;
        desc.size = FrameSyncWindow * DecalsBufChunkSize;
        desc.access = Ren::eBufferAccessType::Draw;
        desc.freq = Ren::eBufferAccessFreq::Dynamic;

        decals_buf_ = builder.WriteBuffer(decals_buf, desc, *this);
    }
    { // create items buffer
        RpBufDesc desc;
        desc.type = Ren::eBufferType::Texture;
        desc.size = FrameSyncWindow * ItemsBufChunkSize;
        desc.access = Ren::eBufferAccessType::Draw;
        desc.freq = Ren::eBufferAccessFreq::Dynamic;

        items_buf_ = builder.WriteBuffer(items_buf, desc, *this);
    }
    { // create uniform buffer
        RpBufDesc desc;
        desc.type = Ren::eBufferType::Uniform;
        desc.size = FrameSyncWindow * SharedDataBlockSize;
        desc.access = Ren::eBufferAccessType::Draw;
        desc.freq = Ren::eBufferAccessFreq::Dynamic;

        shared_data_buf_ = builder.WriteBuffer(shared_data_buf, desc, *this);
    }
}
