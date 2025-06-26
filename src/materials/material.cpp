// Headers
#include "core/core.hpp"
#include "material.hpp"
#include "texture.hpp"
#include "math/pdf.hpp"
#include "ray.hpp"
#include "hittables/hittable.hpp"
#include "utils/utilities.hpp"

// Usings
using Raytracing::color;

// ****** Material Class ****** //

bool Raytracing::Material::scatter(const Ray& incoming_ray, const hit_record& rec, scatter_record& srec) const
{
    return false;
}

color Raytracing::Material::emitted(const Ray& incoming_ray, const hit_record& rec) const
{
    return color(0, 0, 0);
}

vec3 Raytracing::Material::BRDF_value(const Ray& incoming_ray, const Ray& scattered_ray, const hit_record& hrec, const scatter_record& srec) const
{
    return vec3(0.0);
}

const MATERIAL_CLASS Raytracing::Material::get_class() const
{
    return material_class;
}

const MATERIAL_TYPE Raytracing::Material::get_type() const
{
    return material_type;
}

// ****** PBR Class ****** //

Raytracing::PBR::PBR(const PBR_data& data)
{
    material_class = _PBR;
    material_type = _PDF;

    default_albedo = data.albedo_scalar;
    default_metalness = data.metalness_scalar;
    default_roughness = data.roughness_scalar;

    albedo_texture = data.albedo_texture;
    metalness_texture = data.metalness_texture;
    roughness_texture = data.roughness_texture;
    roughness_metalness_texture = data.roughness_metalness_texture;
    reflectance_texture = data.reflectance_texture;
    normal_texture = data.normal_texture;
}

bool Raytracing::PBR::scatter(const Ray& incoming_ray, const hit_record& rec, scatter_record& srec) const
{
    // Calculate view direction and shading normal vectors
    vec3 view_direction = -incoming_ray.direction();
    vec3 shading_normal = rec.normal;

    // Get texture parameters
    optional<double> reflectance = get_reflectance(rec);
    vec3 albedo = get_albedo(rec);
    auto [roughness, metalness] = get_roughness_metalness(rec);

    // Prepare BRDF parameters
    BRDF_Parameters brdf_parameters = prepare_BRDF_params(reflectance, albedo, roughness, metalness, view_direction, shading_normal);

    // Diffuse PDF (cosine-weighted hemisphere sampling)
    auto diffuse_pdf = make_shared<cosine_hemisphere_pdf>(rec.normal);

    // Specular PDF (Visible Normal Distribution Function sampling)
    auto specular_pdf = make_shared<vndf_pdf>(brdf_parameters.alpha, brdf_parameters.alpha_squared, view_direction);

    // Create mixture density PDF (diffuse + specular)
    auto pbr_pdf = make_shared<mixture_pdf>(specular_pdf, diffuse_pdf, brdf_parameters.specular_ratio);

    // Fill scatter record
    srec.non_pdf_ray = nullopt;
    srec.scatter_type = REFLECT;
    srec.pdf = pbr_pdf;
    srec.brdf_parameters = brdf_parameters;

    return true;
}

vec3 Raytracing::PBR::BRDF_value(const Ray& incoming_ray, const Ray& scattered_ray, const hit_record& hrec, const scatter_record& srec) const
{
    // Unwrap pre-computed BRDF parameters
    BRDF_Parameters brdf_parameters = srec.brdf_parameters.value();

    // Parameters
    vec3 diffuse_reflectance = brdf_parameters.diffuse_reflectance; 
    vec3 albedo = brdf_parameters.albedo;          
    double roughness = brdf_parameters.roughness;
    double alpha_squared = brdf_parameters.alpha_squared; 
    vec3 fresnel_term = brdf_parameters.fresnel_term;
    
    // Vectors
    vec3 N = hrec.normal;                   // Shading normal
    vec3 V = brdf_parameters.V;             // Viewer direction (incoming ray inverted direction)
    vec3 L = scattered_ray.direction();     // Scattering direction (light direction)
    vec3 H = normalize(V + L);              // Half vector (microfacet normal)

    // Dot products
    double NdotV = brdf_parameters.NdotV;
    double NdotL = dot(N, L);
    double LdotH = dot(L, H);
    double NdotH = dot(N, H);

    // Compute diffuse term
    BRDF_Diffuse_Data diffuse_term = frostbite_disney_diffuse
    (
        diffuse_reflectance,
        albedo,
        roughness,
        NdotV,
        NdotL,
        LdotH
    );

    // Compute specular term
    BRDF_Specular_Data specular_term = cook_torrance_specular
    (
        fresnel_term,
        alpha_squared,
        NdotV,
        NdotH,
        NdotL
    );

    // Combine diffuse and specular terms
    vec3 BRDF_value = combine_BRDFs(diffuse_term, specular_term);

    return BRDF_value;
}

