#include "core/core.hpp"
#include "pbr.hpp"
#include "utils/utilities.hpp"

// ******** MAPPINGS ********* //

double map_alpha(double perceptive_roughness)
{
    return perceptive_roughness * perceptive_roughness;
}

vec3 map_diffuse_reflectance(vec3 albedo, double metalness)
{
    return albedo * (1.0 - metalness);
}

// ******** FRESNEL ********* //

vec3 compute_F0(optional<double> reflectance, vec3 albedo, double metalness) 
{
    double F0_dielectrics;

    if (reflectance.has_value())
        F0_dielectrics = 0.16 * reflectance.value() * reflectance.value();
    else
        F0_dielectrics = min_F0_dielectrics;

    vec3 F0_dielectrics_vector = vec3(F0_dielectrics);

    return lerp(F0_dielectrics, albedo, metalness);
}

double luminance(vec3 rgb)
{
    return dot(rgb, vec3(0.2126, 0.7152, 0.0722));
}

double shadowed_F90(double F0)
{
    //const double t = 60.0;
    const double t = (1.0 / min_F0_dielectrics);
    return std::min(1.0, t * luminance(F0));
}

vec3 Schlick_Fresnel(vec3 f0, float f90, float NdotV)
{
    return f0 + (f90 - f0) * exp2((-5.55473 * NdotV - 6.983146) * NdotV);
}

// ******** VISIBILITY ********* //

double Smith_G1_Geometry(double alpha, double alpha_squared, double NdotS, double NdotS_squared)
{
    return 2.0f / (sqrt(((alpha_squared * (1.0 - NdotS_squared)) + NdotS_squared) / NdotS_squared) + 1.0);
}

double Smith_Visibility(double alpha_squared, double NdotL, double NdotV)
{
    double a = NdotV * sqrt(alpha_squared + NdotL * (NdotL - alpha_squared * NdotL));
    double b = NdotL * sqrt(alpha_squared + NdotV * (NdotV - alpha_squared * NdotV));
    return 0.5 / (a + b);
}

// ******** DISTRIBUTION ********* //

double GGX_Distribution(double alpha_squared, double NdotH)
{
    double b = (alpha_squared - 1.0f) * NdotH * NdotH + 1.0f;
    return alpha_squared / (Raytracing::pi * b * b);
}

// ******** DIFFUSE MODEL ********* //

BRDF_Diffuse_Data frostbite_disney_diffuse(vec3 diffuse_reflectance, vec3 albedo, double roughness, double NdotV, double NdotL, double LdotH)
{
    // Parameters
    double energy_bias = 0.5 * roughness;
    double energy_factor = std::lerp(1.0, 1.0 / 1.51, roughness);
    double FD_90_minus_one = (energy_bias + 2.0 * LdotH * LdotH * roughness) - 1.0;

    // Calculations
    double light_scatter = 1.0f + (FD_90_minus_one * pow(1.0f - NdotL, 5.0f));
    double view_scatter = 1.0f + (FD_90_minus_one * pow(1.0f - NdotV, 5.0f));
    double factor = light_scatter * view_scatter * energy_factor;

    // Diffuse BRDF value
    vec3 value = (diffuse_reflectance / Raytracing::pi) * factor;

    // Diffuse BRDF data
    BRDF_Diffuse_Data data;
    data.value = value;
    return data;
}

// ******** SPECULAR MODEL ********* //

BRDF_Specular_Data cook_torrance_specular(vec3 fresnel_term, double alpha_squared, double NdotV, double NdotH, double NdotL)
{
    // Functions
    vec3 F = fresnel_term;
    double D = GGX_Distribution(alpha_squared, NdotH);
    double V = Smith_Visibility(alpha_squared, NdotL, NdotV);

    // Specular BRDF value
    vec3 value =  F * (D * V);

    // Specular BRDF data
    BRDF_Specular_Data data;
    data.F = F;
    data.V = V;
    data.D = D;
    data.value = value;
    return data;
}

// ******** BRDF COMBINATION ********* //

BRDF_Parameters prepare_BRDF_params(optional<double> reflectance, vec3 albedo, double roughness, double metalness, vec3 view_direction, vec3 shading_normal)
{
    // Mappins and parameters
    vec3 F0 = compute_F0(reflectance, albedo, metalness);
    double F90 = 1.00; // Metallic workflow invalids shadowed_F90(F0)
    vec3 diffuse_reflectance = map_diffuse_reflectance(albedo, metalness);
    double alpha = map_alpha(roughness);
    double alpha_squared = alpha * alpha;

    // Optimization (assume vectors are already normalized)
    vec3 V = view_direction; 
    vec3 N = shading_normal;
    double NdotV = dot(N, V);

    // We calculate fresnel term here to avoid recomputing in the BRDF functions
    vec3 fresnel_term = Schlick_Fresnel(F0, F90, NdotV);

    // Calculate specular to diffuse scattering ratio 
    double specular_ratio = luminance(fresnel_term);
    specular_ratio = std::clamp(specular_ratio, 0.0, 1.0); // Clamp for safety

    // BRDF parameters
    BRDF_Parameters brdf_params;
    brdf_params.F0 = F0;
    brdf_params.diffuse_reflectance = diffuse_reflectance;
    brdf_params.albedo = albedo;
    brdf_params.roughness = roughness;
    brdf_params.alpha = alpha;
    brdf_params.alpha_squared = alpha_squared;
    brdf_params.V = V;
    brdf_params.NdotV = NdotV;
    brdf_params.fresnel_term = fresnel_term;
    brdf_params.specular_ratio = specular_ratio;

    return brdf_params;
}

vec3 combine_BRDFs(BRDF_Diffuse_Data diffuse, BRDF_Specular_Data specular)
{
    // Energy compensation factor to scale specular lobe to account for multiscattering
    // vec3 energy_compensation = 1.0 + specular.F0 * (1.0 / diffuse.value - 1.0);

    // Combine diffuse and specular BRDFs
    // return diffuse.value * (1.0 - specular.F) + specular.value;
    return diffuse.value + specular.value;
}


