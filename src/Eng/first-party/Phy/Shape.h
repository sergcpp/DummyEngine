#pragma once

#include <vector>

#include "Bounds.h"

namespace Phy {
enum class eShapeType { Sphere, Box, Convex };

class Shape {
  protected:
    Vec3 center_of_mass_ = Vec3{Uninitialize};

  public:
    Vec3 center_of_mass() const { return center_of_mass_; }

    virtual eShapeType type() const = 0;

    virtual Mat3 GetInverseInertiaTensor() const = 0;
    virtual Bounds GetBounds() const = 0;
    virtual Bounds GetBounds(const Vec3 &pos, const Quat &rot) const = 0;
    virtual real GetFastestLinearSpeedDueToRotation(const Vec3 &vel_ang,
                                                    const Vec3 &dir) const {
        return real(0);
    }

    // Returns the furthest point in a particular direction
    virtual Vec3 Support(const Vec3 &dir, const Vec3 &pos, const Quat &rot,
                         real bias) const = 0;
    virtual void Build(const Vec3 pts[], const int pts_count) {}
};

class ShapeSphere : public Shape {
  public:
    explicit ShapeSphere(const real _radius) : radius(_radius) {
        center_of_mass_ = Vec3{0.0f};
    }
    eShapeType type() const override { return eShapeType::Sphere; }

    Mat3 GetInverseInertiaTensor() const override {
        const real v = real(5) / (real(2) * radius * radius);
        return Mat3{Vec3{v, 0, 0}, Vec3{0, v, 0}, Vec3{0, 0, v}};
    }

    Bounds GetBounds() const override {
        Bounds ret;
        ret.mins = Vec3(-radius);
        ret.maxs = Vec3(+radius);
        return ret;
    }

    Bounds GetBounds(const Vec3 &pos, const Quat &rot) const override {
        Bounds ret;
        ret.mins = pos - Vec3(radius);
        ret.maxs = pos + Vec3(radius);
        return ret;
    }

    Vec3 Support(const Vec3 &dir, const Vec3 &pos, const Quat &rot,
                 const real bias) const override {
        return pos + dir * (radius + bias);
    }

    real radius;
};

class ShapeBox : public Shape {
  public:
    explicit ShapeBox(const Vec3 pts[], const int count) { Build(pts, count); }
    eShapeType type() const override { return eShapeType::Box; }

    Mat3 GetInverseInertiaTensor() const override;

    Bounds GetBounds() const override { return bounds; }
    Bounds GetBounds(const Vec3 &pos, const Quat &rot) const override;
    real GetFastestLinearSpeedDueToRotation(const Vec3 &vel_ang,
                                            const Vec3 &dir) const override;

    Vec3 Support(const Vec3 &dir, const Vec3 &pos, const Quat &rot,
                 real bias) const override;
    void Build(const Vec3 pts[], int pts_count) override;

    Vec3 points[8];
    Bounds bounds;
};

class ShapeConvex : public Shape {
  public:
    explicit ShapeConvex(const Vec3 pts[], const int count) { Build(pts, count); }
    eShapeType type() const override { return eShapeType::Convex; }

    Vec3 Support(const Vec3 &dir, const Vec3 &pos, const Quat &rot,
                 real bias) const override;
    void Build(const Vec3 pts[], int pts_count) override;

    std::vector<Vec3> points;
    Bounds bounds;
    Mat3 inertia_tensor;
};

bool SphereSphereStatic(const ShapeSphere &a, const ShapeSphere &b, const Vec3 &pos_a,
                        const Vec3 &pos_b, Vec3 &pt_on_a, Vec3 &pt_on_b);
bool SphereSphereDynamic(const ShapeSphere &a, const ShapeSphere &b, const Vec3 &pos_a,
                         const Vec3 &pos_b, const Vec3 &vel_a, const Vec3 &vel_b, real dt,
                         Vec3 &pt_on_a, Vec3 &pt_on_b, real &toi);

} // namespace Phy