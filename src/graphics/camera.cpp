// Headers
#include "core/core.hpp"
#include "camera.hpp"
#include "scene.hpp"
#include "ray.hpp"
#include "math/pdf.hpp"
#include "utils/utilities.hpp"
#include "utils/image_writer.hpp"
#include "graphics/color.hpp"
#include "math/interval.hpp"
#include "materials/material.hpp"
#include "materials/texture.hpp"
#include "hittables/triangle.hpp"
#include "graphics/raytracing_renderer.hpp"

// External headers
#include <omp.h>

// Usings
using Raytracing::Scene;
using Raytracing::ImageWriter;
using Raytracing::color;
using Raytracing::RendererSettings;

Raytracing::Camera::Camera()
{
    render_chrono = Chrono();
}

void Raytracing::Camera::initialize(const RendererSettings& settings, std::atomic<float>* render_progress, const Scene& scene, ImageWriter& image)
{
    CameraData data = settings.camera_data;

    vertical_fov = data.vertical_fov;
    defocus_angle = data.defocus_angle;
    focus_distance = data.focus_distance;

    lookfrom = data.lookfrom;
    lookat = data.lookat;
    world_up = data.world_up;

    this->render_progress = render_progress;

    initialize(scene, image);
}

void Raytracing::Camera::initialize(const Scene& scene, ImageWriter& image)
{
    // Determine viewport dimensions
    auto theta = degrees_to_radians(vertical_fov);
    auto h = std::tan(theta / 2);
    viewport_height = 2 * h * focus_distance;
    viewport_width = viewport_height * image.aspect_ratio;

    // Calculate the u,v,w unit basis vectors for the camera coordinate frame.
    view = unit_vector(lookfrom - lookat);
    side = unit_vector(cross(world_up, view));
    up = cross(view, side);

    // Calculate the vectors across the horizontal, down the vertical and orthogonal the viewport edges respectively.
    vec3 viewport_u = viewport_width * side;
    vec3 viewport_v = viewport_height * -up;
    vec3 viewport_w = focus_distance * view;

    // Calculate the horizontal and vertical delta vectors from pixel to pixel.
    pixel_delta_u = viewport_u / static_cast<double>(image.get_width());
    pixel_delta_v = viewport_v / static_cast<double>(image.get_height());

    // Calculate the location of the upper left pixel.
    auto viewport_upper_left = lookfrom - viewport_w - viewport_u / 2.0 - viewport_v / 2.0;
    pixel00_loc = viewport_upper_left + 0.5 * (pixel_delta_u + pixel_delta_v);

    // Calculate the camera defocus disk basis vectors.
    auto defocus_radius = std::tan(degrees_to_radians(defocus_angle / 2.0));
    defocus_disk_u = side * defocus_radius * focus_distance;
    defocus_disk_v = up * defocus_radius * focus_distance;

    // Calculate the square root of samples per pixel and its inverse for stratified sampling
    pixel_sample_sqrt = int(sqrt(scene.samples_per_pixel));
    pixel_sample_sqrt_inv = 1.0 / pixel_sample_sqrt;

    // Log info
    Logger::info("CAMERA", "Camera settings succesfully initialized.");
}

