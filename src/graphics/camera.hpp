#pragma once

// Headers
#include "core/core.hpp"
#include "math/vec3.hpp"
#include "utils/chrono.hpp"
#include "hittables/hittable.hpp"

// Forward declarations
struct Ray;
class Triangle;

// Namespace forward declarations
namespace Raytracing
{
    class Scene;
    struct ImageWriter;
    struct RendererSettings;
}

namespace Raytracing
{
    struct CameraData
    {
        // Camera settings
        double vertical_fov = 90;           
        double defocus_angle = 0;           
        double focus_distance = 10;    

        // Camera position and orientation
        point3 lookfrom = point3(0, 0, 0);  
        point3 lookat = point3(0, 0, -1);  
        vec3   world_up = vec3(0, 1, 0);
    };
}

namespace Raytracing
{
    struct Camera
    {
    public:
        // Camera settings
        double vertical_fov = 90;           // Vertical view angle (field of view)
        double defocus_angle = 0;           // Angle of the cone with apex at viewport center and base at camera center (defocus disk)
        double focus_distance = 10;         // Distance from camera lookfrom point to plane of perfect focus

        // Viewport settings
        double viewport_height;	            // Height of viewport
        double viewport_width;              // Width of viewport

        // Camera position and orientation
        point3 lookfrom = point3(0, 0, 0);  // Point camera is looking from
        point3 lookat = point3(0, 0, -1);   // Point camera is looking at
        vec3   world_up = vec3(0, 1, 0);    // Camera-relative "up" direction

        // Benchmark
        int primary_rays = 0;
        int background_rays = 0;
        int light_rays = 0;
        int reflected_rays = 0;
        int refracted_rays = 0;
        int unknwon_rays = 0;
        int rays_casted = 0;
        int average_rays_per_second = 0;
        Chrono render_chrono;

        Camera();

        void initialize(const Raytracing::RendererSettings& settings, std::atomic<float>* render_progres, const Scene& scene, ImageWriter& image);
        void initialize(const Raytracing::Scene& scene, ImageWriter& image); 
        void render(const Raytracing::Scene& scene, ImageWriter& image, std::stop_token s_token = std::stop_token{});

    private:

        // Internal variables
        point3 pixel00_loc;                                 // Location of pixel 0, 0
        vec3   pixel_delta_u;                               // Offset to pixel to the right
        vec3   pixel_delta_v;                               // Offset to pixel below
        vec3   side, up, view;                              // Camera frame basis vectors
        vec3   defocus_disk_u;                              // Defocus disk horizontal radius
        vec3   defocus_disk_v;                              // Defocus disk vertical radius    
        int    pixel_sample_sqrt;	                        // Square root of samples per pixel
        double pixel_sample_sqrt_inv;                       // Inverse of square root of samples per pixel
        std::atomic<float>* render_progress = nullptr;    // Atomic pointer to render progress for ImGui progress bar

        // Auxiliar variables
        vector<unsigned long long> elapsed_nanoseconds;

        const Ray get_ray_sample(int pixel_row, int pixel_column, int sample_row, int sample_column) const; // Construct a camera ray originating from the defocus disk and directed at randomly sampled point around the pixel location pixel_row, pixel_column for stratified sample square sample_row, sample_column.
        Raytracing::color ray_color(const Ray& sample_ray, int depth, const Raytracing::Scene& scene, std::stop_token s_token);

        color compute_pdf_color(const Ray& sample_ray, int depth, const Scene& scene, std::stop_token s_token, const hit_record& hrec);
        color compute_non_pdf_color(const Ray& sample_ray, int depth, const Scene& scene, std::stop_token s_token, const hit_record& hrec);
        color compute_emissive_color(const Ray& sample_ray, int depth, const Scene& scene, std::stop_token s_token, const hit_record& hrec);

        Raytracing::color compute_background_color(const Raytracing::Scene& scene, const Ray& sample_ray) const;
        optional<Raytracing::color> barycentric_color_interpolation(const hit_record& rec, Triangle* t) const;

    };
}



