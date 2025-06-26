// Headers
#include "core/core.hpp" 
#include "pdf.hpp"
#include "utils/utilities.hpp"
#include "vec3.hpp"
#include "hittables/hittable.hpp"
#include "pbr/pbr.hpp"
#include "ray.hpp"

// Usings
using Raytracing::pi;

uniform_sphere_pdf::uniform_sphere_pdf() {}

double uniform_sphere_pdf::value(const PDFSampleData& sample) const
{
    return 1 / (4 * pi);
}

PDFSampleData uniform_sphere_pdf::generate(const Ray& incoming_ray) const
{
    PDFSampleData sample_data;
    sample_data.direction = random_unit_vector();
    return sample_data;
}

cosine_hemisphere_pdf::cosine_hemisphere_pdf(const vec3& normal)
{
    uvw = ONB(normal);
}

double cosine_hemisphere_pdf::value(const PDFSampleData& sample) const
{
    auto cosine_theta = dot(uvw.w(), sample.direction);
    return std::fmax(0, cosine_theta / pi);
}

PDFSampleData cosine_hemisphere_pdf::generate(const Ray& incoming_ray) const
{
    PDFSampleData sample_data;

    // Generate a random cosine-weighted hemisphere direction
    vec3 scatter_direction = random_cosine_hemisphere_direction();

    // Intercept degenerate scatter direction (if the direction is near zero, scatter along the normal)
    // if (scatter_direction.near_zero())
        // scatter_direction = uvw.w(); 

    // Transform the scatter direction to the uvw space
    scatter_direction = uvw.transform(scatter_direction);
    sample_data.direction = scatter_direction;

    return sample_data;
}

vndf_pdf::vndf_pdf(double alpha, double alpha_squared, vec3 view_direciton) : alpha(alpha), alpha_squared(alpha_squared), view_direction(view_direction) {}

double vndf_pdf::value(const PDFSampleData& sample) const
{
    // Get the halfway vector from the sample data
    vec3 halfway_vector = sample.halfway_vector.value();

    // Parameters
    double alpha_squared_clamped = std::max(0.00001, alpha_squared);

    // Clamp dot products here to small value to prevent numerical instability. Assume that rays incident from below the hemisphere have been filtered
    double VdotH = std::clamp(dot(view_direction, halfway_vector), 0.00001, 1.0);
    double NdotH = std::clamp(halfway_vector.z, 0.00001, 1.0); // The input vectors are all in the same local space where the geometric normal is vec3(0,0,1)
    double NdotV = std::clamp(view_direction.z, 0.00001, 1.0);

    // Calculate the PDF of the halfway vector H. The distribution of visible normals is proportional to D * G1.
    double pdf_H = GGX_Distribution(alpha_squared_clamped, NdotH) * Smith_G1_Geometry(alpha, NdotV, alpha_squared, NdotV * NdotV);

    // Convert the PDF of H to the PDF of L by dividing by the Jacobian of reflection operator (4 * VdotH).
    double pdf_L = pdf_H / (4.0 * VdotH);

    return pdf_L;
}

PDFSampleData vndf_pdf::generate(const Ray& incoming_ray) const
{
    PDFSampleData sample_data;

    // Transform the view direction to the hemisphere configuration
    vec3 hemisphere_view = vec3(alpha * view_direction.x, alpha * view_direction.y, view_direction.z).normalize();

    // Generate a random 2D point in the range [0, 1]
    double u_x = random_number<double>();
    double u_y = random_number<double>();

    // Sample a spherical cap in (-Vh.z, 1]
    double phi = 2.0 * Raytracing::pi * u_x;
    double z = ((1.0 - u_y) * (1.0 + hemisphere_view.z)) - hemisphere_view.z;
    double sin_theta = std::sqrt(std::clamp(1.0f - z * z, 0.0, 1.0));
    double x = sin_theta * cos(phi);
    double y = sin_theta * sin(phi);

    // Compute halfway direction
    vec3 hemisphere_halfway = vec3(x, y, z) + hemisphere_view;

    // Transform the halfway direction back to the original space
    vec3 generated_halfway = vec3(alpha * hemisphere_halfway.x, alpha * hemisphere_halfway.y, std::max(0.0, hemisphere_halfway.z)).normalize();

    // Generate the sample direction based on the halfway vector and view direction
    vec3 sample_direction = reflect(-view_direction, generated_halfway);

    // Set sample data
    sample_data.direction = sample_direction;
    sample_data.halfway_vector = generated_halfway;

    return sample_data;
}

hittable_pdf::hittable_pdf(shared_ptr<Hittable> object, const point3& hit_point)
    : object(object), hit_point(hit_point)
{}

double hittable_pdf::value(const PDFSampleData& sample) const
{
    return object->pdf_value(hit_point, sample.direction);
}

PDFSampleData hittable_pdf::generate(const Ray& incoming_ray) const
{
    PDFSampleData sample_data;
    sample_data.direction = object->random_scattering_ray(hit_point);

    return sample_data;
}

hittables_pdf::hittables_pdf(const vector<shared_ptr<Hittable>>& hittables, const point3& hit_point)
    : hittables(hittables), hit_point(hit_point)
{}

double hittables_pdf::value(const PDFSampleData& sample) const
{
    if (hittables.size() == 0)
        return 0.0;

    auto size = hittables.size();
    auto weight = 1.0 / size;
    auto sum = 0.0;

    for (int i = 0; i < size; i++)
    {
        const auto& object = hittables[i];
        sum += weight * object->pdf_value(hit_point, sample.direction);
    }

    return sum;
}

PDFSampleData hittables_pdf::generate(const Ray& incoming_ray) const
{
    PDFSampleData sample_data;

    auto size = int(hittables.size());
    auto random_object_index = random_number<int>(0, size - 1);
    sample_data.direction = hittables[random_object_index]->random_scattering_ray(hit_point);

    return sample_data;
}

mixture_pdf::mixture_pdf(shared_ptr<PDF> p0, shared_ptr<PDF> p1) : mixture_pdf(p0, p1, 0.5) {}

mixture_pdf::mixture_pdf(shared_ptr<PDF> p0, shared_ptr<PDF> p1, double w0)
{
    p[0] = p0;
    p[1] = p1;
    w[0] = w0;
    w[1] = 1.0 - w0; // Ensure that the weights sum to 1
}

double mixture_pdf::value(const PDFSampleData& sample) const
{
    return w[0] * p[0]->value(sample) + w[1] * p[1]->value(sample);
}

PDFSampleData mixture_pdf::generate(const Ray& incoming_ray) const
{
    if (random_number<double>() < w[0])
        return p[0]->generate(incoming_ray);
    else
        return p[1]->generate(incoming_ray);
}