void Raytracing::Camera::render(const Scene& scene, ImageWriter& image, std::stop_token s_token)
{
    // Set number of threads for parallel computing
    int max_threads = omp_get_max_threads();
    int threads_to_use = std::max(1, max_threads - 1); // Leave 1 core free
    omp_set_num_threads(threads_to_use);

    // Log info
    Logger::info("CAMERA", "Rendering started with " + std::to_string(threads_to_use) + " threads.");

    // Aux vars
    auto dynamic_range = image.get_dynamic_range();
    uint32_t total_pixels = image.get_height() * image.get_width();
    uint32_t progress = 0;

    // Start render chrono
    render_chrono.start();

    // Stratified sample square
    #pragma omp parallel for schedule(dynamic, 1) if(scene.parallelize)
    for (int pixel_row = 0; pixel_row < image.get_height(); pixel_row++)
    {
        for (int pixel_column = 0; pixel_column < image.get_width(); pixel_column++)
        {
            // Final pixel color
            color pixel_color(0, 0, 0);

            // Sample points with stratified sampling for antialisasing
            for (int sample_row = 0; sample_row < pixel_sample_sqrt; sample_row++)
            {
                for (int sample_column = 0; sample_column < pixel_sample_sqrt; sample_column++)
                {
                    // Skip execution check
                    if (s_token.stop_requested())
                        continue;

                    // Get ray sample around pixel location
                    auto sample_ray = get_ray_sample(pixel_row, pixel_column, sample_row, sample_column);

                    // Get pixel color of the sample point that ray sample points to
                    pixel_color += ray_color(sample_ray, scene.bounce_max_depth, scene, s_token);
                }
            }

            // Avarage samples
            pixel_color /= scene.samples_per_pixel;

            // Compute color
            auto color_tuple = compute_color(pixel_color, dynamic_range);

            // Save pixel color into image buffer (row-major order)
            image.write_pixel(pixel_row, pixel_column, color_tuple);

            // Update progress atomically
            #pragma omp atomic update
                progress++;
        }

        // Calculate progress percentage
        auto progress_percentage = (static_cast<float>(progress) / static_cast<float>(total_pixels)); 

        // Update progress info for showing
        if (render_progress)
            render_progress->store(progress_percentage);
        else if (!scene.parallelize)
            std::clog << "\rProgress: " << std::fixed << std::setprecision(2) << 100.0f * progress_percentage << '% ' << std::flush;
    }

    // End render chrono
    render_chrono.end();

    // Progress info end line
    std::cout << std::endl;

    // Benchmark rays
    primary_rays = total_pixels * pixel_sample_sqrt * pixel_sample_sqrt;
    rays_casted = primary_rays + background_rays + light_rays + reflected_rays + refracted_rays + unknwon_rays;
    average_rays_per_second = rays_casted / int(render_chrono.elapsed_miliseconds());
}


const Ray Raytracing::Camera::get_ray_sample(int pixel_row, int pixel_column, int sample_row, int sample_column) const
{
    auto offset = sample_square_stratified(sample_row, sample_column, pixel_sample_sqrt_inv);

    auto pixel_sample = pixel00_loc
        + ((pixel_row + offset.y) * pixel_delta_v)
        + ((pixel_column + offset.x) * pixel_delta_u);

    auto ray_origin = (defocus_angle <= 0) ? lookfrom : defocus_disk_sample(lookfrom, defocus_disk_u, defocus_disk_v);
    auto ray_direction = unit_vector(pixel_sample - ray_origin);
    auto ray_time = random_number<double>();

    auto ray = Ray(ray_origin, ray_direction, ray_time);

    return ray;
}

color Raytracing::Camera::ray_color(const Ray& sample_ray, int depth, const Scene& scene, std::stop_token s_token)
{
    // Halt execution check
    if (s_token.stop_requested())
        return color(0.0);

    // If we've exceeded the ray bounce limit, no more light is gathered.
    if (depth <= 0)
        return color(0.0);

    // Intersection details
    hit_record hrec;

    // Define ray intersection interval
    Interval ray_t(scene.min_hit_distance, Raytracing::infinity);

    // Background hit  
    if (!scene.hit(sample_ray, ray_t, hrec))
    {
        #pragma omp atomic update
            background_rays++;

        return compute_background_color(scene, sample_ray);
    }

    // Hit object type
    HITTABLE_TYPE hit_object_type = hrec.type;

    // Check unknown hit
    if (!Hittable::is_primitive_hittable(hit_object_type))
    {
        #pragma omp atomic update
            unknwon_rays++;

        return compute_background_color(scene, sample_ray);
    }

    // Material type
    MATERIAL_TYPE material_type = hrec.material->get_type();

    // Determine material type
    switch (material_type)
    {
        case MATERIAL_TYPE::_PDF:
            return compute_pdf_color(sample_ray, depth, scene, s_token, hrec);
        case MATERIAL_TYPE::NON_PDF:
            return compute_non_pdf_color(sample_ray, depth, scene, s_token, hrec);
        case MATERIAL_TYPE::EMISSIVE:
            return compute_emissive_color(sample_ray, depth, scene, s_token, hrec);
        default:
            string error = Logger::error("CAMERA", "Unknown material type encountered during ray tracing. This should never happen.");
            throw std::runtime_error(error);
            return color(0.0); // Fallback return
    }
}

