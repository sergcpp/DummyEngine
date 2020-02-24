#include "RpUpdateBuffers.h"

#include "../Renderer_Structs.h"

void RpUpdateBuffers::Setup(
    Graph::RpBuilder &builder, const DrawList &list, const ViewState *view_state,
    int orphan_index, void **fences, Graph::ResourceHandle in_skin_transforms_buf,
    Graph::ResourceHandle in_shape_keys_buf, Graph::ResourceHandle in_instances_buf,
    Graph::ResourceHandle in_cells_buf, Graph::ResourceHandle in_lights_buf,
    Graph::ResourceHandle in_decals_buf, Graph::ResourceHandle in_items_buf, Graph::ResourceHandle in_shared_data_buf) {
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

    input_[0] = in_skin_transforms_buf;
    input_[1] = in_shape_keys_buf;
    input_[2] = in_instances_buf;
    input_[3] = in_cells_buf;
    input_[4] = in_lights_buf;
    input_[5] = in_decals_buf;
    input_[6] = in_items_buf;
    input_[7] = in_shared_data_buf;
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
