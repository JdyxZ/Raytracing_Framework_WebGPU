#pragma once

// Internal Headers
#include "core/core.hpp"
#include "math/vec3.hpp"
#include "math/vec4.hpp"

// External headers
#include "external/magic_enum.hpp"

// ************** TIMESTAMP UTILITIES ************** //

bool is_valid_timestamp_format(const char* fmt);
string get_current_timestamp(const char* fmt = "%a %b %d %H_%M_%S %Y", bool miliseconds = false);

// ************** FILE UTILITIES ************** //

struct file_size
{
    double amount;
    string unit;
};

file_size get_file_size(const string& file_path);

// ************** STRING UTILITIES ************** //

string to_list(const vector<string>& vec);
string trim(const string& str);
string trim_trailing_zeros(const double number, const bool remove_point = true);

// ************** ENUM UTILITIES ************** //

template<typename Enum>
inline vector<const char*> get_enum_names()
{
    static const auto names_vector = magic_enum::enum_names<Enum>();
    static vector<const char*> names_array;
    names_array.resize(magic_enum::enum_count<Enum>());

    for (std::size_t i = 0; i < names_vector.size(); ++i)
        names_array[i] = names_vector[i].data();

    return names_array;
}

// ************** MATH UTILITIES ************** //

constexpr double degrees_to_radians(double degrees) 
{ 
    return degrees * Raytracing::pi / 180.0; 
}

constexpr double radians_to_degrees(double radians)
{
    return radians * 180.0 / Raytracing::pi;
}

template<arithmetic T>
inline T random_number()     // This function is thread safe
{
    thread_local static std::mt19937 generator
    (
        std::random_device{}() ^ // XORing with thread ID hash for better uniqueness
        static_cast<unsigned int>(std::hash<std::thread::id>{}(std::this_thread::get_id()))
    );

    if constexpr (std::is_integral_v<T>)
    {
        static std::uniform_int_distribution<T> distribution(0, 1);
        return distribution(generator); // Returns a random T in [0, 1].
    }
    else
    {
        static std::uniform_real_distribution<T> distribution(0, 1);
        return distribution(generator); // Returns a random T in [0, 1).
    }
}

template<arithmetic T>
inline T random_number(T min = T(0), T max = T(1))  // This function is thread safe
{
    thread_local static std::mt19937 generator
    (
        std::random_device{}() ^ // XORing with thread ID hash for better uniqueness
        static_cast<unsigned int>(std::hash<std::thread::id>{}(std::this_thread::get_id()))
    );

    if constexpr (std::is_integral_v<T>)
    {
        std::uniform_int_distribution<T> distribution(min, max); 
        return distribution(generator); // Returns a random T in [min,max].
    }
    else
    {
        std::uniform_real_distribution<T> distribution(min, max);
        return distribution(generator); // Returns a random T in [min,max).
    }
}

// ************** VECTOR UTILITIES ************** //

inline vec3 normalize(vec3 v)
{
    return v.normalize();
}

inline double dot(const vec3& u, const vec3& v)
{
    return u.x * v.x + u.y * v.y + u.z * v.z;
}

inline double dot(const vec4& u, const vec4& v)
{
    return u.x * v.x + u.y * v.y + u.z * v.z + u.w * u.z;
}

inline vec3 cross(const vec3& u, const vec3& v)
{
    return vec3
    (
        u.y * v.z - u.z * v.y,
        u.z * v.x - u.x * v.z,
        u.x * v.y - u.y * v.x
    );
}

inline vec3 unit_vector(const vec3& v)
{
    return v / v.length();
}

inline vec4 unit_vector(const vec4& v)
{
    return v / v.length();
}

inline vec3 reflect(const vec3& v, const vec3& n)
{
    return v - 2 * dot(v, n) * n;
}

inline vec3 refract(const vec3& incoming_ray, const vec3& n, double cos_theta, double refraction_index)
{
    vec3 refracted_ray_perpendicular = refraction_index * (incoming_ray + cos_theta * n);
    vec3 refracted_ray_parellel = -std::sqrt(std::fabs(1.0 - refracted_ray_perpendicular.length_squared())) * n;
    return refracted_ray_perpendicular + refracted_ray_parellel;
}

inline vec3 lerp(vec3 start, vec3 end, double a)
{
    return (1.0 - a) * start + a * end;
}

inline vec3 lerp(vec3 start, vec3 end, bool b)
{
    return b ? end : start;
}

inline vec3 lerp(vec3 start, vec3 end, vec3 a)
{
    return (1.0 - a) * start + a * end;
}

inline vec3 min_vector(const vec3& v1, const vec3& v2)
{
    return vec3(std::fmin(v1.x, v2.x), std::fmin(v1.y, v2.y), std::fmin(v1.z, v2.z));
}

inline vec3 max_vector(const vec3& v1, const vec3& v2)
{
    return vec3(std::fmax(v1.x, v2.x), std::fmax(v1.y, v2.y), std::fmax(v1.z, v2.z));
}

// ************** STD VECTOR UTILITIES ************** //