color Raytracing::Camera::compute_pdf_color(const Ray& sample_ray, int depth, const Scene& scene, std::stop_token s_token, const hit_record& hrec)
{
    // Scatter material to get scattering info
    scatter_record srec;
    hrec.material->scatter(sample_ray, hrec, srec);

    // Aux variables (to make code more readable)
    auto hittables_with_pdf = scene.hittables_with_pdf;
    auto material_pdf = srec.pdf;

    // Create the sampling PDF
    shared_ptr<PDF> sampling_pdf;

    // Determine the sampling PDF
    if (hittables_with_pdf.empty())
    {
        // Material associated samplig PDF
        sampling_pdf = material_pdf;
    }
    else
    {
        // Generate mixture of PDFs (hittable pdf + material pdf)
        auto _hittables_pdf = make_shared<hittables_pdf>(hittables_with_pdf, hrec.p);
        auto _mixture_pdf = make_shared<mixture_pdf>(_hittables_pdf, material_pdf);
        sampling_pdf = _mixture_pdf;
    }

    // Generate random scatter ray using the sampling PDF
    PDFSampleData sample_data = sampling_pdf->generate(sample_ray);
    sample_data.direction.normalize();
    auto scattered_ray = Ray(hrec.p, sample_data.direction, sample_ray.time());

    // Update reflecting rays count
    #pragma omp atomic update
        reflected_rays++;

    // ================== Rendering equation ================== //

    // Recursive call
    color sample_color = ray_color(scattered_ray, depth - 1, scene, s_token);

    // Calculate Bidirectional Reflectance Distribution Function (BRDF) value for the scattered ray
    auto BRDF = hrec.material->BRDF_value(sample_ray, scattered_ray, hrec, srec);

    // Get the weight of the generated scatter ray sample
    auto pdf_value = sampling_pdf->value(sample_data);

    // Get NdotL
    auto cosine_theta = std::fmax(0, dot(hrec.normal, scattered_ray.direction()));

    // Monte carlo estimate of the redering equation
    return (BRDF * sample_color * cosine_theta) / pdf_value;
}

color Raytracing::Camera::compute_non_pdf_color(const Ray& sample_ray, int depth, const Scene& scene, std::stop_token s_token, const hit_record& hrec)
{
    // Scatter material to get scattering info
    scatter_record srec;
    hrec.material->scatter(sample_ray, hrec, srec);

    switch (srec.scatter_type)
    {
    case REFLECT: // Metal or Dielectric
        #pragma omp atomic update
            reflected_rays++;
        break;
    case REFRACT: // Dielectric
        #pragma omp atomic update
            refracted_rays++;
        break;
    }

    // Get scattered ray
    auto scattered_ray = srec.non_pdf_ray.value();

    // Calculate Bidirectional Reflectance Distribution Function (BRDF) value for the scattered ray
    auto BRDF = hrec.material->BRDF_value(sample_ray, scattered_ray, hrec, srec);

    return BRDF * ray_color(srec.non_pdf_ray.value(), depth - 1, scene, s_token);
}

color Raytracing::Camera::compute_emissive_color(const Ray& sample_ray, int depth, const Scene& scene, std::stop_token s_token, const hit_record& hrec)
{
    #pragma omp atomic update
        light_rays++;

    color color_from_emission = hrec.material->emitted(sample_ray, hrec);
    return color_from_emission;
}

Raytracing::color Raytracing::Camera::compute_background_color(const Scene& scene, const Ray& sample_ray) const
{
    switch (scene.background_type)
    {
    case BACKGROUND_TYPE::STATIC_COLOR:
    {
        return scene.background;
    }
    case BACKGROUND_TYPE::GRADIENT:
    {
        vec3 unit_direction = unit_vector(sample_ray.direction());

        auto a = 0.5 * (unit_direction.y + 1.0);
        color start_color = scene.background_primary;
        color end_color = scene.background_secondary;

        return lerp(start_color, end_color, a);
    }
    case BACKGROUND_TYPE::SKYBOX:
    {
        if (!scene.skybox)
        {
            string error = Logger::error("CAMERA", "Skybox background option is set but scene does not contain a skybox texture");
            throw std::runtime_error(error);
        }

        vec3 unit_direction = unit_vector(sample_ray.direction()); // necessary ???
        return scene.skybox->value(unit_direction);
    }
        
    default:
        return scene.background; // Fallback to background color
    }
}

optional<color> Raytracing::Camera::barycentric_color_interpolation(const hit_record& rec, Triangle* t) const
{
    if (!rec.bc.has_value() || !t->has_vertex_colors())
        return nullopt;

    // Barycentric coordinates
    barycentric_coordinates bc = rec.bc.value();
    double u = bc.u;
    double v = bc.v;
    double w = bc.w;

    // Vertex colors
    vec3 color_u = t->A.color.value();
    vec3 color_v = t->B.color.value();
    vec3 color_w = t->C.color.value();

    color c = u * color_u + v * color_v + w * color_w;

    return c;
}
