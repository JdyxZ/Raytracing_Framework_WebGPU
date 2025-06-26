// Headers
#include "core/core.hpp"
#include "vec3.hpp"
#include "vec4.hpp"
#include "utils/utilities.hpp"

vec3::vec3() : x(0), y(0), z(0) {}

vec3::vec3(int i) : x(static_cast<double>(i)), y(static_cast<double>(i)), z(static_cast<double>(i)) {}

vec3::vec3(double d) : x(d), y(d), z(d) {}

vec3::vec3(double x, double y, double z) : x(x), y(y), z(z) {}

vec3::vec3(const vec4& v) : x(v.x), y(v.y), z(v.z) {}

vec3::vec3(const glm::vec3& v) : x(v.x), y(v.y), z(v.z) {}

vec3::vec3(const glm::vec4& v) : x(v.x), y(v.y), z(v.z) {}

vec3 vec3::operator-() const
{
    return vec3(-x, -y, -z);
}

double vec3::operator[](int i) const
{
    switch (i)
    {
    case 0:
        return x;
    case 1:
        return y;
    case 2:
        return z;
    default:
        string error = Logger::error("vec3", "Invalid operator access value!!!");
        throw std::invalid_argument(error);
    }
}
double& vec3::operator[](int i)
{
    switch (i)
    {
    case 0:
        return x;
    case 1:
        return y;
    case 2:
        return z;
    default:
        string error = Logger::error("vec4", "Invalid operator access value!!!");
        throw std::invalid_argument(error);
    }
}

vec3& vec3::operator+=(double t)
{
    x += t;
    y += t;
    z += t;
    return *this;
}

vec3& vec3::operator-=(double t)
{
    x -= t;
    y -= t;
    z -= t;
    return *this;
}

vec3& vec3::operator*=(double t)
{
    x *= t;
    y *= t;
    z *= t;
    return *this;
}

vec3& vec3::operator/=(double t)
{
    return *this *= 1 / t;
}

vec3& vec3::operator+=(const vec3& v)
{
    x += v.x;
    y += v.y;
    z += v.z;
    return *this;
}

vec3& vec3::operator-=(const vec3& v)
{
    x -= v.x;
    y -= v.y;
    z -= v.z;
    return *this;
}

vec3& vec3::operator*=(const vec3& v)
{
    x *= v.x;
    y *= v.y;
    z *= v.z;
    return *this;
}

vec3& vec3::operator/=(const vec3& v)
{
    x /= v.x;
    y /= v.y;
    z /= v.z;
    return *this;
}

std::ostream& operator<<(std::ostream& out, const vec3& v)
{
    return out << "[" << v.x << ", " << v.y << ", " << v.z << "]";
}

double vec3::length() const
{
    return std::sqrt(length_squared());
}

double vec3::length_squared() const
{
    return x * x + y * y + z * z;
}

vec3& vec3::normalize()
{
    double len = length();
    x /= len;
    y /= len;
    z /= len;
    return *this;
}

bool vec3::near_zero() const
{
    return (std::fabs(x) < kEpsilon) && (std::fabs(y) < kEpsilon) && (std::fabs(z) < kEpsilon);
}

bool vec3::is_zero() const
{
    return (x == 0) && (y == 0) && (z == 0);
}

vec3 vec3::random()
{
    return vec3(random_number<double>(), random_number<double>(), random_number<double>());
}

vec3 vec3::random(double min, double max)
{
    return vec3(random_number<double>(min, max), random_number<double>(min, max), random_number<double>(min, max));
}

double vec3::max_component() const
{
    return std::max({ x, y, z });
}

double vec3::min_component() const
{
    return std::min({ x, y, z });
}

double vec3::average() const
{
    return (x + y + z) / 3.0;
}