vec3 Raytracing::PBR::get_albedo(const hit_record& rec) const
{
    if (!albedo_texture)
        return default_albedo;

    return albedo_texture->value(rec.texture_coordinates, rec.p);
}

double Raytracing::PBR::get_metalness(const hit_record& rec) const
{
    if(!metalness_texture)
        return default_metalness;

    return metalness_texture->value(rec.texture_coordinates, rec.p).x;
}

double Raytracing::PBR::get_roughness(const hit_record& rec) const
{
    if (!roughness_texture)
        return default_roughness;

    return roughness_texture->value(rec.texture_coordinates, rec.p).x;
}

pair<double, double> Raytracing::PBR::get_roughness_metalness(const hit_record& rec) const
{
    double roughness, metalness;

    if (!roughness_metalness_texture)
    {
        roughness = get_roughness(rec);
        metalness = get_metalness(rec);
    }
    else
    {
        auto tex_val = roughness_metalness_texture->value(rec.texture_coordinates, rec.p);
        roughness = tex_val.y;  // Green channel is roughness
        metalness = tex_val.z;  // Blue channel is metalness
    }

    return make_pair(roughness, metalness);
}

optional<double> Raytracing::PBR::get_reflectance(const hit_record& rec) const
{
    if (!reflectance_texture)
        return std::nullopt;

    return reflectance_texture->value(rec.texture_coordinates, rec.p).x;
}

shared_ptr<Raytracing::ImageTexture> Raytracing::PBR::get_normal_texture() const
{
    return normal_texture;
}

double Raytracing::PBR::get_alpha(const hit_record& rec) const
{
    auto [roughness, metalness] = get_roughness_metalness(rec);

    return map_alpha(roughness);
}

// ****** Lambertian Class ****** //

Raytracing::Lambertian::Lambertian(const color& albedo) : texture(make_shared<SolidColor>(albedo))
{ 
    material_class = LAMBERTIAN;
    material_type = _PDF;
}

Raytracing::Lambertian::Lambertian(shared_ptr<Raytracing::Texture> texture) : texture(texture)
{ 
    material_class = LAMBERTIAN;
    material_type = _PDF;
}

bool Raytracing::Lambertian::scatter(const Ray& incoming_ray, const hit_record& rec, scatter_record& srec) const
{
    // auto scatter_direction = rec.normal + random_unit_vector();
    srec.non_pdf_ray = nullopt;
    srec.pdf = make_shared<cosine_hemisphere_pdf>(rec.normal);
    srec.scatter_type = REFLECT;

    return true;
}

vec3 Raytracing::Lambertian::BRDF_value(const Ray& incoming_ray, const Ray& scattered_ray, const hit_record& hrec, const scatter_record& srec) const
{
    // Texture coordinates
    vec3 attenuation = get_texture_value(hrec);

    return attenuation / pi;
}

color Raytracing::Lambertian::get_texture_value(const hit_record& rec) const
{
    if (!texture)
        return MAGENTA;

    return texture->value(rec.texture_coordinates, rec.p);
}

// ****** Isotropic Class ****** //

Raytracing::Isotropic::Isotropic(const color& albedo) : texture(make_shared<SolidColor>(albedo))
{
    material_class = ISOTROPIC;
    material_type = _PDF;
}

Raytracing::Isotropic::Isotropic(shared_ptr<Raytracing::Texture> texture) : texture(texture)
{
    material_class = ISOTROPIC;
    material_type = _PDF;
}

bool Raytracing::Isotropic::scatter(const Ray& incoming_ray, const hit_record& rec, scatter_record& srec) const
{
    srec.non_pdf_ray = nullopt;
    srec.pdf = make_shared<uniform_sphere_pdf>();
    srec.scatter_type = REFLECT;
    return true;
}

vec3 Raytracing::Isotropic::BRDF_value(const Ray& incoming_ray, const Ray& scattered_ray, const hit_record& hrec, const scatter_record& srec) const
{
    vec3 attenuation = texture->value(hrec.texture_coordinates, hrec.p);
    return attenuation / (4 * pi);
}

