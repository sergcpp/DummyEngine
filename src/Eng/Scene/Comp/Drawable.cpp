#include "Drawable.h"

#include <Ren/Mesh.h>

void Drawable::Read(const JsObject &js_in, Drawable &dr) {
    dr.flags = 0;
    dr.vis_mask = 0xffffffff;

    if (js_in.Has("visible_to_shadow")) {
        auto v = (JsLiteral)js_in.at("visible_to_shadow");
        if (v.val == JS_FALSE) {
            dr.vis_mask &= ~VisShadow;
        }
    }

    if (js_in.Has("visible_to_probes")) {
        auto v = (JsLiteral)js_in.at("visible_to_probes");
        if (v.val == JS_FALSE) {
            dr.vis_mask &= ~VisProbes;
        }
    }

    if (js_in.Has("mesh_file")) {
        const auto &mesh_name = (const JsString &)js_in.at("mesh_file");
        dr.mesh_file = Ren::String{ mesh_name.val.c_str() };
    }

    if (js_in.Has("material_override")) {
        dr.flags |= DrMaterialOverride;
    }
}

void Drawable::Write(const Drawable &dr, JsObject &js_out) {
    if (dr.mesh) {
        // write mesh file name
        const Ren::String &mesh_name = dr.mesh->name();
        if (mesh_name != dr.mesh_file) {
            js_out.Push("mesh_name", JsString{mesh_name.c_str()});
        }
        js_out.Push("mesh_file", JsString{ dr.mesh_file.empty() ? mesh_name.c_str() : dr.mesh_file.c_str() });
    }

    if (dr.flags & DrMaterialOverride) {
        JsArray js_material_override;

        const Ren::Mesh *mesh = dr.mesh.get();
        for (int i = 0; i < Ren::MaxMeshTriGroupsCount; i++) {
            const Ren::TriGroup &grp = mesh->group(i);
            if (grp.offset == -1) break;

            js_material_override.Push(JsString{ grp.mat->name().c_str() });
        }

        js_out.Push("material_override", std::move(js_material_override));
    }

    {   // write visibility
        if (!(dr.vis_mask & VisShadow)) {
            js_out.Push("visible_to_shadow", JsLiteral(JS_FALSE));
        }
        if (!(dr.vis_mask & VisProbes)) {
            js_out.Push("visible_to_probes", JsLiteral(JS_FALSE));
        }
    }
}