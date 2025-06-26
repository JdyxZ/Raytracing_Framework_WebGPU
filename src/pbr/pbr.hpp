#pragma once

// Headers
#include "math/vec3.hpp"

// ******** MAPPINGS ********* //

double map_alpha(double perceptive_roughness);
vec3 map_diffuse_reflectance(vec3 albedo, double metalness);

// ******** FRESNEL ********* //

constexpr double min_F0_dielectrics = 0.04;

vec3 compute_F0(optional<double> reflectance, vec3 albedo, double metalness); // Compute the specular reflectance at normal incidence (F0)
double luminance(vec3 rgb);
float shadowed_F90(float F0);

vec3 Schlick_Fresnel(vec3 f0, float f90, float NdotV); // Schlick's approximation to Fresnel term calculated using spherical gaussian approximation

// ******** VISIBILITY ********* //

double Smith_G1_Geometry(double alpha, double alpha_squared, double NdotS, double NdotS_squared); // Smith's G1 term for GGX distribution using the height-correlated method by Lagarde & de Rousiers
double Smith_Visibility(double alpha_squared, double NdotL, double NdotV); // Smith's visibility equation for GGX distribution using height-correlated method by Lagarde & de Rousiers

// ******** DISTRIBUTION ********* //

double GGX_Distribution(double alpha_squared, double NdotH); // GGX distribution function for microfacet normals

// ******** DIFFUSE MODEL ********* //

struct BRDF_Diffuse_Data
{
    vec3 value;     // BRDF value
};

BRDF_Diffuse_Data frostbite_disney_diffuse(vec3 diffuse_reflectance, vec3 albedo, double roughness, double NdotV, double NdotL, double LdotH); //  Lagarde & de Rousiers (Frostbite) version of Disney Diffuse BRDF with energy normalization. 

// ******** SPECULAR MODEL ********* //

struct BRDF_Specular_Data
{
    vec3 F;         // Fresnel term
    double V;       // Visibility term
    double D;       // Distribution term

    vec3 value;     // BRDF value
};

BRDF_Specular_Data cook_torrance_specular(vec3 F0, double alpha_squared, double NdotV, double NdotH, double NdotL); // Microfacet specular Cook-Torrance Specular BRDF 

// ******** BRDF COMBINATION ********* //

struct BRDF_Parameters
{
    vec3 F0;                    // Reflectance at normal incidence
    vec3 diffuse_reflectance;   // Mapped diffuse reflectance
    vec3 albedo;                // Albedo color
    double roughness;           // Perceptually linear roughness
    double alpha;               // Mapped perceptually linear roughness
    double alpha_squared;       // Squared alpha

    vec3 V;                     // View direction
    double NdotV;               // Dot product of shading normal and view direction

    vec3 fresnel_term;          // Fresnel term (we need to pre-calculate it here to avoid recomputing it in the BRDF functions)
    double specular_ratio;      // Ratio of specular to diffuse reflectance
};

BRDF_Parameters prepare_BRDF_params(optional<double> reflectance, vec3 albedo, double roughness, double metalness, vec3 view_direction, vec3 shading_normal); // Compute BRDF data

vec3 combine_BRDFs(BRDF_Diffuse_Data diffuse, BRDF_Specular_Data specular);
