#include "Drawable.h"

#include <Ren/Mesh.h>
#include <Sys/Json.h>

const Ren::Bitmask<Eng::Drawable::eVisibility> Eng::Drawable::DefaultVisMask =
    Ren::Bitmask<eVisibility>{eVisibility::Camera} | eVisibility::Shadow;

void Eng::Drawable::Read(const JsObjectP &js_in, Drawable &dr) {
    dr.vis_mask = DefaultVisMask;

    if (js_in.Has("visible_to_camera")) {
        JsLiteral v = js_in.at("visible_to_camera").as_lit();
        if (v.val == JsLiteralType::True) {
            dr.vis_mask |= eVisibility::Camera;
        } else {
            dr.vis_mask &= ~Ren::Bitmask(eVisibility::Camera);
        }
    }

    if (js_in.Has("visible_to_shadow")) {
        JsLiteral v = js_in.at("visible_to_shadow").as_lit();
        if (v.val == JsLiteralType::True) {
            dr.vis_mask |= eVisibility::Shadow;
        } else {
            dr.vis_mask &= ~Ren::Bitmask(eVisibility::Shadow);
        }
    }

    if (js_in.Has("visible_to_probes")) {
        JsLiteral v = js_in.at("visible_to_probes").as_lit();
        if (v.val == JsLiteralType::False) {
            dr.vis_mask &= ~Ren::Bitmask(eVisibility::Probes);
        }
    }

    if (js_in.Has("mesh_file")) {
        const JsStringP &mesh_name = js_in.at("mesh_file").as_str();
        dr.mesh_file = Ren::String{mesh_name.val.c_str()};
    }

    // if (js_in.Has("material_override")) {
    //     dr.flags |= eFlags::MaterialOverride;
    // }

    /*if (js_in.Has("ellipsoids")) {
        const JsArrayP &js_ellipsoids = js_in.at("ellipsoids").as_arr();
        for (size_t i = 0; i < js_ellipsoids.elements.size(); i++) {
            const JsObjectP &js_ellipsoid = js_ellipsoids[i].as_obj();

            const JsArrayP &js_ellipsoid_offset = js_ellipsoid.at("offset").as_arr();
            dr.ellipsoids[i].offset[0] = float(js_ellipsoid_offset[0].as_num().val);
            dr.ellipsoids[i].offset[1] = float(js_ellipsoid_offset[1].as_num().val);
            dr.ellipsoids[i].offset[2] = float(js_ellipsoid_offset[2].as_num().val);
            dr.ellipsoids[i].radius = float(js_ellipsoid.at("radius").as_num().val);

            const JsArrayP &js_ellipsoid_axis = js_ellipsoid.at("axis").as_arr();
            dr.ellipsoids[i].axis[0] = float(js_ellipsoid_axis[0].as_num().val);
            dr.ellipsoids[i].axis[1] = float(js_ellipsoid_axis[1].as_num().val);
            dr.ellipsoids[i].axis[2] = float(js_ellipsoid_axis[2].as_num().val);

            if (js_ellipsoid.Has("bone")) {
                dr.ellipsoids[i].bone_name = Ren::String{js_ellipsoid.at("bone").as_str().val.c_str()};
            }
            dr.ellipsoids[i].bone_index = -1;
        }
        dr.ellipsoids_count = (int)js_ellipsoids.elements.size();
    }*/
}

void Eng::Drawable::Write(const Drawable &dr, JsObjectP &js_out) {
    const auto &alloc = js_out.elements.get_allocator();

    if (dr.mesh) {
        // write mesh file name
        const Ren::String &mesh_name = dr.mesh->name();
        if (mesh_name != dr.mesh_file) {
            js_out.Insert("mesh_name", JsStringP{mesh_name, alloc});
        }
        js_out.Insert("mesh_file", JsStringP{dr.mesh_file.empty() ? mesh_name : dr.mesh_file, alloc});
    }

    if (!dr.material_override.empty()) {
        JsArrayP js_material_override(alloc);

        for (const auto &mat : dr.material_override) {
            js_material_override.Push(JsStringP{mat.first->name(), alloc});
        }

        js_out.Insert("material_override", std::move(js_material_override));
    }

    // write visibility
    if ((dr.vis_mask & eVisibility::Camera) == (DefaultVisMask & eVisibility::Camera)) {
        js_out.Insert("visible_to_camera",
                      JsLiteral((dr.vis_mask & eVisibility::Camera) ? JsLiteralType::True : JsLiteralType::False));
    }
    if ((dr.vis_mask & eVisibility::Shadow) == (DefaultVisMask & eVisibility::Shadow)) {
        js_out.Insert("visible_to_shadow",
                      JsLiteral((dr.vis_mask & eVisibility::Shadow) ? JsLiteralType::True : JsLiteralType::False));
    }
    if ((dr.vis_mask & eVisibility::Probes) == (DefaultVisMask & eVisibility::Probes)) {
        js_out.Insert("visible_to_probes",
                      JsLiteral((dr.vis_mask & eVisibility::Probes) ? JsLiteralType::True : JsLiteralType::False));
    }

    /*if (dr.ellipsoids_count) {
        JsArrayP js_ellipsoids(alloc);

        for (int i = 0; i < dr.ellipsoids_count; i++) {
            JsObjectP js_ellipsoid(alloc);

            { // write offset
                JsArrayP js_ellipsoid_offset(alloc);
                js_ellipsoid_offset.Push(JsNumber{dr.ellipsoids[i].offset[0]});
                js_ellipsoid_offset.Push(JsNumber{dr.ellipsoids[i].offset[1]});
                js_ellipsoid_offset.Push(JsNumber{dr.ellipsoids[i].offset[2]});
                js_ellipsoid.Push("offset", std::move(js_ellipsoid_offset));
            }

            js_ellipsoid.Push("radius", JsNumber{dr.ellipsoids[i].radius});

            { // write axis
                JsArrayP js_ellipsoid_axis(alloc);
                js_ellipsoid_axis.Push(JsNumber{dr.ellipsoids[i].axis[0]});
                js_ellipsoid_axis.Push(JsNumber{dr.ellipsoids[i].axis[1]});
                js_ellipsoid_axis.Push(JsNumber{dr.ellipsoids[i].axis[2]});
                js_ellipsoid.Push("axis", std::move(js_ellipsoid_axis));
            }

            if (!dr.ellipsoids[i].bone_name.empty()) {
                js_ellipsoid.Push("bone", JsStringP{dr.ellipsoids[i].bone_name, alloc});
            }
        }
    }*/
}