// ****** Metal Class ****** //

Raytracing::Metal::Metal(const color& albedo, double fuzz) : albedo(albedo), fuzz(fuzz < 1 ? fuzz : 1)
{
    material_class = METAL;
    material_type = NON_PDF;
}

bool Raytracing::Metal::scatter(const Ray& incoming_ray, const hit_record& rec, scatter_record& srec) const
{
    // Reflect the incoming ray
    vec3 reflected = reflect(incoming_ray.direction(), rec.normal);
    reflected = unit_vector(reflected) + (fuzz * random_unit_vector());

    // Create reflected ray
    auto reflected_ray = Ray(rec.p, reflected, incoming_ray.time());

    // Save data into scatter record
    srec.non_pdf_ray = Ray(reflected_ray);
    srec.pdf = nullptr;
    srec.scatter_type = REFLECT;

    // Absorb the ray if it's reflected into the surface
    // return dot(reflected_ray.direction(), rec.normal) > 0;

    return true;
}

vec3 Raytracing::Metal::BRDF_value(const Ray& incoming_ray, const Ray& scattered_ray, const hit_record& hrec, const scatter_record& srec) const
{
    auto attenuation = albedo;
    return attenuation;
}

// ****** Dielectric Class ****** //

Raytracing::Dielectric::Dielectric(double refraction_index) : refraction_index(refraction_index)
{ 
    material_class = DIELECTRIC;
    material_type = NON_PDF;
}

bool Raytracing::Dielectric::scatter(const Ray& incoming_ray, const hit_record& rec, scatter_record& srec) const
{
    // Check refractive index order
    double ri = rec.front_face ? (1.0 / refraction_index) : refraction_index;

    // Calculate cosinus and sinus of theta (angle between the ray and the normal)
    vec3 unit_direction = unit_vector(incoming_ray.direction());
    double cos_theta = std::fmin(dot(-unit_direction, rec.normal), 1.0);
    double sin_theta = std::sqrt(1.0 - cos_theta * cos_theta);

    // Check if there is total reflection (the ray cannot refract)
    bool cannot_refract = ri * sin_theta > 1.0;

    // Check reflectance probability
    double reflect_prob = reflectance(cos_theta, ri);

    // Check if the ray should reflect or refract
    vec3 scattering_direction;
    if (cannot_refract || reflect_prob > random_number<double>())
    {
        scattering_direction = reflect(unit_direction, rec.normal);
        srec.scatter_type = REFLECT;
    }
    else
    {
        scattering_direction = refract(unit_direction, rec.normal, cos_theta, ri);
        srec.scatter_type = REFRACT;
    }

    // Create scattered ray
    auto scattered_ray = Ray(rec.p, scattering_direction, incoming_ray.time());

    // Save data into scatter record
    srec.non_pdf_ray = Ray(scattered_ray);
    srec.pdf = nullptr;

    return true;
}

double Raytracing::Dielectric::reflectance(double cosine, double refraction_index)
{
    // Use Schlick's approximation for reflectance.
    auto r0 = (1 - refraction_index) / (1 + refraction_index);
    r0 = r0 * r0;
    return r0 + (1 - r0) * std::pow((1 - cosine), 5);
}

vec3 Raytracing::Dielectric::BRDF_value(const Ray& incoming_ray, const Ray& scattered_ray, const hit_record& hrec, const scatter_record& srec) const
{
    // Attenuation of glass is 1.0, so we return a white color
    auto attenuation = color(1.0, 1.0, 1.0);

    return attenuation;
}


// ****** DiffuseLight Class ****** //

Raytracing::DiffuseLight::DiffuseLight(shared_ptr<Raytracing::Texture>& texture) : texture(texture)
{ 
    material_class = DIFFUSE_LIGHT;
    material_type = EMISSIVE;
}

Raytracing::DiffuseLight::DiffuseLight(const color& emit) : texture(make_shared<SolidColor>(emit))
{ 
    material_class = DIFFUSE_LIGHT;
    material_type = EMISSIVE;
}

color Raytracing::DiffuseLight::emitted(const Ray& incoming_ray, const hit_record& rec) const
{
    if (!rec.front_face)
        return color(0, 0, 0);

    return texture->value(rec.texture_coordinates, rec.p);
}
