#include "AccStructure.h"

#include <Ren/Material.h>
#include <Ren/Mesh.h>
#include <Sys/Json.h>

const Ren::Bitmask<Eng::AccStructure::eRayType> Eng::AccStructure::DefaultVisMask =
    Ren::Bitmask<eRayType>{eRayType::Camera} | eRayType::Diffuse | eRayType::Specular | eRayType::Refraction |
    eRayType::Shadow;

void Eng::AccStructure::Read(const Sys::JsObjectP &js_in, AccStructure &acc) {
    acc.vis_mask = DefaultVisMask;

    if (const size_t visible_to_camera_ndx = js_in.IndexOf("visible_to_camera"); visible_to_camera_ndx < js_in.Size()) {
        const Sys::JsLiteral v = js_in[visible_to_camera_ndx].second.as_lit();
        if (v.val == Sys::JsLiteralType::True) {
            acc.vis_mask |= eRayType::Camera;
        } else {
            acc.vis_mask &= ~Ren::Bitmask(eRayType::Camera);
        }
    }

    if (const size_t visible_to_diffuse_ndx = js_in.IndexOf("visible_to_diffuse");
        visible_to_diffuse_ndx < js_in.Size()) {
        const Sys::JsLiteral v = js_in[visible_to_diffuse_ndx].second.as_lit();
        if (v.val == Sys::JsLiteralType::True) {
            acc.vis_mask |= eRayType::Diffuse;
        } else {
            acc.vis_mask &= ~Ren::Bitmask(eRayType::Diffuse);
        }
    }

    if (const size_t visible_to_specular_ndx = js_in.IndexOf("visible_to_specular");
        visible_to_specular_ndx < js_in.Size()) {
        const Sys::JsLiteral v = js_in[visible_to_specular_ndx].second.as_lit();
        if (v.val == Sys::JsLiteralType::True) {
            acc.vis_mask |= eRayType::Specular;
        } else {
            acc.vis_mask &= ~Ren::Bitmask(eRayType::Specular);
        }
    }

    if (const size_t visible_to_refraction_ndx = js_in.IndexOf("visible_to_refraction");
        visible_to_refraction_ndx < js_in.Size()) {
        const Sys::JsLiteral v = js_in[visible_to_refraction_ndx].second.as_lit();
        if (v.val == Sys::JsLiteralType::True) {
            acc.vis_mask |= eRayType::Refraction;
        } else {
            acc.vis_mask &= ~Ren::Bitmask(eRayType::Refraction);
        }
    }

    if (const size_t visible_to_shadow_ndx = js_in.IndexOf("visible_to_shadow"); visible_to_shadow_ndx < js_in.Size()) {
        const Sys::JsLiteral v = js_in[visible_to_shadow_ndx].second.as_lit();
        if (v.val == Sys::JsLiteralType::True) {
            acc.vis_mask |= eRayType::Shadow;
        } else {
            acc.vis_mask &= ~Ren::Bitmask(eRayType::Shadow);
        }
    }

    if (const size_t visible_to_volume_ndx = js_in.IndexOf("visible_to_volume"); visible_to_volume_ndx < js_in.Size()) {
        const Sys::JsLiteral v = js_in[visible_to_volume_ndx].second.as_lit();
        if (v.val == Sys::JsLiteralType::True) {
            acc.vis_mask |= eRayType::Volume;
        } else {
            acc.vis_mask &= ~Ren::Bitmask(eRayType::Volume);
        }
    }
}

void Eng::AccStructure::Write(const AccStructure &acc, Sys::JsObjectP &js_out) {
    const auto &alloc = js_out.elements.get_allocator();

    if (acc.mesh) {
        // write mesh file name
        js_out.Insert("mesh_file", Sys::JsStringP{acc.mesh->name(), alloc});
    }

    if (!acc.material_override.empty()) {
        Sys::JsArrayP js_material_override(alloc);

        for (const auto &mat : acc.material_override) {
            std::string mat_name;
            if (mat[0]) {
                mat_name = mat[0]->name();
            } else if (mat[2]) {
                mat_name = mat[2]->name();
                mat_name = mat_name.substr(0, mat_name.length() - 4);
            }
            js_material_override.Push(Sys::JsStringP{mat_name, alloc});
        }

        js_out.Insert("material_override", std::move(js_material_override));
    }

    // write visibility
    if ((acc.vis_mask & eRayType::Camera) != (DefaultVisMask & eRayType::Camera)) {
        js_out.Insert(
            "visible_to_camera",
            Sys::JsLiteral((acc.vis_mask & eRayType::Camera) ? Sys::JsLiteralType::True : Sys::JsLiteralType::False));
    }
    if ((acc.vis_mask & eRayType::Diffuse) != (DefaultVisMask & eRayType::Diffuse)) {
        js_out.Insert(
            "visible_to_diffuse",
            Sys::JsLiteral((acc.vis_mask & eRayType::Diffuse) ? Sys::JsLiteralType::True : Sys::JsLiteralType::False));
    }
    if ((acc.vis_mask & eRayType::Specular) != (DefaultVisMask & eRayType::Specular)) {
        js_out.Insert(
            "visible_to_specular",
            Sys::JsLiteral((acc.vis_mask & eRayType::Specular) ? Sys::JsLiteralType::True : Sys::JsLiteralType::False));
    }
    if ((acc.vis_mask & eRayType::Refraction) != (DefaultVisMask & eRayType::Refraction)) {
        js_out.Insert("visible_to_refraction",
                      Sys::JsLiteral((acc.vis_mask & eRayType::Refraction) ? Sys::JsLiteralType::True
                                                                           : Sys::JsLiteralType::False));
    }
    if ((acc.vis_mask & eRayType::Shadow) != (DefaultVisMask & eRayType::Shadow)) {
        js_out.Insert(
            "visible_to_shadow",
            Sys::JsLiteral((acc.vis_mask & eRayType::Shadow) ? Sys::JsLiteralType::True : Sys::JsLiteralType::False));
    }
    if ((acc.vis_mask & eRayType::Volume) != (DefaultVisMask & eRayType::Volume)) {
        js_out.Insert(
            "visible_to_volume",
            Sys::JsLiteral((acc.vis_mask & eRayType::Volume) ? Sys::JsLiteralType::True : Sys::JsLiteralType::False));
    }
}