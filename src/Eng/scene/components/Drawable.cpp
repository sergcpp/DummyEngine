#include "Drawable.h"

#include <Ren/Mesh.h>
#include <Sys/Json.h>

const Ren::Bitmask<Eng::Drawable::eVisibility> Eng::Drawable::DefaultVisMask =
    Ren::Bitmask<eVisibility>{eVisibility::Camera} | eVisibility::Shadow;

void Eng::Drawable::Read(const Sys::JsObjectP &js_in, Drawable &dr) {
    dr.vis_mask = DefaultVisMask;

    if (const size_t visible_to_camera_ndx = js_in.IndexOf("visible_to_camera"); visible_to_camera_ndx < js_in.Size()) {
        const Sys::JsLiteral v = js_in[visible_to_camera_ndx].second.as_lit();
        if (v.val == Sys::JsLiteralType::True) {
            dr.vis_mask |= eVisibility::Camera;
        } else {
            dr.vis_mask &= ~Ren::Bitmask(eVisibility::Camera);
        }
    }

    if (const size_t visible_to_shadow_ndx = js_in.IndexOf("visible_to_shadow"); visible_to_shadow_ndx < js_in.Size()) {
        const Sys::JsLiteral v = js_in[visible_to_shadow_ndx].second.as_lit();
        if (v.val == Sys::JsLiteralType::True) {
            dr.vis_mask |= eVisibility::Shadow;
        } else {
            dr.vis_mask &= ~Ren::Bitmask(eVisibility::Shadow);
        }
    }

    if (const size_t visible_to_probes_ndx = js_in.IndexOf("visible_to_probes"); visible_to_probes_ndx < js_in.Size()) {
        const Sys::JsLiteral v = js_in[visible_to_probes_ndx].second.as_lit();
        if (v.val == Sys::JsLiteralType::False) {
            dr.vis_mask &= ~Ren::Bitmask(eVisibility::Probes);
        }
    }

    /*if (const size_t ellipsoids_ndx = js_in.IndexOf("ellipsoids"); ellipsoids_ndx < js_in.Size()) {
        const Sys::JsArrayP &js_ellipsoids = js_in[ellipsoids_ndx].second.as_arr();
        for (size_t i = 0; i < js_ellipsoids.elements.size(); i++) {
            const Sys::JsObjectP &js_ellipsoid = js_ellipsoids[i].as_obj();

            const Sys::JsArrayP &js_ellipsoid_offset = js_ellipsoid.at("offset").as_arr();
            dr.ellipsoids[i].offset[0] = float(js_ellipsoid_offset[0].as_num().val);
            dr.ellipsoids[i].offset[1] = float(js_ellipsoid_offset[1].as_num().val);
            dr.ellipsoids[i].offset[2] = float(js_ellipsoid_offset[2].as_num().val);
            dr.ellipsoids[i].radius = float(js_ellipsoid.at("radius").as_num().val);

            const Sys::JsArrayP &js_ellipsoid_axis = js_ellipsoid.at("axis").as_arr();
            dr.ellipsoids[i].axis[0] = float(js_ellipsoid_axis[0].as_num().val);
            dr.ellipsoids[i].axis[1] = float(js_ellipsoid_axis[1].as_num().val);
            dr.ellipsoids[i].axis[2] = float(js_ellipsoid_axis[2].as_num().val);

            if (const size_t bone_ndx = js_ellipsoid.IndexOf("bone"); bone_ndx < js_ellipsoid.Size()) {
                dr.ellipsoids[i].bone_name = Ren::String{js_ellipsoid[bone_ndx].second.as_str().val.c_str()};
            }
            dr.ellipsoids[i].bone_index = -1;
        }
        dr.ellipsoids_count = int(js_ellipsoids.elements.size());
    }*/
}

void Eng::Drawable::Write(const Drawable &dr, Sys::JsObjectP &js_out) {
    const auto &alloc = js_out.elements.get_allocator();

    if (dr.mesh) {
        // write mesh file name
        js_out.Insert("mesh_file", Sys::JsStringP{dr.mesh->name(), alloc});
    }

    if (!dr.material_override.empty()) {
        Sys::JsArrayP js_material_override(alloc);

        for (const auto &mat : dr.material_override) {
            js_material_override.Push(Sys::JsStringP{mat[0]->name(), alloc});
        }

        js_out.Insert("material_override", std::move(js_material_override));
    }

    // write visibility
    if ((dr.vis_mask & eVisibility::Camera) != (DefaultVisMask & eVisibility::Camera)) {
        js_out.Insert(
            "visible_to_camera",
            Sys::JsLiteral((dr.vis_mask & eVisibility::Camera) ? Sys::JsLiteralType::True : Sys::JsLiteralType::False));
    }
    if ((dr.vis_mask & eVisibility::Shadow) != (DefaultVisMask & eVisibility::Shadow)) {
        js_out.Insert(
            "visible_to_shadow",
            Sys::JsLiteral((dr.vis_mask & eVisibility::Shadow) ? Sys::JsLiteralType::True : Sys::JsLiteralType::False));
    }
    if ((dr.vis_mask & eVisibility::Probes) != (DefaultVisMask & eVisibility::Probes)) {
        js_out.Insert(
            "visible_to_probes",
            Sys::JsLiteral((dr.vis_mask & eVisibility::Probes) ? Sys::JsLiteralType::True : Sys::JsLiteralType::False));
    }

    /*if (dr.ellipsoids_count) {
        Sys::JsArrayP js_ellipsoids(alloc);

        for (int i = 0; i < dr.ellipsoids_count; i++) {
            Sys::JsObjectP js_ellipsoid(alloc);

            { // write offset
                Sys::JsArrayP js_ellipsoid_offset(alloc);
                js_ellipsoid_offset.Push(Sys::JsNumber{dr.ellipsoids[i].offset[0]});
                js_ellipsoid_offset.Push(Sys::JsNumber{dr.ellipsoids[i].offset[1]});
                js_ellipsoid_offset.Push(Sys::JsNumber{dr.ellipsoids[i].offset[2]});
                js_ellipsoid.Push("offset", std::move(js_ellipsoid_offset));
            }

            js_ellipsoid.Push("radius", Sys::JsNumber{dr.ellipsoids[i].radius});

            { // write axis
                Sys::JsArrayP js_ellipsoid_axis(alloc);
                js_ellipsoid_axis.Push(Sys::JsNumber{dr.ellipsoids[i].axis[0]});
                js_ellipsoid_axis.Push(Sys::JsNumber{dr.ellipsoids[i].axis[1]});
                js_ellipsoid_axis.Push(Sys::JsNumber{dr.ellipsoids[i].axis[2]});
                js_ellipsoid.Push("axis", std::move(js_ellipsoid_axis));
            }

            if (!dr.ellipsoids[i].bone_name.empty()) {
                js_ellipsoid.Push("bone", Sys::JsStringP{dr.ellipsoids[i].bone_name, alloc});
            }
        }
    }*/
}