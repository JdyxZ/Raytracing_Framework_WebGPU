// Headers
#include "core/core.hpp"
#include "hittable.hpp"
#include "utils/utilities.hpp"
#include "ray.hpp"

// Framework headers
#include "framework/math/transform.h"

// Usings
using Raytracing::Matrix44;

void hit_record::determine_normal_direction(const vec3& ray_direction, const vec3& outward_normal)
{
    // NOTE: The parameter `outward_normal` is assumed to have unit length.
    front_face = dot(ray_direction, outward_normal) < 0;
    normal = front_face ? outward_normal : -outward_normal;
}

Hittable::Hittable()
{

}

Raytracing::AABB Hittable::get_bbox() const
{
    if (bbox.has_value())
    {
        // Return the transformed bounding box if it exists
        return bbox.value();
    }
    else if (original_bbox.has_value())
    {
        // Return the original bounding box otherwise
        return original_bbox.value();
    }
    else
    {
        // If no bounding box is set, there is a problem
        string error = Logger::error("HITTABLE", "Bounding box is not set yet!!!");
        throw std::runtime_error(error);
    }
        
}

HITTABLE_TYPE Hittable::get_type() const
{
    return type;
}

bool Hittable::is_primitive() const
{
    return type == TRIANGLE || type == SPHERE || type == QUAD;
}

bool Hittable::is_bvh_tree() const
{
    return type == BOX || type == MESH || type == SURFACE;
}

double Hittable::pdf_value(const point3& hit_point, const vec3& scattering_direction) const
{
    return 0.0;
}

const bool Hittable::has_pdf() const
{
    return pdf;
}

vec3 Hittable::random_scattering_ray(const point3& hit_point) const
{
    return vec3(1, 0, 0);
}

Raytracing::Matrix44 Hittable::get_model() const
{
    return model;
}

Raytracing::Transform Hittable::get_transform() const
{
    return transform;
}

void Hittable::translate(const vec3& translation)
{
    if (translation == vec3(0.0))
        return;

    transform.set_translation(translation);
    model = transform.get_model();
    inverse_model = model.inverse();
    transformed = true;

    transform_bbox(model);
}

void Hittable::rotate(const vec3& axis, const double& angle)
{
    if (axis == vec3(0) || angle == 0)
        return;

    transform.set_rotation(axis, angle);
    model = transform.get_model();
    inverse_model = model.inverse();
    transformed = true;

    transform_bbox(model);
}

void Hittable::scale(const vec3& scaling)
{
    if (scaling == vec3(1.0))
        return;

    transform.set_scaling(scaling);
    model = transform.get_model();
    inverse_model = model.inverse();
    transformed = true;

    transform_bbox(model);
}

void Hittable::recompute_bbox()
{
    transform_bbox(model);
}

bool Hittable::is_primitive_hittable(HITTABLE_TYPE type)
{
    return type != SPHERE && type != QUAD && type != TRIANGLE && type != CONSTANT_MEDIUM;
}

void Hittable::set_model(const optional<Raytracing::Matrix44>& model)
{
    if (!model.has_value())
        return;

    if (model.value() == Raytracing::Matrix44::identity(4))
        return;

    transform.set_model(model.value());
    this->model = model.value();
    inverse_model = model.value().inverse();
    transformed = true;

    transform_bbox(model);
}

void Hittable::set_model(const glm::mat4x4& model)
{
    if (model == glm::mat4(1.0f))
        return;

    transform = Raytracing::Transform(model);
    this->model = model;
    inverse_model = glm::inverse(model);
    transformed = true;

    transform_bbox(model);
}

void Hittable::transform_bbox(const optional<Raytracing::Matrix44>& model)
{
    if (!original_bbox.has_value())
    {
        string error = Logger::error("Hittable", "Bounding box is not set yet!!!");
        throw std::runtime_error(error);
    }

    bbox = original_bbox.value().transform(model.value());
}

const Ray Hittable::transform_ray(const Ray& r) const
{
    // Transform ray into object space
    auto transformed_origin = (inverse_model * vec4(r.origin(), 1.0)).dehomogenize();
    auto transformed_direction = inverse_model * vec4(r.direction(), 0.0);
    auto transformed_ray = Ray(transformed_origin, transformed_direction, r.time());

    return transformed_ray;
}

void Hittable::transform_hit_record(hit_record& rec) const
{
    // Transform hit record results back into world space
    rec.p = (model * vec4(rec.p, 1.0)).dehomogenize();
    rec.normal = (model * vec4(rec.normal, 0.0)).normalize();
}