template <arithmetic T>
inline T vector_mean(const vector<T>& data)
{
    if (data.empty())
        return 0.0;

    auto sum = std::accumulate(data.begin(), data.end(), T{ 0 });
    auto mean = sum / data.size();
    return static_cast<T>(mean);
}

template <arithmetic T>
inline T vector_min_value(const vector<T>& data)
{
    if (data.empty())
        return 0.0;

    auto min = *std::ranges::min_element(data);
    return static_cast<T>(min);
}

template <arithmetic T>
inline T vector_max_value(const vector<T>& data)
{
    if (data.empty())
        return 0.0;

    auto max = *std::ranges::max_element(data);
    return static_cast<T>(max);
}

template <arithmetic T>
inline T vector_standard_deviation(const vector<T>& data)
{
    if (data.empty())
        return 0.0;

    double avg = vector_mean(data);

    auto squared_diffs = data
        | std::views::transform([avg](T x) {
        T diff = x - avg;
        return diff * diff;
            });

    double sq_sum = std::accumulate(squared_diffs.begin(), squared_diffs.end(), 0.0);
    auto standard_deviation = std::sqrt(sq_sum / data.size());

    return static_cast<T>(standard_deviation);
}

template <arithmetic T>
inline T vector_sum(const vector<T>& data)
{
    auto sum = std::accumulate(data.begin(), data.end(), T{ 0 });
    return static_cast<T>(sum);
}

template <arithmetic T>
inline void print_vector_statistics(const string name, const std::vector<T>& data)
{
    std::cout << name << " statistics:" << std::endl
        << "\t" << "Size: " << data.size() << std::endl
        << "\t" << "Sum: " << vector_sum(data) / pow(10,6) << std::endl
        << "\t" << "Min: " << vector_min_value(data) << std::endl
        << "\t" << "Max: " << vector_max_value(data) << std::endl
        << "\t" << "Mean: " << vector_mean(data) << std::endl
        << "\t" << "Standard Deviation: " << vector_standard_deviation(data) << std::endl
        ;
}

// ************** INTERVAL UTILITIES ************** //
enum class BoundType
{
    inclusive,
    exclusive
};

template <std::totally_ordered T> // Only comparable types
inline bool is_within(T value, T lower, T upper, BoundType lower_bound, BoundType upper_bound)
{
    bool lower_ok = (lower_bound == BoundType::inclusive) ? (value >= lower) : (value > lower);
    bool upper_ok = (upper_bound == BoundType::inclusive) ? (value <= upper) : (value < upper);
    return lower_ok && upper_ok;
}

// ************** SAMPLE UTILITIES ************** //

inline vec3 sample_square() // Returns the vector to a random point in the [-.5,-.5]-[+.5,+.5] unit square.
{
    return vec3(random_number<double>() - 0.5, random_number<double>() - 0.5, 0);
}

inline vec3 sample_square_stratified(int sample_row, int sample_column, double pixel_sample_sqrt_inv) // Returns the vector to a random point in the square sub-pixel specified by grid indices sample_row and sample_column, for an idealized unit square pixel [-.5,-.5] to [+.5,+.5].
{
    auto px = ((sample_column + random_number<double>()) * pixel_sample_sqrt_inv) - 0.5;
    auto py = ((sample_row + random_number<double>()) * pixel_sample_sqrt_inv) - 0.5;

    return vec3(px, py, 0);
}

inline vec3 random_in_unit_disk() // Returns a random point in the unit disk.
{
    while (true)
    {
        auto p = vec3(random_number<double>(-1, 1), random_number<double>(-1, 1), 0);
        if (p.length_squared() < 1)
            return p;
    }
}

inline vec3 sample_disk(double radius) // Returns a random point in the disk centered at origin.
{
    return radius * random_in_unit_disk();
}

inline point3 defocus_disk_sample(vec3 center, vec3 defocus_disk_u, vec3 defocus_disk_v) // Returns a random point in the defocus disk given.
{
    auto p = random_in_unit_disk();
    return center + (p[0] * defocus_disk_u) + (p[1] * defocus_disk_v);
}

inline vec3 random_unit_vector()
{
    while (true)
    {
        auto p = vec3::random(-1, 1);
        auto lensq = p.length_squared();
        if (practically_zero < lensq && lensq <= 1.0)
            return p / sqrt(lensq);
    }
}

inline vec3 random_on_hemisphere(const vec3& normal)
{
    vec3 unit_vector_on_sphere = random_unit_vector();

    // If the vector is in the same hemisphere than the normal return it, otherwise return its opposite.
    return (dot(unit_vector_on_sphere, normal) > 0.0) ? unit_vector_on_sphere : -unit_vector_on_sphere;
}

inline vec3 random_cosine_hemisphere_direction()
{
    auto r1 = random_number<double>();
    auto r2 = random_number<double>();

    auto phi = 2 * Raytracing::pi * r1;
    auto x = std::cos(phi) * std::sqrt(r2);
    auto y = std::sin(phi) * std::sqrt(r2);
    auto z = std::sqrt(1 - r2);

    return vec3(x, y, z);
}
