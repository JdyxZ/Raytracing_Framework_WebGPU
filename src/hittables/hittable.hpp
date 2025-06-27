#pragma once

// Headers
#include "core/core.hpp"
#include "math/vec3.hpp"
#include "math/aabb.hpp"
#include "math/matrix.hpp"
#include "math/transform.hpp"

// Forward declarations
struct Ray;
struct Interval;

// Namespace forward declarations
namespace Raytracing
{
    class Material;
}

enum HITTABLE_TYPE
{
	SPHERE,
    QUAD,
	TRIANGLE,
    CONSTANT_MEDIUM,
    BOX,
    MESH,
    SURFACE,
    BVH_NODE,
    HITTABLE_LIST,
	NOT_SPECIFIED
};

struct barycentric_coordinates
{
	double u, v, w;
};

struct hit_record
{
public:
    // General attributes
    point3 p;
    vec3 normal;
    double t;
    bool front_face;
    shared_ptr<Raytracing::Material> material;
    optional<pair<double, double>> texture_coordinates = nullopt;
    HITTABLE_TYPE type = NOT_SPECIFIED;
    vector<unsigned long long> elapsed_nanoseconds;

    // Triangle attributes
    optional<barycentric_coordinates> bc;

    virtual ~hit_record() = default; 

    void determine_normal_direction(const vec3& ray_direction, const vec3& outward_normal);     // Sets the hit record normal vector direction.
};

class Hittable
{
public:
    Hittable();
    virtual ~Hittable() = default;

    virtual bool hit(const Ray& r, const Interval& ray_t, hit_record& rec) const = 0;
    Raytracing::AABB get_bbox() const;
    HITTABLE_TYPE get_type() const;
    bool is_primitive() const;
    bool is_bvh_tree() const;
    virtual double pdf_value(const point3& hit_point, const vec3& scattering_direction) const;
    const bool has_pdf() const;
    virtual vec3 random_scattering_ray(const point3& hit_point) const;

    Raytracing::Matrix44 get_model() const;
    Raytracing::Transform get_transform() const;

    void translate(const vec3& translation);
    void rotate(const vec3& axis, const double& angle);
    void scale(const vec3& scaling);

    void recompute_bbox();

    static bool is_not_primitive_hittable(HITTABLE_TYPE type);

protected:
    optional<Raytracing::AABB> original_bbox = nullopt;
    optional<Raytracing::AABB> bbox = nullopt;
    Raytracing::Transform transform = Raytracing::Transform();
    Raytracing::Matrix44 model = Raytracing::Matrix::identity(4);
    Raytracing::Matrix44 inverse_model = Raytracing::Matrix::identity(4);
    HITTABLE_TYPE type = NOT_SPECIFIED;
    bool transformed = false;
    bool pdf = false;

    void set_model(const optional<Raytracing::Matrix44>& model);
    void set_model(const glm::mat4x4& model);

    const Ray transform_ray(const Ray& r) const;
    void transform_hit_record(hit_record& rec) const;

private:
    void transform_bbox(const optional<Raytracing::Matrix44>& model);
};


