#pragma once

// Headers
#include "core/core.hpp"
#include "math/vec3.hpp"
#include "hittables/hittable.hpp"
#include "ray.hpp"
#include "graphics/color.hpp"
#include "pbr/pbr.hpp"

// Forward declarations
class PDF;

// Namespace forward declarations
namespace Raytracing
{
    class Texture;
    class ImageTexture;
}

enum SCATTER_TYPE
{
    REFLECT,
    REFRACT
};

enum MATERIAL_CLASS
{
    _PBR,
    LAMBERTIAN,
    METAL,
    DIELECTRIC,
    DIFFUSE_LIGHT,
    ISOTROPIC,
    NONE
};

enum MATERIAL_TYPE
{
    _PDF,
    NON_PDF,
    EMISSIVE,
    UNSPECIFIED
};

struct scatter_record
{
public:
    optional<Ray> non_pdf_ray;
    shared_ptr<PDF> pdf;
    optional<BRDF_Parameters> brdf_parameters;
    SCATTER_TYPE scatter_type;
};

struct PBR_data
{
    vec3 albedo_scalar = vec3(1.0, 0.0, 1.0);
    double metalness_scalar = 0.0;
    double roughness_scalar = 1.0;

    shared_ptr<Raytracing::ImageTexture> albedo_texture = nullptr;
    shared_ptr<Raytracing::ImageTexture> metalness_texture = nullptr;
    shared_ptr<Raytracing::ImageTexture> roughness_texture = nullptr;
    shared_ptr<Raytracing::ImageTexture> roughness_metalness_texture = nullptr;
    shared_ptr<Raytracing::ImageTexture> normal_texture = nullptr;
    shared_ptr<Raytracing::ImageTexture> reflectance_texture = nullptr;
};

namespace Raytracing
{
    class Material
    {
    public:
        virtual ~Material() = default;

        virtual bool scatter(const Ray& incoming_ray, const hit_record& rec, scatter_record& srec) const;
        virtual color emitted(const Ray& incoming_ray, const hit_record& rec) const;
        virtual vec3 BRDF_value(const Ray& incoming_ray, const Ray& scattered_ray, const hit_record& hrec, const scatter_record& srec) const;
        const MATERIAL_CLASS get_class() const;
        const MATERIAL_TYPE get_type() const;

    protected:
        MATERIAL_CLASS material_class = NONE;
        MATERIAL_TYPE material_type = UNSPECIFIED;
    };

    /************ PBR Materials ************/

    class PBR : public Material
    {
    public:
        PBR(const PBR_data& data);

        bool scatter(const Ray& incoming_ray, const hit_record& rec, scatter_record& srec) const override;
        vec3 BRDF_value(const Ray& incoming_ray, const Ray& scattered_ray, const hit_record& hrec, const scatter_record& srec) const override;

        vec3 get_albedo(const hit_record& rec) const;
        double get_metalness(const hit_record& rec) const;
        double get_roughness(const hit_record& rec) const;
        pair<double, double> get_roughness_metalness(const hit_record& rec) const;
        optional<double> get_reflectance(const hit_record& rec) const;

        shared_ptr<Raytracing::ImageTexture> get_normal_texture() const;

        double get_alpha(const hit_record& rec) const;

    private:
        vec3 default_albedo;
        double default_roughness;
        double default_metalness;

        shared_ptr<Raytracing::ImageTexture> albedo_texture;
        shared_ptr<Raytracing::ImageTexture> roughness_texture;
        shared_ptr<Raytracing::ImageTexture> metalness_texture;
        shared_ptr<Raytracing::ImageTexture> roughness_metalness_texture;
        shared_ptr<Raytracing::ImageTexture> reflectance_texture;
        shared_ptr<Raytracing::ImageTexture> normal_texture;
    };

    /************ Diffuse Materials ************/

    class Lambertian : public Material
    {
    public:
        Lambertian(const color& albedo);
        Lambertian(shared_ptr<Texture> texture);

        bool scatter(const Ray& incoming_ray, const hit_record& rec, scatter_record& srec) const override;
        vec3 BRDF_value(const Ray& incoming_ray, const Ray& scattered_ray, const hit_record& hrec, const scatter_record& srec) const override;
        
    private:
        shared_ptr<Texture> texture;

        color get_texture_value(const hit_record& rec) const;

    };

    class Isotropic : public Material
    {
    public:
        Isotropic(const color& albedo);
        Isotropic(shared_ptr<Texture> texture);

        bool scatter(const Ray& incoming_ray, const hit_record& rec, scatter_record& srec) const override;
        vec3 BRDF_value(const Ray& incoming_ray, const Ray& scattered_ray, const hit_record& hrec, const scatter_record& srec) const override;

    private:
        shared_ptr<Texture> texture;
    };

    /************ Specular Materials ************/

    class Metal : public Material
    {
    public:
        Metal(const color& albedo, double fuzz);

        bool scatter(const Ray& incoming_ray, const hit_record& rec, scatter_record& srec) const override;
        vec3 BRDF_value(const Ray& incoming_ray, const Ray& scattered_ray, const hit_record& hrec, const scatter_record& srec) const override;

    private:
        color albedo;
        double fuzz;
    };

    class Dielectric : public Material
    {
    public:
        Dielectric(double refraction_index);

        bool scatter(const Ray& incoming_ray, const hit_record& rec, scatter_record& srec) const override;
        vec3 BRDF_value(const Ray& incoming_ray, const Ray& scattered_ray, const hit_record& hrec, const scatter_record& srec) const override;

    private:
        double refraction_index; // Refractive index in vacuum or air, or the ratio of the material's refractive index over the refractive index of the enclosing media

        static double reflectance(double cosine, double refraction_index);
    };

    /************ Emissive Materials ************/

    class DiffuseLight : public Material
    {
    public:
        DiffuseLight(shared_ptr<Texture>& texture);
        DiffuseLight(const color& emit);

        color emitted(const Ray& incoming_ray, const hit_record& rec) const override;

    private:
        shared_ptr<Texture> texture;
    };
}

